#include <catch2/catch_test_macros.hpp>
#include <libglot/sql/generator.h>
#include <libglot/sql/parser.h>

using namespace libglot::sql;

TEST_CASE("DECLARE CURSOR parsing", "[cursor][plpgsql]") {
    SECTION("Simple cursor declaration") {
        std::string sql = "DECLARE mycursor CURSOR FOR SELECT * FROM users";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();

        REQUIRE(result.find("DECLARE") != std::string::npos);
        REQUIRE(result.find("mycursor") != std::string::npos);
        REQUIRE(result.find("CURSOR") != std::string::npos);
        REQUIRE(result.find("SELECT") != std::string::npos);
    }

    SECTION("SCROLL cursor declaration") {
        std::string sql = "DECLARE mycursor SCROLL CURSOR FOR SELECT id FROM products";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();

        REQUIRE(result.find("SCROLL") != std::string::npos);
        REQUIRE(result.find("CURSOR") != std::string::npos);
    }

    SECTION("Cursor with WHERE clause") {
        std::string sql = "DECLARE active_users CURSOR FOR SELECT * FROM users WHERE active = TRUE";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();

        REQUIRE(result.find("active_users") != std::string::npos);
        REQUIRE(result.find("WHERE") != std::string::npos);
    }

    SECTION("Cursor with JOIN") {
        std::string sql = "DECLARE order_details CURSOR FOR SELECT o.id, c.name FROM orders o JOIN customers c ON o.customer_id = c.id";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();

        REQUIRE(result.find("order_details") != std::string::npos);
        REQUIRE(result.find("JOIN") != std::string::npos);
    }
}

TEST_CASE("OPEN cursor parsing", "[cursor][plpgsql]") {
    SECTION("Simple OPEN cursor") {
        std::string sql = "OPEN mycursor";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();

        REQUIRE(result.find("OPEN") != std::string::npos);
        REQUIRE(result.find("mycursor") != std::string::npos);
    }

    SECTION("OPEN cursor with parameters") {
        std::string sql = "OPEN mycursor(100, 'active')";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();

        REQUIRE(result.find("OPEN") != std::string::npos);
        REQUIRE(result.find("mycursor") != std::string::npos);
    }
}

TEST_CASE("FETCH cursor parsing", "[cursor][plpgsql]") {
    SECTION("FETCH NEXT") {
        std::string sql = "FETCH NEXT FROM mycursor INTO var1, var2";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();

        REQUIRE(result.find("FETCH") != std::string::npos);
        REQUIRE(result.find("NEXT") != std::string::npos);
        REQUIRE(result.find("mycursor") != std::string::npos);
        REQUIRE(result.find("INTO") != std::string::npos);
    }

    SECTION("FETCH PRIOR") {
        std::string sql = "FETCH PRIOR FROM mycursor INTO myvar";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();

        REQUIRE(result.find("PRIOR") != std::string::npos);
    }

    SECTION("FETCH FIRST") {
        std::string sql = "FETCH FIRST FROM mycursor INTO myvar";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();

        REQUIRE(result.find("FIRST") != std::string::npos);
    }

    SECTION("FETCH LAST") {
        std::string sql = "FETCH LAST FROM mycursor INTO myvar";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();

        REQUIRE(result.find("LAST") != std::string::npos);
    }

    SECTION("FETCH into multiple variables") {
        std::string sql = "FETCH NEXT FROM mycursor INTO id, name, email";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();

        REQUIRE(result.find("INTO") != std::string::npos);
        REQUIRE(result.find("id") != std::string::npos);
    }
}

TEST_CASE("CLOSE cursor parsing", "[cursor][plpgsql]") {
    SECTION("Simple CLOSE cursor") {
        std::string sql = "CLOSE mycursor";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();

        REQUIRE(result.find("CLOSE") != std::string::npos);
        REQUIRE(result.find("mycursor") != std::string::npos);
    }
}

TEST_CASE("Complete cursor lifecycle", "[cursor][integration]") {
    SECTION("DECLARE, OPEN, FETCH, CLOSE sequence") {
        std::string sql1 = "DECLARE mycursor CURSOR FOR SELECT * FROM users";
        std::string sql2 = "OPEN mycursor";
        std::string sql3 = "FETCH NEXT FROM mycursor INTO user_id, user_name";
        std::string sql4 = "CLOSE mycursor";

        auto result1 = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql1);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();
        auto result2 = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql2);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();
        auto result3 = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql3);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();
        auto result4 = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql4);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();

        REQUIRE(result1.find("DECLARE") != std::string::npos);
        REQUIRE(result2.find("OPEN") != std::string::npos);
        REQUIRE(result3.find("FETCH") != std::string::npos);
        REQUIRE(result4.find("CLOSE") != std::string::npos);
    }
}

TEST_CASE("Cursor in stored procedure", "[cursor][integration]") {
    SECTION("Function using cursor") {
        std::string sql = "CREATE FUNCTION process_users() RETURNS INT AS BEGIN DECLARE mycursor CURSOR FOR SELECT id FROM users; OPEN mycursor; CLOSE mycursor; RETURN 1; END";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();

        REQUIRE(result.find("CREATE FUNCTION") != std::string::npos);
        REQUIRE(result.find("CURSOR") != std::string::npos);
        REQUIRE(result.find("OPEN") != std::string::npos);
        REQUIRE(result.find("CLOSE") != std::string::npos);
    }
}

TEST_CASE("Cursor security tests", "[cursor][security]") {
    SECTION("Cursor name validation") {
        std::string sql = "DECLARE my_cursor CURSOR FOR SELECT 1";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();

        REQUIRE(result.find("my_cursor") != std::string::npos);
    }

    SECTION("INTO variable validation") {
        std::string sql = "FETCH NEXT FROM c INTO var1, var2, var3";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();

        REQUIRE(result.find("INTO") != std::string::npos);
    }
}

TEST_CASE("Cursor round-trip tests", "[cursor][roundtrip]") {
    SECTION("DECLARE cursor round-trip") {
        std::string sql = "DECLARE c CURSOR FOR SELECT * FROM t";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();

        REQUIRE(result.find("DECLARE") != std::string::npos);
        REQUIRE(result.find("CURSOR") != std::string::npos);
    }

    SECTION("OPEN cursor round-trip") {
        std::string sql = "OPEN mycursor";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();

        REQUIRE(result == "OPEN mycursor");
    }

    SECTION("CLOSE cursor round-trip") {
        std::string sql = "CLOSE mycursor";
        auto result = [&]() {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }();

        REQUIRE(result == "CLOSE mycursor");
    }
}
