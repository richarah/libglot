// Pathological-but-valid SQL: the parser must either succeed or throw a
// clean libglot::ParseError - it must never crash, hang, or overflow the
// stack. Assertions are exact where the query succeeds.
//
// The parser's recursion guard (ParserBase::kMaxRecursionDepth == 256)
// bounds expression nesting; queries beyond it throw a ParseError that
// mentions the recursion depth.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <libglot/sql/generator.h>
#include <libglot/sql/parser.h>
#include <libglot/util/arena.h>

#include <string>

using namespace libglot::sql;

namespace {

std::string roundtrip(const std::string& sql, SQLDialect dialect = SQLDialect::ANSI) {
    libglot::Arena arena;
    SQLParser parser(arena, sql, dialect);
    auto ast = parser.parse_top_level();
    SQLGenerator gen(dialect);
    return gen.generate(ast);
}

std::string nested_parens_query(size_t depth) {
    std::string sql = "SELECT ";
    sql.append(depth, '(');
    sql += "1";
    sql.append(depth, ')');
    return sql;
}

} // namespace

// ============================================================================
// Deeply nested parentheses
// ============================================================================

TEST_CASE("Mad queries - 50-deep nested parentheses parse", "[mad][nesting]") {
    // Redundant grouping parens around a literal collapse in the output.
    REQUIRE(roundtrip(nested_parens_query(50)) == "SELECT 1");
}

TEST_CASE("Mad queries - 300-deep nested parentheses throw ParseError, not crash",
          "[mad][nesting]") {
    libglot::Arena arena;
    SQLParser parser(arena, nested_parens_query(300));

    REQUIRE_THROWS_MATCHES(parser.parse_top_level(), libglot::ParseError,
                           Catch::Matchers::MessageMatches(Catch::Matchers::ContainsSubstring(
                               "Maximum recursion depth exceeded")));
}

TEST_CASE("Mad queries - 1000-deep nested parentheses also throw cleanly", "[mad][nesting]") {
    libglot::Arena arena;
    SQLParser parser(arena, nested_parens_query(1000));
    REQUIRE_THROWS_AS(parser.parse_top_level(), libglot::ParseError);
}

// ============================================================================
// Very long IN lists
// ============================================================================

TEST_CASE("Mad queries - IN list with 1000 items", "[mad][in-list]") {
    std::string sql = "SELECT * FROM t WHERE id IN (";
    std::string expected = "SELECT * FROM \"t\" WHERE \"id\" IN (";
    for (int i = 1; i <= 1000; ++i) {
        if (i > 1) {
            sql += ", ";
            expected += ", ";
        }
        sql += std::to_string(i);
        expected += std::to_string(i);
    }
    sql += ")";
    expected += ")";

    libglot::Arena arena;
    SQLParser parser(arena, sql);
    auto* ast = parser.parse_top_level();
    auto* stmt = static_cast<SelectStmt*>(ast);
    REQUIRE(stmt->where->type == SQLNodeKind::IN_EXPR);
    REQUIRE(static_cast<InExpr*>(stmt->where)->values.size() == 1000);

    SQLGenerator gen(SQLDialect::ANSI);
    REQUIRE(gen.generate(ast) == expected);
}

// ============================================================================
// Deeply nested subqueries
// ============================================================================

TEST_CASE("Mad queries - nested IN subqueries round-trip", "[mad][subquery]") {
    REQUIRE(roundtrip("SELECT * FROM t WHERE a IN (SELECT b FROM u WHERE c IN "
                      "(SELECT d FROM v WHERE e IN (SELECT f FROM w)))") ==
            "SELECT * FROM \"t\" WHERE \"a\" IN (SELECT \"b\" FROM \"u\" WHERE \"c\" IN "
            "(SELECT \"d\" FROM \"v\" WHERE \"e\" IN (SELECT \"f\" FROM \"w\")))");
}

TEST_CASE("Mad queries - 40 levels of scalar subqueries parse", "[mad][subquery]") {
    std::string sql = "SELECT ";
    for (int i = 0; i < 40; ++i)
        sql += "(SELECT ";
    sql += "1";
    for (int i = 0; i < 40; ++i)
        sql += ")";

    libglot::Arena arena;
    SQLParser parser(arena, sql);
    SQLNode* ast = nullptr;
    REQUIRE_NOTHROW(ast = parser.parse_top_level());
    REQUIRE(ast != nullptr);
    REQUIRE(ast->type == SQLNodeKind::SELECT_STMT);
}

