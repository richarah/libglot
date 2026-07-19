/// ============================================================================
/// ISO-8859-9 (Latin-5, Turkish), ISO-8859-2 (Latin-2, Central European),
/// and KOI8-R (Cyrillic) -> UTF-8 Tests
/// ============================================================================
///
/// Found missing by the raw SpamAssassin differential residual (roadmap
/// stage 5 follow-up): all three charsets appeared in real messages that
/// libglot reported as undecodable while Python's `email` decoded them
/// fine. ISO-8859-9 is a Latin-1 delta (like Latin-9); ISO-8859-2 and
/// KOI8-R are not, and get their own tables.
/// ============================================================================

#include "../../core/include/libglot/util/arena.h"
#include "../include/libglot/mime/mime.h"
#include <catch2/catch_test_macros.hpp>

using namespace libglot::mime;

// ============================================================================
// ISO-8859-9: the six substitutions vs. Latin-1
// ============================================================================

TEST_CASE("Latin-5 (Turkish): all six substituted code points decode correctly",
          "[charset][latin5]") {
    struct Case {
        unsigned char byte;
        const char* utf8;
        const char* name;
    };

    // clang-format off
    static const Case cases[] = {
        {0xD0, "\xC4\x9E", "LATIN CAPITAL LETTER G WITH BREVE"},     // U+011E
        {0xDD, "\xC4\xB0", "LATIN CAPITAL LETTER I WITH DOT ABOVE"}, // U+0130
        {0xDE, "\xC5\x9E", "LATIN CAPITAL LETTER S WITH CEDILLA"},   // U+015E
        {0xF0, "\xC4\x9F", "LATIN SMALL LETTER G WITH BREVE"},       // U+011F
        {0xFD, "\xC4\xB1", "LATIN SMALL LETTER DOTLESS I"},          // U+0131
        {0xFE, "\xC5\x9F", "LATIN SMALL LETTER S WITH CEDILLA"},     // U+015F
    };
    // clang-format on

    for (const auto& c : cases) {
        INFO(c.name);
        std::string input(1, static_cast<char>(c.byte));
        std::string utf8 = CharsetConverter::iso88599_to_utf8(input);
        REQUIRE(utf8 == c.utf8);
        REQUIRE(CharsetConverter::is_valid_utf8(utf8));
    }
}

TEST_CASE("Latin-5: non-substituted bytes match ISO-8859-1 exactly", "[charset][latin5]") {
    // 0xE9 = e-acute, 0xC0 = A-grave, 0xDF = sharp s: identical in both.
    std::string input;
    input.push_back('\xE9');
    input.push_back('\xC0');
    input.push_back('\xDF');

    std::string latin1 = CharsetConverter::iso88591_to_utf8(input);
    std::string latin5 = CharsetConverter::iso88599_to_utf8(input);
    REQUIRE(latin5 == latin1);
}

TEST_CASE("Latin-5: ASCII range passes through unchanged", "[charset][latin5]") {
    std::string input = "Merhaba, Dunya! 123";
    REQUIRE(CharsetConverter::iso88599_to_utf8(input) == input);
}

TEST_CASE("Latin-5: detect_charset recognizes documented aliases", "[charset][latin5]") {
    REQUIRE(CharsetConverter::detect_charset("iso-8859-9") == CharsetConverter::Charset::ISO88599);
    REQUIRE(CharsetConverter::detect_charset("iso8859-9") == CharsetConverter::Charset::ISO88599);
    REQUIRE(CharsetConverter::detect_charset("latin5") == CharsetConverter::Charset::ISO88599);
    REQUIRE(CharsetConverter::detect_charset("latin-5") == CharsetConverter::Charset::ISO88599);
    REQUIRE(CharsetConverter::detect_charset("ISO-8859-9") == CharsetConverter::Charset::ISO88599);
}

TEST_CASE("Pipeline: text/plain part with charset=iso-8859-9 decodes to UTF-8",
          "[mime][pipeline][latin5]") {
    libglot::Arena arena;
    // Turkish "Ğ" byte between two ASCII letters.
    std::string body = "a\xD0"
                        "b";
    std::string source = "Content-Type: text/plain; charset=iso-8859-9\n"
                          "Content-Transfer-Encoding: 8bit\n"
                          "\n";
    auto result = parse_message(arena, source + body);
    REQUIRE(result.message != nullptr);

    auto decoded = decoded_body_utf8(*result.message);
    REQUIRE(decoded.has_value());
    REQUIRE(*decoded == "a\xC4\x9E"
                         "b");
}

// ============================================================================
// ISO-8859-2: spot-check accented letters, not a Latin-1 delta
// ============================================================================

