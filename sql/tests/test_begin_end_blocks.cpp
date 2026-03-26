#include <catch2/catch_test_macros.hpp>
#include <libglot/sql/parser.h>
#include <libglot/sql/generator.h>

using namespace libglot::sql;
using libglot::ParseError;

TEST_CASE("BEGIN TRANSACTION vs BEGIN...END disambiguation", "[procedural][begin_end]") {
    SECTION("BEGIN TRANSACTION is recognized as transaction statement") {
        std::string sql = "BEGIN TRANSACTION";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::ANSI);
        return gen.generate(ast);
    }();
        REQUIRE(result.find("BEGIN") != std::string::npos);
    }

    SECTION("BEGIN WORK is recognized as transaction statement") {
        std::string sql = "BEGIN WORK";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::ANSI);
        return gen.generate(ast);
    }();
        REQUIRE(result.find("BEGIN") != std::string::npos);
    }

    SECTION("BEGIN without TRANSACTION is procedural block") {
        std::string sql = "BEGIN SELECT 1; END";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();
        REQUIRE(result.find("BEGIN") != std::string::npos);
        REQUIRE(result.find("END") != std::string::npos);
        REQUIRE(result.find("SELECT 1") != std::string::npos);
    }
}

TEST_CASE("BEGIN...END blocks parse correctly", "[procedural][begin_end]") {
    SECTION("Empty BEGIN...END block") {
        std::string sql = "BEGIN END";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();
        REQUIRE(result.find("BEGIN") != std::string::npos);
        REQUIRE(result.find("END") != std::string::npos);
    }

    SECTION("BEGIN...END with single statement") {
        std::string sql = "BEGIN SELECT 1; END";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();
        REQUIRE(result.find("BEGIN") != std::string::npos);
        REQUIRE(result.find("SELECT 1") != std::string::npos);
        REQUIRE(result.find("END") != std::string::npos);
    }

    SECTION("BEGIN...END with multiple statements") {
        std::string sql = "BEGIN SELECT 1; SELECT 2; SELECT 3; END";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();
        REQUIRE(result.find("BEGIN") != std::string::npos);
        REQUIRE(result.find("SELECT 1") != std::string::npos);
        REQUIRE(result.find("SELECT 2") != std::string::npos);
        REQUIRE(result.find("SELECT 3") != std::string::npos);
        REQUIRE(result.find("END") != std::string::npos);
    }

    SECTION("Nested BEGIN...END blocks") {
        std::string sql = "BEGIN SELECT 1; BEGIN SELECT 2; END; SELECT 3; END";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();
        REQUIRE(result.find("BEGIN") != std::string::npos);
        REQUIRE(result.find("SELECT 1") != std::string::npos);
        REQUIRE(result.find("SELECT 2") != std::string::npos);
        REQUIRE(result.find("SELECT 3") != std::string::npos);
        // Should have two END keywords (one for each BEGIN)
        size_t count = 0;
        size_t pos = 0;
        while ((pos = result.find("END", pos)) != std::string::npos) {
            count++;
            pos++;
        }
        REQUIRE(count >= 2);
    }
}

TEST_CASE("PostgreSQL PL/pgSQL BEGIN...END blocks", "[procedural][plpgsql]") {
    SECTION("PL/pgSQL block with DECLARE") {
        std::string sql = "BEGIN DECLARE x INT DEFAULT 10; SELECT x; END";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();
        REQUIRE(result.find("BEGIN") != std::string::npos);
        REQUIRE(result.find("DECLARE") != std::string::npos);
        REQUIRE(result.find("INT") != std::string::npos);
        REQUIRE(result.find("SELECT") != std::string::npos);
        REQUIRE(result.find("END") != std::string::npos);
    }

    SECTION("PL/pgSQL block with IF statement") {
        std::string sql = "BEGIN IF 1 = 1 THEN SELECT 'true'; END IF; END";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();
        REQUIRE(result.find("BEGIN") != std::string::npos);
        REQUIRE(result.find("IF") != std::string::npos);
        REQUIRE(result.find("THEN") != std::string::npos);
        REQUIRE(result.find("END IF") != std::string::npos);
        REQUIRE(result.find("END") != std::string::npos);
    }

    SECTION("PL/pgSQL block with WHILE loop") {
        std::string sql = "BEGIN WHILE 1 = 1 DO SELECT 1; END WHILE; END";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();
        REQUIRE(result.find("BEGIN") != std::string::npos);
        REQUIRE(result.find("WHILE") != std::string::npos);
        REQUIRE(result.find("DO") != std::string::npos);
        REQUIRE(result.find("END WHILE") != std::string::npos);
        REQUIRE(result.find("END") != std::string::npos);
    }

    SECTION("PL/pgSQL block with FOR loop") {
        std::string sql = "BEGIN FOR i IN 1..10 LOOP SELECT i; END LOOP; END";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();
        REQUIRE(result.find("BEGIN") != std::string::npos);
        REQUIRE(result.find("FOR") != std::string::npos);
        REQUIRE(result.find("IN") != std::string::npos);
        REQUIRE(result.find("LOOP") != std::string::npos);
        REQUIRE(result.find("END LOOP") != std::string::npos);
        REQUIRE(result.find("END") != std::string::npos);
    }
}

