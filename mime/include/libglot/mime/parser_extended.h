#pragma once

#include "anomalies.h"
#include "boundary.h"
#include "charset.h"
#include "complete_features.h"
#include "limits.h"
#include "mime_type_validator.h"
#include "parser.h"
#include <algorithm>
#include <cctype>
#include <string>
#include <utility>
#include <vector>

namespace libglot::mime {

/// ============================================================================
/// Extended MIME Parser - The Parsing Pipeline
/// ============================================================================
///
/// MimeParserExtended is the engine behind the one public entry point,
/// parse_message() in mime.h (see that header for the pipeline overview).
/// On top of MimeParser (header tokenization + unfolding) it adds:
///
/// - RFC 2046 multipart splitting (boundary.h) with nesting/part limits
/// - Header enhancement: RFC 5322 comment stripping, RFC 2231 parameter
///   continuations with percent/charset decoding, address-group parsing,
///   and message/external-body references (complete_features.h)
/// - Content-Type syntax validation (mime_type_validator.h)
/// - Structural anomaly detection (duplicate/missing headers, boundary
///   issues) recorded against an AnomalyConfig: Ignore-policy anomalies are
///   dropped, Repair-policy ones recorded, and Reject-policy anomalies of
///   Security/DoS severity mark the parse rejected and stop further
///   multipart descent.
/// ============================================================================

namespace detail {

/// Case-insensitive ASCII string comparison (header field names, media types)
inline bool ascii_ieq(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

inline std::string ascii_lower(std::string_view text) {
    std::string lower(text);
    for (char& c : lower) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return lower;
}

/// Media type of a Content-Type value: text up to the first ';', trimmed
inline std::string_view media_type_of(std::string_view content_type_value) {
    size_t semi = content_type_value.find(';');
    std::string_view media =
        (semi == std::string_view::npos) ? content_type_value : content_type_value.substr(0, semi);
    while (!media.empty() && (media.front() == ' ' || media.front() == '\t')) {
        media.remove_prefix(1);
    }
    while (!media.empty() && (media.back() == ' ' || media.back() == '\t')) {
        media.remove_suffix(1);
    }
    return media;
}

} // namespace detail

class MimeParserExtended : public MimeParser {
public:
    explicit MimeParserExtended(libglot::Arena& arena, std::string_view source,
                                ParserLimits limits = ParserLimits::standard(),
                                AnomalyConfig config = AnomalyConfig::standard())
        : MimeParser(arena, source), limits_(limits), config_(config) {
        tracker_.start_parse();
    }

    /// Anomalies recorded while parsing (limits exceeded, missing final
    /// boundary, invalid Content-Type, structural issues, ...).
    [[nodiscard]] const AnomalyReport& anomalies() const noexcept { return report_; }

    /// True when a Reject-policy anomaly of Security/DoS severity was hit;
    /// the returned message tree is then partial and should not be trusted.
    [[nodiscard]] bool rejected() const noexcept { return rejected_; }

    /// Parse message through the full pipeline: headers (with parameters),
    /// header enhancement, multipart splitting, structural anomaly detection.
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

        // Get body
        std::string_view body = "";
        if (this->check(TK::EOF_TOKEN)) {
            size_t body_start = this->current().start;
            if (body_start < source_.size()) {
                body = source_.substr(body_start);
            }
        }

        auto* msg = this->template create_node<Message>(headers, body);

        // Enhance headers and descend into multipart / external-body content
        finish_message(msg);

        // Post-parse structural anomaly detection (top-level message)
        detect_missing_headers(msg);
        detect_duplicate_headers(msg);
        detect_content_type_issues(msg);
        detect_boundary_issues(msg);

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
        return content_type.find("multipart/") == 0;
    }

