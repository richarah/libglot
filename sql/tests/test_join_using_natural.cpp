// USING (col, ...) join conditions and NATURAL [INNER|LEFT|RIGHT|FULL] JOIN.

#include <catch2/catch_test_macros.hpp>
#include <libglot/sql/parser.h>
#include <libglot/sql/generator.h>
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

TEST_CASE("JOIN ... USING (col) - exact string", "[join][using]") {
    REQUIRE(gen("SELECT * FROM a JOIN b USING (id)", SQLDialect::ANSI)
            == "SELECT * FROM \"a\" INNER JOIN \"b\" USING (\"id\")");
    REQUIRE(gen("SELECT * FROM a JOIN b USING (id, name)", SQLDialect::ANSI)
            == "SELECT * FROM \"a\" INNER JOIN \"b\" USING (\"id\", \"name\")");
    REQUIRE(gen("SELECT * FROM a LEFT JOIN b USING (id)", SQLDialect::PostgreSQL)
            == "SELECT * FROM \"a\" LEFT JOIN \"b\" USING (\"id\")");
}

TEST_CASE("NATURAL JOIN - exact string", "[join][natural]") {
    REQUIRE(gen("SELECT * FROM a NATURAL JOIN b", SQLDialect::ANSI)
            == "SELECT * FROM \"a\" NATURAL INNER JOIN \"b\"");
    REQUIRE(gen("SELECT * FROM a NATURAL LEFT JOIN b", SQLDialect::ANSI)
            == "SELECT * FROM \"a\" NATURAL LEFT JOIN \"b\"");
    REQUIRE(gen("SELECT * FROM a NATURAL RIGHT JOIN b", SQLDialect::ANSI)
            == "SELECT * FROM \"a\" NATURAL RIGHT JOIN \"b\"");
    REQUIRE(gen("SELECT * FROM a NATURAL FULL JOIN b", SQLDialect::ANSI)
            == "SELECT * FROM \"a\" NATURAL FULL JOIN \"b\"");
}

TEST_CASE("USING with a plain ON condition is unaffected", "[join][using]") {
    REQUIRE(gen("SELECT * FROM a JOIN b ON a.id = b.id", SQLDialect::ANSI)
            == "SELECT * FROM \"a\" INNER JOIN \"b\" ON \"a\".\"id\" = \"b\".\"id\"");
}

TEST_CASE("USING clause - malformed column list is a clean ParseError", "[join][using][error]") {
    libglot::Arena arena;
    SQLParser parser(arena, "SELECT * FROM a JOIN b USING (id", SQLDialect::ANSI);
    REQUIRE_THROWS_AS(parser.parse_top_level(), libglot::ParseError);
}

TEST_CASE("NATURAL/USING JOIN - generated SQL is a fixed point", "[join][fixpoint]") {
    const std::string queries[] = {
        "SELECT * FROM a JOIN b USING (id)",
        "SELECT * FROM a NATURAL LEFT JOIN b",
    };
    for (auto d : {SQLDialect::ANSI, SQLDialect::PostgreSQL, SQLDialect::MySQL, SQLDialect::SQLServer}) {
        for (const auto& q : queries) {
            const std::string g1 = gen(q, d);
            REQUIRE(gen(g1, d) == g1);
        }
    }
}
