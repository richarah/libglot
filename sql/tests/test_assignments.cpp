#include <catch2/catch_test_macros.hpp>
#include <libglot/sql/generator.h>
#include <libglot/sql/parser.h>

using namespace libglot::sql;

TEST_CASE("Assignment statement parsing", "[assignment][procedural]") {
    SECTION("Simple assignment with :=") {
        std::string sql = "x := 5";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();

        REQUIRE(result.find("x") != std::string::npos);
        REQUIRE(result.find(":=") != std::string::npos);
    }

    SECTION("Assignment with expression") {
        std::string sql = "total := price * quantity";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();

        REQUIRE(result.find("total") != std::string::npos);
        REQUIRE(result.find(":=") != std::string::npos);
    }

    SECTION("Assignment with function call") {
        std::string sql = "result := SQRT(x)";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();

        REQUIRE(result.find("result") != std::string::npos);
        REQUIRE(result.find("SQRT") != std::string::npos);
    }
}

TEST_CASE("Assignment dialect transpilation", "[assignment][dialects]") {
    SECTION("PostgreSQL assignment with :=") {
        std::string sql = "x := 10";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();

        REQUIRE(result.find("x := 10") != std::string::npos);
    }

    SECTION("PostgreSQL to MySQL transpilation") {
        std::string sql = "x := 10";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::MySQL);
        return gen.generate(ast);
    }();

        // MySQL should use SET x = 10
        REQUIRE(result.find("SET") != std::string::npos);
        REQUIRE(result.find("x") != std::string::npos);
    }

    SECTION("PostgreSQL to SQL Server transpilation") {
        std::string sql = "x := 10";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::SQLServer);
        return gen.generate(ast);
    }();

        // SQL Server should use SET x = 10
        REQUIRE(result.find("SET") != std::string::npos);
        REQUIRE(result.find("x") != std::string::npos);
    }

    SECTION("Oracle assignment with :=") {
        std::string sql = "x := 100";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::Oracle);
        return gen.generate(ast);
    }();

        REQUIRE(result.find("x := 100") != std::string::npos);
    }
}

TEST_CASE("Assignment in procedure context", "[assignment][integration]") {
    SECTION("Assignment within BEGIN...END block") {
        std::string sql = "BEGIN x := 5; y := 10; END";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();

        REQUIRE(result.find("BEGIN") != std::string::npos);
        REQUIRE(result.find("x :=") != std::string::npos);
        REQUIRE(result.find("y :=") != std::string::npos);
        REQUIRE(result.find("END") != std::string::npos);
    }
}

TEST_CASE("Assignment with complex expressions", "[assignment][complex]") {
    SECTION("Assignment with subquery") {
        std::string sql = "total := (SELECT SUM(amount) FROM orders)";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();

        REQUIRE(result.find("total") != std::string::npos);
        REQUIRE(result.find("SELECT SUM") != std::string::npos);
    }

    SECTION("Assignment with CASE expression") {
        std::string sql = "status := CASE WHEN x > 0 THEN 'positive' ELSE 'negative' END";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();

        REQUIRE(result.find("status") != std::string::npos);
        REQUIRE(result.find("CASE") != std::string::npos);
    }

    SECTION("Assignment with concatenation") {
        std::string sql = "fullname := firstname || ' ' || lastname";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();

        REQUIRE(result.find("fullname") != std::string::npos);
        REQUIRE(result.find("||") != std::string::npos);
    }
}

TEST_CASE("Assignment security tests", "[assignment][security]") {
    SECTION("Variable name validation") {
        // SQLParser validates variable names are identifiers
        std::string sql = "myvar := 123";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();

        REQUIRE(result.find("myvar") != std::string::npos);
    }

    SECTION("Assignment with NULL") {
        std::string sql = "x := NULL";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();

        REQUIRE(result.find("NULL") != std::string::npos);
    }
}