    /// Header fields where RFC 5322 comments "(...)" are syntax, not content
    static bool is_structured_field(std::string_view field) {
        static constexpr std::string_view kStructured[] = {
            "Content-Type",
            "Content-Disposition",
            "Content-Transfer-Encoding",
            "MIME-Version",
            "Date",
            "From",
            "To",
            "Cc",
            "Bcc",
            "Sender",
            "Reply-To",
            "Message-ID",
            "In-Reply-To",
            "References",
            "Content-ID",
            "Content-Location",
            "Content-Description",
            "Content-Language",
        };
        for (auto name : kStructured) {
            if (detail::ascii_ieq(field, name)) {
                return true;
            }
        }
        return false;
    }

    /// Header fields that carry RFC 5322 address lists (group syntax allowed)
    static bool is_address_field(std::string_view field) {
        static constexpr std::string_view kAddress[] = {
            "To", "Cc", "Bcc", "From", "Sender", "Reply-To",
        };
        for (auto name : kAddress) {
            if (detail::ascii_ieq(field, name)) {
                return true;
            }
        }
        return false;
    }

    static bool has_continued_parameter(
        const std::vector<std::pair<std::string_view, std::string_view>>& params) {
        for (const auto& param : params) {
            if (param.first.find('*') != std::string_view::npos) {
                return true;
            }
        }
        return false;
    }

    /// Apply the header-level pipeline stages to one parsed header:
    /// comment stripping, RFC 2231 continuations, Content-Type validation,
    /// address-group parsing.
    void enhance_header(Header* header) {
        // RFC 6532: headers may carry raw UTF-8 directly, not just RFC 2047
        // encoded-words. Bytes >= 0x80 are legal here; the header value is
        // never modified either way (it is always a plain slice of the
        // arena-owned source) -- only genuinely invalid UTF-8 is flagged.
        for (unsigned char c : header->value) {
            if (c >= 0x80) {
                if (!CharsetConverter::is_valid_utf8(header->value)) {
                    record_anomaly(AnomalyKind::InvalidUtf8Header,
                                   "header value contains bytes >= 0x80 that are not valid "
                                   "UTF-8 (RFC 6532)");
                }
                break;
            }
        }

        // RFC 5322 comments in structured fields are not part of the value
        if (header->value.find('(') != std::string_view::npos &&
            is_structured_field(header->field)) {
            std::string stripped = HeaderCommentParser::remove_comments(header->value);
            if (stripped != header->value) {
                header->value = this->arena().copy_source(stripped);
                header->parameters = parse_parameters(header->value);
            }
        }

        const bool parameterized = detail::ascii_ieq(header->field, "Content-Type") ||
                                   detail::ascii_ieq(header->field, "Content-Disposition");

        // RFC 2231 parameter continuations: reassemble name*0/name*1/... into
        // a single percent-decoded (and charset-converted) parameter.
        if (parameterized && has_continued_parameter(header->parameters)) {
            AnomalyReport rfc2231_report;
            auto continued =
                RFC2231Parser::parse_continued_parameters(header->parameters, &rfc2231_report);
            for (const auto& rec : rfc2231_report.records) {
                record_anomaly(rec.kind, rec.detail);
            }
            for (const auto& [name, param] : continued) {
                std::string value = param.value;
                if (param.encoded && !param.charset.empty()) {
                    auto cs = CharsetConverter::detect_charset(detail::ascii_lower(param.charset));
                    if (cs != CharsetConverter::Charset::Unknown) {
                        value = CharsetConverter::to_utf8(value, cs);
                    }
                }
                header->parameters.emplace_back(this->arena().copy_source(name),
                                                this->arena().copy_source(value));
            }
        }

        // Content-Type syntax validation (RFC 2045/6838)
        if (detail::ascii_ieq(header->field, "Content-Type")) {
            auto validation = MimeTypeValidator::validate(header->value);
            if (!validation.valid) {
                if (header->value.find('/') == std::string_view::npos) {
                    record_anomaly(AnomalyKind::MissingMediaSubtype,
                                   "Content-Type lacks a media subtype");
                } else {
                    record_anomaly(AnomalyKind::InvalidMediaType, validation.error_message);
                }
            }
        }

        // RFC 5322 address group syntax ("Team: a@x, b@y;") on address headers
        if (header->value.find(':') != std::string_view::npos && is_address_field(header->field)) {
            auto groups = AddressGroupParser::parse(header->value);
            if (!groups.empty()) {
                header->address_groups =
                    this->arena().create<std::vector<AddressGroup>>(std::move(groups));
            }
        }
    }

