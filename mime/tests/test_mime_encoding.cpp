#include <catch2/catch_test_macros.hpp>
#include "../include/libglot/mime/encoding.h"
#include "../include/libglot/mime/charset.h"

using namespace libglot::mime;

TEST_CASE("Transfer Encoding: Base64 decode simple", "[encoding][base64]") {
    std::string_view encoded = "SGVsbG8gV29ybGQ=";
    std::string decoded = TransferEncoding::decode_base64(encoded);
    REQUIRE(decoded == "Hello World");
}

TEST_CASE("Transfer Encoding: Base64 decode longer text", "[encoding][base64]") {
    std::string_view encoded = "VGhlIHF1aWNrIGJyb3duIGZveCBqdW1wcyBvdmVyIHRoZSBsYXp5IGRvZw==";
    std::string decoded = TransferEncoding::decode_base64(encoded);
    REQUIRE(decoded == "The quick brown fox jumps over the lazy dog");
}

TEST_CASE("Transfer Encoding: Base64 with whitespace", "[encoding][base64]") {
    std::string_view encoded = "SGVs\nbG8g\r\nV29y\nbGQ=";
    std::string decoded = TransferEncoding::decode_base64(encoded);
    REQUIRE(decoded == "Hello World");
}

TEST_CASE("Transfer Encoding: Quoted-Printable simple", "[encoding][qp]") {
    std::string_view encoded = "Hello=20World";
    std::string decoded = TransferEncoding::decode_quoted_printable(encoded);
    REQUIRE(decoded == "Hello World");
}

TEST_CASE("Transfer Encoding: Quoted-Printable with soft line breaks", "[encoding][qp]") {
    std::string_view encoded = "This is a long line=\n that continues on the next line";
    std::string decoded = TransferEncoding::decode_quoted_printable(encoded);
    REQUIRE(decoded == "This is a long line that continues on the next line");
}

TEST_CASE("Transfer Encoding: Quoted-Printable hex encoding", "[encoding][qp]") {
    std::string_view encoded = "Caf=E9";  // é in ISO-8859-1
    std::string decoded = TransferEncoding::decode_quoted_printable(encoded);
    REQUIRE(decoded.find("Caf") != std::string::npos);
    REQUIRE(decoded.size() == 4);
}

TEST_CASE("Transfer Encoding: Detect encoding types", "[encoding][detect]") {
    REQUIRE(TransferEncoding::detect_encoding("base64") == TransferEncoding::Encoding::Base64);
    REQUIRE(TransferEncoding::detect_encoding("quoted-printable") == TransferEncoding::Encoding::QuotedPrintable);
    REQUIRE(TransferEncoding::detect_encoding("7bit") == TransferEncoding::Encoding::SevenBit);
    REQUIRE(TransferEncoding::detect_encoding("8bit") == TransferEncoding::Encoding::EightBit);
    REQUIRE(TransferEncoding::detect_encoding("binary") == TransferEncoding::Encoding::Binary);
    REQUIRE(TransferEncoding::detect_encoding("Base64") == TransferEncoding::Encoding::Base64);
    REQUIRE(TransferEncoding::detect_encoding("  base64  ") == TransferEncoding::Encoding::Base64);
}

TEST_CASE("Transfer Encoding: Decode body with encoding", "[encoding][decode_body]") {
    std::string_view base64_body = "SGVsbG8gV29ybGQ=";
    std::string decoded = TransferEncoding::decode_body(base64_body, TransferEncoding::Encoding::Base64);
    REQUIRE(decoded == "Hello World");

    std::string_view qp_body = "Hello=20World";
    decoded = TransferEncoding::decode_body(qp_body, TransferEncoding::Encoding::QuotedPrintable);
    REQUIRE(decoded == "Hello World");

    std::string_view plain_body = "Hello World";
    decoded = TransferEncoding::decode_body(plain_body, TransferEncoding::Encoding::SevenBit);
    REQUIRE(decoded == "Hello World");
}

TEST_CASE("Encoded-Word: RFC 2047 base64 decoding", "[encoding][rfc2047]") {
    std::string_view encoded = "=?UTF-8?B?SGVsbG8gV29ybGQ=?=";
    std::string decoded = EncodedWordDecoder::decode(encoded);
    REQUIRE(decoded == "Hello World");
}

TEST_CASE("Encoded-Word: RFC 2047 quoted-printable decoding", "[encoding][rfc2047]") {
    std::string_view encoded = "=?ISO-8859-1?Q?Caf=E9?=";
    std::string decoded = EncodedWordDecoder::decode(encoded);
    REQUIRE(decoded.find("Caf") != std::string::npos);
}

