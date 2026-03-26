#pragma once

#include <string_view>
#include <string>
#include <cctype>

namespace libglot::mime {

/// ============================================================================
/// Header Folding/Unfolding (RFC 5322)
/// ============================================================================
///
/// Handles multi-line headers with continuation lines (LWS - Linear White Space)
/// Example:
///   Subject: This is a long
///    subject that spans
///    multiple lines
/// Should be unfolded to:
///   Subject: This is a long subject that spans multiple lines
/// ============================================================================

class HeaderFolding {
public:
    /// Unfold a header value by removing line breaks and collapsing whitespace
    /// According to RFC 5322, a folding point is CRLF followed by whitespace
    static std::string unfold(std::string_view header_value) {
        std::string result;
        result.reserve(header_value.size());

        bool in_whitespace_run = false;
        bool last_was_newline = false;

        for (size_t i = 0; i < header_value.size(); ++i) {
            char c = header_value[i];

            // Handle CRLF or LF
            if (c == '\r' || c == '\n') {
                // Check if next char is newline (for CRLF)
                if (c == '\r' && i + 1 < header_value.size() && header_value[i + 1] == '\n') {
                    ++i;  // Skip the LF in CRLF
                }

                // Check if this is a folding point (followed by whitespace)
                if (i + 1 < header_value.size() && (header_value[i + 1] == ' ' || header_value[i + 1] == '\t')) {
                    // This is a folding point - replace with single space
                    if (!result.empty() && !in_whitespace_run) {
                        result.push_back(' ');
                        in_whitespace_run = true;
                    }
                    last_was_newline = true;
                } else {
                    // Not a folding point - this is end of header
                    break;
                }
                continue;
            }

            // Handle whitespace
            if (c == ' ' || c == '\t') {
                // Skip leading whitespace after newline
                if (last_was_newline) {
                    last_was_newline = false;
                    in_whitespace_run = true;
                    continue;
                }

                // Collapse multiple spaces into one
                if (!in_whitespace_run && !result.empty()) {
                    result.push_back(' ');
                    in_whitespace_run = true;
                }
                continue;
            }

            // Regular character
            result.push_back(c);
            in_whitespace_run = false;
            last_was_newline = false;
        }

        // Trim trailing whitespace
        while (!result.empty() && std::isspace(result.back())) {
            result.pop_back();
        }

        return result;
    }

    /// Fold a long header value to fit within line length limits
    /// RFC 5322 recommends max 78 characters per line
    static std::string fold(std::string_view header_value, size_t max_line_length = 78) {
        if (header_value.size() <= max_line_length) {
            return std::string(header_value);
        }

        std::string result;
        result.reserve(header_value.size() + header_value.size() / max_line_length * 3);

        size_t current_line_length = 0;
        size_t last_break_point = 0;

        for (size_t i = 0; i < header_value.size(); ++i) {
            char c = header_value[i];

            result.push_back(c);
            ++current_line_length;

            // Remember potential break points (spaces)
            if (c == ' ' || c == '\t') {
                last_break_point = result.size();
            }

            // Need to fold?
            if (current_line_length >= max_line_length && last_break_point > 0) {
                // Insert folding point at last space
                result.insert(last_break_point, "\r\n ");
                current_line_length = result.size() - last_break_point;
                last_break_point = 0;
            }
        }

        return result;
    }

    /// Check if a header value is folded (contains continuation lines)
    static bool is_folded(std::string_view header_value) {
        for (size_t i = 0; i + 1 < header_value.size(); ++i) {
            // Look for CRLF or LF followed by whitespace
            if ((header_value[i] == '\n' || header_value[i] == '\r') &&
                (header_value[i + 1] == ' ' || header_value[i + 1] == '\t')) {
                return true;
            }
        }
        return false;
    }
};

} // namespace libglot::mime
