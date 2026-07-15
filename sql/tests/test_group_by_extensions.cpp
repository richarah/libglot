// GROUP BY extensions (SQL:1999 T431): GROUPING SETS, ROLLUP, CUBE,
// nested combinations, the empty grouping set, and plain mixed lists.
// Exact-string assertions pin the canonical generated form; every set in
// GROUPING SETS is emitted parenthesized except a nested ROLLUP/CUBE/
// GROUPING SETS element, which stays bare.

#include <catch2/catch_test_macros.hpp>
#include <libglot/sql/generator.h>
#include <libglot/sql/parser.h>
#include <libglot/util/arena.h>

#include <string>

using namespace libglot::sql;

namespace {

std::string transpile(const std::string& sql, SQLDialect d = SQLDialect::PostgreSQL) {
    libglot::Arena arena;
    SQLParser parser(arena, sql, d);
    auto ast = parser.parse_top_level();
    SQLGenerator gen(d);
    return gen.generate(ast);
}

} // namespace

// ============================================================================
// ROLLUP
// ============================================================================

TEST_CASE("GROUP BY ROLLUP - basic", "[group-by][rollup]") {
    REQUIRE(transpile("SELECT a, b, SUM(c) FROM t GROUP BY ROLLUP(a, b, c)") ==
            "SELECT \"a\", \"b\", SUM(\"c\") FROM \"t\" GROUP BY ROLLUP(\"a\", \"b\", \"c\")");
    // Single column
    REQUIRE(transpile("SELECT a FROM t GROUP BY ROLLUP(a)") ==
            "SELECT \"a\" FROM \"t\" GROUP BY ROLLUP(\"a\")");
    // Generated output is an exact fixed point
    const std::string once = transpile("SELECT a FROM t GROUP BY ROLLUP(a, b)");
    REQUIRE(transpile(once) == once);
}

TEST_CASE("GROUP BY ROLLUP - expressions inside", "[group-by][rollup]") {
    REQUIRE(transpile("SELECT 1 FROM t GROUP BY ROLLUP(a + b, c)") ==
            "SELECT 1 FROM \"t\" GROUP BY ROLLUP(\"a\" + \"b\", \"c\")");
}

TEST_CASE("GROUP BY ROLLUP - MySQL quoting", "[group-by][rollup][mysql]") {
    REQUIRE(transpile("SELECT a FROM t GROUP BY ROLLUP(a, b)", SQLDialect::MySQL) ==
            "SELECT `a` FROM `t` GROUP BY ROLLUP(`a`, `b`)");
}

// ============================================================================
// CUBE
// ============================================================================

TEST_CASE("GROUP BY CUBE - basic", "[group-by][cube]") {
    REQUIRE(transpile("SELECT a, b, COUNT(*) FROM t GROUP BY CUBE(a, b)") ==
            "SELECT \"a\", \"b\", COUNT(*) FROM \"t\" GROUP BY CUBE(\"a\", \"b\")");
    const std::string once = transpile("SELECT a FROM t GROUP BY CUBE(a, b)");
    REQUIRE(transpile(once) == once);
}

// ============================================================================
// GROUPING SETS
// ============================================================================

TEST_CASE("GROUP BY GROUPING SETS - basic with empty set", "[group-by][grouping-sets]") {
    REQUIRE(transpile("SELECT a, b FROM t GROUP BY GROUPING SETS ((a, b), (a), ())") ==
            "SELECT \"a\", \"b\" FROM \"t\" GROUP BY GROUPING SETS ((\"a\", \"b\"), (\"a\"), ())");
}

TEST_CASE("GROUP BY GROUPING SETS - bare single item is canonicalized",
          "[group-by][grouping-sets]") {
    // A bare column element is normalized to its parenthesized form
    REQUIRE(transpile("SELECT a FROM t GROUP BY GROUPING SETS (a, (b, c))") ==
            "SELECT \"a\" FROM \"t\" GROUP BY GROUPING SETS ((\"a\"), (\"b\", \"c\"))");
}

