/// ============================================================================
/// ISO-8859-15 (Latin-9) -> UTF-8 Conversion Tests (issue #8)
/// ============================================================================
///
/// Exercises CharsetConverter::iso885915_to_utf8 directly (all eight
/// substituted code points, plus a Latin-1-identical pass-through range),
/// detect_charset alias recognition, and the wiring into decoded_body_utf8()
/// via the charset=iso-8859-15 Content-Type parameter.
/// ============================================================================

#include "../../core/include/libglot/util/arena.h"
#include "../include/libglot/mime/mime.h"
#include <catch2/catch_test_macros.hpp>

using namespace libglot::mime;

// ============================================================================
// The eight substitutions (Latin-9 vs. Latin-1), table-driven
// ============================================================================

TEST_CASE("Latin-9: all eight substituted code points decode correctly",
          "[charset][latin9]") {
    struct Case {
        unsigned char byte;
        const char* utf8;
        const char* name;
    };

    // clang-format off
    static const Case cases[] = {
        {0xA4, "\xE2\x82\xAC", "EURO SIGN"},                         // U+20AC
        {0xA6, "\xC5\xA0",     "LATIN CAPITAL LETTER S WITH CARON"},  // U+0160
        {0xA8, "\xC5\xA1",     "LATIN SMALL LETTER S WITH CARON"},    // U+0161
        {0xB4, "\xC5\xBD",     "LATIN CAPITAL LETTER Z WITH CARON"},  // U+017D
        {0xB8, "\xC5\xBE",     "LATIN SMALL LETTER Z WITH CARON"},    // U+017E
        {0xBC, "\xC5\x92",     "LATIN CAPITAL LIGATURE OE"},          // U+0152
        {0xBD, "\xC5\x93",     "LATIN SMALL LIGATURE OE"},            // U+0153
        {0xBE, "\xC5\xB8",     "LATIN CAPITAL LETTER Y WITH DIAERESIS"}, // U+0178
    };
    // clang-format on

    for (const auto& c : cases) {
        INFO(c.name);
        std::string input(1, static_cast<char>(c.byte));
        std::string utf8 = CharsetConverter::iso885915_to_utf8(input);
        REQUIRE(utf8 == c.utf8);
        REQUIRE(CharsetConverter::is_valid_utf8(utf8));
    }
}

TEST_CASE("Latin-9: all eight substitutions in one string", "[charset][latin9]") {
    std::string input;
    for (unsigned char b : {0xA4, 0xA6, 0xA8, 0xB4, 0xB8, 0xBC, 0xBD, 0xBE}) {
        input.push_back(static_cast<char>(b));
    }
    std::string utf8 = CharsetConverter::iso885915_to_utf8(input);
    REQUIRE(utf8 == "\xE2\x82\xAC"     // EUR
                     "\xC5\xA0"        // S-caron
                     "\xC5\xA1"        // s-caron
                     "\xC5\xBD"        // Z-caron
                     "\xC5\xBE"        // z-caron
                     "\xC5\x92"        // OE
                     "\xC5\x93"        // oe
                     "\xC5\xB8");      // Y-diaeresis
    REQUIRE(CharsetConverter::is_valid_utf8(utf8));
}

// ============================================================================
// Everything else is Latin-1-identical
// ============================================================================

TEST_CASE("Latin-9: ASCII range passes through unchanged", "[charset][latin9]") {
    std::string input = "Hello, World! 123";
    std::string utf8 = CharsetConverter::iso885915_to_utf8(input);
    REQUIRE(utf8 == input);
}