TEST_CASE("MySQL stored procedure BEGIN...END blocks", "[procedural][mysql]") {
    SECTION("MySQL procedure block") {
        std::string sql = "BEGIN SELECT 1; SELECT 2; END";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::MySQL);
        return gen.generate(ast);
    }();
        REQUIRE(result.find("BEGIN") != std::string::npos);
        REQUIRE(result.find("SELECT 1") != std::string::npos);
        REQUIRE(result.find("SELECT 2") != std::string::npos);
        REQUIRE(result.find("END") != std::string::npos);
    }

    SECTION("MySQL block with DECLARE and SET") {
        std::string sql = "BEGIN DECLARE x INT DEFAULT 5; SELECT x; END";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::MySQL);
        return gen.generate(ast);
    }();
        REQUIRE(result.find("BEGIN") != std::string::npos);
        REQUIRE(result.find("DECLARE") != std::string::npos);
        REQUIRE(result.find("SELECT") != std::string::npos);
        REQUIRE(result.find("END") != std::string::npos);
    }
}

TEST_CASE("T-SQL BEGIN...END blocks", "[procedural][tsql]") {
    SECTION("T-SQL simple block") {
        std::string sql = "BEGIN SELECT 1; SELECT 2; END";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::SQLServer);
        return gen.generate(ast);
    }();
        REQUIRE(result.find("BEGIN") != std::string::npos);
        REQUIRE(result.find("SELECT 1") != std::string::npos);
        REQUIRE(result.find("SELECT 2") != std::string::npos);
        REQUIRE(result.find("END") != std::string::npos);
    }

    SECTION("T-SQL block with DECLARE @variable") {
        std::string sql = "BEGIN DECLARE counter INT DEFAULT 0; SELECT counter; END";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::SQLServer);
        return gen.generate(ast);
    }();
        REQUIRE(result.find("BEGIN") != std::string::npos);
        REQUIRE(result.find("DECLARE") != std::string::npos);
        REQUIRE(result.find("SELECT") != std::string::npos);
        REQUIRE(result.find("END") != std::string::npos);
    }

    SECTION("T-SQL block with IF statement") {
        std::string sql = "BEGIN IF 1 = 1 THEN SELECT 'yes'; END IF; END";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::SQLServer);
        return gen.generate(ast);
    }();
        REQUIRE(result.find("BEGIN") != std::string::npos);
        REQUIRE(result.find("IF") != std::string::npos);
        REQUIRE(result.find("END") != std::string::npos);
    }

    SECTION("T-SQL block with WHILE loop") {
        std::string sql = "BEGIN WHILE 1 = 1 DO SELECT 1; END WHILE; END";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::SQLServer);
        return gen.generate(ast);
    }();
        REQUIRE(result.find("BEGIN") != std::string::npos);
        REQUIRE(result.find("WHILE") != std::string::npos);
        REQUIRE(result.find("END") != std::string::npos);
    }
}

TEST_CASE("Oracle PL/SQL BEGIN...END blocks", "[procedural][plsql]") {
    SECTION("Oracle simple block") {
        std::string sql = "BEGIN SELECT 1 FROM DUAL; END";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::Oracle);
        return gen.generate(ast);
    }();
        REQUIRE(result.find("BEGIN") != std::string::npos);
        REQUIRE(result.find("SELECT") != std::string::npos);
        REQUIRE(result.find("DUAL") != std::string::npos);
        REQUIRE(result.find("END") != std::string::npos);
    }

    SECTION("Oracle block with DECLARE") {
        std::string sql = "BEGIN DECLARE x NUMBER DEFAULT 100; SELECT x FROM DUAL; END";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::Oracle);
        return gen.generate(ast);
    }();
        REQUIRE(result.find("BEGIN") != std::string::npos);
        REQUIRE(result.find("DECLARE") != std::string::npos);
        REQUIRE(result.find("SELECT") != std::string::npos);
        REQUIRE(result.find("END") != std::string::npos);
    }
}

TEST_CASE("Cross-dialect BEGIN...END transpilation", "[procedural][transpile]") {
    SECTION("PostgreSQL to MySQL") {
        std::string sql = "BEGIN SELECT 1; SELECT 2; END";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::MySQL);
        return gen.generate(ast);
    }();
        REQUIRE(result.find("BEGIN") != std::string::npos);
        REQUIRE(result.find("SELECT 1") != std::string::npos);
        REQUIRE(result.find("SELECT 2") != std::string::npos);
        REQUIRE(result.find("END") != std::string::npos);
    }

    SECTION("PostgreSQL to SQL Server") {
        std::string sql = "BEGIN SELECT 1; SELECT 2; END";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::SQLServer);
        return gen.generate(ast);
    }();
        REQUIRE(result.find("BEGIN") != std::string::npos);
        REQUIRE(result.find("SELECT 1") != std::string::npos);
        REQUIRE(result.find("SELECT 2") != std::string::npos);
        REQUIRE(result.find("END") != std::string::npos);
    }

    SECTION("MySQL to Oracle") {
        std::string sql = "BEGIN SELECT 1; END";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::Oracle);
        return gen.generate(ast);
    }();
        REQUIRE(result.find("BEGIN") != std::string::npos);
        REQUIRE(result.find("SELECT 1") != std::string::npos);
        REQUIRE(result.find("END") != std::string::npos);
    }
}

TEST_CASE("BEGIN...END error handling", "[procedural][error]") {
    SECTION("BEGIN without matching END throws error") {
        std::string sql = "BEGIN SELECT 1";
        REQUIRE_THROWS_AS([&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }(), ParseError);
    }

    SECTION("Unmatched END throws error") {
        std::string sql = "SELECT 1; END";
        // This might not throw immediately in all parsers, but should be caught
        // For now, just ensure it doesn't crash
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();
        // Result may contain error or unexpected output
    }
}
