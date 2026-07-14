#pragma once

#include "parser.h"
#include "anomalies.h"
#include "limits.h"
#include <algorithm>
#include <cctype>
#include <string>

namespace libglot::mime {

/// ============================================================================
/// RFC 2046 Boundary Delimiter Matching
/// ============================================================================
///
/// A boundary delimiter line is:
///   CRLF "--" boundary [ "--" ] *WSP CRLF
/// - It must start at the beginning of a line (position 0 or right after a
///   line break); boundary text appearing mid-line is part content.
/// - The line break immediately preceding the delimiter belongs to the
///   delimiter, not to the previous part's content.
/// - "--boundary--" is the close delimiter; content after it is the
///   epilogue, content before the first delimiter is the preamble.
/// ============================================================================

struct BoundaryDelimiter {
    bool found = false;
    bool is_close = false;    ///< Close delimiter ("--boundary--")
    size_t line_start = 0;    ///< Position of the "--" that starts the line
    size_t content_end = 0;   ///< End of preceding part content (excludes the
                              ///< line break owned by the delimiter)
    size_t next_pos = 0;      ///< Position just past the delimiter line
};

/// Find the next RFC 2046 boundary delimiter line at or after `from`.
inline BoundaryDelimiter find_boundary_delimiter(std::string_view body,
                                                 std::string_view boundary,
                                                 size_t from) {
    BoundaryDelimiter result;
    if (boundary.empty()) {
        return result;
    }

    const std::string marker = "--" + std::string(boundary);

    size_t pos = from;
    while (pos < body.size()) {
        size_t p = body.find(marker, pos);
        if (p == std::string_view::npos) {
            return result;
        }

        // Must be at the start of a line (CRLF, LF, or bare CR before it)
        if (p != 0 && body[p - 1] != '\n' && body[p - 1] != '\r') {
            pos = p + 1;
            continue;
        }

        size_t q = p + marker.size();
        bool is_close = false;
        if (body.substr(q, 2) == "--") {
            is_close = true;
            q += 2;
        }

        // Optional transport padding (whitespace) after the marker
        while (q < body.size() && (body[q] == ' ' || body[q] == '\t')) {
            ++q;
        }

        // The rest of the line must be empty (line break or end of body);
        // otherwise the boundary text merely appears as a prefix of some
        // longer token and this is NOT a delimiter line.
        if (q != body.size() && body[q] != '\n' && body[q] != '\r') {
            pos = p + 1;
            continue;
        }

        result.found = true;
        result.is_close = is_close;
        result.line_start = p;

        // The line break before the delimiter belongs to the delimiter
        size_t content_end = p;
        if (content_end > 0 && body[content_end - 1] == '\n') {
            --content_end;
            if (content_end > 0 && body[content_end - 1] == '\r') {
                --content_end;
            }
        } else if (content_end > 0 && body[content_end - 1] == '\r') {
            --content_end;
        }
        result.content_end = content_end;

        // Skip past the delimiter line's own break
        size_t next = q;
        if (next < body.size()) {
            if (body[next] == '\r') {
                ++next;
                if (next < body.size() && body[next] == '\n') {
                    ++next;
                }
            } else if (body[next] == '\n') {
                ++next;
            }
        }
        result.next_pos = next;
        return result;
    }

    return result;
}

/// ============================================================================
/// Extended MIME Parser with Multipart and Anomaly Detection
/// ============================================================================

class MimeParserExtended : public MimeParser {
public:
    explicit MimeParserExtended(libglot::Arena& arena, std::string_view source,
                                ParserLimits limits = ParserLimits::standard())
        : MimeParser(arena, source)
        , limits_(limits)
    {
        tracker_.start_parse();
    }

    /// Anomalies recorded while parsing (limits exceeded, missing final
    /// boundary, ...). Populated by the multipart parse path.
    [[nodiscard]] const AnomalyReport& anomalies() const noexcept {
        return report_;
    }

    /// Parse message with multipart support
    Message* parse_message_multipart() {
        std::vector<Header*> headers;

        // Parse headers until blank line or EOF
        while (!this->check(TK::EOF_TOKEN)) {
            if (this->check(TK::NEWLINE)) {
                this->advance();
                break;
            }
            headers.push_back(parse_header_with_parameters());
        }

        // Extract Content-Type to check for multipart
        std::string_view content_type;
        std::string_view boundary;
        for (auto* hdr : headers) {
            if (hdr->field == "Content-Type" || hdr->field == "content-type") {
                content_type = hdr->value;
                // Extract boundary parameter
                for (const auto& param : hdr->parameters) {
                    if (param.first == "boundary") {
                        boundary = param.second;
                        break;
                    }
                }
                break;
            }
        }

        // Get body
        std::string_view body = "";
        if (this->check(TK::EOF_TOKEN)) {
            size_t body_start = this->current().start;
            if (body_start < source_.size()) {
                body = source_.substr(body_start);
            }
        }

        auto* msg = this->template create_node<Message>(headers, body);

        // Parse multipart if boundary is present
        if (!boundary.empty() && is_multipart(content_type)) {
            msg->parts = parse_multipart_body(body, boundary);
        }

        return msg;
    }