TEST_CASE("GROUP BY GROUPING SETS - nested ROLLUP and CUBE", "[group-by][grouping-sets]") {
    REQUIRE(transpile("SELECT a FROM t GROUP BY GROUPING SETS (ROLLUP(a, b), (c), ())") ==
            "SELECT \"a\" FROM \"t\" GROUP BY GROUPING SETS (ROLLUP(\"a\", \"b\"), (\"c\"), ())");
    REQUIRE(transpile("SELECT a FROM t GROUP BY GROUPING SETS (CUBE(a), (b))") ==
            "SELECT \"a\" FROM \"t\" GROUP BY GROUPING SETS (CUBE(\"a\"), (\"b\"))");
    const std::string once =
        transpile("SELECT a FROM t GROUP BY GROUPING SETS (ROLLUP(a, b), (c), ())");
    REQUIRE(transpile(once) == once);
}

// ============================================================================
// Mixed plain / extension lists
// ============================================================================

TEST_CASE("GROUP BY - plain items mixed with ROLLUP/CUBE", "[group-by][mixed]") {
    REQUIRE(transpile("SELECT a, b, c FROM t GROUP BY a, ROLLUP(b, c)") ==
            "SELECT \"a\", \"b\", \"c\" FROM \"t\" GROUP BY \"a\", ROLLUP(\"b\", \"c\")");
    REQUIRE(transpile("SELECT a FROM t GROUP BY CUBE(a), b, GROUPING SETS ((c), ())") ==
            "SELECT \"a\" FROM \"t\" GROUP BY CUBE(\"a\"), \"b\", GROUPING SETS ((\"c\"), ())");
}

TEST_CASE("GROUP BY - full clause tail still parses after extensions", "[group-by][mixed]") {
    REQUIRE(
        transpile(
            "SELECT a, SUM(b) FROM t GROUP BY ROLLUP(a) HAVING SUM(b) > 1 ORDER BY a LIMIT 5") ==
        "SELECT \"a\", SUM(\"b\") FROM \"t\" GROUP BY ROLLUP(\"a\") "
        "HAVING SUM(\"b\") > 1 ORDER BY \"a\" LIMIT 5");
}

// ============================================================================
// GROUPING(col) stays an ordinary function call
// ============================================================================

TEST_CASE("GROUPING(col) parses as a normal function call", "[group-by][grouping-fn]") {
    REQUIRE(transpile("SELECT GROUPING(a), SUM(b) FROM t GROUP BY ROLLUP(a)") ==
            "SELECT GROUPING(\"a\"), SUM(\"b\") FROM \"t\" GROUP BY ROLLUP(\"a\")");
    // GROUPING with multiple args (SQL Server style)
    REQUIRE(transpile("SELECT GROUPING(a, b) FROM t GROUP BY CUBE(a, b)") ==
            "SELECT GROUPING(\"a\", \"b\") FROM \"t\" GROUP BY CUBE(\"a\", \"b\")");
    // Plain identifiers named ROLLUP/CUBE without parens are still columns
    REQUIRE(transpile("SELECT a FROM t GROUP BY cube") ==
            "SELECT \"a\" FROM \"t\" GROUP BY \"cube\"");
}

// ============================================================================
// Strictness: trailing input after grouping extensions is still an error
// ============================================================================

TEST_CASE("GROUP BY extensions do not relax trailing-input checking", "[group-by][strict]") {
    libglot::Arena arena;
    SQLParser parser(arena, "SELECT a FROM t GROUP BY ROLLUP(a) bogus", SQLDialect::PostgreSQL);
    REQUIRE_THROWS_AS(parser.parse_top_level(), libglot::ParseError);

    libglot::Arena arena2;
    SQLParser parser2(arena2, "SELECT a FROM t GROUP BY GROUPING SETS ((a)",
                      SQLDialect::PostgreSQL);
    REQUIRE_THROWS_AS(parser2.parse_top_level(), libglot::ParseError);
}
