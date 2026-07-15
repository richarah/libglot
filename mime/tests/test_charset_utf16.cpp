/// ============================================================================
/// UTF-16 -> UTF-8 Conversion Tests (RFC 2781)
/// ============================================================================
///
/// Exercises CharsetConverter::utf16_to_utf8 directly (BOM detection,
/// explicit endianness, surrogate pairs, unpaired surrogates, truncated
/// input) and its wiring into decoded_body_utf8() via the charset=UTF-16 /
/// UTF-16BE / UTF-16LE Content-Type parameter.
/// ============================================================================

#include "../../core/include/libglot/util/arena.h"
#include "../include/libglot/mime/mime.h"
#include <catch2/catch_test_macros.hpp>

using namespace libglot::mime;

namespace {

/// Build a big-endian UTF-16 byte string from a list of 16-bit code units.
std::string utf16be(std::initializer_list<uint16_t> units) {
    std::string bytes;
    for (uint16_t u : units) {
        bytes.push_back(static_cast<char>((u >> 8) & 0xFF));
        bytes.push_back(static_cast<char>(u & 0xFF));
    }
    return bytes;
}

std::string utf16le(std::initializer_list<uint16_t> units) {
    std::string bytes;
    for (uint16_t u : units) {
        bytes.push_back(static_cast<char>(u & 0xFF));
        bytes.push_back(static_cast<char>((u >> 8) & 0xFF));
    }
    return bytes;
}

} // namespace

TEST_CASE("UTF-16: ASCII text, big-endian, no BOM", "[charset][utf16]") {
    // "Hi" -> U+0048 U+0069
    std::string input = utf16be({0x0048, 0x0069});
    std::string utf8 = CharsetConverter::utf16_to_utf8(input, Endianness::Big);
    REQUIRE(utf8 == "Hi");
    REQUIRE(CharsetConverter::is_valid_utf8(utf8));
}

TEST_CASE("UTF-16: ASCII text, little-endian, no BOM", "[charset][utf16]") {
    std::string input = utf16le({0x0048, 0x0069});
    std::string utf8 = CharsetConverter::utf16_to_utf8(input, Endianness::Little);
    REQUIRE(utf8 == "Hi");
}

TEST_CASE("UTF-16: default endianness is big-endian per RFC 2781 when no BOM is given",
          "[charset][utf16]") {
    std::string input = utf16be({0x0041}); // 'A'
    // No explicit endianness argument -> defaults to Big
    std::string utf8 = CharsetConverter::utf16_to_utf8(input);
    REQUIRE(utf8 == "A");
}

TEST_CASE("UTF-16: FEFF BOM selects big-endian and is consumed", "[charset][utf16][bom]") {
    std::string input = "\xFE\xFF" + utf16be({0x0041, 0x0042});
    // default_endianness passed as Little to prove the BOM overrides it
    std::string utf8 = CharsetConverter::utf16_to_utf8(input, Endianness::Little);
    REQUIRE(utf8 == "AB");
    // BOM must not be re-emitted as a UTF-8 codepoint (U+FEFF -> EF BB BF)
    REQUIRE(utf8.find('\xEF') == std::string::npos);
}

TEST_CASE("UTF-16: FFFE BOM selects little-endian and is consumed", "[charset][utf16][bom]") {
    std::string input = "\xFF\xFE" + utf16le({0x0041, 0x0042});
    // default_endianness passed as Big to prove the BOM overrides it
    std::string utf8 = CharsetConverter::utf16_to_utf8(input, Endianness::Big);
    REQUIRE(utf8 == "AB");
}

TEST_CASE("UTF-16: surrogate pair decodes an astral codepoint (emoji)",
          "[charset][utf16][surrogates]") {
    // U+1F600 GRINNING FACE = high surrogate D83D, low surrogate DE00
    std::string input = utf16be({0xD83D, 0xDE00});
    std::string utf8 = CharsetConverter::utf16_to_utf8(input, Endianness::Big);
    REQUIRE(utf8 == "\xF0\x9F\x98\x80");
    REQUIRE(CharsetConverter::is_valid_utf8(utf8));
}

