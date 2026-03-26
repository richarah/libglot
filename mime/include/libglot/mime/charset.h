#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <cstdint>

namespace libglot::mime {

/// ============================================================================
/// MIME Charset Conversion
/// ============================================================================
///
/// Handles character set conversions for MIME messages per RFC 2047/2231.
/// Supports common charsets: UTF-8, ISO-8859-1, US-ASCII, Windows-1252
///
/// Limitations:
/// - Full conversion requires external libraries (like iconv)
/// - This provides basic conversions for common cases
/// - For production, integrate with ICU or iconv
/// ============================================================================

class CharsetConverter {
public:
    enum class Charset {
        UTF8,
        ISO88591,      // Latin-1
        USASCII,
        WINDOWS1252,
        UTF16BE,
        UTF16LE,
        Unknown
    };

    /// Detect charset from MIME charset name
    static Charset detect_charset(std::string_view charset_name) {
        static const std::unordered_map<std::string_view, Charset> charset_map = {
            {"UTF-8", Charset::UTF8},
            {"utf-8", Charset::UTF8},
            {"ISO-8859-1", Charset::ISO88591},
            {"iso-8859-1", Charset::ISO88591},
            {"latin1", Charset::ISO88591},
            {"US-ASCII", Charset::USASCII},
            {"us-ascii", Charset::USASCII},
            {"ASCII", Charset::USASCII},
            {"ascii", Charset::USASCII},
            {"windows-1252", Charset::WINDOWS1252},
            {"Windows-1252", Charset::WINDOWS1252},
            {"UTF-16BE", Charset::UTF16BE},
            {"utf-16be", Charset::UTF16BE},
            {"UTF-16LE", Charset::UTF16LE},
            {"utf-16le", Charset::UTF16LE},
        };

        auto it = charset_map.find(charset_name);
        return (it != charset_map.end()) ? it->second : Charset::Unknown;
    }

    /// Convert from source charset to UTF-8
    static std::string to_utf8(std::string_view input, Charset from_charset) {
        if (from_charset == Charset::UTF8) {
            return std::string(input);  // Already UTF-8
        }

        if (from_charset == Charset::USASCII) {
            // US-ASCII is a subset of UTF-8, direct copy
            return std::string(input);
        }

        if (from_charset == Charset::ISO88591) {
            return iso88591_to_utf8(input);
        }

        if (from_charset == Charset::WINDOWS1252) {
            return windows1252_to_utf8(input);
        }

        // For other charsets, return as-is (would need external library)
        return std::string(input);
    }

    /// Convert ISO-8859-1 (Latin-1) to UTF-8
    static std::string iso88591_to_utf8(std::string_view input) {
        std::string result;
        result.reserve(input.size() * 2);  // UTF-8 can be up to 2 bytes per char

        for (unsigned char c : input) {
            if (c < 0x80) {
                // ASCII range, 1 byte in UTF-8
                result.push_back(static_cast<char>(c));
            } else {
                // Extended range, 2 bytes in UTF-8
                result.push_back(static_cast<char>(0xC0 | (c >> 6)));
                result.push_back(static_cast<char>(0x80 | (c & 0x3F)));
            }
        }

        return result;
    }

    /// Convert Windows-1252 to UTF-8
    static std::string windows1252_to_utf8(std::string_view input) {
        // Windows-1252 mapping for 0x80-0x9F range (differs from ISO-8859-1)
        static const uint16_t win1252_map[32] = {
            0x20AC, 0x0081, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
            0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x008D, 0x017D, 0x008F,
            0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
            0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x009D, 0x017E, 0x0178
        };

        std::string result;
        result.reserve(input.size() * 3);  // UTF-8 can be up to 3 bytes per char

        for (unsigned char c : input) {
            if (c < 0x80) {
                // ASCII range
                result.push_back(static_cast<char>(c));
            } else if (c < 0xA0) {
                // Windows-1252 special range (0x80-0x9F)
                uint16_t unicode = win1252_map[c - 0x80];
                append_utf8(result, unicode);
            } else {
                // 0xA0-0xFF: same as ISO-8859-1
                result.push_back(static_cast<char>(0xC0 | (c >> 6)));
                result.push_back(static_cast<char>(0x80 | (c & 0x3F)));
            }
        }

        return result;
    }

    /// Validate UTF-8 encoding
    static bool is_valid_utf8(std::string_view input) {
        size_t i = 0;
        while (i < input.size()) {
            unsigned char c = input[i];

            if (c < 0x80) {
                // ASCII, 1 byte
                i++;
                continue;
            }

            // Multi-byte sequence
            int bytes = 0;
            if ((c & 0xE0) == 0xC0) bytes = 2;
            else if ((c & 0xF0) == 0xE0) bytes = 3;
            else if ((c & 0xF8) == 0xF0) bytes = 4;
            else return false;  // Invalid start byte

            // Check we have enough bytes
            if (i + bytes > input.size()) return false;

            // Check continuation bytes
            for (int j = 1; j < bytes; j++) {
                if ((input[i + j] & 0xC0) != 0x80) {
                    return false;  // Invalid continuation byte
                }
            }

            i += bytes;
        }

        return true;
    }

private:
    /// Append Unicode codepoint as UTF-8
    static void append_utf8(std::string& output, uint16_t codepoint) {
        if (codepoint < 0x80) {
            output.push_back(static_cast<char>(codepoint));
        } else if (codepoint < 0x800) {
            output.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
            output.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        } else {
            output.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
            output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
            output.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        }
    }
};

} // namespace libglot::mime
