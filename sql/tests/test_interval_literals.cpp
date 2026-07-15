// INTERVAL literals: INTERVAL '1 day' (bare form, unit embedded in the
// string) and INTERVAL '2' HOUR / INTERVAL 7 DAY (value + trailing unit
// keyword). Previously these were mis-parsed as a FunctionCall named
// "INTERVAL" that regenerated as INTERVAL(7, DAY) - not valid SQL in any
// dialect and not a fixed point. Now they parse into a dedicated
// IntervalLiteral node that regenerates verbatim.

#include <catch2/catch_test_macros.hpp>
#include <libglot/sql/parser.h>
#include <libglot/sql/generator.h>
#include <libglot/util/arena.h>

#include <string>

using namespace libglot::sql;

namespace {

std::string gen(const std::string& sql, SQLDialect d = SQLDialect::ANSI) {
    libglot::Arena arena;
    SQLParser parser(arena, sql, d);
    auto ast = parser.parse_top_level();
    SQLGenerator generator(d);
    return generator.generate(ast);
}

} // namespace

TEST_CASE("INTERVAL literal - bare string form", "[interval]") {
    REQUIRE(gen("SELECT INTERVAL '1 day'") == "SELECT INTERVAL '1 day'");
    REQUIRE(gen("SELECT INTERVAL '30 days'") == "SELECT INTERVAL '30 days'");
}

TEST_CASE("INTERVAL literal - value + unit form", "[interval]") {
    REQUIRE(gen("SELECT INTERVAL '2' HOUR") == "SELECT INTERVAL '2' HOUR");
    REQUIRE(gen("SELECT INTERVAL 7 DAY") == "SELECT INTERVAL 7 DAY");
    REQUIRE(gen("SELECT INTERVAL 1 MONTH") == "SELECT INTERVAL 1 MONTH");
}

TEST_CASE("INTERVAL literal - used in an arithmetic expression", "[interval]") {
    REQUIRE(gen("SELECT NOW() - INTERVAL '1 day'") == "SELECT NOW() - INTERVAL '1 day'");
    REQUIRE(gen("SELECT d + INTERVAL 7 DAY FROM t") == "SELECT \"d\" + INTERVAL 7 DAY FROM \"t\"");
}

TEST_CASE("INTERVAL literal - malformed clause is a clean ParseError", "[interval][error]") {
    libglot::Arena arena;
    SQLParser parser(arena, "SELECT INTERVAL", SQLDialect::ANSI);
    REQUIRE_THROWS_AS(parser.parse_top_level(), libglot::ParseError);
}

TEST_CASE("INTERVAL literal - generated SQL is a fixed point in every dialect", "[interval][fixpoint]") {
    const std::string queries[] = {
        "SELECT INTERVAL '1 day'",
        "SELECT INTERVAL '2' HOUR",
        "SELECT INTERVAL 7 DAY",
        "SELECT NOW() - INTERVAL '1 day'",
    };
    for (auto d : {SQLDialect::ANSI, SQLDialect::PostgreSQL, SQLDialect::MySQL, SQLDialect::SQLServer}) {
        for (const auto& q : queries) {
            const std::string g1 = gen(q, d);
            REQUIRE(gen(g1, d) == g1);
        }
    }
}