TEST_CASE("UTF-16: surrogate pair round-trips surrounded by ASCII text",
          "[charset][utf16][surrogates]") {
    std::string input = utf16be({0x0048, 0xD83D, 0xDE00, 0x0021}); // "H" emoji "!"
    std::string utf8 = CharsetConverter::utf16_to_utf8(input, Endianness::Big);
    REQUIRE(utf8 == "H\xF0\x9F\x98\x80!");
}

TEST_CASE("UTF-16: unpaired high surrogate becomes U+FFFD, never crashes",
          "[charset][utf16][surrogates][security]") {
    // High surrogate D800 followed by an ordinary BMP char, not a low surrogate
    std::string input = utf16be({0xD800, 0x0041});
    std::string utf8;
    REQUIRE_NOTHROW(utf8 = CharsetConverter::utf16_to_utf8(input, Endianness::Big));
    REQUIRE(utf8 == "\xEF\xBF\xBD"
                    "A"); // U+FFFD then 'A'
    REQUIRE(CharsetConverter::is_valid_utf8(utf8));
}

TEST_CASE("UTF-16: unpaired high surrogate at end of input becomes U+FFFD",
          "[charset][utf16][surrogates][security]") {
    std::string input = utf16be({0x0041, 0xD800});
    std::string utf8;
    REQUIRE_NOTHROW(utf8 = CharsetConverter::utf16_to_utf8(input, Endianness::Big));
    REQUIRE(utf8 == "A\xEF\xBF\xBD");
    REQUIRE(CharsetConverter::is_valid_utf8(utf8));
}

TEST_CASE("UTF-16: unpaired low surrogate becomes U+FFFD, never crashes",
          "[charset][utf16][surrogates][security]") {
    // Low surrogate DC00 with no preceding high surrogate
    std::string input = utf16be({0xDC00, 0x0041});
    std::string utf8;
    REQUIRE_NOTHROW(utf8 = CharsetConverter::utf16_to_utf8(input, Endianness::Big));
    REQUIRE(utf8 == "\xEF\xBF\xBD"
                    "A");
    REQUIRE(CharsetConverter::is_valid_utf8(utf8));
}

TEST_CASE("UTF-16: two consecutive high surrogates each become U+FFFD",
          "[charset][utf16][surrogates][security]") {
    std::string input = utf16be({0xD800, 0xD801});
    std::string utf8;
    REQUIRE_NOTHROW(utf8 = CharsetConverter::utf16_to_utf8(input, Endianness::Big));
    REQUIRE(utf8 == "\xEF\xBF\xBD\xEF\xBF\xBD");
    REQUIRE(CharsetConverter::is_valid_utf8(utf8));
}

TEST_CASE("UTF-16: odd trailing byte becomes U+FFFD, never crashes or reads out of bounds",
          "[charset][utf16][security]") {
    std::string input = utf16be({0x0041}) + std::string(1, '\x00'); // "A" + one stray byte
    std::string utf8;
    REQUIRE_NOTHROW(utf8 = CharsetConverter::utf16_to_utf8(input, Endianness::Big));
    REQUIRE(utf8 == "A\xEF\xBF\xBD");
    REQUIRE(CharsetConverter::is_valid_utf8(utf8));
}

TEST_CASE("UTF-16: single stray odd byte (no complete code unit at all)",
          "[charset][utf16][security]") {
    std::string input(1, '\x41');
    std::string utf8;
    REQUIRE_NOTHROW(utf8 = CharsetConverter::utf16_to_utf8(input, Endianness::Big));
    REQUIRE(utf8 == "\xEF\xBF\xBD");
}

TEST_CASE("UTF-16: empty input yields empty output", "[charset][utf16]") {
    REQUIRE(CharsetConverter::utf16_to_utf8("", Endianness::Big) == "");
}