    /// Parse header with parameters (Content-Type: text/plain; charset=utf-8)
    Header* parse_header_with_parameters() {
        // Field name
        if (!this->check(TK::IDENTIFIER)) {
            this->error("Expected header field name");
        }
        auto field_tok = this->advance();

        // Colon
        if (!this->match(TK::COLON)) {
            this->error("Expected ':' after header field name");
        }

        // Value (may be empty)
        std::string_view value = "";
        if (this->check(TK::STRING)) {
            value = this->advance().text;
        }

        // Newline
        if (!this->match(TK::NEWLINE)) {
            this->error("Expected newline after header value");
        }

        auto* header = this->template create_node<Header>(field_tok.text, value);

        // Parse parameters from value
        header->parameters = parse_parameters(value);

        return header;
    }

private:
    /// Check if Content-Type indicates multipart
    bool is_multipart(std::string_view content_type) const {
        // Simple check - could be more sophisticated
        return content_type.find("multipart/") == 0;
    }

    /// Parse parameters from header value (e.g., "text/plain; charset=utf-8")
    std::vector<std::pair<std::string_view, std::string_view>> parse_parameters(std::string_view value) {
        std::vector<std::pair<std::string_view, std::string_view>> params;

        // Find semicolon that starts parameters
        size_t semi_pos = value.find(';');
        if (semi_pos == std::string_view::npos) {
            return params;  // No parameters
        }

        // Parse each parameter
        size_t pos = semi_pos + 1;
        while (pos < value.size()) {
            // Skip whitespace
            while (pos < value.size() && std::isspace(value[pos])) {
                ++pos;
            }
            if (pos >= value.size()) break;

            // Find parameter name
            size_t name_start = pos;
            while (pos < value.size() && value[pos] != '=' && value[pos] != ';') {
                ++pos;
            }
            if (pos >= value.size() || value[pos] != '=') break;

            std::string_view param_name = value.substr(name_start, pos - name_start);
            // Trim trailing whitespace from name
            while (!param_name.empty() && std::isspace(param_name.back())) {
                param_name.remove_suffix(1);
            }

            ++pos;  // Skip '='

            // Skip whitespace after =
            while (pos < value.size() && std::isspace(value[pos])) {
                ++pos;
            }

            // Parse parameter value (may be quoted)
            std::string_view param_value;
            if (pos < value.size() && value[pos] == '"') {
                // Quoted value
                ++pos;  // Skip opening quote
                size_t value_start = pos;
                while (pos < value.size() && value[pos] != '"') {
                    ++pos;
                }
                param_value = value.substr(value_start, pos - value_start);
                if (pos < value.size()) ++pos;  // Skip closing quote
            } else {
                // Unquoted value (until semicolon or end)
                size_t value_start = pos;
                while (pos < value.size() && value[pos] != ';') {
                    ++pos;
                }
                param_value = value.substr(value_start, pos - value_start);
                // Trim trailing whitespace
                while (!param_value.empty() && std::isspace(param_value.back())) {
                    param_value.remove_suffix(1);
                }
            }

            params.emplace_back(param_name, param_value);

            // Skip to next parameter
            while (pos < value.size() && value[pos] != ';') {
                ++pos;
            }
            if (pos < value.size() && value[pos] == ';') {
                ++pos;
            }
        }

        return params;
    }

    /// Parse multipart body by splitting on RFC 2046 boundary delimiter lines.
    /// Content before the first delimiter (preamble) and after the close
    /// delimiter (epilogue) is discarded. Enforces nesting-depth and
    /// part-count limits; violations stop parsing cleanly and are recorded
    /// as anomalies.
    std::vector<Part*> parse_multipart_body(std::string_view body, std::string_view boundary) {
        std::vector<Part*> parts;

        if (boundary.empty()) {
            return parts;
        }

        // DoS protection: cap recursion into nested multiparts
        if (tracker_.current_nesting_depth >= limits_.max_nesting_depth) {
            record_anomaly(AnomalyKind::ExcessiveNestingDepth,
                           "multipart nesting depth limit reached; not descending further");
            return parts;
        }
        tracker_.enter_level();

        auto delim = find_boundary_delimiter(body, boundary, 0);
        if (!delim.found) {
            tracker_.exit_level();
            return parts;
        }

        // Everything before the first delimiter is the preamble (discarded)
        bool closed = delim.is_close;
        size_t part_start = delim.next_pos;

        while (!closed) {
            // DoS protection: cap total number of parts
            if (tracker_.total_parts >= limits_.max_total_parts) {
                record_anomaly(AnomalyKind::ExcessivePartCount,
                               "multipart part count limit reached; remaining parts skipped");
                break;
            }

            auto next = find_boundary_delimiter(body, boundary, part_start);

            // The line break preceding a delimiter belongs to the delimiter,
            // not the part content. If no further delimiter exists, the final
            // close delimiter is missing: recover by taking the rest of the
            // body as the last part.
            size_t content_end = next.found ? std::max(next.content_end, part_start)
                                            : body.size();

            tracker_.add_part();
            Part* part = parse_part(body.substr(part_start, content_end - part_start));
            if (part) {
                parts.push_back(part);
            }

            if (!next.found) {
                record_anomaly(AnomalyKind::MissingFinalBoundary,
                               "multipart body lacks the final close delimiter (--boundary--)");
                break;
            }

            closed = next.is_close;
            part_start = next.next_pos;
        }

        // Everything after the close delimiter is the epilogue (discarded)
        tracker_.exit_level();
        return parts;
    }