TEST_CASE("Latin-2 (Central European): sample accented letters decode correctly",
          "[charset][latin2]") {
    struct Case {
        unsigned char byte;
        const char* utf8;
        const char* name;
    };

    // clang-format off
    static const Case cases[] = {
        {0xA1, "\xC4\x84", "LATIN CAPITAL LETTER A WITH OGONEK"},   // U+0104 (Ą, Polish)
        {0xE8, "\xC4\x8D", "LATIN SMALL LETTER C WITH CARON"},      // U+010D (č, Czech)
        {0xF3, "\xC3\xB3", "LATIN SMALL LETTER O WITH ACUTE"},      // U+00F3 (ó, matches Latin-1)
        {0xFA, "\xC3\xBA", "LATIN SMALL LETTER U WITH ACUTE"},      // U+00FA (ú, matches Latin-1)
        {0xFC, "\xC3\xBC", "LATIN SMALL LETTER U WITH DIAERESIS"},  // U+00FC (ü, matches Latin-1)
    };
    // clang-format on

    for (const auto& c : cases) {
        INFO(c.name);
        std::string input(1, static_cast<char>(c.byte));
        std::string utf8 = CharsetConverter::iso88592_to_utf8(input);
        REQUIRE(utf8 == c.utf8);
        REQUIRE(CharsetConverter::is_valid_utf8(utf8));
    }
}

TEST_CASE("Latin-2: ASCII range passes through unchanged", "[charset][latin2]") {
    std::string input = "Dobry den! 123";
    REQUIRE(CharsetConverter::iso88592_to_utf8(input) == input);
}

TEST_CASE("Latin-2: detect_charset recognizes documented aliases", "[charset][latin2]") {
    REQUIRE(CharsetConverter::detect_charset("iso-8859-2") == CharsetConverter::Charset::ISO88592);
    REQUIRE(CharsetConverter::detect_charset("iso8859-2") == CharsetConverter::Charset::ISO88592);
    REQUIRE(CharsetConverter::detect_charset("latin2") == CharsetConverter::Charset::ISO88592);
    REQUIRE(CharsetConverter::detect_charset("latin-2") == CharsetConverter::Charset::ISO88592);
    REQUIRE(CharsetConverter::detect_charset("ISO-8859-2") == CharsetConverter::Charset::ISO88592);
}

TEST_CASE("Latin-2: 'Zażółć' round-trips byte-for-byte", "[charset][latin2]") {
    // "Zażółć" spelled via explicit ISO-8859-2 bytes, avoiding any dependency
    // on the source file's own encoding: Z a ż ó ł ć.
    std::string input;
    for (unsigned char b : {0x5A, 0x61, 0xBF, 0xF3, 0xB3, 0xE6}) {
        input.push_back(static_cast<char>(b));
    }
    std::string utf8 = CharsetConverter::iso88592_to_utf8(input);
    REQUIRE(CharsetConverter::is_valid_utf8(utf8));
    REQUIRE(utf8 == "Za\xC5\xBC\xC3\xB3\xC5\x82\xC4\x87");
}

TEST_CASE("Pipeline: text/plain part with charset=iso-8859-2 decodes to UTF-8",
          "[mime][pipeline][latin2]") {
    libglot::Arena arena;
    // Polish "Ą" byte between two ASCII letters.
    std::string body = "a\xA1"
                        "b";
    std::string source = "Content-Type: text/plain; charset=iso-8859-2\n"
                          "Content-Transfer-Encoding: 8bit\n"
                          "\n";
    auto result = parse_message(arena, source + body);
    REQUIRE(result.message != nullptr);

    auto decoded = decoded_body_utf8(*result.message);
    REQUIRE(decoded.has_value());
    REQUIRE(*decoded == "a\xC4\x84"
                         "b");
}

// ============================================================================
// KOI8-R: spot-check the Cyrillic letters, not a Latin-1 delta
// ============================================================================

