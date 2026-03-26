#include <catch2/catch_test_macros.hpp>
#include <libglot/sql/generator.h>
#include <libglot/sql/parser.h>

using namespace libglot::sql;

TEST_CASE("RAISE statement parsing - PostgreSQL", "[raise][plpgsql]") {
    SECTION("RAISE EXCEPTION") {
        std::string sql = "RAISE EXCEPTION 'Division by zero'";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();

        REQUIRE(result.find("RAISE") != std::string::npos);
        REQUIRE(result.find("EXCEPTION") != std::string::npos);
    }

    SECTION("RAISE NOTICE") {
        std::string sql = "RAISE NOTICE 'Debug message'";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();

        REQUIRE(result.find("RAISE") != std::string::npos);
        REQUIRE(result.find("NOTICE") != std::string::npos);
    }

    SECTION("RAISE WARNING") {
        std::string sql = "RAISE WARNING 'Warning message'";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();

        REQUIRE(result.find("WARNING") != std::string::npos);
    }

    SECTION("RAISE INFO") {
        std::string sql = "RAISE INFO 'Information message'";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();

        REQUIRE(result.find("INFO") != std::string::npos);
    }

    SECTION("RAISE LOG") {
        std::string sql = "RAISE LOG 'Log message'";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();

        REQUIRE(result.find("LOG") != std::string::npos);
    }

    SECTION("RAISE DEBUG") {
        std::string sql = "RAISE DEBUG 'Debug message'";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();

        REQUIRE(result.find("DEBUG") != std::string::npos);
    }
}

TEST_CASE("SIGNAL statement parsing - MySQL", "[signal][mysql]") {
    SECTION("SIGNAL SQLSTATE") {
        std::string sql = "SIGNAL SQLSTATE '45000'";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::MySQL);
        return gen.generate(ast);
    }();

        REQUIRE(result.find("SIGNAL") != std::string::npos);
        REQUIRE(result.find("SQLSTATE") != std::string::npos);
    }

    SECTION("SIGNAL with MESSAGE_TEXT") {
        std::string sql = "SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Custom error'";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::MySQL);
        return gen.generate(ast);
    }();

        REQUIRE(result.find("SIGNAL") != std::string::npos);
        REQUIRE(result.find("MESSAGE_TEXT") != std::string::npos);
    }
}

TEST_CASE("RAISE in exception handler", "[raise][integration]") {
    SECTION("RAISE in EXCEPTION block") {
        std::string sql = "BEGIN SELECT 1; EXCEPTION WHEN others THEN RAISE EXCEPTION 'Error occurred'; END";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();

        REQUIRE(result.find("EXCEPTION") != std::string::npos);
        REQUIRE(result.find("RAISE") != std::string::npos);
    }
}

TEST_CASE("RAISE in stored procedure", "[raise][integration]") {
    SECTION("Function with RAISE") {
        std::string sql = "CREATE FUNCTION validate_age(age INT) RETURNS VOID AS BEGIN IF age < 0 THEN RAISE EXCEPTION 'Age cannot be negative'; END IF; END";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();

        REQUIRE(result.find("CREATE FUNCTION") != std::string::npos);
        REQUIRE(result.find("RAISE") != std::string::npos);
        REQUIRE(result.find("EXCEPTION") != std::string::npos);
    }
}

TEST_CASE("RAISE dialect transpilation", "[raise][dialects]") {
    SECTION("PostgreSQL RAISE to MySQL SIGNAL") {
        std::string sql = "RAISE EXCEPTION 'Error'";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::MySQL);
        return gen.generate(ast);
    }();

        // MySQL should convert to SIGNAL
        REQUIRE(result.find("SIGNAL") != std::string::npos);
    }

    SECTION("MySQL SIGNAL to PostgreSQL RAISE") {
        std::string sql = "SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Error'";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();

        // PostgreSQL should convert to RAISE
        REQUIRE(result.find("RAISE") != std::string::npos);
    }
}

TEST_CASE("RAISE security tests", "[raise][security]") {
    SECTION("RAISE with safe message") {
        std::string sql = "RAISE EXCEPTION 'Safe error message'";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();

        REQUIRE(result.find("RAISE") != std::string::npos);
    }

    SECTION("SIGNAL with valid SQLSTATE") {
        std::string sql = "SIGNAL SQLSTATE '23505'";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::MySQL);
        return gen.generate(ast);
    }();

        REQUIRE(result.find("23505") != std::string::npos);
    }
}

TEST_CASE("RAISE round-trip", "[raise][roundtrip]") {
    SECTION("PostgreSQL RAISE EXCEPTION round-trip") {
        std::string sql = "RAISE EXCEPTION 'error'";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();

        REQUIRE(result.find("RAISE EXCEPTION") != std::string::npos);
        REQUIRE(result.find("error") != std::string::npos);
    }

    SECTION("MySQL SIGNAL round-trip") {
        std::string sql = "SIGNAL SQLSTATE '45000'";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::MySQL);
        return gen.generate(ast);
    }();

        REQUIRE(result.find("SIGNAL SQLSTATE") != std::string::npos);
    }
}

TEST_CASE("Common error conditions", "[raise][integration]") {
    SECTION("Division by zero error") {
        std::string sql = "RAISE EXCEPTION 'division_by_zero'";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();

        REQUIRE(result.find("division_by_zero") != std::string::npos);
    }

    SECTION("Unique violation error") {
        std::string sql = "SIGNAL SQLSTATE '23505' SET MESSAGE_TEXT = 'Duplicate key'";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::MySQL);
        return gen.generate(ast);
    }();

        REQUIRE(result.find("23505") != std::string::npos);
        REQUIRE(result.find("Duplicate key") != std::string::npos);
    }
}
