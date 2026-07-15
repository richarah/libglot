#pragma once

#include "charset.h"

#include <array>
#include <cctype>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace libglot::mime {

namespace detail {

/// Sentinel marking bytes that are not part of the base64 alphabet
inline constexpr std::uint8_t kBase64Invalid = 0xFF;

/// Compile-time base64 reverse lookup table (no lazy runtime init, so no
/// data race). Invalid bytes map to kBase64Invalid instead of silently
/// aliasing 'A' (value 0).
inline constexpr std::array<std::uint8_t, 256> kBase64ReverseTable = [] {
    constexpr std::string_view alphabet =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::array<std::uint8_t, 256> table{};
    for (auto& entry : table) {
        entry = kBase64Invalid;
    }
    for (std::size_t i = 0; i < alphabet.size(); ++i) {
        table[static_cast<unsigned char>(alphabet[i])] = static_cast<std::uint8_t>(i);
    }
    return table;
}();

} // namespace detail

/// ============================================================================
/// Content-Transfer-Encoding Handlers
/// ============================================================================
///
/// Implements decoding for MIME transfer encodings:
/// - base64 (RFC 2045)
/// - quoted-printable (RFC 2045)
/// - 7bit, 8bit, binary (pass-through)
/// ============================================================================

class TransferEncoding {
public:
    /// Strictly decode base64 encoded data (RFC 2045).
    /// Whitespace is ignored (base64 transfer encoding is line-wrapped);
    /// decoding stops at the first '=' padding character. Any other byte
    /// outside the base64 alphabet makes the input invalid and yields
    /// std::nullopt -- invalid characters are never silently decoded as
    /// zero bits.
    static std::optional<std::string> decode_base64_strict(std::string_view encoded) {
        std::string decoded;
        decoded.reserve(encoded.size() * 3 / 4);

        uint32_t buffer = 0;
        int bits_collected = 0;

        for (char c : encoded) {
            if (std::isspace(static_cast<unsigned char>(c)))
                continue; // Skip whitespace
            if (c == '=')
                break; // Padding

            std::uint8_t val = detail::kBase64ReverseTable[static_cast<unsigned char>(c)];
            if (val == detail::kBase64Invalid) {
                return std::nullopt;
            }

            buffer = (buffer << 6) | val;
            bits_collected += 6;

            if (bits_collected >= 8) {
                bits_collected -= 8;
                decoded.push_back(static_cast<char>((buffer >> bits_collected) & 0xFF));
            }
        }

        return decoded;
    }

    /// Decode base64 encoded data.
    /// Convenience wrapper around decode_base64_strict that keeps the
    /// historical std::string signature: invalid input (any non-whitespace
    /// byte outside the base64 alphabet) yields an empty string rather than
    /// corrupted output. Callers that must distinguish "empty" from
    /// "invalid" should use decode_base64_strict.
    static std::string decode_base64(std::string_view encoded) {
        return decode_base64_strict(encoded).value_or(std::string());
    }

    /// Encode data as base64 (RFC 2045), without line wrapping. Useful when
    /// the caller does its own line folding (e.g. RFC 2047 encoded-words).
    static std::string encode_base64_raw(std::string_view data) {
        static constexpr std::string_view kAlphabet =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

        std::string out;
        out.reserve(((data.size() + 2) / 3) * 4);

        size_t i = 0;
        for (; i + 3 <= data.size(); i += 3) {
            uint32_t n = (static_cast<unsigned char>(data[i]) << 16) |
                         (static_cast<unsigned char>(data[i + 1]) << 8) |
                         static_cast<unsigned char>(data[i + 2]);
            out.push_back(kAlphabet[(n >> 18) & 0x3F]);
            out.push_back(kAlphabet[(n >> 12) & 0x3F]);
            out.push_back(kAlphabet[(n >> 6) & 0x3F]);
            out.push_back(kAlphabet[n & 0x3F]);
        }

        size_t rem = data.size() - i;
        if (rem == 1) {
            uint32_t n = static_cast<unsigned char>(data[i]) << 16;
            out.push_back(kAlphabet[(n >> 18) & 0x3F]);
            out.push_back(kAlphabet[(n >> 12) & 0x3F]);
            out.push_back('=');
            out.push_back('=');
        } else if (rem == 2) {
            uint32_t n = (static_cast<unsigned char>(data[i]) << 16) |
                         (static_cast<unsigned char>(data[i + 1]) << 8);
            out.push_back(kAlphabet[(n >> 18) & 0x3F]);
            out.push_back(kAlphabet[(n >> 12) & 0x3F]);
            out.push_back(kAlphabet[(n >> 6) & 0x3F]);
            out.push_back('=');
        }

        return out;
    }