TEST_CASE("Mad queries - subquery nesting beyond the guard throws cleanly", "[mad][subquery]") {
    std::string sql = "SELECT ";
    for (int i = 0; i < 400; ++i)
        sql += "(SELECT ";
    sql += "1";
    for (int i = 0; i < 400; ++i)
        sql += ")";

    libglot::Arena arena;
    SQLParser parser(arena, sql);
    REQUIRE_THROWS_AS(parser.parse_top_level(), libglot::ParseError);
}

// ============================================================================
// Absurd identifier lengths
// ============================================================================

TEST_CASE("Mad queries - 5000-character identifier survives round-trip", "[mad][identifier]") {
    const std::string long_name(5000, 'a');
    const std::string sql = "SELECT " + long_name + " FROM t";
    const std::string expected = "SELECT \"" + long_name + "\" FROM \"t\"";

    REQUIRE(roundtrip(sql) == expected);
}

// ============================================================================
// Set-operation chains
// ============================================================================

TEST_CASE("Mad queries - 5-way mixed set-op chain round-trips exactly", "[mad][setops]") {
    REQUIRE(roundtrip(
                "SELECT 1 UNION SELECT 2 UNION ALL SELECT 3 INTERSECT SELECT 4 EXCEPT SELECT 5") ==
            "SELECT 1 UNION SELECT 2 UNION ALL SELECT 3 INTERSECT SELECT 4 EXCEPT SELECT 5");
}

TEST_CASE("Mad queries - 100-way UNION ALL chain parses without recursion failure",
          "[mad][setops]") {
    std::string sql = "SELECT 1";
    for (int i = 0; i < 100; ++i)
        sql += " UNION ALL SELECT 1";

    libglot::Arena arena;
    SQLParser parser(arena, sql);
    SQLNode* ast = nullptr;
    REQUIRE_NOTHROW(ast = parser.parse_top_level());
    REQUIRE(ast != nullptr);

    SQLGenerator gen(SQLDialect::ANSI);
    REQUIRE(gen.generate(ast) == sql);
}

// ============================================================================
// Minimal / degenerate inputs
// ============================================================================

TEST_CASE("Mad queries - SELECT 1 is the identity", "[mad][minimal]") {
    REQUIRE(roundtrip("SELECT 1") == "SELECT 1");
}

TEST_CASE("Mad queries - empty and whitespace-only input throw ParseError", "[mad][minimal]") {
    {
        libglot::Arena arena;
        SQLParser parser(arena, "");
        REQUIRE_THROWS_AS(parser.parse_top_level(), libglot::ParseError);
    }
    {
        libglot::Arena arena;
        SQLParser parser(arena, "   \t\n  ");
        REQUIRE_THROWS_AS(parser.parse_top_level(), libglot::ParseError);
    }
}

TEST_CASE("Mad queries - comment-only input throws ParseError", "[mad][minimal]") {
    {
        libglot::Arena arena;
        SQLParser parser(arena, "-- just a comment");
        REQUIRE_THROWS_MATCHES(parser.parse_top_level(), libglot::ParseError,
                               Catch::Matchers::MessageMatches(
                                   Catch::Matchers::ContainsSubstring("Expected SQL statement")));
    }
    {
        libglot::Arena arena;
        SQLParser parser(arena, "/* block comment only */");
        REQUIRE_THROWS_AS(parser.parse_top_level(), libglot::ParseError);
    }
}

TEST_CASE("Mad queries - huge whitespace padding is harmless", "[mad][minimal]") {
    const std::string padding(10000, ' ');
    REQUIRE(roundtrip(padding + "SELECT 1" + padding) == "SELECT 1");
}

// ============================================================================
// Wide rather than deep
// ============================================================================

TEST_CASE("Mad queries - 500-column select list", "[mad][wide]") {
    std::string sql = "SELECT ";
    std::string expected = "SELECT ";
    for (int i = 1; i <= 500; ++i) {
        if (i > 1) {
            sql += ", ";
            expected += ", ";
        }
        sql += std::to_string(i);
        expected += std::to_string(i);
    }

    libglot::Arena arena;
    SQLParser parser(arena, sql);
    auto* ast = parser.parse_top_level();
    REQUIRE(static_cast<SelectStmt*>(ast)->columns.size() == 500);

    SQLGenerator gen(SQLDialect::ANSI);
    REQUIRE(gen.generate(ast) == expected);
}

TEST_CASE("Mad queries - long flat AND chain does not exhaust recursion", "[mad][wide]") {
    // Left-associative binary chains grow the AST, not the recursion depth.
    std::string sql = "SELECT * FROM t WHERE 1 = 1";
    for (int i = 0; i < 200; ++i)
        sql += " AND 1 = 1";

    libglot::Arena arena;
    SQLParser parser(arena, sql);
    SQLNode* ast = nullptr;
    REQUIRE_NOTHROW(ast = parser.parse_top_level());
    REQUIRE(ast != nullptr);
}
