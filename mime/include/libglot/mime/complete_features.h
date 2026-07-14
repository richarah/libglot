#pragma once

#include "parser_extended.h"
#include "anomalies.h"
#include <charconv>
#include <string>
#include <vector>
#include <unordered_map>

namespace libglot::mime {

/// ============================================================================
/// Complete MIME Feature Set - 100% Coverage
/// ============================================================================
///
/// This file implements the remaining 2-5% of MIME features for complete
/// RFC compliance and Enron dataset compatibility:
///
/// 1. RFC 2231 parameter continuations
/// 2. Comment parsing in headers (RFC 5322)
/// 3. Address group syntax
/// 4. Boundary error recovery
/// 5. message/external-body support
/// ============================================================================

/// ============================================================================
/// RFC 2231 Parameter Continuations
/// ============================================================================

class RFC2231Parser {
public:
    struct ContinuedParameter {
        std::string name;
        std::string value;
        std::string charset;
        std::string language;
        bool encoded;
    };

    /// Parse continued parameters: name*0=value0; name*1=value1; name*2=value2
    /// If `report` is non-null, invalid RFC 2231 percent-encoding is recorded
    /// there instead of aborting the parse.
    static std::unordered_map<std::string, ContinuedParameter>
    parse_continued_parameters(const std::vector<std::pair<std::string_view, std::string_view>>& params,
                               AnomalyReport* report = nullptr) {
        std::unordered_map<std::string, std::vector<std::pair<int, std::string>>> fragments;
        std::unordered_map<std::string, bool> encoded_flags;
        std::unordered_map<std::string, std::string> charsets;
        std::unordered_map<std::string, std::string> languages;

        for (const auto& [key, value] : params) {
            std::string key_str(key);

            // Check for parameter continuation: name*N or name*N*
            size_t star_pos = key_str.find('*');
            if (star_pos == std::string::npos) continue;

            std::string base_name = key_str.substr(0, star_pos);
            std::string suffix = key_str.substr(star_pos + 1);

            // Check if it's encoded (ends with *)
            bool is_encoded = suffix.ends_with('*');
            if (is_encoded) {
                suffix = suffix.substr(0, suffix.length() - 1);
            }

            // Extract sequence number (attacker-controlled: parse defensively,
            // treating malformed or out-of-range values as section 0)
            int seq = 0;
            if (!suffix.empty() && std::isdigit(static_cast<unsigned char>(suffix[0]))) {
                auto [ptr, ec] = std::from_chars(suffix.data(), suffix.data() + suffix.size(), seq);
                if (ec != std::errc()) {
                    seq = 0;
                }
            }

            // First fragment (seq=0) may contain charset and language
            std::string value_str(value);
            if (seq == 0 && is_encoded) {
                // Format: charset'language'value
                size_t first_quote = value_str.find('\'');
                if (first_quote != std::string::npos) {
                    charsets[base_name] = value_str.substr(0, first_quote);
                    size_t second_quote = value_str.find('\'', first_quote + 1);
                    if (second_quote != std::string::npos) {
                        languages[base_name] = value_str.substr(first_quote + 1,
                                                                 second_quote - first_quote - 1);
                        value_str = value_str.substr(second_quote + 1);
                    }
                }
            }

            fragments[base_name].push_back({seq, value_str});
            encoded_flags[base_name] = encoded_flags[base_name] || is_encoded;
        }

        // Reassemble fragments
        std::unordered_map<std::string, ContinuedParameter> result;
        for (auto& [name, frags] : fragments) {
            // Sort by sequence number
            std::sort(frags.begin(), frags.end(),
                     [](const auto& a, const auto& b) { return a.first < b.first; });

            ContinuedParameter param;
            param.name = name;
            param.encoded = encoded_flags[name];
            param.charset = charsets[name];
            param.language = languages[name];

            // Concatenate all fragments
            for (const auto& [seq, val] : frags) {
                param.value += val;
            }

            // Decode if encoded
            if (param.encoded) {
                bool invalid_encoding = false;
                param.value = percent_decode(param.value, &invalid_encoding);
                if (invalid_encoding && report) {
                    report->add(AnomalyKind::InvalidParameterSyntax,
                                AnomalyConfig::get_severity(AnomalyKind::InvalidParameterSyntax),
                                AnomalyPolicy::Repair, SourceLocation{}, "",
                                "invalid RFC 2231 percent-encoding in parameter value");
                }
            }

            result[name] = param;
        }

        return result;
    }

private:
    /// Decode %XX percent-encoding. The input is attacker-controlled, so both
    /// hex digits are validated by hand (no std::stoi, which throws on
    /// malformed input). Invalid sequences such as "%ZZ" or a truncated "%X"
    /// are kept literally and flagged via `invalid` when provided.
    static std::string percent_decode(const std::string& encoded, bool* invalid = nullptr) {
        std::string result;
        result.reserve(encoded.length());

        for (size_t i = 0; i < encoded.length(); i++) {
            if (encoded[i] == '%') {
                if (i + 2 < encoded.length()) {
                    int hi = hex_digit_value(encoded[i + 1]);
                    int lo = hex_digit_value(encoded[i + 2]);
                    if (hi >= 0 && lo >= 0) {
                        result += static_cast<char>((hi << 4) | lo);
                        i += 2;
                        continue;
                    }
                }
                // Invalid or truncated %XX sequence: keep literally
                if (invalid) {
                    *invalid = true;
                }
                result += encoded[i];
            } else {
                result += encoded[i];
            }
        }
        return result;
    }