    /// Encode data as base64 (RFC 2045) with 76-character line wrapping.
    /// Every line, including the last, is CRLF-terminated (so the output is
    /// ready to drop directly into a MIME body); empty input yields an empty
    /// string. Round-trips through decode_base64_strict (which ignores
    /// whitespace and stops at '=' padding).
    static std::string encode_base64(std::string_view data) {
        std::string raw = encode_base64_raw(data);

        std::string out;
        out.reserve(raw.size() + (raw.size() / 76 + 1) * 2);
        for (size_t pos = 0; pos < raw.size(); pos += 76) {
            size_t len = std::min<size_t>(76, raw.size() - pos);
            out.append(raw, pos, len);
            out += "\r\n";
        }
        return out;
    }

    /// Decode quoted-printable encoded data
    static std::string decode_quoted_printable(std::string_view encoded) {
        std::string decoded;
        decoded.reserve(encoded.size());

        for (size_t i = 0; i < encoded.size(); ++i) {
            char c = encoded[i];

            if (c == '=') {
                // Soft line break (=\n or =\r\n)
                if (i + 1 < encoded.size()) {
                    if (encoded[i + 1] == '\r' && i + 2 < encoded.size() &&
                        encoded[i + 2] == '\n') {
                        i += 2; // Skip =\r\n
                        continue;
                    } else if (encoded[i + 1] == '\n') {
                        i += 1; // Skip =\n
                        continue;
                    }
                }

                // Hex-encoded byte
                if (i + 2 < encoded.size()) {
                    char hex1 = encoded[i + 1];
                    char hex2 = encoded[i + 2];

                    if (std::isxdigit(hex1) && std::isxdigit(hex2)) {
                        int val = (hex_to_int(hex1) << 4) | hex_to_int(hex2);
                        decoded.push_back(static_cast<char>(val));
                        i += 2;
                        continue;
                    }
                }

                // Invalid sequence, keep as-is
                decoded.push_back(c);
            } else {
                decoded.push_back(c);
            }
        }

        return decoded;
    }

    /// Encode data as quoted-printable (RFC 2045).
    /// - Printable ASCII (0x21-0x7E) other than '=' passes through literally
    /// - '=' is always escaped ("=3D")
    /// - space/tab are literal, *except* when they are the last character
    ///   before a line break or at the very end of the data (trailing
    ///   whitespace can be altered/stripped in transit), in which case they
    ///   are escaped ("=20"/"=09")
    /// - every other byte (control chars, 8-bit) is escaped as "=XX" (upper
    ///   case hex)
    /// - existing CR/LF bytes are passed through untouched as hard line
    ///   breaks (never escaped), so already-canonical CRLF text round-trips
    ///   byte for byte
    /// - soft line breaks ("=" + CRLF) are inserted so no encoded line
    ///   exceeds 76 characters (RFC 2045 section 6.7 rule 5)
    static std::string encode_quoted_printable(std::string_view data) {
        static constexpr char kHex[] = "0123456789ABCDEF";
        // Content budget before a line is soft-broken. Kept at 75 (not 76)
        // so a line that ends up needing the trailing '=' soft-break marker
        // still never exceeds the 76-character RFC 2045 line limit.
        constexpr size_t kSoftLineLimit = 75;

        std::string out;
        out.reserve(data.size() + data.size() / 20);
        size_t line_len = 0;

        auto soft_break_if_needed = [&](size_t needed) {
            if (line_len + needed > kSoftLineLimit) {
                out += "=\r\n";
                line_len = 0;
            }
        };
        auto emit_hex = [&](unsigned char c) {
            soft_break_if_needed(3);
            out.push_back('=');
            out.push_back(kHex[(c >> 4) & 0xF]);
            out.push_back(kHex[c & 0xF]);
            line_len += 3;
        };
        auto emit_lit = [&](char c) {
            soft_break_if_needed(1);
            out.push_back(c);
            line_len += 1;
        };

        for (size_t i = 0; i < data.size(); ++i) {
            unsigned char c = static_cast<unsigned char>(data[i]);

            if (c == '\r' || c == '\n') {
                // Hard line break: passed through untouched, never escaped.
                out.push_back(static_cast<char>(c));
                line_len = 0;
                continue;
            }

            if (c == '=') {
                emit_hex(c);
                continue;
            }

            if (c == ' ' || c == '\t') {
                bool trailing =
                    (i + 1 == data.size()) || (data[i + 1] == '\r' || data[i + 1] == '\n');
                if (trailing) {
                    emit_hex(c);
                } else {
                    emit_lit(static_cast<char>(c));
                }
                continue;
            }

            if (c >= 33 && c <= 126) {
                emit_lit(static_cast<char>(c));
            } else {
                emit_hex(c);
            }
        }

        return out;
    }

