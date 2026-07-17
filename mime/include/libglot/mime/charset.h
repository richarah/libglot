#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace libglot::mime {

/// Byte order for UTF-16 conversion (see CharsetConverter::utf16_to_utf8)
enum class Endianness { Big, Little };

/// ============================================================================
/// MIME Charset Conversion
/// ============================================================================
///
/// Handles character set conversions for MIME messages per RFC 2047/2231.
/// Supports common charsets: UTF-8, ISO-8859-1, ISO-8859-15 (Latin-9),
/// US-ASCII, Windows-1252, UTF-16 (BE/LE, with or without a byte-order mark)
///
/// Limitations:
/// - Full conversion requires external libraries (like iconv)
/// - This provides basic conversions for common cases
/// - Asian legacy charsets (Shift-JIS, EUC-KR, GB2312, ...) are out of
///   scope: reported as Unknown, never mislabeled
/// - For production, integrate with ICU or iconv
/// ============================================================================

class CharsetConverter {
public:
    enum class Charset {
        UTF8,
        ISO88591, // Latin-1
        ISO885915, // Latin-9 (Latin-1 with 8 substitutions, incl. the Euro sign)
        USASCII,
        WINDOWS1252,
        UTF16, // bare "UTF-16": BOM-detected, big-endian default (RFC 2781)
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
            {"ISO-8859-15", Charset::ISO885915},
            {"iso-8859-15", Charset::ISO885915},
            {"iso8859-15", Charset::ISO885915},
            {"latin9", Charset::ISO885915},
            {"latin-9", Charset::ISO885915},
            {"iso_8859-15", Charset::ISO885915},
            {"US-ASCII", Charset::USASCII},
            {"us-ascii", Charset::USASCII},
            {"ASCII", Charset::USASCII},
            {"ascii", Charset::USASCII},
            {"windows-1252", Charset::WINDOWS1252},
            {"Windows-1252", Charset::WINDOWS1252},
            {"UTF-16", Charset::UTF16},
            {"utf-16", Charset::UTF16},
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
            return std::string(input); // Already UTF-8
        }

        if (from_charset == Charset::USASCII) {
            // US-ASCII is a subset of UTF-8, direct copy
            return std::string(input);
        }

        if (from_charset == Charset::ISO88591) {
            return iso88591_to_utf8(input);
        }

        if (from_charset == Charset::ISO885915) {
            return iso885915_to_utf8(input);
        }

        if (from_charset == Charset::WINDOWS1252) {
            return windows1252_to_utf8(input);
        }

        if (from_charset == Charset::UTF16BE) {
            return utf16_to_utf8(input, Endianness::Big);
        }

        if (from_charset == Charset::UTF16LE) {
            return utf16_to_utf8(input, Endianness::Little);
        }

        if (from_charset == Charset::UTF16) {
            // Bare "UTF-16": a BOM (if present) picks the byte order; RFC
            // 2781 mandates big-endian as the default when there is none.
            return utf16_to_utf8(input, Endianness::Big);
        }