    /// Shared post-header pipeline for the top-level message and every part:
    /// enhance headers, then descend by media type (multipart splitting,
    /// message/external-body references).
    void finish_message(Message* msg) {
        for (auto* header : msg->headers) {
            enhance_header(header);
        }
        if (rejected_) {
            return;
        }

        // Structured values that apply regardless of Content-Type (e.g. a
        // plain RFC 5322 message with no MIME headers at all still has a
        // Date / Message-ID / References worth parsing -- this matters for
        // message/rfc822 encapsulated messages in particular, which are
        // routinely non-MIME).
        parse_date_header(msg);
        parse_threading_headers(msg);

        // The first Content-Type header drives the message structure
        Header* content_type = nullptr;
        for (auto* header : msg->headers) {
            if (detail::ascii_ieq(header->field, "Content-Type")) {
                content_type = header;
                break;
            }
        }
        if (!content_type) {
            return;
        }

        std::string_view media = detail::media_type_of(content_type->value);

        if (is_multipart(content_type->value)) {
            if (detail::ascii_ieq(media, "multipart/report")) {
                bool has_report_type = false;
                for (const auto& param : content_type->parameters) {
                    if (detail::ascii_ieq(param.first, "report-type")) {
                        has_report_type = true;
                        break;
                    }
                }
                if (!has_report_type) {
                    record_anomaly(AnomalyKind::MissingReportTypeParameter,
                                   "multipart/report Content-Type lacks the required "
                                   "report-type parameter (RFC 6522 §4)");
                }
            } else if (detail::ascii_ieq(media, "multipart/signed") ||
                       detail::ascii_ieq(media, "multipart/encrypted")) {
                bool has_protocol = false;
                for (const auto& param : content_type->parameters) {
                    if (detail::ascii_ieq(param.first, "protocol")) {
                        has_protocol = true;
                        break;
                    }
                }
                if (!has_protocol) {
                    record_anomaly(AnomalyKind::MissingProtocolParameter,
                                   "multipart/signed or multipart/encrypted Content-Type "
                                   "lacks the required protocol parameter (RFC 1847 §2)");
                }
            }

            for (const auto& param : content_type->parameters) {
                if (param.first == "boundary") {
                    if (!param.second.empty()) {
                        msg->parts = parse_multipart_body(msg->body, param.second);
                    }
                    break;
                }
            }

            if (detail::ascii_ieq(media, "multipart/related")) {
                resolve_related_start(msg, content_type);
            }
        } else if (detail::ascii_ieq(media, "message/external-body")) {
            msg->external_body = this->arena().create<ExternalBodyRef>(
                ExternalBodyParser::parse(content_type->parameters));
        } else if (detail::ascii_ieq(media, "message/partial")) {
            msg->message_partial = this->arena().create<MessagePartialRef>(
                MessagePartialParser::parse(content_type->parameters));
            record_anomaly(AnomalyKind::MessagePartialDetected,
                           "message/partial part detected; reassembly with sibling "
                           "fragments (matching id, ordered by number/total) is required");
        } else if (detail::ascii_ieq(media, "message/rfc822")) {
            msg->encapsulated = parse_encapsulated_message(msg->body);
        } else if (detail::ascii_ieq(media, "message/delivery-status")) {
            msg->delivery_status =
                this->arena().create<DeliveryStatusRef>(DeliveryStatusParser::parse(msg->body));
        }
    }

