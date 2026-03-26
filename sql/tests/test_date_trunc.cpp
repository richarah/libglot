#include <catch2/catch_test_macros.hpp>
#include <libglot/sql/parser.h>
#include <libglot/sql/generator.h>
#include <libglot/util/arena.h>

using namespace libglot::sql;

TEST_CASE("DATE_TRUNC: Basic date truncation to day", "[date_trunc]") {
    libglot::Arena arena;
    std::string_view sql = "SELECT DATE_TRUNC('day', created_at) FROM events";

    SQLParser parser(arena, sql);
    auto* node = parser.parse_select();

    REQUIRE(node != nullptr);
    REQUIRE(node->type == SQLNodeKind::SELECT_STMT);

    auto* stmt = static_cast<SelectStmt*>(node);
    REQUIRE(stmt->columns.size() == 1);
    REQUIRE(stmt->columns[0]->type == SQLNodeKind::FUNCTION_CALL);

    auto* func = static_cast<FunctionCall*>(stmt->columns[0]);
    REQUIRE(func->name == "DATE_TRUNC");
    REQUIRE(func->args.size() == 2);
}

TEST_CASE("DATE_TRUNC: Truncate to month", "[date_trunc]") {
    libglot::Arena arena;
    std::string_view sql = "SELECT DATE_TRUNC('month', order_date) FROM orders";

    SQLParser parser(arena, sql);
    auto* node = parser.parse_select();

    REQUIRE(node != nullptr);
    auto* stmt = static_cast<SelectStmt*>(node);
    REQUIRE(stmt->columns.size() == 1);

    SQLGenerator gen(SQLDialect::PostgreSQL);
    std::string output = gen.generate(node);
    REQUIRE(output.find("DATE_TRUNC") != std::string::npos);
    REQUIRE(output.find("month") != std::string::npos);
}

TEST_CASE("DATE_TRUNC: Truncate to year with WHERE", "[date_trunc]") {
    libglot::Arena arena;
    std::string_view sql = "SELECT id, DATE_TRUNC('year', created_at) as year_created FROM users WHERE active = TRUE";

    SQLParser parser(arena, sql);
    auto* node = parser.parse_select();

    REQUIRE(node != nullptr);
    auto* stmt = static_cast<SelectStmt*>(node);
    REQUIRE(stmt->columns.size() == 2);
    REQUIRE(stmt->where != nullptr);
}

TEST_CASE("DATE_TRUNC: Multiple truncations", "[date_trunc]") {
    libglot::Arena arena;
    std::string_view sql = "SELECT DATE_TRUNC('hour', start_time), DATE_TRUNC('day', end_time) FROM sessions";

    SQLParser parser(arena, sql);
    auto* node = parser.parse_select();

    REQUIRE(node != nullptr);
    auto* stmt = static_cast<SelectStmt*>(node);
    REQUIRE(stmt->columns.size() == 2);
    REQUIRE(stmt->columns[0]->type == SQLNodeKind::FUNCTION_CALL);
    REQUIRE(stmt->columns[1]->type == SQLNodeKind::FUNCTION_CALL);
}

TEST_CASE("DATE_TRUNC: With GROUP BY", "[date_trunc]") {
    libglot::Arena arena;
    std::string_view sql = "SELECT DATE_TRUNC('week', created_at) as week, COUNT(*) FROM orders GROUP BY week";

    SQLParser parser(arena, sql);
    auto* node = parser.parse_select();

    REQUIRE(node != nullptr);
    auto* stmt = static_cast<SelectStmt*>(node);
    REQUIRE(stmt->columns.size() == 2);
    REQUIRE(stmt->group_by.size() == 1);
}

TEST_CASE("DATE_TRUNC: Roundtrip test", "[date_trunc]") {
    libglot::Arena arena;
    std::string_view original = "SELECT DATE_TRUNC('quarter', sales_date) FROM sales";

    SQLParser parser(arena, original);
    auto* node = parser.parse_select();

    SQLGenerator gen(SQLDialect::PostgreSQL);
    std::string output = gen.generate(node);

    REQUIRE(output.find("DATE_TRUNC") != std::string::npos);
    REQUIRE(output.find("quarter") != std::string::npos);
    REQUIRE(output.find("sales_date") != std::string::npos);
}
