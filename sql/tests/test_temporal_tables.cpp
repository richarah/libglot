// Wave 2: SQL:2011 system-versioned temporal tables.
//
//   FROM t FOR SYSTEM_TIME AS OF '2020-01-01'
//   FROM t FOR SYSTEM_TIME FROM 'a' TO 'b'
//   FROM t FOR SYSTEM_TIME BETWEEN 'a' AND 'b'
//   FROM t FOR SYSTEM_TIME CONTAINED IN ('a', 'b')
//   FROM t FOR SYSTEM_TIME ALL
//
// T-SQL (SQL Server / Azure Synapse) and MariaDB (which adopted the same
// syntax) support this; every other dialect throws std::logic_error at
// generation time.

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

TEST_CASE("Temporal table - AS OF", "[temporal]") {
    REQUIRE(
        transpile("SELECT * FROM t FOR SYSTEM_TIME AS OF '2020-01-01'", SQLDialect::SQLServer) ==
        "SELECT * FROM [t] FOR SYSTEM_TIME AS OF '2020-01-01'");
    REQUIRE(
        transpile("SELECT * FROM t FOR SYSTEM_TIME AS OF '2020-01-01'", SQLDialect::AzureSynapse) ==
        "SELECT * FROM \"t\" FOR SYSTEM_TIME AS OF '2020-01-01'");
    REQUIRE(transpile("SELECT * FROM t FOR SYSTEM_TIME AS OF '2020-01-01'", SQLDialect::MariaDB) ==
            "SELECT * FROM `t` FOR SYSTEM_TIME AS OF '2020-01-01'");
}

TEST_CASE("Temporal table - FROM ... TO ...", "[temporal]") {
    REQUIRE(transpile("SELECT * FROM t FOR SYSTEM_TIME FROM 'a' TO 'b'", SQLDialect::SQLServer) ==
            "SELECT * FROM [t] FOR SYSTEM_TIME FROM 'a' TO 'b'");
}

TEST_CASE("Temporal table - BETWEEN ... AND ...", "[temporal]") {
    REQUIRE(
        transpile("SELECT * FROM t FOR SYSTEM_TIME BETWEEN 'a' AND 'b'", SQLDialect::SQLServer) ==
        "SELECT * FROM [t] FOR SYSTEM_TIME BETWEEN 'a' AND 'b'");
}

TEST_CASE("Temporal table - CONTAINED IN (...)", "[temporal]") {
    REQUIRE(transpile("SELECT * FROM t FOR SYSTEM_TIME CONTAINED IN ('a', 'b')",
                      SQLDialect::SQLServer) ==
            "SELECT * FROM [t] FOR SYSTEM_TIME CONTAINED IN ('a', 'b')");
}

TEST_CASE("Temporal table - ALL", "[temporal]") {
    REQUIRE(transpile("SELECT * FROM t FOR SYSTEM_TIME ALL", SQLDialect::SQLServer) ==
            "SELECT * FROM [t] FOR SYSTEM_TIME ALL");
}

TEST_CASE("Temporal table - clause comes before the alias", "[temporal]") {
    REQUIRE(transpile("SELECT * FROM t FOR SYSTEM_TIME AS OF '2020-01-01' AS t1",
                      SQLDialect::SQLServer) ==
            "SELECT * FROM [t] FOR SYSTEM_TIME AS OF '2020-01-01' AS [t1]");
}

TEST_CASE("Temporal table - AST shape", "[temporal]") {
    libglot::Arena arena;
    SQLParser parser(arena, "SELECT * FROM t FOR SYSTEM_TIME AS OF '2020-01-01'",
                     SQLDialect::SQLServer);
    auto* ast = static_cast<SelectStmt*>(parser.parse_top_level());
    REQUIRE(ast->from->type == SQLNodeKind::TABLE_REF);
    auto* tbl = static_cast<TableRef*>(ast->from);
    REQUIRE(tbl->temporal_kind == TemporalKind::AS_OF);
    REQUIRE(tbl->temporal_arg1 != nullptr);
    REQUIRE(tbl->temporal_arg2 == nullptr);
}

TEST_CASE("Temporal table - fixed point (SQL Server)", "[temporal][roundtrip]") {
    const std::string queries[] = {
        "SELECT * FROM t FOR SYSTEM_TIME AS OF '2020-01-01'",
        "SELECT * FROM t FOR SYSTEM_TIME FROM 'a' TO 'b'",
        "SELECT * FROM t FOR SYSTEM_TIME BETWEEN 'a' AND 'b'",
        "SELECT * FROM t FOR SYSTEM_TIME CONTAINED IN ('a', 'b')",
        "SELECT * FROM t FOR SYSTEM_TIME ALL",
    };
    for (const auto& q : queries) {
        const std::string g1 = transpile(q, SQLDialect::SQLServer);
        libglot::Arena arena;
        SQLParser p2(arena, g1, SQLDialect::SQLServer);
        auto* ast2 = p2.parse_top_level();
        SQLGenerator gen2(SQLDialect::SQLServer);
        REQUIRE(gen2.generate(ast2) == g1);
    }
}

TEST_CASE("Temporal table - unsupported dialects throw a clean std::logic_error",
          "[temporal][error]") {
    for (auto d :
         {SQLDialect::PostgreSQL, SQLDialect::MySQL, SQLDialect::Oracle, SQLDialect::ANSI}) {
        REQUIRE_THROWS_AS(transpile("SELECT * FROM t FOR SYSTEM_TIME ALL", d), std::logic_error);
    }
}

TEST_CASE("Temporal table - bad syntax raises a clean ParseError", "[temporal][error]") {
    libglot::Arena arena;
    SQLParser parser(arena, "SELECT * FROM t FOR SYSTEM_TIME SNAPSHOT '2020-01-01'",
                     SQLDialect::SQLServer);
    REQUIRE_THROWS_AS(parser.parse_top_level(), libglot::ParseError);
}
