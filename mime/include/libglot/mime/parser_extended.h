#pragma once

#include "parser.h"
#include "anomalies.h"
#include <algorithm>
#include <cctype>

namespace libglot::mime {

/// ============================================================================
/// Extended MIME Parser with Multipart and Anomaly Detection
/// ============================================================================

class MimeParserExtended : public MimeParser {
public:
    using MimeParser::MimeParser;

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

    /// Parse multipart body by splitting on boundary
    std::vector<Part*> parse_multipart_body(std::string_view body, std::string_view boundary) {
        std::vector<Part*> parts;

        // Construct boundary markers
        std::string boundary_start = "--" + std::string(boundary);
        std::string boundary_end = "--" + std::string(boundary) + "--";

        size_t pos = 0;
        while (pos < body.size()) {
            // Find next boundary
            size_t boundary_pos = body.find(boundary_start, pos);
            if (boundary_pos == std::string_view::npos) {
                break;
            }

            // Check if this is the final boundary
            size_t after_boundary = boundary_pos + boundary_start.size();
            if (after_boundary + 2 <= body.size() &&
                body.substr(boundary_pos, boundary_end.size()) == boundary_end) {
                break;  // Final boundary found
            }

            // Skip to content after boundary (past CRLF)
            size_t content_start = after_boundary;
            if (content_start < body.size() && body[content_start] == '\r') ++content_start;
            if (content_start < body.size() && body[content_start] == '\n') ++content_start;

            // Find next boundary
            size_t next_boundary = body.find(boundary_start, content_start);
            if (next_boundary == std::string_view::npos) {
                next_boundary = body.size();
            }

            // Extract part content
            std::string_view part_content = body.substr(content_start, next_boundary - content_start);

            // Parse part headers and body
            Part* part = parse_part(part_content);
            if (part) {
                parts.push_back(part);
            }

            pos = next_boundary;
        }

        return parts;
    }

    /// Parse a single MIME part (headers + body)
    Part* parse_part(std::string_view content) {
        std::vector<Header*> headers;

        // Find blank line separating headers from body
        size_t blank_line = content.find("\n\n");
        if (blank_line == std::string_view::npos) {
            blank_line = content.find("\r\n\r\n");
            if (blank_line != std::string_view::npos) {
                blank_line += 2;  // Point after \r\n
            }
        }

        std::string_view headers_text;
        std::string_view body_text;

        if (blank_line != std::string_view::npos) {
            headers_text = content.substr(0, blank_line);
            size_t body_start = blank_line;
            // Skip the blank line
            if (body_start < content.size() && content[body_start] == '\n') ++body_start;
            if (body_start < content.size() && content[body_start] == '\r') ++body_start;
            if (body_start < content.size() && content[body_start] == '\n') ++body_start;

            body_text = content.substr(body_start);
        } else {
            // No blank line - treat entire content as body
            body_text = content;
        }

        // Parse headers (simple line-by-line)
        size_t line_start = 0;
        while (line_start < headers_text.size()) {
            size_t line_end = headers_text.find('\n', line_start);
            if (line_end == std::string_view::npos) {
                line_end = headers_text.size();
            }

            std::string_view line = headers_text.substr(line_start, line_end - line_start);
            // Remove trailing \r if present
            if (!line.empty() && line.back() == '\r') {
                line.remove_suffix(1);
            }

            // Parse header line
            size_t colon_pos = line.find(':');
            if (colon_pos != std::string_view::npos) {
                std::string_view field = line.substr(0, colon_pos);
                std::string_view value = line.substr(colon_pos + 1);

                // Trim leading whitespace from value
                while (!value.empty() && std::isspace(value.front())) {
                    value.remove_prefix(1);
                }

                auto* header = this->template create_node<Header>(field, value);
                header->parameters = parse_parameters(value);
                headers.push_back(header);
            }

            line_start = line_end + 1;
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
};

} // namespace libglot::mime