    /// Parse a single MIME part (headers + body)
    Part* parse_part(std::string_view content) {
        std::vector<Header*> headers;

        // Split headers from body at the first empty line (CRLF, LF, or
        // lenient bare CR conventions all supported).
        auto [headers_end, body_start] = find_blank_line(content);

        std::string_view headers_text;
        std::string_view body_text;

        if (headers_end != std::string_view::npos) {
            headers_text = content.substr(0, headers_end);
            body_text = content.substr(body_start);
        } else {
            // No blank line: header-only part if the first line looks like a
            // header field, otherwise the entire content is the body.
            size_t colon_pos = content.find(':');
            size_t eol = content.find_first_of("\r\n");
            if (colon_pos != std::string_view::npos &&
                (eol == std::string_view::npos || colon_pos < eol)) {
                headers_text = content;
            } else {
                body_text = content;
            }
        }

        // Unfold folded (continuation) header lines before splitting
        if (HeaderFolding::is_folded(headers_text)) {
            headers_text = this->arena().copy_source(HeaderFolding::unfold_headers(headers_text));
        }

        // Parse headers (simple line-by-line)
        size_t line_start = 0;
        while (line_start < headers_text.size()) {
            size_t line_end = headers_text.find_first_of("\r\n", line_start);
            size_t next_line;
            if (line_end == std::string_view::npos) {
                line_end = headers_text.size();
                next_line = line_end;
            } else {
                next_line = line_end + 1;
                if (headers_text[line_end] == '\r' && next_line < headers_text.size() &&
                    headers_text[next_line] == '\n') {
                    ++next_line;
                }
            }

            std::string_view line = headers_text.substr(line_start, line_end - line_start);

            // Parse header line
            size_t colon_pos = line.find(':');
            if (colon_pos != std::string_view::npos) {
                std::string_view field = line.substr(0, colon_pos);
                std::string_view value = line.substr(colon_pos + 1);

                // Trim leading whitespace from value
                while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
                    value.remove_prefix(1);
                }

                auto* header = this->template create_node<Header>(field, value);
                header->parameters = parse_parameters(value);
                headers.push_back(header);
            }

            line_start = next_line;
        }

        // Create part
        auto* part = this->template create_node<Part>(headers, body_text);

        // Check if this part is also multipart
        for (auto* hdr : headers) {
            if (hdr->field == "Content-Type" || hdr->field == "content-type") {
                if (is_multipart(hdr->value)) {
                    // Extract boundary
                    for (const auto& param : hdr->parameters) {
                        if (param.first == "boundary") {
                            part->parts = parse_multipart_body(body_text, param.second);
                            break;
                        }
                    }
                }
                break;
            }
        }

        return part;
    }

    /// Find the first empty line in `content`.
    /// Returns {headers_end, body_start}: headers_end is the position where
    /// the header section ends (start of the blank line), body_start is the
    /// position just past the blank line. Returns {npos, npos} when no blank
    /// line exists.
    static std::pair<size_t, size_t> find_blank_line(std::string_view content) {
        size_t line_start = 0;
        while (line_start < content.size()) {
            size_t eol = content.find_first_of("\r\n", line_start);
            if (eol == std::string_view::npos) {
                break;  // Last line has no terminator: no blank line found
            }

            size_t next = eol + 1;
            if (content[eol] == '\r' && next < content.size() && content[next] == '\n') {
                ++next;
            }

            if (eol == line_start) {
                // Empty line: headers end here, body starts after it
                return {line_start, next};
            }

            line_start = next;
        }
        return {std::string_view::npos, std::string_view::npos};
    }

protected:
    /// Record an anomaly detected during parsing
    void record_anomaly(AnomalyKind kind, std::string_view detail) {
        report_.add(kind, AnomalyConfig::get_severity(kind), AnomalyPolicy::Repair,
                    SourceLocation{}, "", detail);
    }

    ParserLimits limits_;
    LimitTracker tracker_;
    AnomalyReport report_;
};

} // namespace libglot::mime
