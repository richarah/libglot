#include <catch2/catch_test_macros.hpp>
#include "libglot/sql/complete_features.h"
#include "libglot/core/arena.h"

using namespace libglot::sql;
using namespace libglot;
using TK = libsqlglot::TokenType;

TEST_CASE("CUBE - Single column", "[sql][cube]") {
    const char* sql = "GROUP BY CUBE (region)";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    parser.advance(); parser.advance(); parser.advance();
    auto* stmt = parser.parse_cube();

    REQUIRE(stmt != nullptr);
    REQUIRE(stmt->expressions.size() == 1);
}

TEST_CASE("CUBE - Two columns all combinations", "[sql][cube]") {
    const char* sql = "GROUP BY CUBE (region, product)";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    parser.advance(); parser.advance(); parser.advance();
    auto* stmt = parser.parse_cube();

    REQUIRE(stmt != nullptr);
    REQUIRE(stmt->expressions.size() == 2);
    // CUBE(a,b) generates: (a,b), (a), (b), ()
}

TEST_CASE("CUBE - Three columns", "[sql][cube]") {
    const char* sql = "GROUP BY CUBE (year, month, day)";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    parser.advance(); parser.advance(); parser.advance();
    auto* stmt = parser.parse_cube();

    REQUIRE(stmt != nullptr);
    REQUIRE(stmt->expressions.size() == 3);
    // CUBE(a,b,c) generates 8 combinations: 2^3
}

TEST_CASE("CUBE - With function expressions", "[sql][cube]") {
    const char* sql = "GROUP BY CUBE (YEAR(date), QUARTER(date))";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    parser.advance(); parser.advance(); parser.advance();
    auto* stmt = parser.parse_cube();

    REQUIRE(stmt != nullptr);
    REQUIRE(stmt->expressions.size() == 2);
}

TEST_CASE("CUBE - Four columns", "[sql][cube]") {
    const char* sql = "GROUP BY CUBE (country, region, city, store)";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    parser.advance(); parser.advance(); parser.advance();
    auto* stmt = parser.parse_cube();

    REQUIRE(stmt != nullptr);
    REQUIRE(stmt->expressions.size() == 4);
    // 2^4 = 16 combinations
}

TEST_CASE("CUBE - Generator output", "[sql][cube][generator]") {
    const char* sql = "GROUP BY CUBE (region, product)";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    parser.advance(); parser.advance(); parser.advance();
    auto* stmt = parser.parse_cube();

    REQUIRE(stmt != nullptr);

    class TestGenerator : public CompleteSQLGenerator<TestGenerator> {
    public:
        using CompleteSQLGenerator::CompleteSQLGenerator;
        std::string generate(CubeClause* cube) {
            visit_cube(cube);
            return get_output();
        }
    };

    TestGenerator gen(arena, SQLDialect::PostgreSQL);
    std::string result = gen.generate(stmt);

    REQUIRE(result.find("CUBE") != std::string::npos);
    REQUIRE(result.find("(") != std::string::npos);
    REQUIRE(result.find(")") != std::string::npos);
}

TEST_CASE("CUBE - In complete SELECT", "[sql][cube][complete]") {
    const char* sql = R"(
        SELECT region, product, SUM(sales)
        FROM orders
        GROUP BY CUBE (region, product)
    )";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    auto* stmt = parser.parse();

    REQUIRE(stmt != nullptr);
}

TEST_CASE("CUBE - With HAVING clause", "[sql][cube]") {
    const char* sql = R"(
        SELECT region, product, SUM(sales)
        FROM orders
        GROUP BY CUBE (region, product)
        HAVING SUM(sales) > 5000
    )";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    auto* stmt = parser.parse();

    REQUIRE(stmt != nullptr);
}

TEST_CASE("CUBE - With ORDER BY", "[sql][cube]") {
    const char* sql = R"(
        SELECT year, quarter, SUM(revenue)
        FROM sales
        GROUP BY CUBE (year, quarter)
        ORDER BY year NULLS LAST, quarter NULLS LAST
    )";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    auto* stmt = parser.parse();

    REQUIRE(stmt != nullptr);
}

TEST_CASE("CUBE - Five dimensions", "[sql][cube]") {
    const char* sql = "GROUP BY CUBE (a, b, c, d, e)";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    parser.advance(); parser.advance(); parser.advance();
    auto* stmt = parser.parse_cube();

    REQUIRE(stmt != nullptr);
    REQUIRE(stmt->expressions.size() == 5);
    // 2^5 = 32 combinations
}

TEST_CASE("CUBE - PostgreSQL dialect", "[sql][cube][dialect]") {
    const char* sql = R"(
        SELECT category, brand, COUNT(*)
        FROM products
        GROUP BY CUBE (category, brand)
    )";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    auto* stmt = parser.parse();

    REQUIRE(stmt != nullptr);
}

TEST_CASE("CUBE - Oracle dialect", "[sql][cube][dialect]") {
    const char* sql = R"(
        SELECT department_id, job_id, manager_id, SUM(salary)
        FROM employees
        GROUP BY CUBE (department_id, job_id, manager_id)
    )";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    auto* stmt = parser.parse();

    REQUIRE(stmt != nullptr);
}

TEST_CASE("CUBE - SQL Server dialect", "[sql][cube][dialect]") {
    const char* sql = R"(
        SELECT Region, Product, Quarter, SUM(Sales)
        FROM SalesData
        GROUP BY CUBE (Region, Product, Quarter)
    )";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    auto* stmt = parser.parse();

    REQUIRE(stmt != nullptr);
}

TEST_CASE("CUBE - With aggregate functions", "[sql][cube]") {
    const char* sql = R"(
        SELECT region, product,
               SUM(quantity) as total_qty,
               AVG(price) as avg_price,
               COUNT(*) as count
        FROM orders
        GROUP BY CUBE (region, product)
    )";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    auto* stmt = parser.parse();

    REQUIRE(stmt != nullptr);
}

TEST_CASE("CUBE - With CASE expression", "[sql][cube]") {
    const char* sql = "GROUP BY CUBE (CASE WHEN status = 'active' THEN 1 ELSE 0 END, category)";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    parser.advance(); parser.advance(); parser.advance();
    auto* stmt = parser.parse_cube();

    REQUIRE(stmt != nullptr);
    REQUIRE(stmt->expressions.size() == 2);
}

TEST_CASE("CUBE - Nested with ROLLUP compatibility", "[sql][cube]") {
    // CUBE can coexist with other GROUP BY extensions in some dialects
    const char* sql = R"(
        SELECT year, quarter, month, SUM(sales)
        FROM revenue
        GROUP BY year, CUBE (quarter, month)
    )";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    auto* stmt = parser.parse();

    REQUIRE(stmt != nullptr);
}