TEST_CASE("Encoded-Word: Mixed encoded and plain text", "[encoding][rfc2047]") {
    std::string_view encoded = "Subject: =?UTF-8?B?SGVsbG8=?= World";
    std::string decoded = EncodedWordDecoder::decode(encoded);
    REQUIRE(decoded == "Subject: Hello World");
}

TEST_CASE("Encoded-Word: Multiple encoded words", "[encoding][rfc2047]") {
    std::string_view encoded = "=?UTF-8?B?SGVsbG8=?= =?UTF-8?B?V29ybGQ=?=";
    std::string decoded = EncodedWordDecoder::decode(encoded);
    REQUIRE(decoded.find("Hello") != std::string::npos);
    REQUIRE(decoded.find("World") != std::string::npos);
}

TEST_CASE("Encoded-Word: Underscore as space in Q-encoding", "[encoding][rfc2047]") {
    std::string_view encoded = "=?UTF-8?Q?Hello_World?=";
    std::string decoded = EncodedWordDecoder::decode(encoded);
    REQUIRE(decoded == "Hello World");
}

TEST_CASE("Encoded-Word: Invalid format pass-through", "[encoding][rfc2047]") {
    std::string_view encoded = "=?INVALID";
    std::string decoded = EncodedWordDecoder::decode(encoded);
    REQUIRE(decoded == "=?INVALID");
}

// ============================================================================
// Base64 strictness (invalid characters must not silently decode to zeros)
// ============================================================================

TEST_CASE("Transfer Encoding: Base64 rejects invalid characters", "[encoding][base64][security]") {
    // '!!!!' previously decoded to three zero bytes because the reverse
    // table mapped every invalid byte (and 'A') to 0.
    REQUIRE(!TransferEncoding::decode_base64_strict("!!!!").has_value());
    REQUIRE(TransferEncoding::decode_base64("!!!!").empty());

    REQUIRE(!TransferEncoding::decode_base64_strict("SGVs*bG8=").has_value());
    REQUIRE(TransferEncoding::decode_base64("SGVs*bG8=").empty());
}

TEST_CASE("Transfer Encoding: Base64 strict accepts whitespace", "[encoding][base64]") {
    auto decoded = TransferEncoding::decode_base64_strict("SGVs\nbG8g\r\nV29y\nbGQ=");
    REQUIRE(decoded.has_value());
    REQUIRE(*decoded == "Hello World");
}

TEST_CASE("Transfer Encoding: Base64 strict on valid and empty input", "[encoding][base64]") {
    auto decoded = TransferEncoding::decode_base64_strict("SGVsbG8gV29ybGQ=");
    REQUIRE(decoded.has_value());
    REQUIRE(*decoded == "Hello World");

    auto empty = TransferEncoding::decode_base64_strict("");
    REQUIRE(empty.has_value());
    REQUIRE(empty->empty());
}

TEST_CASE("Transfer Encoding: Base64 'A' still decodes correctly", "[encoding][base64]") {
    // 'A' maps to value 0 and must remain distinguishable from invalid bytes
    auto decoded = TransferEncoding::decode_base64_strict("QUFB");  // "AAA"
    REQUIRE(decoded.has_value());
    REQUIRE(*decoded == "AAA");

    auto zeros = TransferEncoding::decode_base64_strict("AAAA");  // 3 zero bytes
    REQUIRE(zeros.has_value());
    REQUIRE(*zeros == std::string("\0\0\0", 3));
}

// ============================================================================
// RFC 2047 charset conversion
// ============================================================================

TEST_CASE("Encoded-Word: ISO-8859-1 decodes to UTF-8", "[encoding][rfc2047][charset]") {
    std::string decoded = EncodedWordDecoder::decode("=?ISO-8859-1?Q?caf=E9?=");
    REQUIRE(decoded == "caf\xC3\xA9");  // UTF-8 "café"
}

TEST_CASE("Encoded-Word: ISO-8859-1 base64 decodes to UTF-8", "[encoding][rfc2047][charset]") {
    // "caf\xE9" base64-encoded: Y2Fm6Q==
    std::string decoded = EncodedWordDecoder::decode("=?ISO-8859-1?B?Y2Fm6Q==?=");
    REQUIRE(decoded == "caf\xC3\xA9");
}