    /// Parse the Date header (RFC 5322 §3.3) into a structured value,
    /// attached to msg->date. A syntactically invalid Date is never
    /// thrown -- it is recorded as AnomalyKind::InvalidDateFormat and left
    /// unparsed (msg->date stays nullptr).
    void parse_date_header(Message* msg) {
        for (auto* header : msg->headers) {
            if (!detail::ascii_ieq(header->field, "Date")) {
                continue;
            }
            auto parsed = DateTimeParser::parse(header->value);
            if (parsed.valid) {
                msg->date = this->arena().create<ParsedDateTime>(parsed);
            } else {
                record_anomaly(AnomalyKind::InvalidDateFormat,
                               "Date header could not be parsed as an RFC 5322 date-time");
            }
            break; // Only the first Date header is meaningful
        }
    }

    /// Parse Message-ID / In-Reply-To / References (RFC 5322 §3.6.4) into
    /// the AST. Malformed msg-ids are recorded as anomalies rather than
    /// thrown or silently dropped.
    void parse_threading_headers(Message* msg) {
        for (auto* header : msg->headers) {
            if (detail::ascii_ieq(header->field, "Message-ID") && !msg->message_id) {
                auto ids = MessageIdParser::parse_list(header->value);
                if (!ids.empty()) {
                    if (!ids.front().valid) {
                        record_anomaly(AnomalyKind::InvalidMessageIdSyntax,
                                       "Message-ID does not contain a well-formed msg-id "
                                       "(RFC 5322 §3.6.4)");
                    }
                    msg->message_id = this->arena().create<MessageId>(ids.front());
                }
            } else if (detail::ascii_ieq(header->field, "In-Reply-To") && !msg->in_reply_to) {
                auto ids = MessageIdParser::parse_list(header->value);
                for (const auto& id : ids) {
                    if (!id.valid) {
                        record_anomaly(AnomalyKind::InvalidMessageIdSyntax,
                                       "In-Reply-To contains a malformed msg-id "
                                       "(RFC 5322 §3.6.4)");
                    }
                }
                msg->in_reply_to = this->arena().create<std::vector<MessageId>>(std::move(ids));
            } else if (detail::ascii_ieq(header->field, "References") && !msg->references) {
                auto ids = MessageIdParser::parse_list(header->value);
                for (const auto& id : ids) {
                    if (!id.valid) {
                        record_anomaly(AnomalyKind::InvalidMessageIdSyntax,
                                       "References contains a malformed msg-id "
                                       "(RFC 5322 §3.6.4)");
                    }
                }
                msg->references = this->arena().create<std::vector<MessageId>>(std::move(ids));
            }
        }
    }

    /// Recursively parse the body of a message/rfc822 part as a full
    /// encapsulated RFC 5322 message (RFC 2046 §5.2.1), reusing the same
    /// header+body pipeline as multipart parts (parse_part). Enforces the
    /// SAME nesting-depth/part-count DoS limits as multipart: a chain of
    /// nested message/rfc822 parts must not recurse unbounded.
    Message* parse_encapsulated_message(std::string_view body) {
        if (tracker_.current_nesting_depth >= limits_.max_nesting_depth) {
            record_anomaly(AnomalyKind::ExcessiveNestingDepth,
                           "message/rfc822 nesting depth limit reached; not descending further");
            return nullptr;
        }
        if (tracker_.total_parts >= limits_.max_total_parts) {
            record_anomaly(AnomalyKind::ExcessivePartCount,
                           "message/rfc822 part count limit reached; not descending further");
            return nullptr;
        }
        if (rejected_) {
            return nullptr;
        }

        tracker_.enter_level();
        tracker_.add_part();
        Message* nested = parse_part(body);
        tracker_.exit_level();
        return nested;
    }

    /// Resolve multipart/related's "start" parameter (RFC 2387 §3.4) to the
    /// root part by matching it against each part's Content-ID header. When
    /// "start" is absent, or present but unresolved, the root part falls
    /// back to the first part (RFC 2387 §3.4: "the 'start' parameter... In
    /// its absence the first body part is the root").
    void resolve_related_start(Message* msg, Header* content_type) {
        if (msg->parts.empty()) {
            return;
        }

        std::string_view start;
        bool has_start = false;
        for (const auto& param : content_type->parameters) {
            if (detail::ascii_ieq(param.first, "start")) {
                start = param.second;
                has_start = true;
                break;
            }
        }

        if (!has_start || start.empty()) {
            msg->related_root = msg->parts.front();
            return;
        }

        std::string_view start_id = strip_angle_brackets(start);
        for (auto* part : msg->parts) {
            const Header* cid = find_header(*part, "Content-ID");
            if (cid && strip_angle_brackets(cid->value) == start_id) {
                msg->related_root = part;
                return;
            }
        }

        record_anomaly(AnomalyKind::InvalidRelatedStart,
                       "multipart/related start parameter does not match any part's "
                       "Content-ID; falling back to the first part (RFC 2387 §3.4)");
        msg->related_root = msg->parts.front();
    }

