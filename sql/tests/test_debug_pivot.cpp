#include <catch2/catch_test_macros.hpp>
#include <libglot/sql/parser.h>
#include <libglot/sql/tokens.h>
#include <iostream>

using namespace libglot::sql;

TEST_CASE("Debug - PIVOT hash calculation", "[debug]") {
    const char* text = "PIVOT";
    char upper[17];
    size_t len = 5;

    // Uppercase conversion
    for (size_t i = 0; i < len; ++i) {
        char c = text[i];
        upper[i] = (c >= 'a' && c <= 'z') ? (c - 32) : c;
    }
    upper[len] = '\0';

    // Hash calculation
    uint32_t hash = (upper[0] * 31 + upper[len - 1] + len) & 127;

    std::cout << "Text: " << text << std::endl;
    std::cout << "Upper: " << upper << std::endl;
    std::cout << "First char: " << upper[0] << " (" << (int)upper[0] << ")" << std::endl;
    std::cout << "Last char: " << upper[len-1] << " (" << (int)upper[len-1] << ")" << std::endl;
    std::cout << "Length: " << len << std::endl;
    std::cout << "Hash: " << hash << std::endl;

    // Now test KeywordLookup::lookup
    TokenType result = KeywordLookup::lookup("PIVOT");
    std::cout << "KeywordLookup::lookup(\"PIVOT\") = " << (int)result << std::endl;
    std::cout << "Expected: " << (int)TokenType::PIVOT << std::endl;

    REQUIRE(hash == 9);
    REQUIRE(result == TokenType::PIVOT);
}

TEST_CASE("Debug - UNPIVOT hash calculation", "[debug]") {
    const char* text = "UNPIVOT";
    char upper[17];
    size_t len = 7;

    // Uppercase conversion
    for (size_t i = 0; i < len; ++i) {
        char c = text[i];
        upper[i] = (c >= 'a' && c <= 'z') ? (c - 32) : c;
    }
    upper[len] = '\0';

    // Hash calculation
    uint32_t hash = (upper[0] * 31 + upper[len - 1] + len) & 127;

    std::cout << "Text: " << text << std::endl;
    std::cout << "Upper: " << upper << std::endl;
    std::cout << "First char: " << upper[0] << " (" << (int)upper[0] << ")" << std::endl;
    std::cout << "Last char: " << upper[len-1] << " (" << (int)upper[len-1] << ")" << std::endl;
    std::cout << "Length: " << len << std::endl;
    std::cout << "Hash: " << hash << std::endl;

    // Now test KeywordLookup::lookup
    TokenType result = KeywordLookup::lookup("UNPIVOT");
    std::cout << "KeywordLookup::lookup(\"UNPIVOT\") = " << (int)result << std::endl;
    std::cout << "Expected: " << (int)TokenType::UNPIVOT << std::endl;

    REQUIRE(hash == 38);
    REQUIRE(result == TokenType::UNPIVOT);
}
