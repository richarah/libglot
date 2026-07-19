// PostgreSQL DISTINCT ON (expr, ...).
//
// Design choice (documented in generator.h and the feature matrix):
// DISTINCT ON is parsed for every dialect (it's just DISTINCT followed by
// an optional ON (...) target list) but only generated for PostgreSQL;
// every other dialect throws std::logic_error since there is no
// equivalent construct to transpile to.

#include <catch2/catch_test_macros.hpp>
#include <libglot/sql/generator.h>
#include <libglot/sql/parser.h>
#include <libglot/util/arena.h>

#include <stdexcept>
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

TEST_CASE("DISTINCT ON - exact string (PostgreSQL)", "[distinct-on]") {
    REQUIRE(gen("SELECT DISTINCT ON (a) a, b FROM t", SQLDialect::PostgreSQL) ==
            "SELECT DISTINCT ON (\"a\") \"a\", \"b\" FROM \"t\"");
    REQUIRE(gen("SELECT DISTINCT ON (a, b) a, b, c FROM t ORDER BY a, b, c",
                SQLDialect::PostgreSQL) == "SELECT DISTINCT ON (\"a\", \"b\") \"a\", \"b\", \"c\" "
                                           "FROM \"t\" ORDER BY \"a\", \"b\", \"c\"");
}

TEST_CASE("Plain DISTINCT is unaffected", "[distinct-on]") {
    REQUIRE(gen("SELECT DISTINCT a FROM t", SQLDialect::PostgreSQL) ==
            "SELECT DISTINCT \"a\" FROM \"t\"");
}

TEST_CASE("DISTINCT ON throws for non-PostgreSQL dialects", "[distinct-on][error]") {
    for (auto d :
         {SQLDialect::ANSI, SQLDialect::MySQL, SQLDialect::SQLServer, SQLDialect::Snowflake}) {
        REQUIRE_THROWS_AS(gen("SELECT DISTINCT ON (a) a FROM t", d), std::logic_error);
    }
}

TEST_CASE("DISTINCT ON - malformed clause is a clean ParseError", "[distinct-on][error]") {
    libglot::Arena arena;
    SQLParser parser(arena, "SELECT DISTINCT ON a FROM t", SQLDialect::PostgreSQL);
    REQUIRE_THROWS_AS(parser.parse_top_level(), libglot::ParseError);
}

TEST_CASE("DISTINCT ON - generated SQL is a fixed point (PostgreSQL)", "[distinct-on][fixpoint]") {
    const std::string queries[] = {
        "SELECT DISTINCT ON (a) a, b FROM t",
        "SELECT DISTINCT ON (a, b) a, b, c FROM t ORDER BY a, b",
    };
    for (const auto& q : queries) {
        const std::string g1 = gen(q, SQLDialect::PostgreSQL);
        REQUIRE(gen(g1, SQLDialect::PostgreSQL) == g1);
    }
}