    /// Find a header by (case-insensitive) field name within a single part.
    static const Header* find_header(const Message& part, std::string_view field) {
        for (const auto* header : part.headers) {
            if (header && detail::ascii_ieq(header->field, field)) {
                return header;
            }
        }
        return nullptr;
    }

    /// Trim whitespace and one layer of angle brackets / surrounding quotes.
    static std::string_view strip_angle_brackets(std::string_view value) {
        while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
            value.remove_prefix(1);
        }
        while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
            value.remove_suffix(1);
        }
        if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
            value.remove_prefix(1);
            value.remove_suffix(1);
        }
        if (!value.empty() && value.front() == '<') {
            value.remove_prefix(1);
        }
        if (!value.empty() && value.back() == '>') {
            value.remove_suffix(1);
        }
        return value;
    }

    /// Parse parameters from header value (e.g., "text/plain; charset=utf-8")
    std::vector<std::pair<std::string_view, std::string_view>>
    parse_parameters(std::string_view value) {
        std::vector<std::pair<std::string_view, std::string_view>> params;

        // Find semicolon that starts parameters
        size_t semi_pos = value.find(';');
        if (semi_pos == std::string_view::npos) {
            return params; // No parameters
        }

        // Parse each parameter
        size_t pos = semi_pos + 1;
        while (pos < value.size()) {
            // Skip whitespace
            while (pos < value.size() && std::isspace(static_cast<unsigned char>(value[pos]))) {
                ++pos;
            }
            if (pos >= value.size())
                break;

            // Find parameter name
            size_t name_start = pos;
            while (pos < value.size() && value[pos] != '=' && value[pos] != ';') {
                ++pos;
            }
            if (pos >= value.size() || value[pos] != '=')
                break;

            std::string_view param_name = value.substr(name_start, pos - name_start);
            // Trim trailing whitespace from name
            while (!param_name.empty() &&
                   std::isspace(static_cast<unsigned char>(param_name.back()))) {
                param_name.remove_suffix(1);
            }

            ++pos; // Skip '='

            // Skip whitespace after =
            while (pos < value.size() && std::isspace(static_cast<unsigned char>(value[pos]))) {
                ++pos;
            }

            // Parse parameter value (may be quoted)
            std::string_view param_value;
            if (pos < value.size() && value[pos] == '"') {
                // Quoted value
                ++pos; // Skip opening quote
                size_t value_start = pos;
                while (pos < value.size() && value[pos] != '"') {
                    ++pos;
                }
                param_value = value.substr(value_start, pos - value_start);
                if (pos < value.size())
                    ++pos; // Skip closing quote
            } else {
                // Unquoted value (until semicolon or end)
                size_t value_start = pos;
                while (pos < value.size() && value[pos] != ';') {
                    ++pos;
                }
                param_value = value.substr(value_start, pos - value_start);
                // Trim trailing whitespace
                while (!param_value.empty() &&
                       std::isspace(static_cast<unsigned char>(param_value.back()))) {
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
    /// as anomalies. Once the parse is rejected, no further parts are read.
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

        while (!closed && !rejected_) {
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
            size_t content_end = next.found ? std::max(next.content_end, part_start) : body.size();

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
        // The exact bytes as transmitted between boundary delimiters,
        // before header unfolding or any decoding -- preserved verbatim on
        // the resulting part as raw_source (see Message::raw_source; this
        // is what a multipart/signed (RFC 1847) signature would cover).
        std::string_view raw = content;

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

        // Create part and run it through the same pipeline as the message
        auto* part = this->template create_node<Part>(headers, body_text);
        part->raw_source = raw;
        finish_message(part);
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
                break; // Last line has no terminator: no blank line found
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

    // ========================================================================
    // Structural Anomaly Detection (post-parse, top-level message)
    // ========================================================================

    void detect_missing_headers(Message* msg) {
        bool has_mime_version = false;
        bool has_content_type = false;

        for (auto* header : msg->headers) {
            if (detail::ascii_ieq(header->field, "MIME-Version")) {
                has_mime_version = true;
            }
            if (detail::ascii_ieq(header->field, "Content-Type")) {
                has_content_type = true;
            }
        }

        if (!has_mime_version && !msg->parts.empty()) {
            record_anomaly(AnomalyKind::MissingMIMEVersion,
                           "multipart message lacks a MIME-Version header");
        }

        if (!has_content_type && !msg->parts.empty()) {
            record_anomaly(AnomalyKind::MissingContentType,
                           "multipart message lacks a Content-Type header");
        }
    }

    void detect_duplicate_headers(Message* msg) {
        bool seen_content_type = false;

        for (auto* header : msg->headers) {
            if (detail::ascii_ieq(header->field, "Content-Type")) {
                if (seen_content_type) {
                    record_anomaly(AnomalyKind::DuplicateContentType,
                                   "message contains multiple Content-Type headers");
                    break;
                }
                seen_content_type = true;
            }
        }
    }

    void detect_content_type_issues(Message* msg) {
        for (auto* header : msg->headers) {
            if (!detail::ascii_ieq(header->field, "Content-Type")) {
                continue;
            }
            // Missing charset in text/* types (subtype syntax itself is
            // validated per-header by enhance_header)
            if (header->value.find("text/") == 0) {
                bool has_charset = false;
                for (const auto& param : header->parameters) {
                    if (param.first == "charset") {
                        has_charset = true;
                        break;
                    }
                }
                if (!has_charset) {
                    record_anomaly(AnomalyKind::MissingCharsetInfo,
                                   "text/* Content-Type lacks a charset parameter");
                }
            }
        }
    }

    void detect_boundary_issues(Message* msg) {
        for (auto* header : msg->headers) {
            if (!detail::ascii_ieq(header->field, "Content-Type")) {
                continue;
            }
            if (header->value.find("multipart/") != 0) {
                continue;
            }

            bool has_boundary = false;
            bool boundary_empty = false;
            for (const auto& param : header->parameters) {
                if (param.first == "boundary") {
                    has_boundary = true;
                    boundary_empty = param.second.empty();
                    break;
                }
            }

            if (!has_boundary) {
                record_anomaly(AnomalyKind::MissingBoundaryParameter,
                               "multipart Content-Type lacks a boundary parameter");
            } else if (boundary_empty) {
                record_anomaly(AnomalyKind::EmptyBoundary,
                               "multipart Content-Type has an empty boundary parameter");
            }
        }
    }

protected:
    /// Record an anomaly against the configured policy. Ignore-policy
    /// anomalies are dropped; Reject-policy anomalies of Security/DoS
    /// severity mark the parse rejected (stopping further multipart
    /// descent). Returns the applied policy.
    AnomalyPolicy record_anomaly(AnomalyKind kind, std::string_view detail) {
        const AnomalySeverity severity = AnomalyConfig::get_severity(kind);
        const AnomalyPolicy policy = config_.get_policy(kind);

        if (policy != AnomalyPolicy::Ignore) {
            report_.add(kind, severity, policy, SourceLocation{}, "", detail);
        }
        if (policy == AnomalyPolicy::Reject && severity >= AnomalySeverity::Security) {
            rejected_ = true;
        }
        return policy;
    }

    ParserLimits limits_;
    AnomalyConfig config_;
    LimitTracker tracker_;
    AnomalyReport report_;
    bool rejected_ = false;
};

} // namespace libglot::mime