        // For other charsets, return as-is (would need external library)
        return std::string(input);
    }

    /// Convert UTF-16 (optionally BOM-prefixed) to UTF-8 (RFC 2781).
    ///
    /// - A byte-order mark (bytes FE FF => big-endian, or FF FE =>
    ///   little-endian) is detected, consumed, and never re-emitted; it
    ///   overrides `default_endianness`.
    /// - Without a BOM, `default_endianness` applies (RFC 2781 mandates
    ///   big-endian when there is no BOM and no other out-of-band
    ///   indication -- see Charset::UTF16 above).
    /// - Surrogate pairs (high surrogate U+D800-DBFF followed by low
    ///   surrogate U+DC00-DFFF) combine into one astral codepoint
    ///   (U+10000-U+10FFFF), e.g. emoji.
    /// - Unpaired high/low surrogates, and a truncated trailing byte, are
    ///   replaced with U+FFFD. This function never throws and always
    ///   produces valid UTF-8 (verifiable with is_valid_utf8).
    static std::string utf16_to_utf8(std::string_view bytes,
                                     Endianness default_endianness = Endianness::Big) {
        Endianness endianness = default_endianness;
        size_t pos = 0;

        if (bytes.size() >= 2) {
            unsigned char b0 = static_cast<unsigned char>(bytes[0]);
            unsigned char b1 = static_cast<unsigned char>(bytes[1]);
            if (b0 == 0xFE && b1 == 0xFF) {
                endianness = Endianness::Big;
                pos = 2;
            } else if (b0 == 0xFF && b1 == 0xFE) {
                endianness = Endianness::Little;
                pos = 2;
            }
        }

        std::string result;
        result.reserve(bytes.size());

        auto read_unit = [&](size_t p) -> uint16_t {
            unsigned char a = static_cast<unsigned char>(bytes[p]);
            unsigned char b = static_cast<unsigned char>(bytes[p + 1]);
            return (endianness == Endianness::Big) ? static_cast<uint16_t>((a << 8) | b)
                                                   : static_cast<uint16_t>((b << 8) | a);
        };

        while (pos < bytes.size()) {
            if (pos + 2 > bytes.size()) {
                // Odd trailing byte: a truncated code unit
                append_utf8_codepoint(result, 0xFFFD);
                break;
            }

            uint16_t unit = read_unit(pos);
            pos += 2;

            if (unit >= 0xD800 && unit <= 0xDBFF) {
                // High surrogate: look for a following low surrogate
                if (pos + 2 <= bytes.size()) {
                    uint16_t low = read_unit(pos);
                    if (low >= 0xDC00 && low <= 0xDFFF) {
                        pos += 2;
                        uint32_t cp = 0x10000 + ((static_cast<uint32_t>(unit) - 0xD800) << 10) +
                                      (static_cast<uint32_t>(low) - 0xDC00);
                        append_utf8_codepoint(result, cp);
                        continue;
                    }
                }
                append_utf8_codepoint(result, 0xFFFD); // unpaired high surrogate
            } else if (unit >= 0xDC00 && unit <= 0xDFFF) {
                append_utf8_codepoint(result, 0xFFFD); // unpaired low surrogate
            } else {
                append_utf8_codepoint(result, unit);
            }
        }

        return result;
    }

    /// Convert ISO-8859-1 (Latin-1) to UTF-8
    static std::string iso88591_to_utf8(std::string_view input) {
        std::string result;
        result.reserve(input.size() * 2); // UTF-8 can be up to 2 bytes per char

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

    /// Convert ISO-8859-15 (Latin-9) to UTF-8.
    ///
    /// Latin-9 (the 1999 revision of Latin-1) is byte-identical to
    /// ISO-8859-1 except for exactly eight code points: 0xA4 EURO SIGN,
    /// 0xA6 S WITH CARON, 0xA8 s WITH CARON, 0xB4 Z WITH CARON, 0xB8 z WITH
    /// CARON, 0xBC OE LIGATURE, 0xBD oe LIGATURE, 0xBE Y WITH DIAERESIS.
    /// Every other byte (including the 0x80-0x9F C1 range) maps exactly as
    /// ISO-8859-1.
    static std::string iso885915_to_utf8(std::string_view input) {
        std::string result;
        result.reserve(input.size() * 3); // the Euro sign (U+20AC) needs 3 bytes in UTF-8

        for (unsigned char c : input) {
            uint32_t codepoint = c;
            switch (c) {
            case 0xA4:
                codepoint = 0x20AC; // EURO SIGN
                break;
            case 0xA6:
                codepoint = 0x0160; // LATIN CAPITAL LETTER S WITH CARON
                break;
            case 0xA8:
                codepoint = 0x0161; // LATIN SMALL LETTER S WITH CARON
                break;
            case 0xB4:
                codepoint = 0x017D; // LATIN CAPITAL LETTER Z WITH CARON
                break;
            case 0xB8:
                codepoint = 0x017E; // LATIN SMALL LETTER Z WITH CARON
                break;
            case 0xBC:
                codepoint = 0x0152; // LATIN CAPITAL LIGATURE OE
                break;
            case 0xBD:
                codepoint = 0x0153; // LATIN SMALL LIGATURE OE
                break;
            case 0xBE:
                codepoint = 0x0178; // LATIN CAPITAL LETTER Y WITH DIAERESIS
                break;
            default:
                break; // identical to ISO-8859-1 elsewhere
            }
            append_utf8_codepoint(result, codepoint);
        }

        return result;
    }

    /// Convert Windows-1252 to UTF-8
    static std::string windows1252_to_utf8(std::string_view input) {
        // Windows-1252 mapping for 0x80-0x9F range (differs from ISO-8859-1)
        static const uint16_t win1252_map[32] = {
            0x20AC, 0x0081, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160,
            0x2039, 0x0152, 0x008D, 0x017D, 0x008F, 0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022,
            0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x009D, 0x017E, 0x0178};

        std::string result;
        result.reserve(input.size() * 3); // UTF-8 can be up to 3 bytes per char

        for (unsigned char c : input) {
            if (c < 0x80) {
                // ASCII range
                result.push_back(static_cast<char>(c));
            } else if (c < 0xA0) {
                // Windows-1252 special range (0x80-0x9F)
                uint16_t unicode = win1252_map[c - 0x80];
                append_utf8_codepoint(result, unicode);
            } else {
                // 0xA0-0xFF: same as ISO-8859-1
                result.push_back(static_cast<char>(0xC0 | (c >> 6)));
                result.push_back(static_cast<char>(0x80 | (c & 0x3F)));
            }
        }

        return result;
    }

    /// Validate UTF-8 encoding (RFC 3629).
    /// Rejects overlong encodings (C0/C1 lead bytes, E0 with second byte
    /// below A0, F0 with second byte below 90), UTF-16 surrogates
    /// (ED A0-BF ..), codepoints above U+10FFFF (F4 with second byte above
    /// 8F, F5-FF lead bytes), stray continuation bytes, and truncated
    /// sequences.
    static bool is_valid_utf8(std::string_view input) {
        const size_t n = input.size();
        size_t i = 0;

        while (i < n) {
            const unsigned char c = static_cast<unsigned char>(input[i]);

            if (c < 0x80) {
                // ASCII, 1 byte
                i++;
                continue;
            }

            // Determine sequence length and the valid range for the second
            // byte (the constrained one); remaining bytes must be 80-BF.
            size_t bytes;
            unsigned char second_lo = 0x80;
            unsigned char second_hi = 0xBF;

            if (c >= 0xC2 && c <= 0xDF) {
                bytes = 2; // U+0080..U+07FF
            } else if (c == 0xE0) {
                bytes = 3;
                second_lo = 0xA0; // no overlong: U+0800..
            } else if (c >= 0xE1 && c <= 0xEC) {
                bytes = 3;
            } else if (c == 0xED) {
                bytes = 3;
                second_hi = 0x9F; // exclude surrogates D800-DFFF
            } else if (c >= 0xEE && c <= 0xEF) {
                bytes = 3;
            } else if (c == 0xF0) {
                bytes = 4;
                second_lo = 0x90; // no overlong: U+10000..
            } else if (c >= 0xF1 && c <= 0xF3) {
                bytes = 4;
            } else if (c == 0xF4) {
                bytes = 4;
                second_hi = 0x8F; // cap at U+10FFFF
            } else {
                // 80-BF: stray continuation byte
                // C0-C1: overlong 2-byte encoding
                // F5-FF: codepoint above U+10FFFF / invalid
                return false;
            }

            // Check we have enough bytes
            if (i + bytes > n)
                return false;

            const unsigned char second = static_cast<unsigned char>(input[i + 1]);
            if (second < second_lo || second > second_hi)
                return false;

            for (size_t j = 2; j < bytes; j++) {
                const unsigned char cont = static_cast<unsigned char>(input[i + j]);
                if (cont < 0x80 || cont > 0xBF)
                    return false;
            }

            i += bytes;
        }

        return true;
    }

private:
    /// Append a Unicode codepoint (up to U+10FFFF, the full range produced
    /// by UTF-16 surrogate pairs) as UTF-8.
    static void append_utf8_codepoint(std::string& output, uint32_t codepoint) {
        if (codepoint < 0x80) {
            output.push_back(static_cast<char>(codepoint));
        } else if (codepoint < 0x800) {
            output.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
            output.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        } else if (codepoint < 0x10000) {
            output.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
            output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
            output.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        } else {
            output.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
            output.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
            output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
            output.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        }
    }
};

} // namespace libglot::mime