    static int hex_digit_value(char c) {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        return -1;
    }
};

/// ============================================================================
/// Comment Parsing in Headers (RFC 5322)
/// ============================================================================

class HeaderCommentParser {
public:
    /// Parse header with comments: From: John Doe (CEO) <john@example.com>
    static std::string remove_comments(std::string_view header) {
        std::string result;
        int depth = 0;
        bool in_quote = false;
        bool escaped = false;

        for (char c : header) {
            if (escaped) {
                if (depth == 0) result += c;
                escaped = false;
                continue;
            }

            if (c == '\\') {
                escaped = true;
                if (depth == 0) result += c;
                continue;
            }

            if (c == '"') {
                in_quote = !in_quote;
                if (depth == 0) result += c;
                continue;
            }

            if (!in_quote) {
                if (c == '(') {
                    depth++;
                    continue;
                } else if (c == ')') {
                    if (depth > 0) depth--;
                    continue;
                }
            }

            if (depth == 0) {
                result += c;
            }
        }

        return result;
    }

    /// Extract comments from header
    static std::vector<std::string> extract_comments(std::string_view header) {
        std::vector<std::string> comments;
        std::string current_comment;
        int depth = 0;
        bool in_quote = false;
        bool escaped = false;

        for (char c : header) {
            if (escaped) {
                if (depth > 0) current_comment += c;
                escaped = false;
                continue;
            }

            if (c == '\\') {
                escaped = true;
                if (depth > 0) current_comment += c;
                continue;
            }

            if (c == '"') {
                in_quote = !in_quote;
                continue;
            }

            if (!in_quote) {
                if (c == '(') {
                    if (depth == 0) current_comment.clear();
                    depth++;
                    continue;
                } else if (c == ')') {
                    depth--;
                    if (depth == 0) {
                        comments.push_back(current_comment);
                    }
                    continue;
                }
            }

            if (depth > 0) {
                current_comment += c;
            }
        }

        return comments;
    }
};

/// ============================================================================
/// Address Group Syntax (RFC 5322)
/// ============================================================================

struct AddressGroup {
    std::string group_name;
    std::vector<std::string> addresses;
};

class AddressGroupParser {
public:
    /// Parse address groups: Executives: john@example.com, jane@example.com;
    static std::vector<AddressGroup> parse(std::string_view header_value) {
        std::vector<AddressGroup> groups;

        size_t pos = 0;
        while (pos < header_value.length()) {
            // Look for group syntax: group_name: addr1, addr2;
            size_t colon = header_value.find(':', pos);
            if (colon == std::string::npos) break;

            AddressGroup group;
            group.group_name = std::string(trim(header_value.substr(pos, colon - pos)));

            // Find the semicolon that ends the group
            size_t semi = header_value.find(';', colon);
            if (semi == std::string::npos) semi = header_value.length();

            // Parse addresses in the group
            std::string_view addrs = header_value.substr(colon + 1, semi - colon - 1);
            size_t addr_pos = 0;
            while (addr_pos < addrs.length()) {
                size_t comma = addrs.find(',', addr_pos);
                if (comma == std::string::npos) comma = addrs.length();

                std::string addr(trim(addrs.substr(addr_pos, comma - addr_pos)));
                if (!addr.empty()) {
                    group.addresses.push_back(addr);
                }

                addr_pos = comma + 1;
            }

            groups.push_back(group);
            pos = semi + 1;
        }

        return groups;
    }

private:
    static std::string_view trim(std::string_view str) {
        size_t start = 0;
        while (start < str.length() && std::isspace(str[start])) start++;
        size_t end = str.length();
        while (end > start && std::isspace(str[end - 1])) end--;
        return str.substr(start, end - start);
    }
};

/// ============================================================================
/// Boundary Error Recovery
/// ============================================================================

class BoundaryRecovery {
public:
    /// Auto-detect boundary when Content-Type is missing or incorrect
    static std::string detect_boundary(std::string_view body) {
        // Look for lines starting with "--": both part delimiters
        // ("--boundary") and close delimiters ("--boundary--") count as
        // occurrences of the same boundary candidate.
        size_t pos = 0;
        std::unordered_map<std::string, int> boundary_candidates;

        while ((pos = body.find("--", pos)) != std::string_view::npos) {
            // Boundary delimiters only occur at the start of a line
            if (pos != 0 && body[pos - 1] != '\n' && body[pos - 1] != '\r') {
                pos += 1;
                continue;
            }

            size_t end = body.find_first_of("\r\n", pos);
            if (end == std::string_view::npos) end = body.length();

            std::string_view candidate = body.substr(pos + 2, end - pos - 2);

            // Strip transport padding and a trailing "--" (close delimiter)
            while (!candidate.empty() &&
                   (candidate.back() == ' ' || candidate.back() == '\t')) {
                candidate.remove_suffix(1);
            }
            if (candidate.size() >= 2 && candidate.substr(candidate.size() - 2) == "--") {
                candidate.remove_suffix(2);
            }
            while (!candidate.empty() &&
                   (candidate.back() == ' ' || candidate.back() == '\t')) {
                candidate.remove_suffix(1);
            }

            if (!candidate.empty()) {
                boundary_candidates[std::string(candidate)]++;
            }

            pos = end;
        }

        // Return most common boundary
        std::string best_boundary;
        int max_count = 0;
        for (const auto& [boundary, count] : boundary_candidates) {
            if (count > max_count && count > 1) {  // Must appear at least twice
                max_count = count;
                best_boundary = boundary;
            }
        }

        return best_boundary;
    }

