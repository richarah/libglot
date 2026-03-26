#include <catch2/catch_test_macros.hpp>
#include "../include/libglot/mime/encoding.h"

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
