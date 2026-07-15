// TABLESAMPLE BERNOULLI(n) / SYSTEM(n) [REPEATABLE(seed)].
//
// This was previously double-broken: the check gating the whole branch
// tested for TK::IDENTIFIER, but TABLESAMPLE is a reserved keyword token
// (TK::TABLESAMPLE) - so the branch could never fire, and even if it had,
// the Tablesample AST node had no field for the sampled table, so the
// FROM-clause table reference was silently discarded. Both are fixed here:
// the keyword is recognized, and Tablesample now wraps the table/alias
// it samples plus an optional REPEATABLE(seed).

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

TEST_CASE("TABLESAMPLE - exact string, table reference preserved", "[tablesample]") {
    REQUIRE(gen("SELECT * FROM t TABLESAMPLE BERNOULLI(10)", SQLDialect::ANSI) ==
            "SELECT * FROM \"t\" TABLESAMPLE BERNOULLI(10)");
    REQUIRE(gen("SELECT * FROM t TABLESAMPLE SYSTEM(20)", SQLDialect::PostgreSQL) ==
            "SELECT * FROM \"t\" TABLESAMPLE SYSTEM(20)");
}

TEST_CASE("TABLESAMPLE - alias is preserved", "[tablesample]") {
    REQUIRE(gen("SELECT * FROM t AS x TABLESAMPLE BERNOULLI(10)", SQLDialect::ANSI) ==
            "SELECT * FROM \"t\" AS \"x\" TABLESAMPLE BERNOULLI(10)");
    REQUIRE(gen("SELECT * FROM t x TABLESAMPLE BERNOULLI(10)", SQLDialect::ANSI) ==
            "SELECT * FROM \"t\" AS \"x\" TABLESAMPLE BERNOULLI(10)");
}

TEST_CASE("TABLESAMPLE - REPEATABLE(seed)", "[tablesample]") {
    REQUIRE(
        gen("SELECT * FROM t TABLESAMPLE BERNOULLI(10) REPEATABLE(42)", SQLDialect::PostgreSQL) ==
        "SELECT * FROM \"t\" TABLESAMPLE BERNOULLI(10) REPEATABLE(42)");
}

TEST_CASE("TABLESAMPLE - usable in a join", "[tablesample]") {
    REQUIRE(gen("SELECT * FROM a JOIN b TABLESAMPLE BERNOULLI(50) ON a.id = b.id",
                SQLDialect::ANSI) == "SELECT * FROM \"a\" INNER JOIN \"b\" TABLESAMPLE "
                                     "BERNOULLI(50) ON \"a\".\"id\" = \"b\".\"id\"");
}

TEST_CASE("TABLESAMPLE throws for MySQL", "[tablesample][error]") {
    REQUIRE_THROWS_AS(gen("SELECT * FROM t TABLESAMPLE BERNOULLI(10)", SQLDialect::MySQL),
                      std::logic_error);
}

TEST_CASE("TABLESAMPLE - malformed clause is a clean ParseError", "[tablesample][error]") {
    libglot::Arena arena;
    SQLParser parser(arena, "SELECT * FROM t TABLESAMPLE BERNOULLI 10)", SQLDialect::ANSI);
    REQUIRE_THROWS_AS(parser.parse_top_level(), libglot::ParseError);
}

TEST_CASE("TABLESAMPLE - generated SQL is a fixed point (PG/ANSI)", "[tablesample][fixpoint]") {
    const std::string queries[] = {
        "SELECT * FROM t TABLESAMPLE BERNOULLI(10)",
        "SELECT * FROM t AS x TABLESAMPLE SYSTEM(20) REPEATABLE(7)",
    };
    for (auto d : {SQLDialect::ANSI, SQLDialect::PostgreSQL}) {
        for (const auto& q : queries) {
            const std::string g1 = gen(q, d);
            REQUIRE(gen(g1, d) == g1);
        }
    }
}
