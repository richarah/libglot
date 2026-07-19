// Named windows: SELECT ... FROM t WINDOW w AS (PARTITION BY a ORDER BY b),
// with OVER w references. WINDOW is not a reserved word in the tokenizer
// (it lexes as a plain identifier), so it is recognized via the same
// soft-keyword lookahead used for ROLLUP/CUBE/GROUPING SETS.

#include <catch2/catch_test_macros.hpp>
#include <libglot/sql/generator.h>
#include <libglot/sql/parser.h>
#include <libglot/util/arena.h>

#include <string>

using namespace libglot::sql;

namespace {

std::string gen(const std::string& sql, SQLDialect d) {
    libglot::Arena arena;
    SQLParser parser(arena, sql, d);
    auto ast = parser.parse_top_level();
    SQLGenerator generator(d);
    return generator.generate(ast);
}

} // namespace

TEST_CASE("Named window - exact string", "[named-window]") {
    REQUIRE(gen("SELECT a, ROW_NUMBER() OVER w FROM t WINDOW w AS (PARTITION BY a ORDER BY b)",
                SQLDialect::ANSI) == "SELECT \"a\", ROW_NUMBER() OVER \"w\" FROM \"t\" "
                                     "WINDOW \"w\" AS (PARTITION BY \"a\" ORDER BY \"b\")");
}

TEST_CASE("Named window - multiple named windows", "[named-window]") {
    REQUIRE(gen("SELECT a FROM t WINDOW w1 AS (PARTITION BY a), w2 AS (ORDER BY b)",
                SQLDialect::ANSI) == "SELECT \"a\" FROM \"t\" WINDOW \"w1\" AS (PARTITION BY "
                                     "\"a\"), \"w2\" AS (ORDER BY \"b\")");
}

TEST_CASE("Named window - referenced by more than one function", "[named-window]") {
    REQUIRE(gen("SELECT RANK() OVER w, ROW_NUMBER() OVER w FROM t WINDOW w AS (ORDER BY a)",
                SQLDialect::ANSI) == "SELECT RANK() OVER \"w\", ROW_NUMBER() OVER \"w\" FROM \"t\" "
                                     "WINDOW \"w\" AS (ORDER BY \"a\")");
}

TEST_CASE("Inline OVER (...) is unaffected by named window support", "[named-window]") {
    REQUIRE(gen("SELECT ROW_NUMBER() OVER (PARTITION BY a ORDER BY b) FROM t", SQLDialect::ANSI) ==
            "SELECT ROW_NUMBER() OVER (PARTITION BY \"a\" ORDER BY \"b\") FROM \"t\"");
}

TEST_CASE("Named window - malformed clause is a clean ParseError", "[named-window][error]") {
    {
        libglot::Arena arena;
        SQLParser parser(arena, "SELECT a FROM t WINDOW w (PARTITION BY a)", SQLDialect::ANSI);
        REQUIRE_THROWS_AS(parser.parse_top_level(), libglot::ParseError);
    }
    {
        libglot::Arena arena;
        SQLParser parser(arena, "SELECT a FROM t WINDOW AS (PARTITION BY a)", SQLDialect::ANSI);
        REQUIRE_THROWS_AS(parser.parse_top_level(), libglot::ParseError);
    }
}

TEST_CASE("Named window - generated SQL is a fixed point in every dialect",
          "[named-window][fixpoint]") {
    const std::string q =
        "SELECT a, ROW_NUMBER() OVER w FROM t WINDOW w AS (PARTITION BY a ORDER BY b)";
    for (auto d :
         {SQLDialect::ANSI, SQLDialect::PostgreSQL, SQLDialect::MySQL, SQLDialect::SQLServer}) {
        const std::string g1 = gen(q, d);
        REQUIRE(gen(g1, d) == g1);
    }
}
