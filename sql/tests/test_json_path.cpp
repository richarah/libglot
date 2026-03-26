#include <catch2/catch_test_macros.hpp>
#include "libglot/sql/complete_features.h"
#include "libglot/core/arena.h"

using namespace libglot::sql;
using namespace libglot;
using TK = libsqlglot::TokenType;

TEST_CASE("JSON_PATH - Basic path expression", "[sql][json_path]") {
    const char* sql = "JSON_QUERY(data, '$.name')";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    parser.advance(); // Skip JSON_QUERY
    parser.advance(); // Skip (
    auto* stmt = parser.parse_json_path();

    REQUIRE(stmt != nullptr);
    REQUIRE(stmt->json_expr != nullptr);
    REQUIRE(stmt->lax == true); // Default is LAX
}

TEST_CASE("JSON_PATH - Array index access", "[sql][json_path]") {
    const char* sql = "JSON_QUERY(data, '$.items[0]')";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    parser.advance(); parser.advance();
    auto* stmt = parser.parse_json_path();

    REQUIRE(stmt != nullptr);
}

TEST_CASE("JSON_PATH - Nested property access", "[sql][json_path]") {
    const char* sql = "JSON_QUERY(data, '$.store.book[0].title')";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    parser.advance(); parser.advance();
    auto* stmt = parser.parse_json_path();

    REQUIRE(stmt != nullptr);
    REQUIRE(stmt->path == "'$.store.book[0].title'");
}

TEST_CASE("JSON_PATH - STRICT mode", "[sql][json_path]") {
    const char* sql = "JSON_QUERY(data, '$.price' STRICT)";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    parser.advance(); parser.advance();
    auto* stmt = parser.parse_json_path();

    REQUIRE(stmt != nullptr);
    REQUIRE(stmt->lax == false);
}

TEST_CASE("JSON_PATH - LAX mode explicit", "[sql][json_path]") {
    const char* sql = "JSON_QUERY(data, '$.total' LAX)";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    parser.advance(); parser.advance();
    auto* stmt = parser.parse_json_path();

    REQUIRE(stmt != nullptr);
    REQUIRE(stmt->lax == true);
}

TEST_CASE("JSON_PATH - Generator output", "[sql][json_path][generator]") {
    const char* sql = "JSON_QUERY(data, '$.name' STRICT)";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    parser.advance(); parser.advance();
    auto* stmt = parser.parse_json_path();

    REQUIRE(stmt != nullptr);

    class TestGenerator : public CompleteSQLGenerator<TestGenerator> {
    public:
        using CompleteSQLGenerator::CompleteSQLGenerator;
        std::string generate(JsonPathExpr* jp) {
            visit_json_path(jp);
            return get_output();
        }
    };

    TestGenerator gen(arena, SQLDialect::TSQL);
    std::string result = gen.generate(stmt);

    REQUIRE(result.find("JSON_QUERY") != std::string::npos);
    REQUIRE(result.find("STRICT") != std::string::npos);
}

TEST_CASE("JSON_PATH - In SELECT statement", "[sql][json_path][complete]") {
    const char* sql = R"(
        SELECT
            id,
            JSON_QUERY(metadata, '$.author') AS author,
            JSON_QUERY(metadata, '$.tags[0]') AS first_tag
        FROM documents
    )";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    auto* stmt = parser.parse();

    REQUIRE(stmt != nullptr);
}

TEST_CASE("JSON_PATH - JSON_VALUE function", "[sql][json_path]") {
    const char* sql = "JSON_VALUE(data, '$.price')";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    parser.advance(); parser.advance();
    auto* stmt = parser.parse_json_path();

    REQUIRE(stmt != nullptr);
}

TEST_CASE("JSON_PATH - Wildcard in path", "[sql][json_path]") {
    const char* sql = "JSON_QUERY(data, '$.items[*].name')";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    parser.advance(); parser.advance();
    auto* stmt = parser.parse_json_path();

    REQUIRE(stmt != nullptr);
}

TEST_CASE("JSON_PATH - Recursive descent", "[sql][json_path]") {
    const char* sql = "JSON_QUERY(data, '$..price')";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    parser.advance(); parser.advance();
    auto* stmt = parser.parse_json_path();

    REQUIRE(stmt != nullptr);
}

TEST_CASE("JSON_PATH - Filter expression", "[sql][json_path]") {
    const char* sql = "JSON_QUERY(data, '$.books[?(@.price < 10)]')";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    parser.advance(); parser.advance();
    auto* stmt = parser.parse_json_path();

    REQUIRE(stmt != nullptr);
}

TEST_CASE("JSON_PATH - Multiple array indices", "[sql][json_path]") {
    const char* sql = "JSON_QUERY(data, '$.matrix[0][1]')";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    parser.advance(); parser.advance();
    auto* stmt = parser.parse_json_path();

    REQUIRE(stmt != nullptr);
}

TEST_CASE("JSON_PATH - Property with special characters", "[sql][json_path]") {
    const char* sql = "JSON_QUERY(data, '$.\"property-name\"')";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    parser.advance(); parser.advance();
    auto* stmt = parser.parse_json_path();

    REQUIRE(stmt != nullptr);
}

TEST_CASE("JSON_PATH - In WHERE clause", "[sql][json_path][complete]") {
    const char* sql = R"(
        SELECT *
        FROM products
        WHERE JSON_VALUE(attributes, '$.color') = 'red'
    )";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    auto* stmt = parser.parse();

    REQUIRE(stmt != nullptr);
}

TEST_CASE("JSON_PATH - PostgreSQL dialect", "[sql][json_path][dialect]") {
    const char* sql = R"(
        SELECT data->'store'->'book'->0->'title' AS title
        FROM documents
    )";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    auto* stmt = parser.parse();

    REQUIRE(stmt != nullptr);
}

TEST_CASE("JSON_PATH - SQL Server dialect", "[sql][json_path][dialect]") {
    const char* sql = R"(
        SELECT
            JSON_VALUE(Info, '$.Customer.Name') AS CustomerName,
            JSON_QUERY(Info, '$.Customer.Address') AS CustomerAddress
        FROM Orders
    )";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    auto* stmt = parser.parse();

    REQUIRE(stmt != nullptr);
}

TEST_CASE("JSON_PATH - MySQL dialect", "[sql][json_path][dialect]") {
    const char* sql = R"(
        SELECT
            id,
            JSON_EXTRACT(data, '$.name') AS name,
            JSON_UNQUOTE(JSON_EXTRACT(data, '$.email')) AS email
        FROM users
    )";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    auto* stmt = parser.parse();

    REQUIRE(stmt != nullptr);
}

TEST_CASE("JSON_PATH - Array slice", "[sql][json_path]") {
    const char* sql = "JSON_QUERY(data, '$.items[0:3]')";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    parser.advance(); parser.advance();
    auto* stmt = parser.parse_json_path();

    REQUIRE(stmt != nullptr);
}

TEST_CASE("JSON_PATH - Root element", "[sql][json_path]") {
    const char* sql = "JSON_QUERY(data, '$')";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    parser.advance(); parser.advance();
    auto* stmt = parser.parse_json_path();

    REQUIRE(stmt != nullptr);
}

TEST_CASE("JSON_PATH - Last array element", "[sql][json_path]") {
    const char* sql = "JSON_QUERY(data, '$.items[-1]')";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    parser.advance(); parser.advance();
    auto* stmt = parser.parse_json_path();

    REQUIRE(stmt != nullptr);
}
