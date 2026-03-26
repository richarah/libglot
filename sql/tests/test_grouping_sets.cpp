#include <catch2/catch_test_macros.hpp>
#include "libglot/sql/complete_features.h"
#include "libglot/core/arena.h"

using namespace libglot::sql;
using namespace libglot;
using TK = libsqlglot::TokenType;

TEST_CASE("GROUPING SETS - Basic single grouping set", "[sql][grouping_sets]") {
    const char* sql = "SELECT region, product, SUM(sales) FROM orders GROUP BY GROUPING SETS ((region), (product))";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    auto* stmt = parser.parse_grouping_sets();

    REQUIRE(stmt != nullptr);
    REQUIRE(stmt->sets.size() == 2);
    REQUIRE(stmt->sets[0].size() == 1);
    REQUIRE(stmt->sets[1].size() == 1);
}

TEST_CASE("GROUPING SETS - Multiple columns in set", "[sql][grouping_sets]") {
    const char* sql = "GROUP BY GROUPING SETS ((region, product), (region), ())";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    parser.advance(); // Skip GROUP
    parser.advance(); // Skip BY
    parser.advance(); // Skip GROUPING
    parser.advance(); // Skip SETS
    auto* stmt = parser.parse_grouping_sets();

    REQUIRE(stmt != nullptr);
    REQUIRE(stmt->sets.size() == 3);
    REQUIRE(stmt->sets[0].size() == 2); // (region, product)
    REQUIRE(stmt->sets[1].size() == 1); // (region)
    REQUIRE(stmt->sets[2].size() == 0); // () - grand total
}

TEST_CASE("GROUPING SETS - Empty set for grand total", "[sql][grouping_sets]") {
    const char* sql = "GROUP BY GROUPING SETS ((year, month), ())";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    parser.advance(); parser.advance(); parser.advance(); parser.advance();
    auto* stmt = parser.parse_grouping_sets();

    REQUIRE(stmt != nullptr);
    REQUIRE(stmt->sets.size() == 2);
    REQUIRE(stmt->sets[0].size() == 2);
    REQUIRE(stmt->sets[1].size() == 0); // Empty set
}

TEST_CASE("GROUPING SETS - Complex with expressions", "[sql][grouping_sets]") {
    const char* sql = "GROUP BY GROUPING SETS ((YEAR(date), MONTH(date)), (region))";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    parser.advance(); parser.advance(); parser.advance(); parser.advance();
    auto* stmt = parser.parse_grouping_sets();

    REQUIRE(stmt != nullptr);
    REQUIRE(stmt->sets.size() == 2);
    REQUIRE(stmt->sets[0].size() == 2); // Two function calls
}

TEST_CASE("GROUPING SETS - Single set", "[sql][grouping_sets]") {
    const char* sql = "GROUP BY GROUPING SETS ((a, b, c))";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    parser.advance(); parser.advance(); parser.advance(); parser.advance();
    auto* stmt = parser.parse_grouping_sets();

    REQUIRE(stmt != nullptr);
    REQUIRE(stmt->sets.size() == 1);
    REQUIRE(stmt->sets[0].size() == 3);
}

TEST_CASE("GROUPING SETS - Generator output", "[sql][grouping_sets][generator]") {
    const char* sql = "GROUP BY GROUPING SETS ((region, product), (region), ())";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    parser.advance(); parser.advance(); parser.advance(); parser.advance();
    auto* stmt = parser.parse_grouping_sets();

    REQUIRE(stmt != nullptr);

    // Test generator
    std::string output;
    class TestGenerator : public CompleteSQLGenerator<TestGenerator> {
    public:
        using CompleteSQLGenerator::CompleteSQLGenerator;
        std::string generate(GroupingSets* gs) {
            visit_grouping_sets(gs);
            return get_output();
        }
    };

    TestGenerator gen(arena, SQLDialect::PostgreSQL);
    std::string result = gen.generate(stmt);

    REQUIRE(result.find("GROUPING SETS") != std::string::npos);
    REQUIRE(result.find("()") != std::string::npos); // Empty set
}

TEST_CASE("GROUPING SETS - All single column sets", "[sql][grouping_sets]") {
    const char* sql = "GROUP BY GROUPING SETS ((a), (b), (c))";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    parser.advance(); parser.advance(); parser.advance(); parser.advance();
    auto* stmt = parser.parse_grouping_sets();

    REQUIRE(stmt != nullptr);
    REQUIRE(stmt->sets.size() == 3);
    for (const auto& set : stmt->sets) {
        REQUIRE(set.size() == 1);
    }
}

TEST_CASE("GROUPING SETS - Nested in complete query", "[sql][grouping_sets][complete]") {
    const char* sql = R"(
        SELECT region, product, SUM(sales) as total_sales
        FROM orders
        GROUP BY GROUPING SETS ((region, product), (region), ())
        ORDER BY region, product
    )";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    auto* stmt = parser.parse();

    REQUIRE(stmt != nullptr);
    // This tests that GROUPING SETS integrates properly into full SELECT
}

TEST_CASE("GROUPING SETS - Four sets with mixed sizes", "[sql][grouping_sets]") {
    const char* sql = "GROUP BY GROUPING SETS ((a, b, c), (a, b), (a), ())";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    parser.advance(); parser.advance(); parser.advance(); parser.advance();
    auto* stmt = parser.parse_grouping_sets();

    REQUIRE(stmt != nullptr);
    REQUIRE(stmt->sets.size() == 4);
    REQUIRE(stmt->sets[0].size() == 3);
    REQUIRE(stmt->sets[1].size() == 2);
    REQUIRE(stmt->sets[2].size() == 1);
    REQUIRE(stmt->sets[3].size() == 0);
}

TEST_CASE("GROUPING SETS - With CASE expressions", "[sql][grouping_sets]") {
    const char* sql = "GROUP BY GROUPING SETS ((CASE WHEN x > 10 THEN 1 ELSE 0 END), (y))";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    parser.advance(); parser.advance(); parser.advance(); parser.advance();
    auto* stmt = parser.parse_grouping_sets();

    REQUIRE(stmt != nullptr);
    REQUIRE(stmt->sets.size() == 2);
}
