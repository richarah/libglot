#pragma once

#include <cctype>
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
/// ISO-8859-9 (Latin-5, Turkish), ISO-8859-2 (Latin-2, Central European),
/// KOI8-R (Cyrillic), US-ASCII, Windows-1252, UTF-16 (BE/LE, with or
/// without a byte-order mark)
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
        ISO88599, // Latin-5 (Turkish; Latin-1 with 6 substitutions)
        ISO88592, // Latin-2 (Central European); not a Latin-1 delta, its own table
        KOI8R, // Cyrillic (Russian); not a Latin-1 delta, its own table
        USASCII,
        WINDOWS1252,
        UTF16, // bare "UTF-16": BOM-detected, big-endian default (RFC 2781)
        UTF16BE,
        UTF16LE,
        Unknown
    };

    /// Detect charset from MIME charset name
    static Charset detect_charset(std::string_view charset_name) {
        static const std::unordered_map<std::string, Charset> charset_map = {
            {"utf-8", Charset::UTF8},
            {"utf8", Charset::UTF8},
            {"iso-8859-1", Charset::ISO88591},
            {"iso8859-1", Charset::ISO88591},
            {"iso_8859-1", Charset::ISO88591},
            {"latin1", Charset::ISO88591},
            {"latin-1", Charset::ISO88591},
            {"l1", Charset::ISO88591},
            {"iso-ir-100", Charset::ISO88591},
            {"csisolatin1", Charset::ISO88591},
            {"iso-8859-15", Charset::ISO885915},
            {"iso8859-15", Charset::ISO885915},
            {"iso_8859-15", Charset::ISO885915},
            {"latin9", Charset::ISO885915},
            {"latin-9", Charset::ISO885915},
            {"l9", Charset::ISO885915},
            {"iso-ir-203", Charset::ISO885915},
            {"csisolatin9", Charset::ISO885915},
            {"iso-8859-9", Charset::ISO88599},
            {"iso8859-9", Charset::ISO88599},
            {"iso_8859-9", Charset::ISO88599},
            {"latin5", Charset::ISO88599},
            {"latin-5", Charset::ISO88599},
            {"l5", Charset::ISO88599},
            {"iso-ir-148", Charset::ISO88599},
            {"csisolatin5", Charset::ISO88599},
            {"iso-8859-2", Charset::ISO88592},
            {"iso8859-2", Charset::ISO88592},
            {"iso_8859-2", Charset::ISO88592},
            {"latin2", Charset::ISO88592},
            {"latin-2", Charset::ISO88592},
            {"l2", Charset::ISO88592},
            {"iso-ir-101", Charset::ISO88592},
            {"csisolatin2", Charset::ISO88592},
            {"koi8-r", Charset::KOI8R},
            {"koi8r", Charset::KOI8R},
            {"cskoi8r", Charset::KOI8R},
            {"us-ascii", Charset::USASCII},
            {"ascii", Charset::USASCII},
            // IANA registers ANSI_X3.4-1968 as the PRIMARY name of this
            // charset; "US-ASCII" is one of its aliases. Real mail uses the
            // primary name: it labels ~10% of the Enron corpus (JavaMail
            // emits it), and without these entries those bodies decode as
            // unknown-charset. Full IANA alias set for US-ASCII.
            {"ansi_x3.4-1968", Charset::USASCII},
            {"ansi_x3.4-1986", Charset::USASCII},
            {"iso-ir-6", Charset::USASCII},
            {"iso646-us", Charset::USASCII},
            {"iso_646.irv:1991", Charset::USASCII},
            {"ibm367", Charset::USASCII},
            {"cp367", Charset::USASCII},
            {"csascii", Charset::USASCII},
            {"us", Charset::USASCII},
            {"windows-1252", Charset::WINDOWS1252},
            {"windows1252", Charset::WINDOWS1252},
            {"cp1252", Charset::WINDOWS1252},
            {"utf-16", Charset::UTF16},
            {"utf16", Charset::UTF16},
            {"utf-16be", Charset::UTF16BE},
            {"utf16be", Charset::UTF16BE},
            {"utf-16le", Charset::UTF16LE},
            {"utf16le", Charset::UTF16LE},
        };

        // Charset names are case-insensitive (RFC 2045 5.1: "the charset
        // parameter value ... is not case sensitive"), so normalize here
        // rather than relying on every caller to do it and rather than
        // carrying one table entry per spelling. Surrounding whitespace and
        // any quoting are already stripped by the parameter parser.
        std::string key;
        key.reserve(charset_name.size());
        for (char c : charset_name) {
            key.push_back(static_cast<char>(
                std::tolower(static_cast<unsigned char>(c))));
        }

        auto it = charset_map.find(key);
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

        if (from_charset == Charset::ISO88599) {
            return iso88599_to_utf8(input);
        }

        if (from_charset == Charset::ISO88592) {
            return iso88592_to_utf8(input);
        }

        if (from_charset == Charset::KOI8R) {
            return koi8r_to_utf8(input);
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

    /// Convert ISO-8859-9 (Latin-5, Turkish) to UTF-8.
    ///
    /// Byte-identical to ISO-8859-1 except six code points that replace the
    /// Icelandic letters Latin-1 has no use for in Turkish: 0xD0 G WITH
    /// BREVE, 0xDD I WITH DOT ABOVE, 0xDE S WITH CEDILLA, and their
    /// lowercase forms at 0xF0, 0xFD, 0xFE.
    static std::string iso88599_to_utf8(std::string_view input) {
        std::string result;
        result.reserve(input.size() * 2); // every substitution still fits in 2 UTF-8 bytes

        for (unsigned char c : input) {
            uint32_t codepoint = c;
            switch (c) {
            case 0xD0:
                codepoint = 0x011E; // LATIN CAPITAL LETTER G WITH BREVE
                break;
            case 0xDD:
                codepoint = 0x0130; // LATIN CAPITAL LETTER I WITH DOT ABOVE
                break;
            case 0xDE:
                codepoint = 0x015E; // LATIN CAPITAL LETTER S WITH CEDILLA
                break;
            case 0xF0:
                codepoint = 0x011F; // LATIN SMALL LETTER G WITH BREVE
                break;
            case 0xFD:
                codepoint = 0x0131; // LATIN SMALL LETTER DOTLESS I
                break;
            case 0xFE:
                codepoint = 0x015F; // LATIN SMALL LETTER S WITH CEDILLA
                break;
            default:
                break; // identical to ISO-8859-1 elsewhere
            }
            append_utf8_codepoint(result, codepoint);
        }

        return result;
    }

    /// Convert ISO-8859-2 (Latin-2, Central European) to UTF-8.
    ///
    /// Unlike ISO-8859-15/-9, Latin-2 is not a Latin-1 delta: only the
    /// 0xA0-0xFF ASCII-adjacent punctuation positions that are identical
    /// across every Latin-N page (space, degree sign, etc.) coincide with
    /// Latin-1; every letter position is remapped to a Czech/Polish/
    /// Hungarian/Slovak/... accented letter.
    static std::string iso88592_to_utf8(std::string_view input) {
        static constexpr uint16_t kHighMap[128] = {
            0x0080, 0x0081, 0x0082, 0x0083, 0x0084, 0x0085, 0x0086, 0x0087, 0x0088, 0x0089, 0x008A,
            0x008B, 0x008C, 0x008D, 0x008E, 0x008F, 0x0090, 0x0091, 0x0092, 0x0093, 0x0094, 0x0095,
            0x0096, 0x0097, 0x0098, 0x0099, 0x009A, 0x009B, 0x009C, 0x009D, 0x009E, 0x009F, 0x00A0,
            0x0104, 0x02D8, 0x0141, 0x00A4, 0x013D, 0x015A, 0x00A7, 0x00A8, 0x0160, 0x015E, 0x0164,
            0x0179, 0x00AD, 0x017D, 0x017B, 0x00B0, 0x0105, 0x02DB, 0x0142, 0x00B4, 0x013E, 0x015B,
            0x02C7, 0x00B8, 0x0161, 0x015F, 0x0165, 0x017A, 0x02DD, 0x017E, 0x017C, 0x0154, 0x00C1,
            0x00C2, 0x0102, 0x00C4, 0x0139, 0x0106, 0x00C7, 0x010C, 0x00C9, 0x0118, 0x00CB, 0x011A,
            0x00CD, 0x00CE, 0x010E, 0x0110, 0x0143, 0x0147, 0x00D3, 0x00D4, 0x0150, 0x00D6, 0x00D7,
            0x0158, 0x016E, 0x00DA, 0x0170, 0x00DC, 0x00DD, 0x0162, 0x00DF, 0x0155, 0x00E1, 0x00E2,
            0x0103, 0x00E4, 0x013A, 0x0107, 0x00E7, 0x010D, 0x00E9, 0x0119, 0x00EB, 0x011B, 0x00ED,
            0x00EE, 0x010F, 0x0111, 0x0144, 0x0148, 0x00F3, 0x00F4, 0x0151, 0x00F6, 0x00F7, 0x0159,
            0x016F, 0x00FA, 0x0171, 0x00FC, 0x00FD, 0x0163, 0x02D9,
        };

        std::string result;
        result.reserve(input.size() * 2); // every Latin-2 codepoint fits in 2 UTF-8 bytes

        for (unsigned char c : input) {
            uint32_t codepoint = c < 0x80 ? c : kHighMap[c - 0x80];
            append_utf8_codepoint(result, codepoint);
        }

        return result;
    }

    /// Convert KOI8-R (Russian Cyrillic) to UTF-8.
    ///
    /// Unlike ISO-8859-15/-9, KOI8-R is not a Latin-1 delta: 0x00-0x7F is
    /// plain ASCII, but 0x80-0xFF is its own table (box-drawing characters
    /// and Cyrillic letters), fixed by the standard.
    static std::string koi8r_to_utf8(std::string_view input) {
        static constexpr uint16_t kHighMap[128] = {
            0x2500, 0x2502, 0x250C, 0x2510, 0x2514, 0x2518, 0x251C, 0x2524, 0x252C, 0x2534, 0x253C,
            0x2580, 0x2584, 0x2588, 0x258C, 0x2590, 0x2591, 0x2592, 0x2593, 0x2320, 0x25A0, 0x2219,
            0x221A, 0x2248, 0x2264, 0x2265, 0x00A0, 0x2321, 0x00B0, 0x00B2, 0x00B7, 0x00F7, 0x2550,
            0x2551, 0x2552, 0x0451, 0x2553, 0x2554, 0x2555, 0x2556, 0x2557, 0x2558, 0x2559, 0x255A,
            0x255B, 0x255C, 0x255D, 0x255E, 0x255F, 0x2560, 0x2561, 0x0401, 0x2562, 0x2563, 0x2564,
            0x2565, 0x2566, 0x2567, 0x2568, 0x2569, 0x256A, 0x256B, 0x256C, 0x00A9, 0x044E, 0x0430,
            0x0431, 0x0446, 0x0434, 0x0435, 0x0444, 0x0433, 0x0445, 0x0438, 0x0439, 0x043A, 0x043B,
            0x043C, 0x043D, 0x043E, 0x043F, 0x044F, 0x0440, 0x0441, 0x0442, 0x0443, 0x0436, 0x0432,
            0x044C, 0x044B, 0x0437, 0x0448, 0x044D, 0x0449, 0x0447, 0x044A, 0x042E, 0x0410, 0x0411,
            0x0426, 0x0414, 0x0415, 0x0424, 0x0413, 0x0425, 0x0418, 0x0419, 0x041A, 0x041B, 0x041C,
            0x041D, 0x041E, 0x041F, 0x042F, 0x0420, 0x0421, 0x0422, 0x0423, 0x0416, 0x0412, 0x042C,
            0x042B, 0x0417, 0x0428, 0x042D, 0x0429, 0x0427, 0x042A,
        };

        std::string result;
        result.reserve(input.size() * 3); // Cyrillic/box-drawing codepoints need up to 3 UTF-8 bytes

        for (unsigned char c : input) {
            uint32_t codepoint = c < 0x80 ? c : kHighMap[c - 0x80];
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
