#include <catch2/catch_test_macros.hpp>
#include <libglot/sql/parser.h>
#include <iostream>

using namespace libglot::sql;
using TK = libsqlglot::TokenType;

TEST_CASE("RETURN keyword lookup", "[keywords][return]") {
    // Test RETURN keyword lookup
    const char* text = "RETURN";

    // Manually calculate hash
    char upper[17];
    size_t len = 6;
    for (size_t i = 0; i < len; ++i) {
        char c = text[i];
        upper[i] = (c >= 'a' && c <= 'z') ? (c - 32) : c;
    }
    upper[len] = '\0';

    // Hash: (first * 31 + last + length) & 127
    uint32_t hash = (upper[0] * 31 + upper[len - 1] + len) & 127;
    std::cout << "RETURN: first=" << (int)upper[0]
              << ", last=" << (int)upper[len-1]
              << ", len=" << len
              << ", hash=" << hash << std::endl;

    // Test lookup
    TK result = libsqlglot::KeywordLookup::lookup("RETURN");
    std::cout << "KeywordLookup::lookup(\"RETURN\") returned: " << static_cast<int>(result) << std::endl;
    std::cout << "TokenType::RETURN_KW = " << static_cast<int>(TK::RETURN_KW) << std::endl;
    std::cout << "TokenType::IDENTIFIER = " << static_cast<int>(TK::IDENTIFIER) << std::endl;

    REQUIRE(result == TK::RETURN_KW);
}

TEST_CASE("RETURN_KW token value", "[keywords][return]") {
    // Verify RETURN_KW has correct value
    REQUIRE(TK::RETURN_KW != TK::IDENTIFIER);
}
