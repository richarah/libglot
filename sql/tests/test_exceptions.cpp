#include <catch2/catch_test_macros.hpp>
#include <libglot/sql/generator.h>
#include <libglot/sql/parser.h>
#include <iostream>

using namespace libglot::sql;

TEST_CASE("EXCEPTION block parsing", "[exception][plpgsql]") {
    SECTION("Simple EXCEPTION block") {
        std::string sql = "BEGIN SELECT 1; EXCEPTION WHEN division_by_zero THEN SELECT 0; END";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();

        REQUIRE(result.find("BEGIN") != std::string::npos);
        REQUIRE(result.find("EXCEPTION") != std::string::npos);
        REQUIRE(result.find("WHEN") != std::string::npos);
        REQUIRE(result.find("division_by_zero") != std::string::npos);
        REQUIRE(result.find("END") != std::string::npos);
    }

    SECTION("EXCEPTION block with multiple handlers") {
        std::string sql = "BEGIN SELECT 1/0; EXCEPTION WHEN division_by_zero THEN SELECT 0; WHEN others THEN SELECT -1; END";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();

        REQUIRE(result.find("EXCEPTION") != std::string::npos);
        REQUIRE(result.find("division_by_zero") != std::string::npos);
        REQUIRE(result.find("others") != std::string::npos);
    }

    SECTION("EXCEPTION block with no exceptions raised") {
        std::string sql = "BEGIN SELECT 1; SELECT 2; EXCEPTION WHEN no_data_found THEN SELECT NULL; END";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();

        REQUIRE(result.find("BEGIN") != std::string::npos);
        REQUIRE(result.find("EXCEPTION") != std::string::npos);
        REQUIRE(result.find("no_data_found") != std::string::npos);
    }
}

TEST_CASE("EXCEPTION handler with multiple statements", "[exception][plpgsql]") {
    SECTION("Multiple statements in exception handler") {
        std::string sql = "BEGIN SELECT 1; EXCEPTION WHEN others THEN SELECT 0; SELECT -1; SELECT -2; END";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();

        REQUIRE(result.find("EXCEPTION") != std::string::npos);
        REQUIRE(result.find("WHEN others THEN") != std::string::npos);
    }
}

TEST_CASE("Common PostgreSQL exception names", "[exception][plpgsql]") {
    SECTION("division_by_zero exception") {
        std::string sql = "BEGIN SELECT 1/0; EXCEPTION WHEN division_by_zero THEN SELECT 0; END";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();

        REQUIRE(result.find("division_by_zero") != std::string::npos);
    }

    SECTION("no_data_found exception") {
        std::string sql = "BEGIN SELECT * FROM empty_table; EXCEPTION WHEN no_data_found THEN SELECT NULL; END";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();

        REQUIRE(result.find("no_data_found") != std::string::npos);
    }

    SECTION("unique_violation exception") {
        std::string sql = "BEGIN INSERT INTO users VALUES (1); EXCEPTION WHEN unique_violation THEN SELECT 'exists'; END";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();

        REQUIRE(result.find("unique_violation") != std::string::npos);
    }

    SECTION("others catch-all exception") {
        std::string sql = "BEGIN SELECT 1; EXCEPTION WHEN others THEN SELECT 'error'; END";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();

        REQUIRE(result.find("others") != std::string::npos);
    }
}

TEST_CASE("EXCEPTION block integration", "[exception][integration]") {
    SECTION("EXCEPTION in stored procedure") {
        std::string sql = "CREATE FUNCTION safe_divide(a INT, b INT) RETURNS INT AS BEGIN RETURN a / b; EXCEPTION WHEN division_by_zero THEN RETURN 0; END";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();

        REQUIRE(result.find("CREATE FUNCTION") != std::string::npos);
        REQUIRE(result.find("EXCEPTION") != std::string::npos);
        REQUIRE(result.find("division_by_zero") != std::string::npos);
    }
}

TEST_CASE("EXCEPTION block round-trip", "[exception][roundtrip]") {
    SECTION("Parse and regenerate simple exception block") {
        std::string sql = "BEGIN SELECT 1; EXCEPTION WHEN others THEN SELECT 0; END";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();

        REQUIRE(result.find("BEGIN") != std::string::npos);
        REQUIRE(result.find("EXCEPTION") != std::string::npos);
        REQUIRE(result.find("WHEN others") != std::string::npos);
        REQUIRE(result.find("END") != std::string::npos);
    }
}

TEST_CASE("EXCEPTION block security", "[exception][security]") {
    SECTION("Exception handler with safe exception names") {
        std::string sql = "BEGIN SELECT 1; EXCEPTION WHEN my_exception THEN SELECT 0; END";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();

        REQUIRE(result.find("my_exception") != std::string::npos);
    }
}