    /// Detect transfer encoding from Content-Transfer-Encoding header
    enum class Encoding { SevenBit, EightBit, Binary, QuotedPrintable, Base64, Unknown };

    static Encoding detect_encoding(std::string_view header_value) {
        // Convert to lowercase for comparison
        std::string lower;
        lower.reserve(header_value.size());
        for (char c : header_value) {
            lower.push_back(std::tolower(c));
        }

        std::string_view lv = lower;

        // Trim whitespace
        while (!lv.empty() && std::isspace(lv.front()))
            lv.remove_prefix(1);
        while (!lv.empty() && std::isspace(lv.back()))
            lv.remove_suffix(1);

        if (lv == "base64")
            return Encoding::Base64;
        if (lv == "quoted-printable")
            return Encoding::QuotedPrintable;
        if (lv == "7bit")
            return Encoding::SevenBit;
        if (lv == "8bit")
            return Encoding::EightBit;
        if (lv == "binary")
            return Encoding::Binary;

        return Encoding::Unknown;
    }

    /// Decode body based on Content-Transfer-Encoding
    static std::string decode_body(std::string_view body, Encoding encoding) {
        switch (encoding) {
        case Encoding::Base64:
            return decode_base64(body);

        case Encoding::QuotedPrintable:
            return decode_quoted_printable(body);

        case Encoding::SevenBit:
        case Encoding::EightBit:
        case Encoding::Binary:
        case Encoding::Unknown:
        default:
            // Pass through
            return std::string(body);
        }
    }

private:
    static int hex_to_int(char c) {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'A' && c <= 'F')
            return c - 'A' + 10;
        if (c >= 'a' && c <= 'f')
            return c - 'a' + 10;
        return 0;
    }
};

/// ============================================================================
/// Encoded-Word Decoder (RFC 2047)
/// ============================================================================
///
/// Decodes =?charset?encoding?encoded-text?= format
/// Used in email headers for non-ASCII text
/// ============================================================================

class EncodedWordDecoder {
public:
    /// Result of decoding a header value containing RFC 2047 encoded words
    struct DecodeResult {
        /// Decoded text, converted to UTF-8 for supported charsets
        /// (UTF-8, US-ASCII, ISO-8859-1, Windows-1252)
        std::string text;

        /// True if any encoded word used a charset that could not be
        /// converted to UTF-8; its decoded bytes are included verbatim
        /// (in the source charset).
        bool has_unknown_charset = false;
    };

    /// Decode RFC 2047 encoded words in a header value
    /// Example: "=?UTF-8?B?SGVsbG8gV29ybGQ=?=" → "Hello World"
    /// Decoded text is converted to UTF-8; use decode_with_charset_info to
    /// learn whether an unsupported charset was passed through verbatim.
    static std::string decode(std::string_view header_value) {
        return decode_with_charset_info(header_value).text;
    }

