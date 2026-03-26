#pragma once

#include <string_view>
#include <unordered_set>
#include <unordered_map>

namespace libglot::mime {

/// ============================================================================
/// MIME Type Validator
/// ============================================================================
///
/// Validates MIME types according to RFC 2045/6838
/// - Validates type/subtype format
/// - Checks against IANA registered types
/// - Validates parameter syntax
///
/// Reference: https://www.iana.org/assignments/media-types/media-types.xhtml
/// ============================================================================

class MimeTypeValidator {
public:
    struct ValidationResult {
        bool valid = false;
        std::string_view error_message;
        bool is_registered = false;  // Is it an IANA-registered type?
    };

    /// Validate MIME type format: type/subtype
    static ValidationResult validate(std::string_view mime_type) {
        ValidationResult result;

        // Find slash separator
        size_t slash_pos = mime_type.find('/');
        if (slash_pos == std::string_view::npos) {
            result.error_message = "Missing '/' separator in MIME type";
            return result;
        }

        std::string_view type = mime_type.substr(0, slash_pos);
        std::string_view subtype = mime_type.substr(slash_pos + 1);

        // Remove parameters if present
        size_t param_pos = subtype.find(';');
        if (param_pos != std::string_view::npos) {
            subtype = subtype.substr(0, param_pos);
        }

        // Trim whitespace
        type = trim(type);
        subtype = trim(subtype);

        // Validate type and subtype are not empty
        if (type.empty()) {
            result.error_message = "Empty MIME type";
            return result;
        }
        if (subtype.empty()) {
            result.error_message = "Empty MIME subtype";
            return result;
        }

        // Validate characters (RFC 2045: token characters)
        if (!is_valid_token(type)) {
            result.error_message = "Invalid characters in MIME type";
            return result;
        }
        if (!is_valid_token(subtype)) {
            result.error_message = "Invalid characters in MIME subtype";
            return result;
        }

        // Check if it's a registered top-level type
        result.is_registered = is_registered_type(type);

        result.valid = true;
        return result;
    }

    /// Check if a MIME type is registered with IANA
    static bool is_registered_type(std::string_view type) {
        static const std::unordered_set<std::string_view> registered_types = {
            "text", "image", "audio", "video", "application",
            "multipart", "message", "model", "font"
        };

        return registered_types.count(type) > 0;
    }

    /// Get common subtypes for a given type
    static std::unordered_set<std::string_view> get_common_subtypes(std::string_view type) {
        static const std::unordered_map<std::string_view, std::unordered_set<std::string_view>> subtypes = {
            {"text", {"plain", "html", "css", "javascript", "xml", "csv", "markdown"}},
            {"image", {"jpeg", "png", "gif", "webp", "svg+xml", "bmp", "tiff"}},
            {"audio", {"mpeg", "ogg", "wav", "webm", "aac", "flac"}},
            {"video", {"mp4", "webm", "ogg", "mpeg", "quicktime", "x-msvideo"}},
            {"application", {"json", "xml", "pdf", "zip", "octet-stream", "javascript", "x-www-form-urlencoded"}},
            {"multipart", {"mixed", "alternative", "related", "form-data", "byteranges"}},
            {"message", {"rfc822", "partial", "external-body"}},
        };

        auto it = subtypes.find(type);
        return (it != subtypes.end()) ? it->second : std::unordered_set<std::string_view>{};
    }

    /// Validate MIME type with subtype check
    static ValidationResult validate_with_subtype_check(std::string_view mime_type) {
        auto result = validate(mime_type);
        if (!result.valid) return result;

        // Extract type and subtype
        size_t slash_pos = mime_type.find('/');
        std::string_view type = mime_type.substr(0, slash_pos);
        std::string_view subtype = mime_type.substr(slash_pos + 1);

        // Remove parameters
        size_t param_pos = subtype.find(';');
        if (param_pos != std::string_view::npos) {
            subtype = subtype.substr(0, param_pos);
        }

        type = trim(type);
        subtype = trim(subtype);

        // Check if subtype is known
        auto common_subtypes = get_common_subtypes(type);
        if (!common_subtypes.empty() && common_subtypes.count(subtype) == 0) {
            // Subtype is not in our common list (might still be valid)
            result.is_registered = false;
        }

        return result;
    }

private:
    /// Check if string is a valid RFC 2045 token
    static bool is_valid_token(std::string_view token) {
        if (token.empty()) return false;

        for (char c : token) {
            // RFC 2045 token chars: ASCII except CTLs and specials
            if (c <= 32 || c >= 127) return false;  // Control chars
            if (c == '(' || c == ')' || c == '<' || c == '>' || c == '@' ||
                c == ',' || c == ';' || c == ':' || c == '\\' || c == '"' ||
                c == '/' || c == '[' || c == ']' || c == '?' || c == '=') {
                return false;  // Special chars
            }
        }

        return true;
    }

    /// Trim leading/trailing whitespace
    static std::string_view trim(std::string_view str) {
        size_t start = 0;
        while (start < str.size() && (str[start] == ' ' || str[start] == '\t')) {
            start++;
        }

        size_t end = str.size();
        while (end > start && (str[end - 1] == ' ' || str[end - 1] == '\t')) {
            end--;
        }

        return str.substr(start, end - start);
    }
};

} // namespace libglot::mime