TEST_CASE("KOI8-R: sample Cyrillic letters decode correctly", "[charset][koi8r]") {
    struct Case {
        unsigned char byte;
        const char* utf8;
        const char* name;
    };

    // clang-format off
    static const Case cases[] = {
        {0xC1, "\xD0\xB0", "CYRILLIC SMALL LETTER A"},      // U+0430 (а)
        {0xD2, "\xD1\x80", "CYRILLIC SMALL LETTER ER"},     // U+0440 (р)
        {0xE1, "\xD0\x90", "CYRILLIC CAPITAL LETTER A"},    // U+0410 (А)
        {0xF2, "\xD0\xA0", "CYRILLIC CAPITAL LETTER ER"},   // U+0420 (Р)
        {0xA3, "\xD1\x91", "CYRILLIC SMALL LETTER IO"},     // U+0451 (ё)
        {0xB3, "\xD0\x81", "CYRILLIC CAPITAL LETTER IO"},   // U+0401 (Ё)
    };
    // clang-format on

    for (const auto& c : cases) {
        INFO(c.name);
        std::string input(1, static_cast<char>(c.byte));
        std::string utf8 = CharsetConverter::koi8r_to_utf8(input);
        REQUIRE(utf8 == c.utf8);
        REQUIRE(CharsetConverter::is_valid_utf8(utf8));
    }
}

TEST_CASE("KOI8-R: ASCII range passes through unchanged", "[charset][koi8r]") {
    std::string input = "Hello, World! 123";
    REQUIRE(CharsetConverter::koi8r_to_utf8(input) == input);
}

TEST_CASE("KOI8-R: 'Привет' round-trips byte-for-byte", "[charset][koi8r]") {
    // KOI8-R bytes for "Привет" (Hello), spelled out so the test doesn't
    // depend on the source file's own encoding.
    std::string input;
    for (unsigned char b : {0xf0, 0xd2, 0xc9, 0xd7, 0xc5, 0xd4}) {
        input.push_back(static_cast<char>(b));
    }
    std::string utf8 = CharsetConverter::koi8r_to_utf8(input);
    REQUIRE(utf8 == "\xD0\x9F\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82");
    REQUIRE(CharsetConverter::is_valid_utf8(utf8));
}

TEST_CASE("KOI8-R: detect_charset recognizes documented aliases", "[charset][koi8r]") {
    REQUIRE(CharsetConverter::detect_charset("koi8-r") == CharsetConverter::Charset::KOI8R);
    REQUIRE(CharsetConverter::detect_charset("KOI8-R") == CharsetConverter::Charset::KOI8R);
    REQUIRE(CharsetConverter::detect_charset("koi8r") == CharsetConverter::Charset::KOI8R);
    REQUIRE(CharsetConverter::detect_charset("cskoi8r") == CharsetConverter::Charset::KOI8R);
}

// ============================================================================
// RFC 2047 encoded-words: the same charsets must decode in header values
// too (EncodedWordDecoder::decode has its own charset dispatch, separate
// from decoded_body_utf8's -- both need every charset wired in)
// ============================================================================

TEST_CASE("Encoded-Word: RFC 2047 quoted-printable decodes iso-8859-9",
          "[encoding][rfc2047][latin5]") {
    // "=?iso-8859-9?Q?a=D0b?=" -> "a" + Turkish G-with-breve + "b"
    std::string_view encoded = "=?iso-8859-9?Q?a=D0b?=";
    std::string decoded = EncodedWordDecoder::decode(encoded);
    REQUIRE(decoded == "a\xC4\x9E"
                        "b");
}

TEST_CASE("Encoded-Word: RFC 2047 quoted-printable decodes iso-8859-2",
          "[encoding][rfc2047][latin2]") {
    // "=?iso-8859-2?Q?a=A1b?=" -> "a" + Polish A-with-ogonek + "b"
    std::string_view encoded = "=?iso-8859-2?Q?a=A1b?=";
    std::string decoded = EncodedWordDecoder::decode(encoded);
    REQUIRE(decoded == "a\xC4\x84"
                        "b");
}

TEST_CASE("Encoded-Word: RFC 2047 quoted-printable decodes koi8-r",
          "[encoding][rfc2047][koi8r]") {
    // "=?koi8-r?Q?=F2?=" -> KOI8-R 0xF2, CYRILLIC CAPITAL LETTER ER
    std::string_view encoded = "=?koi8-r?Q?=F2?=";
    std::string decoded = EncodedWordDecoder::decode(encoded);
    REQUIRE(decoded == "\xD0\xA0");
}

TEST_CASE("Pipeline: text/plain part with charset=koi8-r decodes to UTF-8",
          "[mime][pipeline][koi8r]") {
    libglot::Arena arena;
    std::string body(1, '\xf2'); // KOI8-R "р" (CYRILLIC SMALL LETTER ER)
    std::string source = "Content-Type: text/plain; charset=koi8-r\n"
                          "Content-Transfer-Encoding: 8bit\n"
                          "\n";
    auto result = parse_message(arena, source + body);
    REQUIRE(result.message != nullptr);

    auto decoded = decoded_body_utf8(*result.message);
    REQUIRE(decoded.has_value());
    REQUIRE(*decoded == "\xD0\xA0");
}
