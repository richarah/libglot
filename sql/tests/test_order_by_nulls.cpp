// ORDER BY ... NULLS FIRST / NULLS LAST.
//
// Design choice (documented in generator.h): MySQL/MariaDB and T-SQL
// (SQLServer/AzureSynapse) have no NULLS FIRST/LAST syntax at all, so
// generating it for those dialects throws std::logic_error rather than
// silently reordering nulls differently than the source query intended.
// Every other dialect emits the clause verbatim.

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

TEST_CASE("ORDER BY NULLS FIRST/LAST - exact string (PostgreSQL)", "[order-by][nulls]") {
    REQUIRE(gen("SELECT a FROM t ORDER BY a NULLS FIRST", SQLDialect::PostgreSQL) ==
            "SELECT \"a\" FROM \"t\" ORDER BY \"a\" NULLS FIRST");
    REQUIRE(gen("SELECT a FROM t ORDER BY a NULLS LAST", SQLDialect::PostgreSQL) ==
            "SELECT \"a\" FROM \"t\" ORDER BY \"a\" NULLS LAST");
    REQUIRE(gen("SELECT a FROM t ORDER BY a DESC NULLS FIRST", SQLDialect::PostgreSQL) ==
            "SELECT \"a\" FROM \"t\" ORDER BY \"a\" DESC NULLS FIRST");
}

TEST_CASE("ORDER BY NULLS FIRST/LAST - multiple items, mixed NULLS specs", "[order-by][nulls]") {
    REQUIRE(gen("SELECT a, b FROM t ORDER BY a NULLS FIRST, b DESC NULLS LAST", SQLDialect::ANSI) ==
            "SELECT \"a\", \"b\" FROM \"t\" ORDER BY \"a\" NULLS FIRST, \"b\" DESC NULLS LAST");
}

TEST_CASE("ORDER BY without NULLS clause is unaffected", "[order-by][nulls]") {
    REQUIRE(gen("SELECT a FROM t ORDER BY a DESC", SQLDialect::ANSI) ==
            "SELECT \"a\" FROM \"t\" ORDER BY \"a\" DESC");
}

TEST_CASE("ORDER BY NULLS FIRST/LAST throws for MySQL and SQL Server", "[order-by][nulls][error]") {
    for (auto d : {SQLDialect::MySQL, SQLDialect::MariaDB, SQLDialect::SQLServer,
                   SQLDialect::AzureSynapse}) {
        REQUIRE_THROWS_AS(gen("SELECT a FROM t ORDER BY a NULLS FIRST", d), std::logic_error);
        REQUIRE_THROWS_AS(gen("SELECT a FROM t ORDER BY a NULLS LAST", d), std::logic_error);
    }
}

TEST_CASE("ORDER BY NULLS FIRST/LAST - malformed clause is a clean ParseError",
          "[order-by][nulls][error]") {
    libglot::Arena arena;
    SQLParser parser(arena, "SELECT a FROM t ORDER BY a NULLS", SQLDialect::PostgreSQL);
    REQUIRE_THROWS_AS(parser.parse_top_level(), libglot::ParseError);
}

TEST_CASE("ORDER BY NULLS FIRST/LAST - generated SQL is a fixed point",
          "[order-by][nulls][fixpoint]") {
    for (auto d :
         {SQLDialect::ANSI, SQLDialect::PostgreSQL, SQLDialect::Snowflake, SQLDialect::SQLite}) {
        const std::string queries[] = {
            "SELECT a FROM t ORDER BY a NULLS FIRST",
            "SELECT a FROM t ORDER BY a DESC NULLS LAST",
        };
        for (const auto& q : queries) {
            const std::string g1 = gen(q, d);
            REQUIRE(gen(g1, d) == g1);
        }
    }
}
