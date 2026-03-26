#pragma once

#include <string_view>
#include <string>
#include <vector>
#include <cctype>

namespace libglot::mime {

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
    /// Decode base64 encoded data
    static std::string decode_base64(std::string_view encoded) {
        static const char base64_table[] =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

        // Build reverse lookup table
        static unsigned char reverse_table[256] = {0};
        static bool table_initialized = false;
        if (!table_initialized) {
            for (int i = 0; i < 64; ++i) {
                reverse_table[static_cast<unsigned char>(base64_table[i])] = i;
            }
            table_initialized = true;
        }

        std::string decoded;
        decoded.reserve(encoded.size() * 3 / 4);

        uint32_t buffer = 0;
        int bits_collected = 0;

        for (char c : encoded) {
            if (std::isspace(c)) continue;  // Skip whitespace
            if (c == '=') break;  // Padding

            unsigned char val = reverse_table[static_cast<unsigned char>(c)];
            buffer = (buffer << 6) | val;
            bits_collected += 6;

            if (bits_collected >= 8) {
                bits_collected -= 8;
                decoded.push_back(static_cast<char>((buffer >> bits_collected) & 0xFF));
            }
        }

        return decoded;
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
                    if (encoded[i + 1] == '\r' && i + 2 < encoded.size() && encoded[i + 2] == '\n') {
                        i += 2;  // Skip =\r\n
                        continue;
                    } else if (encoded[i + 1] == '\n') {
                        i += 1;  // Skip =\n
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

    /// Detect transfer encoding from Content-Transfer-Encoding header
    enum class Encoding {
        SevenBit,
        EightBit,
        Binary,
        QuotedPrintable,
        Base64,
        Unknown
    };

    static Encoding detect_encoding(std::string_view header_value) {
        // Convert to lowercase for comparison
        std::string lower;
        lower.reserve(header_value.size());
        for (char c : header_value) {
            lower.push_back(std::tolower(c));
        }

        std::string_view lv = lower;

        // Trim whitespace
        while (!lv.empty() && std::isspace(lv.front())) lv.remove_prefix(1);
        while (!lv.empty() && std::isspace(lv.back())) lv.remove_suffix(1);

        if (lv == "base64") return Encoding::Base64;
        if (lv == "quoted-printable") return Encoding::QuotedPrintable;
        if (lv == "7bit") return Encoding::SevenBit;
        if (lv == "8bit") return Encoding::EightBit;
        if (lv == "binary") return Encoding::Binary;

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
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
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
    /// Decode RFC 2047 encoded words in a header value
    /// Example: "=?UTF-8?B?SGVsbG8gV29ybGQ=?=" → "Hello World"
    static std::string decode(std::string_view header_value) {
        std::string result;
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
            std::string_view encoding = header_value.substr(charset_end + 1, encoding_end - (charset_end + 1));
            std::string_view text = header_value.substr(encoding_end + 1, text_end - (encoding_end + 1));

            // Decode based on encoding
            std::string decoded_text;
            if (encoding == "B" || encoding == "b") {
                // Base64
                decoded_text = TransferEncoding::decode_base64(text);
            } else if (encoding == "Q" || encoding == "q") {
                // Quoted-printable (with _ instead of space)
                std::string qp_text(text);
                for (char& c : qp_text) {
                    if (c == '_') c = ' ';
                }
                decoded_text = TransferEncoding::decode_quoted_printable(qp_text);
            } else {
                // Unknown encoding, keep as-is
                decoded_text = std::string(text);
            }

            result.append(decoded_text);
            pos = text_end + 2;
        }

        return result;
    }
};

} // namespace libglot::mime
