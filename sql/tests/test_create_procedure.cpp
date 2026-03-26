#include <catch2/catch_test_macros.hpp>
#include <libglot/sql/generator.h>
#include <libglot/sql/parser.h>

using namespace libglot::sql;

TEST_CASE("CREATE PROCEDURE parsing and generation", "[procedural][create_procedure]") {
    SECTION("Simple CREATE PROCEDURE with no parameters") {
        std::string sql = "CREATE PROCEDURE test_proc() AS BEGIN SELECT 1; END";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();

        REQUIRE(result.find("CREATE PROCEDURE test_proc") != std::string::npos);
        REQUIRE(result.find("BEGIN") != std::string::npos);
        REQUIRE(result.find("END") != std::string::npos);
    }

    SECTION("CREATE PROCEDURE with parameters") {
        std::string sql = "CREATE PROCEDURE add_user(name VARCHAR(100), age INT) AS BEGIN INSERT INTO users VALUES (name, age); END";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();

        INFO("Generated SQL: " << result);
        REQUIRE(result.find("CREATE PROCEDURE add_user") != std::string::npos);
        REQUIRE(result.find("name VARCHAR(100)") != std::string::npos);
        REQUIRE(result.find("age INT") != std::string::npos);
    }

    SECTION("CREATE OR REPLACE PROCEDURE") {
        std::string sql = "CREATE OR REPLACE PROCEDURE test_proc() AS BEGIN SELECT 1; END";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();

        REQUIRE(result.find("CREATE OR REPLACE PROCEDURE") != std::string::npos);
    }

    SECTION("CREATE FUNCTION with RETURNS clause") {
        std::string sql = "CREATE FUNCTION get_total(x INT, y INT) RETURNS INT AS BEGIN RETURN x + y; END";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();

        REQUIRE(result.find("CREATE FUNCTION get_total") != std::string::npos);
        REQUIRE(result.find("RETURNS INT") != std::string::npos);
        REQUIRE(result.find("RETURN") != std::string::npos);
    }

    SECTION("CREATE FUNCTION with LANGUAGE clause (PostgreSQL)") {
        std::string sql = "CREATE FUNCTION test_func() RETURNS INT LANGUAGE plpgsql AS BEGIN RETURN 42; END";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();


        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();

        INFO("Generated SQL: " << result);
        REQUIRE(result.find("LANGUAGE plpgsql") != std::string::npos);
    }

    SECTION("CREATE PROCEDURE with IN/OUT parameters") {
        std::string sql = "CREATE PROCEDURE swap(IN a INT, OUT b INT, INOUT c INT) AS BEGIN SELECT a; END";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();

        REQUIRE(result.find("IN a INT") != std::string::npos);
        REQUIRE(result.find("OUT b INT") != std::string::npos);
        REQUIRE(result.find("INOUT c INT") != std::string::npos);
    }

    SECTION("CREATE PROCEDURE with multiple statements in body") {
        std::string sql = "CREATE PROCEDURE multi_stmt() AS BEGIN SELECT 1; SELECT 2; SELECT 3; END";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();

        REQUIRE(result.find("SELECT 1") != std::string::npos);
        REQUIRE(result.find("SELECT 2") != std::string::npos);
        REQUIRE(result.find("SELECT 3") != std::string::npos);
    }
}

TEST_CASE("CREATE PROCEDURE dialect transpilation", "[procedural][create_procedure][dialects]") {
    SECTION("PostgreSQL PL/pgSQL procedure") {
        std::string sql = "CREATE OR REPLACE FUNCTION calculate_total(amount INT) RETURNS INT LANGUAGE plpgsql AS BEGIN RETURN amount * 2; END";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();

        REQUIRE(result.find("CREATE OR REPLACE FUNCTION") != std::string::npos);
        REQUIRE(result.find("LANGUAGE plpgsql") != std::string::npos);
        REQUIRE(result.find("RETURNS INT") != std::string::npos);
    }

    SECTION("MySQL stored procedure") {
        std::string sql = "CREATE PROCEDURE update_count() AS BEGIN UPDATE counters SET value = value + 1; END";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::MySQL);
        return gen.generate(ast);
    }();

        REQUIRE(result.find("CREATE PROCEDURE") != std::string::npos);
        // SQLGenerator quotes identifiers
    REQUIRE(result.find("UPDATE") != std::string::npos);
    }

    SECTION("T-SQL procedure") {
        std::string sql = "CREATE PROCEDURE get_data() AS BEGIN SELECT * FROM data; END";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::SQLServer);
        return gen.generate(ast);
    }();

        REQUIRE(result.find("CREATE PROCEDURE") != std::string::npos);
    }

    SECTION("Oracle PL/SQL procedure") {
        std::string sql = "CREATE OR REPLACE PROCEDURE test_proc(p_id IN INT) AS BEGIN SELECT p_id; END";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::Oracle);
        return gen.generate(ast);
    }();

        INFO("Generated SQL: " << result);
        REQUIRE(result.find("CREATE OR REPLACE PROCEDURE") != std::string::npos);
        REQUIRE(result.find("IN p_id INT") != std::string::npos);
    }
}

TEST_CASE("CREATE FUNCTION with procedural logic", "[procedural][create_function]") {
    SECTION("Function with IF statement") {
        std::string sql = "CREATE FUNCTION check_value(x INT) RETURNS INT AS BEGIN IF x > 10 THEN RETURN 1; ELSE RETURN 0; END IF; END";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();

        REQUIRE(result.find("CREATE FUNCTION check_value") != std::string::npos);
        REQUIRE(result.find("IF") != std::string::npos);
        REQUIRE(result.find("THEN") != std::string::npos);
        REQUIRE(result.find("RETURN") != std::string::npos);
    }

    SECTION("Function with WHILE loop") {
        std::string sql = "CREATE FUNCTION count_up(n INT) RETURNS INT AS BEGIN DECLARE i INT; WHILE i < n DO SELECT i; END WHILE; END";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();

        REQUIRE(result.find("WHILE") != std::string::npos);
        REQUIRE(result.find("DO") != std::string::npos);
    }

    SECTION("Function with FOR loop") {
        std::string sql = "CREATE FUNCTION iterate(n INT) RETURNS INT AS BEGIN FOR i IN 1..n LOOP SELECT i; END LOOP; END";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();

        REQUIRE(result.find("FOR") != std::string::npos);
        REQUIRE(result.find("LOOP") != std::string::npos);
    }
}

TEST_CASE("CREATE PROCEDURE round-trip", "[procedural][create_procedure][roundtrip]") {
    SECTION("Parse and generate PostgreSQL procedure") {
        std::string sql = "CREATE OR REPLACE PROCEDURE test_proc(x INT) AS BEGIN SELECT x; END";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();

        // Should contain all essential elements
        REQUIRE(result.find("CREATE") != std::string::npos);
        REQUIRE(result.find("PROCEDURE") != std::string::npos);
        REQUIRE(result.find("test_proc") != std::string::npos);
        REQUIRE(result.find("BEGIN") != std::string::npos);
        REQUIRE(result.find("END") != std::string::npos);
    }

    SECTION("Parse and generate function with parameters") {
        std::string sql = "CREATE FUNCTION add(a INT, b INT) RETURNS INT AS BEGIN RETURN a + b; END";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();

        REQUIRE(result.find("FUNCTION add") != std::string::npos);
        REQUIRE(result.find("a INT") != std::string::npos);
        REQUIRE(result.find("b INT") != std::string::npos);
        REQUIRE(result.find("RETURNS INT") != std::string::npos);
    }
}