    /// Decode RFC 2047 encoded words, reporting unconvertible charsets
    static DecodeResult decode_with_charset_info(std::string_view header_value) {
        DecodeResult decode_result;
        std::string& result = decode_result.text;
        result.reserve(header_value.size());

        size_t pos = 0;
        while (pos < header_value.size()) {
            // Look for encoded-word marker
            size_t start = header_value.find("=?", pos);
            if (start == std::string_view::npos) {
                // No more encoded words
                result.append(header_value.substr(pos));
                break;
            }

            // Copy text before encoded word
            result.append(header_value.substr(pos, start - pos));

            // Parse encoded word: =?charset?encoding?text?=
            size_t charset_end = header_value.find('?', start + 2);
            if (charset_end == std::string_view::npos) {
                // Invalid format, keep as-is
                result.append(header_value.substr(start, 2));
                pos = start + 2;
                continue;
            }

            size_t encoding_end = header_value.find('?', charset_end + 1);
            if (encoding_end == std::string_view::npos) {
                result.append(header_value.substr(start, 2));
                pos = start + 2;
                continue;
            }

            size_t text_end = header_value.find("?=", encoding_end + 1);
            if (text_end == std::string_view::npos) {
                result.append(header_value.substr(start, 2));
                pos = start + 2;
                continue;
            }

            // Extract parts
            std::string_view charset = header_value.substr(start + 2, charset_end - (start + 2));
            std::string_view encoding =
                header_value.substr(charset_end + 1, encoding_end - (charset_end + 1));
            std::string_view text =
                header_value.substr(encoding_end + 1, text_end - (encoding_end + 1));

            // Decode based on encoding
            std::string decoded_text;
            if (encoding == "B" || encoding == "b") {
                // Base64
                decoded_text = TransferEncoding::decode_base64(text);
            } else if (encoding == "Q" || encoding == "q") {
                // Quoted-printable (with _ instead of space)
                std::string qp_text(text);
                for (char& c : qp_text) {
                    if (c == '_')
                        c = ' ';
                }
                decoded_text = TransferEncoding::decode_quoted_printable(qp_text);
            } else {
                // Unknown encoding, keep as-is
                decoded_text = std::string(text);
            }

            // The decoded bytes are in the declared source charset: convert
            // them to UTF-8 for the charsets we support. Unknown (or not yet
            // convertible) charsets are passed through verbatim and flagged.
            std::string charset_lower(charset);
            for (char& lc : charset_lower) {
                lc = static_cast<char>(std::tolower(static_cast<unsigned char>(lc)));
            }
            auto cs = CharsetConverter::detect_charset(charset_lower);
            switch (cs) {
            case CharsetConverter::Charset::UTF8:
            case CharsetConverter::Charset::USASCII:
            case CharsetConverter::Charset::ISO88591:
            case CharsetConverter::Charset::WINDOWS1252:
                decoded_text = CharsetConverter::to_utf8(decoded_text, cs);
                break;
            default:
                decode_result.has_unknown_charset = true;
                break;
            }

            result.append(decoded_text);
            pos = text_end + 2;
        }

        return decode_result;
    }

    /// Produce one or more RFC 2047 encoded-words ("=?UTF-8?B?...?=" /
    /// "=?UTF-8?Q?...?=") for a UTF-8 header value fragment.
    ///
    /// `encoding` selects TransferEncoding::Encoding::Base64 or
    /// ::QuotedPrintable (any other value is treated as Base64).
    ///
    /// Each encoded-word is kept within the RFC 2047 75-character limit
    /// ("=?" + charset + "?" + B/Q + "?" + encoded-text + "?="): when
    /// `utf8_text` doesn't fit in one word, it is split into multiple
    /// words, always at a UTF-8 codepoint boundary (never inside a
    /// multi-byte sequence). Adjacent words are emitted back-to-back with
    /// no separating whitespace, since decode()/decode_with_charset_info()
    /// treat any text between a word's "?=" and the next "=?" as ordinary
    /// literal content -- so this is what makes
    /// decode(encode_word(text, enc)) == text an exact round trip.
    static std::string encode_word(std::string_view utf8_text,
                                   TransferEncoding::Encoding encoding) {
        if (utf8_text.empty()) {
            return "";
        }

        const bool use_base64 = (encoding != TransferEncoding::Encoding::QuotedPrintable);
        static constexpr std::string_view kCharset = "UTF-8";
        // "=?" + "UTF-8" + "?" + B-or-Q + "?" + "?=" = 2+5+1+1+1+2 = 12
        constexpr size_t kOverhead = 12;
        constexpr size_t kBudget = 75 - kOverhead; // 63 chars of encoded-text

        std::string result;
        size_t pos = 0;

        while (pos < utf8_text.size()) {
            size_t chunk_bytes = use_base64 ? base64_word_chunk_bytes(utf8_text, pos, kBudget)
                                            : qp_word_chunk_bytes(utf8_text, pos, kBudget);
            if (chunk_bytes == 0) {
                // Defensive: guarantee forward progress even in a case this
                // logic didn't anticipate (never happens for valid UTF-8).
                chunk_bytes = utf8_text.size() - pos;
            }

            std::string_view chunk = utf8_text.substr(pos, chunk_bytes);

            result += "=?";
            result += kCharset;
            if (use_base64) {
                result += "?B?";
                result += TransferEncoding::encode_base64_raw(chunk);
            } else {
                result += "?Q?";
                append_qp_word(result, chunk);
            }
            result += "?=";

            pos += chunk_bytes;
        }

        return result;
    }

private:
    /// Length in bytes of the UTF-8 sequence starting at text[pos]. Clamps
    /// to the remaining buffer for a truncated/invalid trailing sequence, so
    /// this never reads out of bounds even on malformed input.
    static size_t utf8_rune_length(std::string_view text, size_t pos) {
        unsigned char lead = static_cast<unsigned char>(text[pos]);
        size_t len;
        if ((lead & 0x80) == 0x00) {
            len = 1;
        } else if ((lead & 0xE0) == 0xC0) {
            len = 2;
        } else if ((lead & 0xF0) == 0xE0) {
            len = 3;
        } else if ((lead & 0xF8) == 0xF0) {
            len = 4;
        } else {
            len = 1; // invalid lead byte: treat as one byte, never loop forever
        }
        if (pos + len > text.size()) {
            len = text.size() - pos;
        }
        return len;
    }

