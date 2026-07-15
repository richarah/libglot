// QUALIFY clause (Snowflake / BigQuery / DuckDB): a post-window-function
// filter, analogous to HAVING for GROUP BY aggregates.
//
// This previously parsed into the QualifyClause AST (SelectStmt::qualify)
// but the generator never read that field back out - QUALIFY was silently
// dropped from the output with no error. Now it is emitted for the
// dialects that support it and throws std::logic_error everywhere else
// (there is no ANSI equivalent short of wrapping the query in a subquery
// with a WHERE filter, which callers must do by hand).

#include <catch2/catch_test_macros.hpp>
#include <libglot/sql/parser.h>
#include <libglot/sql/generator.h>
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

TEST_CASE("QUALIFY - exact string (Snowflake)", "[qualify]") {
    REQUIRE(gen("SELECT a FROM t QUALIFY ROW_NUMBER() OVER (ORDER BY a) = 1", SQLDialect::Snowflake)
            == "SELECT \"a\" FROM \"t\" QUALIFY ROW_NUMBER() OVER (ORDER BY \"a\") = 1");
}

TEST_CASE("QUALIFY - exact string (BigQuery, DuckDB)", "[qualify]") {
    // BigQuery quotes identifiers with backticks; DuckDB with double quotes.
    REQUIRE(gen("SELECT a FROM t QUALIFY row_number() OVER (PARTITION BY a) = 1", SQLDialect::BigQuery)
            == "SELECT `a` FROM `t` QUALIFY row_number() OVER (PARTITION BY `a`) = 1");
    REQUIRE(gen("SELECT a FROM t QUALIFY row_number() OVER (PARTITION BY a) = 1", SQLDialect::DuckDB)
            == "SELECT \"a\" FROM \"t\" QUALIFY row_number() OVER (PARTITION BY \"a\") = 1");
}

TEST_CASE("QUALIFY combined with WHERE/GROUP BY/HAVING", "[qualify]") {
    REQUIRE(gen("SELECT a, SUM(b) FROM t WHERE a > 0 GROUP BY a HAVING SUM(b) > 10 "
                "QUALIFY RANK() OVER (ORDER BY a) <= 5",
                SQLDialect::Snowflake)
            == "SELECT \"a\", SUM(\"b\") FROM \"t\" WHERE \"a\" > 0 GROUP BY \"a\" "
               "HAVING SUM(\"b\") > 10 QUALIFY RANK() OVER (ORDER BY \"a\") <= 5");
}

TEST_CASE("QUALIFY throws for dialects without QUALIFY support", "[qualify][error]") {
    for (auto d : {SQLDialect::PostgreSQL, SQLDialect::MySQL, SQLDialect::ANSI, SQLDialect::SQLServer}) {
        REQUIRE_THROWS_AS(
            gen("SELECT a FROM t QUALIFY ROW_NUMBER() OVER (ORDER BY a) = 1", d),
            std::logic_error);
    }
}

TEST_CASE("QUALIFY - malformed clause is a clean ParseError", "[qualify][error]") {
    libglot::Arena arena;
    SQLParser parser(arena, "SELECT a FROM t QUALIFY", SQLDialect::Snowflake);
    REQUIRE_THROWS_AS(parser.parse_top_level(), libglot::ParseError);
}

TEST_CASE("QUALIFY - generated SQL is a fixed point", "[qualify][fixpoint]") {
    const std::string q = "SELECT a FROM t QUALIFY ROW_NUMBER() OVER (ORDER BY a) = 1";
    for (auto d : {SQLDialect::Snowflake, SQLDialect::BigQuery, SQLDialect::DuckDB}) {
        const std::string g1 = gen(q, d);
        REQUIRE(gen(g1, d) == g1);
    }
}
