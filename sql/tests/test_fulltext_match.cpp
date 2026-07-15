// Wave 2: MySQL/MariaDB fulltext search.
//
//   MATCH (col, ...) AGAINST ('expr' [IN NATURAL LANGUAGE MODE
//                                     | IN NATURAL LANGUAGE MODE WITH QUERY EXPANSION
//                                     | IN BOOLEAN MODE
//                                     | WITH QUERY EXPANSION])
//
// MySQL/MariaDB only; every other dialect throws std::logic_error.

#include <catch2/catch_test_macros.hpp>
#include <libglot/sql/generator.h>
#include <libglot/sql/parser.h>
#include <libglot/util/arena.h>

#include <string>

using namespace libglot::sql;

namespace {

std::string transpile(const std::string& sql, SQLDialect dialect) {
    libglot::Arena arena;
    SQLParser parser(arena, sql, dialect);
    auto* ast = parser.parse_top_level();
    SQLGenerator gen(dialect);
    return gen.generate(ast);
}

} // namespace

TEST_CASE("MATCH AGAINST - bare (default natural language mode)", "[fulltext]") {
    REQUIRE(transpile("SELECT * FROM articles WHERE MATCH (title, body) AGAINST ('database')",
                      SQLDialect::MySQL) ==
            "SELECT * FROM `articles` WHERE MATCH (`title`, `body`) AGAINST ('database')");
}

TEST_CASE("MATCH AGAINST - IN NATURAL LANGUAGE MODE", "[fulltext]") {
    REQUIRE(transpile("SELECT * FROM articles WHERE MATCH (title) AGAINST ('database' IN NATURAL "
                      "LANGUAGE MODE)",
                      SQLDialect::MySQL) == "SELECT * FROM `articles` WHERE MATCH (`title`) "
                                            "AGAINST ('database' IN NATURAL LANGUAGE MODE)");
}

TEST_CASE("MATCH AGAINST - IN NATURAL LANGUAGE MODE WITH QUERY EXPANSION", "[fulltext]") {
    REQUIRE(transpile("SELECT * FROM articles WHERE MATCH (title) AGAINST "
                      "('database' IN NATURAL LANGUAGE MODE WITH QUERY EXPANSION)",
                      SQLDialect::MySQL) ==
            "SELECT * FROM `articles` WHERE MATCH (`title`) AGAINST "
            "('database' IN NATURAL LANGUAGE MODE WITH QUERY EXPANSION)");
}

TEST_CASE("MATCH AGAINST - IN BOOLEAN MODE", "[fulltext]") {
    REQUIRE(transpile("SELECT * FROM articles WHERE MATCH (title) AGAINST ('+database -mysql' IN "
                      "BOOLEAN MODE)",
                      SQLDialect::MySQL) == "SELECT * FROM `articles` WHERE MATCH (`title`) "
                                            "AGAINST ('+database -mysql' IN BOOLEAN MODE)");
}

TEST_CASE("MATCH AGAINST - WITH QUERY EXPANSION", "[fulltext]") {
    REQUIRE(
        transpile(
            "SELECT * FROM articles WHERE MATCH (title) AGAINST ('database' WITH QUERY EXPANSION)",
            SQLDialect::MySQL) ==
        "SELECT * FROM `articles` WHERE MATCH (`title`) AGAINST ('database' WITH QUERY EXPANSION)");
}

TEST_CASE("MATCH AGAINST - multiple columns", "[fulltext]") {
    REQUIRE(transpile("SELECT * FROM articles WHERE MATCH (title, body, tags) AGAINST ('database')",
                      SQLDialect::MariaDB) ==
            "SELECT * FROM `articles` WHERE MATCH (`title`, `body`, `tags`) AGAINST ('database')");
}

TEST_CASE("MATCH AGAINST - AST shape", "[fulltext]") {
    libglot::Arena arena;
    SQLParser parser(arena, "SELECT * FROM t WHERE MATCH (a, b) AGAINST ('x' IN BOOLEAN MODE)",
                     SQLDialect::MySQL);
    auto* ast = static_cast<SelectStmt*>(parser.parse_top_level());
    REQUIRE(ast->where->type == SQLNodeKind::MATCH_AGAINST);
    auto* m = static_cast<MatchAgainst*>(ast->where);
    REQUIRE(m->columns.size() == 2);
    REQUIRE(m->columns[0] == "a");
    REQUIRE(m->columns[1] == "b");
    REQUIRE(m->mode == FulltextMode::BOOLEAN_MODE);
    REQUIRE(m->mode_specified == true);
}

TEST_CASE("MATCH AGAINST - fixed point (MySQL/MariaDB)", "[fulltext][roundtrip]") {
    const std::string queries[] = {
        "SELECT * FROM t WHERE MATCH (a) AGAINST ('x')",
        "SELECT * FROM t WHERE MATCH (a) AGAINST ('x' IN NATURAL LANGUAGE MODE)",
        "SELECT * FROM t WHERE MATCH (a) AGAINST ('x' IN NATURAL LANGUAGE MODE WITH QUERY "
        "EXPANSION)",
        "SELECT * FROM t WHERE MATCH (a) AGAINST ('x' IN BOOLEAN MODE)",
        "SELECT * FROM t WHERE MATCH (a) AGAINST ('x' WITH QUERY EXPANSION)",
    };
    for (auto d : {SQLDialect::MySQL, SQLDialect::MariaDB}) {
        for (const auto& q : queries) {
            const std::string g1 = transpile(q, d);
            REQUIRE(transpile(g1, d) == g1);
        }
    }
}

TEST_CASE("MATCH AGAINST - unsupported dialects throw a clean std::logic_error",
          "[fulltext][error]") {
    for (auto d :
         {SQLDialect::PostgreSQL, SQLDialect::SQLServer, SQLDialect::ANSI, SQLDialect::Oracle}) {
        REQUIRE_THROWS_AS(transpile("SELECT * FROM t WHERE MATCH (a) AGAINST ('x')", d),
                          std::logic_error);
    }
}

TEST_CASE("MATCH AGAINST - missing AGAINST is a clean ParseError", "[fulltext][error]") {
    libglot::Arena arena;
    SQLParser parser(arena, "SELECT * FROM t WHERE MATCH (a)", SQLDialect::MySQL);
    REQUIRE_THROWS_AS(parser.parse_top_level(), libglot::ParseError);
}

TEST_CASE("MATCH AGAINST - bad modifier is a clean ParseError", "[fulltext][error]") {
    libglot::Arena arena;
    SQLParser parser(arena, "SELECT * FROM t WHERE MATCH (a) AGAINST ('x' IN WEIRD MODE)",
                     SQLDialect::MySQL);
    REQUIRE_THROWS_AS(parser.parse_top_level(), libglot::ParseError);
}