TEST_CASE("Encoded-Word: Windows-1252 decodes to UTF-8", "[encoding][rfc2047][charset]") {
    // 0x93/0x94 are curly quotes in Windows-1252
    std::string decoded = EncodedWordDecoder::decode("=?windows-1252?Q?=93quoted=94?=");
    REQUIRE(decoded == "\xE2\x80\x9Cquoted\xE2\x80\x9D");
}

TEST_CASE("Encoded-Word: charset name is case-insensitive", "[encoding][rfc2047][charset]") {
    std::string decoded = EncodedWordDecoder::decode("=?iso-8859-1?Q?caf=E9?=");
    REQUIRE(decoded == "caf\xC3\xA9");

    auto result = EncodedWordDecoder::decode_with_charset_info("=?Iso-8859-1?Q?caf=E9?=");
    REQUIRE(result.text == "caf\xC3\xA9");
    REQUIRE(!result.has_unknown_charset);
}

TEST_CASE("Encoded-Word: unknown charset returns raw bytes and is flagged", "[encoding][rfc2047][charset]") {
    auto result = EncodedWordDecoder::decode_with_charset_info("=?KOI8-R?Q?=D0=D2=C9?=");
    REQUIRE(result.has_unknown_charset);
    REQUIRE(result.text == "\xD0\xD2\xC9");  // raw bytes, unconverted
}

TEST_CASE("Encoded-Word: UTF-8 input is not flagged", "[encoding][rfc2047][charset]") {
    auto result = EncodedWordDecoder::decode_with_charset_info("=?UTF-8?B?SGVsbG8=?=");
    REQUIRE(result.text == "Hello");
    REQUIRE(!result.has_unknown_charset);
}

// ============================================================================
// UTF-8 validation (overlongs, surrogates, out-of-range codepoints)
// ============================================================================

TEST_CASE("Charset: is_valid_utf8 accepts valid sequences", "[charset][utf8]") {
    struct ValidCase {
        const char* label;
        std::string input;
    };
    const ValidCase cases[] = {
        {"empty", ""},
        {"ascii", "plain ASCII text"},
        {"2-byte U+00E9", "\xC3\xA9"},
        {"2-byte minimum U+0080", "\xC2\x80"},
        {"3-byte U+20AC euro", "\xE2\x82\xAC"},
        {"3-byte E0 minimum U+0800", "\xE0\xA0\x80"},
        {"3-byte before surrogates U+D7FF", "\xED\x9F\xBF"},
        {"3-byte after surrogates U+E000", "\xEE\x80\x80"},
        {"4-byte U+1F600 emoji", "\xF0\x9F\x98\x80"},
        {"4-byte minimum U+10000", "\xF0\x90\x80\x80"},
        {"4-byte maximum U+10FFFF", "\xF4\x8F\xBF\xBF"},
        {"mixed", "abc\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80xyz"},
    };

    for (const auto& c : cases) {
        INFO(c.label);
        REQUIRE(CharsetConverter::is_valid_utf8(c.input));
    }
}

TEST_CASE("Charset: is_valid_utf8 rejects invalid sequences", "[charset][utf8]") {
    struct InvalidCase {
        const char* label;
        std::string input;
    };
    const InvalidCase cases[] = {
        {"C0 overlong start", "\xC0\xAF"},
        {"C1 overlong start", "\xC1\xBF"},
        {"E0 overlong (second byte below A0)", "\xE0\x80\xA0"},
        {"E0 overlong slash", "\xE0\x80\xAF"},
        {"F0 overlong (second byte below 90)", "\xF0\x80\x80\x80"},
        {"UTF-16 surrogate U+D800", "\xED\xA0\x80"},
        {"UTF-16 surrogate U+DFFF", "\xED\xBF\xBF"},
        {"above U+10FFFF (F4 9x)", "\xF4\x90\x80\x80"},
        {"F5 lead byte invalid", "\xF5\x80\x80\x80"},
        {"FE invalid", "\xFE"},
        {"FF invalid", "\xFF"},
        {"stray continuation byte", "\x80"},
        {"stray continuation after ascii", "a\xBFz"},
        {"truncated 2-byte", "\xC3"},
        {"truncated 3-byte", "\xE2\x82"},
        {"truncated 4-byte", "\xF0\x9F\x98"},
        {"bad continuation in 2-byte", "\xC3\x29"},
        {"bad continuation in 3-byte", "\xE2\x82\x20"},
        {"bad continuation in 4-byte", "\xF0\x9F\x20\x80"},
    };

    for (const auto& c : cases) {
        INFO(c.label);
        REQUIRE(!CharsetConverter::is_valid_utf8(c.input));
    }
}