    /// Bytes to include in the next base64 encoded-word so the base64 text
    /// stays within `budget` characters. Only full 3-byte/4-char groups are
    /// used for every word (the final word may still need '=' padding, but
    /// mid-message words never do), and the split always lands on a UTF-8
    /// codepoint boundary.
    static size_t base64_word_chunk_bytes(std::string_view text, size_t pos, size_t budget) {
        const size_t max_bytes = (budget / 4) * 3;
        size_t bytes = 0;
        while (pos + bytes < text.size() && bytes < max_bytes) {
            size_t rune_len = utf8_rune_length(text, pos + bytes);
            if (bytes + rune_len > max_bytes) {
                break;
            }
            bytes += rune_len;
        }
        return bytes;
    }

    /// Q-encoded length of a single byte: 1 for the literal/underscore form,
    /// 3 for the "=XX" escape.
    static size_t qp_word_byte_len(unsigned char c) {
        if (c == ' ')
            return 1;
        if (c >= 0x21 && c <= 0x7E && c != '=' && c != '?' && c != '_')
            return 1;
        return 3;
    }

    /// Bytes to include in the next Q-encoded word so its encoded length
    /// stays within `budget` characters, split on a UTF-8 codepoint boundary.
    static size_t qp_word_chunk_bytes(std::string_view text, size_t pos, size_t budget) {
        size_t bytes = 0;
        size_t encoded_len = 0;
        while (pos + bytes < text.size()) {
            size_t rune_len = utf8_rune_length(text, pos + bytes);
            size_t rune_encoded_len = 0;
            for (size_t k = 0; k < rune_len; ++k) {
                rune_encoded_len +=
                    qp_word_byte_len(static_cast<unsigned char>(text[pos + bytes + k]));
            }
            if (encoded_len + rune_encoded_len > budget) {
                break;
            }
            encoded_len += rune_encoded_len;
            bytes += rune_len;
        }
        return bytes;
    }

    /// RFC 2047 Q-encoding of one word's raw bytes (space -> '_'; '='/'?'/'_'
    /// and anything outside printable ASCII -> "=XX").
    static void append_qp_word(std::string& out, std::string_view chunk) {
        static constexpr char kHex[] = "0123456789ABCDEF";
        for (unsigned char c : chunk) {
            if (c == ' ') {
                out.push_back('_');
            } else if (c >= 0x21 && c <= 0x7E && c != '=' && c != '?' && c != '_') {
                out.push_back(static_cast<char>(c));
            } else {
                out.push_back('=');
                out.push_back(kHex[(c >> 4) & 0xF]);
                out.push_back(kHex[c & 0xF]);
            }
        }
    }
};

} // namespace libglot::mime