TEST_CASE("Latin-9: non-substituted high bytes match ISO-8859-1 exactly",
          "[charset][latin9]") {
    // 0xE9 = 'e' with acute accent (é) in both Latin-1 and Latin-9.
    // 0xC0 = 'A' with grave accent (À) in both.
    // 0xBF = inverted question mark (¿) in both -- adjacent to the 0xBE
    // substitution but itself untouched.
    // 0xA0 = non-breaking space in both -- adjacent to the 0xA4
    // substitution but itself untouched.
    std::string input;
    input.push_back('\xE9');
    input.push_back('\xC0');
    input.push_back('\xBF');
    input.push_back('\xA0');

    std::string latin1 = CharsetConverter::iso88591_to_utf8(input);
    std::string latin9 = CharsetConverter::iso885915_to_utf8(input);
    REQUIRE(latin9 == latin1);
}

TEST_CASE("Latin-9: C1 control byte range (0x80-0x9F) matches ISO-8859-1",
          "[charset][latin9]") {
    std::string input;
    for (int b = 0x80; b <= 0x9F; ++b) {
        input.push_back(static_cast<char>(b));
    }
    std::string latin1 = CharsetConverter::iso88591_to_utf8(input);
    std::string latin9 = CharsetConverter::iso885915_to_utf8(input);
    REQUIRE(latin9 == latin1);
}

// ============================================================================
// Alias detection
// ============================================================================

TEST_CASE("Latin-9: detect_charset recognizes all documented aliases",
          "[charset][latin9]") {
    REQUIRE(CharsetConverter::detect_charset("iso-8859-15") ==
            CharsetConverter::Charset::ISO885915);
    REQUIRE(CharsetConverter::detect_charset("iso8859-15") ==
            CharsetConverter::Charset::ISO885915);
    REQUIRE(CharsetConverter::detect_charset("latin9") == CharsetConverter::Charset::ISO885915);
    REQUIRE(CharsetConverter::detect_charset("latin-9") == CharsetConverter::Charset::ISO885915);
    REQUIRE(CharsetConverter::detect_charset("iso_8859-15") ==
            CharsetConverter::Charset::ISO885915);
    REQUIRE(CharsetConverter::detect_charset("ISO-8859-15") ==
            CharsetConverter::Charset::ISO885915);
}

TEST_CASE("Latin-9: to_utf8 dispatches through the Charset enum", "[charset][latin9]") {
    std::string input(1, '\xA4'); // EURO SIGN byte
    std::string utf8 = CharsetConverter::to_utf8(input, CharsetConverter::Charset::ISO885915);
    REQUIRE(utf8 == "\xE2\x82\xAC");
}

// ============================================================================
// Pipeline wiring: charset=iso-8859-15 decodes via decoded_body_utf8()
// ============================================================================

TEST_CASE("Pipeline: text/plain part with charset=iso-8859-15 decodes to UTF-8",
          "[mime][pipeline][latin9]") {
    libglot::Arena arena;
    // "10\xA4" -> "10" followed by the EURO SIGN byte
    std::string body = "10\xA4";
    std::string source = "Content-Type: text/plain; charset=iso-8859-15\n"
                          "Content-Transfer-Encoding: 8bit\n"
                          "\n";
    std::string full = source + body;

    auto result = parse_message(arena, full);
    REQUIRE(result.message != nullptr);

    auto decoded = decoded_body_utf8(*result.message);
    REQUIRE(decoded.has_value());
    REQUIRE(*decoded == "10\xE2\x82\xAC");
}

TEST_CASE("Pipeline: text/plain part with charset=latin9 (alias) decodes to UTF-8",
          "[mime][pipeline][latin9]") {
    libglot::Arena arena;
    std::string body = "Pri\xBF"
                        "e"; // "Pri" + inverted-question-mark byte 0xBF + "e"
    std::string source = "Content-Type: text/plain; charset=latin9\n"
                          "Content-Transfer-Encoding: 8bit\n"
                          "\n";
    std::string full = source + body;

    auto result = parse_message(arena, full);
    REQUIRE(result.message != nullptr);

    auto decoded = decoded_body_utf8(*result.message);
    REQUIRE(decoded.has_value());
    REQUIRE(*decoded == "Pri\xC2\xBF"
                         "e");
}
