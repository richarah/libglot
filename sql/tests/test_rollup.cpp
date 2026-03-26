#include <catch2/catch_test_macros.hpp>
#include "libglot/sql/complete_features.h"
#include "libglot/core/arena.h"

using namespace libglot::sql;
using namespace libglot;
using TK = libsqlglot::TokenType;

TEST_CASE("ROLLUP - Single column", "[sql][rollup]") {
    const char* sql = "GROUP BY ROLLUP (region)";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    parser.advance(); parser.advance(); parser.advance();
    auto* stmt = parser.parse_rollup();

    REQUIRE(stmt != nullptr);
    REQUIRE(stmt->expressions.size() == 1);
}

TEST_CASE("ROLLUP - Two columns", "[sql][rollup]") {
    const char* sql = "GROUP BY ROLLUP (region, product)";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    parser.advance(); parser.advance(); parser.advance();
    auto* stmt = parser.parse_rollup();

    REQUIRE(stmt != nullptr);
    REQUIRE(stmt->expressions.size() == 2);
}

TEST_CASE("ROLLUP - Three columns hierarchical", "[sql][rollup]") {
    const char* sql = "GROUP BY ROLLUP (year, month, day)";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    parser.advance(); parser.advance(); parser.advance();
    auto* stmt = parser.parse_rollup();

    REQUIRE(stmt != nullptr);
    REQUIRE(stmt->expressions.size() == 3);
}

TEST_CASE("ROLLUP - With function expressions", "[sql][rollup]") {
    const char* sql = "GROUP BY ROLLUP (YEAR(date), MONTH(date))";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    parser.advance(); parser.advance(); parser.advance();
    auto* stmt = parser.parse_rollup();

    REQUIRE(stmt != nullptr);
    REQUIRE(stmt->expressions.size() == 2);
}

TEST_CASE("ROLLUP - Four level hierarchy", "[sql][rollup]") {
    const char* sql = "GROUP BY ROLLUP (country, region, city, store)";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    parser.advance(); parser.advance(); parser.advance();
    auto* stmt = parser.parse_rollup();

    REQUIRE(stmt != nullptr);
    REQUIRE(stmt->expressions.size() == 4);
}

TEST_CASE("ROLLUP - Generator output", "[sql][rollup][generator]") {
    const char* sql = "GROUP BY ROLLUP (region, product)";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    parser.advance(); parser.advance(); parser.advance();
    auto* stmt = parser.parse_rollup();

    REQUIRE(stmt != nullptr);

    class TestGenerator : public CompleteSQLGenerator<TestGenerator> {
    public:
        using CompleteSQLGenerator::CompleteSQLGenerator;
        std::string generate(RollupClause* rollup) {
            visit_rollup(rollup);
            return get_output();
        }
    };

    TestGenerator gen(arena, SQLDialect::PostgreSQL);
    std::string result = gen.generate(stmt);

    REQUIRE(result.find("ROLLUP") != std::string::npos);
    REQUIRE(result.find("(") != std::string::npos);
    REQUIRE(result.find(")") != std::string::npos);
}

TEST_CASE("ROLLUP - In complete SELECT", "[sql][rollup][complete]") {
    const char* sql = R"(
        SELECT region, product, SUM(sales)
        FROM orders
        GROUP BY ROLLUP (region, product)
    )";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    auto* stmt = parser.parse();

    REQUIRE(stmt != nullptr);
}

TEST_CASE("ROLLUP - With HAVING clause", "[sql][rollup]") {
    const char* sql = R"(
        SELECT region, SUM(sales)
        FROM orders
        GROUP BY ROLLUP (region)
        HAVING SUM(sales) > 1000
    )";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    auto* stmt = parser.parse();

    REQUIRE(stmt != nullptr);
}

TEST_CASE("ROLLUP - With ORDER BY", "[sql][rollup]") {
    const char* sql = R"(
        SELECT year, month, SUM(revenue)
        FROM sales
        GROUP BY ROLLUP (year, month)
        ORDER BY year, month
    )";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    auto* stmt = parser.parse();

    REQUIRE(stmt != nullptr);
}

TEST_CASE("ROLLUP - Five columns deep", "[sql][rollup]") {
    const char* sql = "GROUP BY ROLLUP (a, b, c, d, e)";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    parser.advance(); parser.advance(); parser.advance();
    auto* stmt = parser.parse_rollup();

    REQUIRE(stmt != nullptr);
    REQUIRE(stmt->expressions.size() == 5);
}

TEST_CASE("ROLLUP - PostgreSQL dialect", "[sql][rollup][dialect]") {
    const char* sql = R"(
        SELECT category, subcategory, COUNT(*)
        FROM products
        GROUP BY ROLLUP (category, subcategory)
    )";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    auto* stmt = parser.parse();

    REQUIRE(stmt != nullptr);
}

TEST_CASE("ROLLUP - Oracle dialect", "[sql][rollup][dialect]") {
    const char* sql = R"(
        SELECT department_id, job_id, SUM(salary)
        FROM employees
        GROUP BY ROLLUP (department_id, job_id)
    )";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    auto* stmt = parser.parse();

    REQUIRE(stmt != nullptr);
}

TEST_CASE("ROLLUP - SQL Server dialect", "[sql][rollup][dialect]") {
    const char* sql = R"(
        SELECT Year, Quarter, SUM(SalesAmount)
        FROM Sales
        GROUP BY ROLLUP (Year, Quarter)
    )";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    auto* stmt = parser.parse();

    REQUIRE(stmt != nullptr);
}

TEST_CASE("ROLLUP - With CASE expression", "[sql][rollup]") {
    const char* sql = "GROUP BY ROLLUP (CASE WHEN amount > 100 THEN 'High' ELSE 'Low' END, region)";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    parser.advance(); parser.advance(); parser.advance();
    auto* stmt = parser.parse_rollup();

    REQUIRE(stmt != nullptr);
    REQUIRE(stmt->expressions.size() == 2);
}

TEST_CASE("ROLLUP - With EXTRACT function", "[sql][rollup]") {
    const char* sql = "GROUP BY ROLLUP (EXTRACT(YEAR FROM order_date), EXTRACT(MONTH FROM order_date))";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    parser.advance(); parser.advance(); parser.advance();
    auto* stmt = parser.parse_rollup();

    REQUIRE(stmt != nullptr);
    REQUIRE(stmt->expressions.size() == 2);
}
