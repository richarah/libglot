#include <catch2/catch_test_macros.hpp>
#include <libglot/sql/tokenizer.h>
#include <libglot/util/arena.h>
#include <iostream>

using namespace libglot::sql;

TEST_CASE("Tokenizer - PIVOT query tokens", "[tokenizer][pivot]") {
    libglot::Arena arena;
    LocalStringPool pool;
    const char* sql = "SELECT * FROM sales PIVOT (SUM(amount) FOR quarter IN ('Q1', 'Q2'))";

    Tokenizer tokenizer(sql, &pool);
    auto tokens = tokenizer.tokenize_all();

    std::cout << "\nTokenizing: " << sql << std::endl;
    std::cout << "Token count: " << tokens.size() << std::endl;

    for (size_t i = 0; i < tokens.size(); ++i) {
        const auto& tok = tokens[i];
        std::cout << i << ": " << token_type_name(tok.type)
                  << " (" << static_cast<int>(tok.type) << ")";
        if (tok.text) {
            std::cout << " = \"" << tok.text << "\"";
        }
        std::cout << std::endl;
    }

    // Count and find specific tokens
    int pivot_count = 0;
    int for_count = 0;
    for (const auto& tok : tokens) {
        if (tok.type == TokenType::PIVOT) pivot_count++;
        if (tok.type == TokenType::FOR) for_count++;
    }

    std::cout << "\nPIVOT tokens found: " << pivot_count << std::endl;
    std::cout << "FOR tokens found: " << for_count << std::endl;

    REQUIRE(pivot_count == 1);
    REQUIRE(for_count == 1);
}
