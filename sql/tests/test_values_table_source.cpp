// VALUES as a FROM-clause table source: FROM (VALUES (1, 'a'), (2, 'b')) AS
// v(id, name). Reuses the previously-dormant ValuesClause AST node (only
// ever constructed here; INSERT ... VALUES keeps its own separate
// vector-of-rows representation), extended with an alias and an optional
// column list.

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

TEST_CASE("VALUES table source - exact string with column list", "[values-source]") {
    REQUIRE(gen("SELECT * FROM (VALUES (1, 'a'), (2, 'b')) AS v(id, name)", SQLDialect::ANSI)
            == "SELECT * FROM (VALUES (1, 'a'), (2, 'b')) AS \"v\"(\"id\", \"name\")");
}

TEST_CASE("VALUES table source - alias without column list", "[values-source]") {
    REQUIRE(gen("SELECT * FROM (VALUES (1), (2)) AS v", SQLDialect::PostgreSQL)
            == "SELECT * FROM (VALUES (1), (2)) AS \"v\"");
}

TEST_CASE("VALUES table source - alias without AS keyword", "[values-source]") {
    REQUIRE(gen("SELECT * FROM (VALUES (1, 2)) v(a, b)", SQLDialect::MySQL)
            == "SELECT * FROM (VALUES (1, 2)) AS `v`(`a`, `b`)");
}

TEST_CASE("VALUES table source - usable in a join", "[values-source]") {
    REQUIRE(gen("SELECT * FROM t JOIN (VALUES (1, 'a')) AS v(id, name) ON t.id = v.id", SQLDialect::ANSI)
            == "SELECT * FROM \"t\" INNER JOIN (VALUES (1, 'a')) AS \"v\"(\"id\", \"name\") ON \"t\".\"id\" = \"v\".\"id\"");
}

TEST_CASE("VALUES table source - missing closing paren is a clean ParseError", "[values-source][error]") {
    libglot::Arena arena;
    SQLParser parser(arena, "SELECT * FROM (VALUES (1, 2) AS v(a, b)", SQLDialect::ANSI);
    REQUIRE_THROWS_AS(parser.parse_top_level(), libglot::ParseError);
}

TEST_CASE("VALUES table source - generated SQL is a fixed point in every dialect", "[values-source][fixpoint]") {
    const std::string q = "SELECT * FROM (VALUES (1, 'a'), (2, 'b')) AS v(id, name)";
    for (auto d : {SQLDialect::ANSI, SQLDialect::PostgreSQL, SQLDialect::MySQL, SQLDialect::SQLServer}) {
        const std::string g1 = gen(q, d);
        REQUIRE(gen(g1, d) == g1);
    }
}