TEST_CASE("UTF-16: detect_charset recognizes UTF-16/UTF-16BE/UTF-16LE case-insensitively",
          "[charset][utf16]") {
    REQUIRE(CharsetConverter::detect_charset("UTF-16") == CharsetConverter::Charset::UTF16);
    REQUIRE(CharsetConverter::detect_charset("utf-16") == CharsetConverter::Charset::UTF16);
    REQUIRE(CharsetConverter::detect_charset("UTF-16BE") == CharsetConverter::Charset::UTF16BE);
    REQUIRE(CharsetConverter::detect_charset("utf-16be") == CharsetConverter::Charset::UTF16BE);
    REQUIRE(CharsetConverter::detect_charset("UTF-16LE") == CharsetConverter::Charset::UTF16LE);
    REQUIRE(CharsetConverter::detect_charset("utf-16le") == CharsetConverter::Charset::UTF16LE);
}

TEST_CASE("UTF-16: to_utf8 dispatches UTF16/UTF16BE/UTF16LE correctly", "[charset][utf16]") {
    std::string be = utf16be({0x0048, 0x0069});
    std::string le = utf16le({0x0048, 0x0069});

    REQUIRE(CharsetConverter::to_utf8(be, CharsetConverter::Charset::UTF16BE) == "Hi");
    REQUIRE(CharsetConverter::to_utf8(le, CharsetConverter::Charset::UTF16LE) == "Hi");
    // Bare "UTF-16" with no BOM defaults to big-endian
    REQUIRE(CharsetConverter::to_utf8(be, CharsetConverter::Charset::UTF16) == "Hi");
}

// ============================================================================
// Pipeline wiring: charset=UTF-16* parts decode via decoded_body_utf8()
// ============================================================================

TEST_CASE("Pipeline: text/plain part with charset=UTF-16BE decodes to UTF-8",
          "[mime][pipeline][utf16]") {
    libglot::Arena arena;
    std::string body = utf16be({0x0048, 0x0069}); // "Hi"
    std::string source = "Content-Type: text/plain; charset=UTF-16BE\n"
                         "Content-Transfer-Encoding: 8bit\n"
                         "\n";
    std::string full = source + body;

    auto result = parse_message(arena, full);
    REQUIRE(result.message != nullptr);

    auto decoded = decoded_body_utf8(*result.message);
    REQUIRE(decoded.has_value());
    REQUIRE(*decoded == "Hi");
}

TEST_CASE("Pipeline: text/plain part with charset=UTF-16LE decodes to UTF-8",
          "[mime][pipeline][utf16]") {
    libglot::Arena arena;
    std::string body = utf16le({0x0048, 0x0069}); // "Hi"
    std::string source = "Content-Type: text/plain; charset=UTF-16LE\n"
                         "Content-Transfer-Encoding: 8bit\n"
                         "\n";
    std::string full = source + body;

    auto result = parse_message(arena, full);
    REQUIRE(result.message != nullptr);

    auto decoded = decoded_body_utf8(*result.message);
    REQUIRE(decoded.has_value());
    REQUIRE(*decoded == "Hi");
}

TEST_CASE("Pipeline: text/plain part with bare charset=UTF-16 (BOM) decodes to UTF-8",
          "[mime][pipeline][utf16]") {
    libglot::Arena arena;
    std::string body = "\xFF\xFE" + utf16le({0x0048, 0x0069}); // LE BOM + "Hi"
    std::string source = "Content-Type: text/plain; charset=UTF-16\n"
                         "Content-Transfer-Encoding: 8bit\n"
                         "\n";
    std::string full = source + body;

    auto result = parse_message(arena, full);
    REQUIRE(result.message != nullptr);

    auto decoded = decoded_body_utf8(*result.message);
    REQUIRE(decoded.has_value());
    REQUIRE(*decoded == "Hi");
}