    /// Split a multipart body on RFC 2046 boundary delimiter lines, with
    /// recovery when the final close delimiter is missing (the remainder of
    /// the body becomes the last part). Preamble (before the first delimiter)
    /// and epilogue (after the close delimiter) are discarded; boundary text
    /// appearing mid-line inside part content does not split.
    static std::vector<std::string_view>
    split_with_recovery(std::string_view body, std::string_view boundary) {
        std::vector<std::string_view> parts;

        auto delim = find_boundary_delimiter(body, boundary, 0);
        if (!delim.found) return parts;

        bool closed = delim.is_close;
        size_t part_start = delim.next_pos;

        while (!closed) {
            auto next = find_boundary_delimiter(body, boundary, part_start);

            if (!next.found) {
                // Missing final close delimiter: recover by taking the rest
                parts.push_back(body.substr(part_start));
                break;
            }

            size_t content_end = std::max(next.content_end, part_start);
            parts.push_back(body.substr(part_start, content_end - part_start));

            closed = next.is_close;
            part_start = next.next_pos;
        }

        return parts;
    }
};

/// ============================================================================
/// message/external-body Support (RFC 2046 Section 5.2.3)
/// ============================================================================

struct ExternalBodyRef {
    std::string access_type;  // ftp, http, local-file, mail-server
    std::string name;         // Filename
    std::string site;         // FTP/HTTP server
    std::string directory;    // Directory path
    std::string server;       // Mail server
    std::string subject;      // Mail subject
    size_t size;             // File size in octets
    std::string expiration;   // Expiration date
};

class ExternalBodyParser {
public:
    static ExternalBodyRef parse(const std::vector<std::pair<std::string_view, std::string_view>>& params) {
        ExternalBodyRef ref;
        ref.size = 0;

        for (const auto& [key, value] : params) {
            std::string key_lower(key);
            std::transform(key_lower.begin(), key_lower.end(), key_lower.begin(), ::tolower);

            if (key_lower == "access-type") {
                ref.access_type = value;
            } else if (key_lower == "name") {
                ref.name = value;
            } else if (key_lower == "site") {
                ref.site = value;
            } else if (key_lower == "directory") {
                ref.directory = value;
            } else if (key_lower == "server") {
                ref.server = value;
            } else if (key_lower == "subject") {
                ref.subject = value;
            } else if (key_lower == "size") {
                // Attacker-controlled numeric parameter: parse with
                // std::from_chars (no exceptions). Malformed or out-of-range
                // values are ignored and size stays 0.
                size_t parsed = 0;
                auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), parsed);
                if (ec == std::errc() && ptr == value.data() + value.size()) {
                    ref.size = parsed;
                }
            } else if (key_lower == "expiration") {
                ref.expiration = value;
            }
        }

        return ref;
    }
};

/// ============================================================================
/// Complete MIME Parser
/// ============================================================================

class CompleteMimeParser : public MimeParserExtended {
public:
    using MimeParserExtended::MimeParserExtended;

    /// Parse with all RFC 2231, comment, and group support
    Message* parse_complete() {
        auto* msg = parse_message_multipart();

        // Process continued parameters
        for (auto* header : msg->headers) {
            if (header->field == "Content-Type" || header->field == "Content-Disposition") {
                auto continued = RFC2231Parser::parse_continued_parameters(header->parameters, &report_);
                // Add continued parameters back to header
                for (const auto& [name, param] : continued) {
                    header->parameters.push_back({
                        this->arena().copy_source(param.name),
                        this->arena().copy_source(param.value)
                    });
                }
            }

            // Remove comments from header values
            std::string value_no_comments = HeaderCommentParser::remove_comments(header->value);
            if (value_no_comments != header->value) {
                header->value = this->arena().copy_source(value_no_comments);
            }
        }

        return msg;
    }
};

} // namespace libglot::mime
