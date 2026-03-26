#include <catch2/catch_test_macros.hpp>
#include <libglot/sql/parser.h>
#include <libglot/sql/generator.h>
#include <libglot/sql/parser.h>
#include <libglot/sql/generator.h>

using namespace libglot::sql;

TEST_CASE("FOR loop to WHILE loop transpilation for T-SQL", "[procedural][for_while][tsql]") {
    SECTION("Simple FOR loop transpiles to WHILE for T-SQL") {
        std::string sql = "FOR i IN 1..10 LOOP SELECT i; END LOOP";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::SQLServer);
        return gen.generate(ast);
    }();

        // T-SQL doesn't support FOR loops, should transpile to WHILE
        REQUIRE(result.find("DECLARE @i INT = 1") != std::string::npos);
        REQUIRE(result.find("WHILE @i <= 10") != std::string::npos);
        REQUIRE(result.find("BEGIN") != std::string::npos);
        // Different dialects handle variables differently
    REQUIRE((result.find("SELECT") != std::string::npos));
        REQUIRE(result.find("SET @i = @i + 1") != std::string::npos);
        REQUIRE(result.find("END") != std::string::npos);
    }

    SECTION("FOR loop with expression as end value") {
        std::string sql = "FOR counter IN 0..100 LOOP SELECT counter; END LOOP";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::SQLServer);
        return gen.generate(ast);
    }();

        REQUIRE(result.find("DECLARE @counter INT = 0") != std::string::npos);
        REQUIRE(result.find("WHILE @counter <= 100") != std::string::npos);
        REQUIRE(result.find("SET @counter = @counter + 1") != std::string::npos);
    }

    SECTION("Nested FOR loops transpile to nested WHILE loops") {
        std::string sql = "FOR i IN 1..3 LOOP FOR j IN 1..3 LOOP SELECT i; END LOOP; END LOOP";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::SQLServer);
        return gen.generate(ast);
    }();

        // Should have two DECLARE statements
        REQUIRE(result.find("DECLARE @i") != std::string::npos);
        REQUIRE(result.find("DECLARE @j") != std::string::npos);

        // Should have two WHILE statements
        size_t count = 0;
        size_t pos = 0;
        while ((pos = result.find("WHILE", pos)) != std::string::npos) {
            count++;
            pos++;
        }
        REQUIRE(count >= 2);
    }
}

TEST_CASE("FOR loop preserved for PostgreSQL", "[procedural][for_loop][postgresql]") {
    SECTION("PostgreSQL preserves FOR loop syntax") {
        std::string sql = "FOR i IN 1..10 LOOP SELECT i; END LOOP";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();

        // PostgreSQL supports FOR loops natively
        REQUIRE(result.find("FOR i IN") != std::string::npos);
        REQUIRE(result.find("LOOP") != std::string::npos);
        REQUIRE(result.find("END LOOP") != std::string::npos);

        // Should NOT have WHILE or DECLARE for transpilation
        REQUIRE(result.find("WHILE") == std::string::npos);
        REQUIRE(result.find("DECLARE") == std::string::npos);
    }
}

TEST_CASE("FOR loop preserved for MySQL", "[procedural][for_loop][mysql]") {
    SECTION("MySQL preserves FOR loop syntax") {
        std::string sql = "FOR i IN 1..10 LOOP SELECT i; END LOOP";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::MySQL);
        return gen.generate(ast);
    }();

        // MySQL supports FOR loops
        REQUIRE(result.find("FOR") != std::string::npos);
        REQUIRE(result.find("LOOP") != std::string::npos);
        REQUIRE(result.find("END LOOP") != std::string::npos);
    }
}

TEST_CASE("FOR loop preserved for Oracle", "[procedural][for_loop][oracle]") {
    SECTION("Oracle preserves FOR loop syntax") {
        std::string sql = "FOR i IN 1..10 LOOP SELECT i FROM DUAL; END LOOP";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::Oracle);
        return gen.generate(ast);
    }();

        // Oracle supports FOR loops
        REQUIRE(result.find("FOR i IN") != std::string::npos);
        REQUIRE(result.find("LOOP") != std::string::npos);
        REQUIRE(result.find("END LOOP") != std::string::npos);
    }
}

TEST_CASE("Cross-dialect FOR loop transpilation", "[procedural][for_loop][transpile]") {
    SECTION("PostgreSQL FOR to MySQL FOR") {
        std::string sql = "FOR idx IN 5..15 LOOP SELECT idx; END LOOP";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::MySQL);
        return gen.generate(ast);
    }();

        // MySQL supports FOR, should preserve
        REQUIRE(result.find("FOR") != std::string::npos);
        REQUIRE(result.find("LOOP") != std::string::npos);
    }

    SECTION("Oracle FOR to T-SQL WHILE") {
        std::string sql = "FOR x IN 1..100 LOOP SELECT x FROM DUAL; END LOOP";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::SQLServer);
        return gen.generate(ast);
    }();

        // T-SQL doesn't support FOR, should transpile to WHILE
        REQUIRE(result.find("DECLARE @x INT") != std::string::npos);
        REQUIRE(result.find("WHILE @x") != std::string::npos);
        REQUIRE(result.find("SET @x") != std::string::npos);
    }

    SECTION("MySQL FOR to Oracle FOR") {
        std::string sql = "FOR counter IN 1..50 LOOP SELECT counter; END LOOP";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::Oracle);
        return gen.generate(ast);
    }();

        // Both support FOR loops
        REQUIRE(result.find("FOR") != std::string::npos);
        REQUIRE(result.find("LOOP") != std::string::npos);
    }
}
