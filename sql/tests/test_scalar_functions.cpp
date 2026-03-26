#include <catch2/catch_test_macros.hpp>
#include <libglot/sql/parser.h>
#include <libglot/sql/generator.h>
#include <libglot/sql/dialect_traits.h>
#include <libglot/util/arena.h>

using namespace libglot::sql;

TEST_CASE("String scalar functions", "[scalar][functions][string]") {
    SECTION("SUBSTRING function") {
        libglot::Arena arena;
        SQLParser parser(arena, "SELECT SUBSTRING(name, 1, 10) FROM users");
        auto stmt = parser.parse_top_level();

        REQUIRE(stmt != nullptr);
        REQUIRE(stmt->type == SQLNodeKind::SELECT_STMT);
    }

    SECTION("SUBSTR function") {
        libglot::Arena arena;
        SQLParser parser(arena, "SELECT SUBSTR(name, 1, 10) FROM users");
        auto stmt = parser.parse_top_level();

        REQUIRE(stmt != nullptr);
    }

    SECTION("LENGTH function") {
        libglot::Arena arena;
        SQLParser parser(arena, "SELECT LENGTH(name) FROM users");
        auto stmt = parser.parse_top_level();

        REQUIRE(stmt != nullptr);
    }

    SECTION("TRIM function") {
        libglot::Arena arena;
        SQLParser parser(arena, "SELECT TRIM(name) FROM users");
        auto stmt = parser.parse_top_level();

        REQUIRE(stmt != nullptr);
    }

    SECTION("UPPER function") {
        libglot::Arena arena;
        SQLParser parser(arena, "SELECT UPPER(name) FROM users");
        auto stmt = parser.parse_top_level();

        REQUIRE(stmt != nullptr);
    }

    SECTION("LOWER function") {
        libglot::Arena arena;
        SQLParser parser(arena, "SELECT LOWER(name) FROM users");
        auto stmt = parser.parse_top_level();

        REQUIRE(stmt != nullptr);
    }

    SECTION("REPLACE function") {
        libglot::Arena arena;
        SQLParser parser(arena, "SELECT REPLACE(name, 'old', 'new') FROM users");
        auto stmt = parser.parse_top_level();

        REQUIRE(stmt != nullptr);
    }

    SECTION("CONCAT function") {
        libglot::Arena arena;
        SQLParser parser(arena, "SELECT CONCAT(first_name, ' ', last_name) FROM users");
        auto stmt = parser.parse_top_level();

        REQUIRE(stmt != nullptr);
    }
}

TEST_CASE("Math scalar functions", "[scalar][functions][math]") {
    SECTION("ROUND function") {
        libglot::Arena arena;
        SQLParser parser(arena, "SELECT ROUND(price, 2) FROM products");
        auto stmt = parser.parse_top_level();

        REQUIRE(stmt != nullptr);
    }

    SECTION("FLOOR function") {
        libglot::Arena arena;
        SQLParser parser(arena, "SELECT FLOOR(price) FROM products");
        auto stmt = parser.parse_top_level();

        REQUIRE(stmt != nullptr);
    }

    SECTION("CEIL function") {
        libglot::Arena arena;
        SQLParser parser(arena, "SELECT CEIL(price) FROM products");
        auto stmt = parser.parse_top_level();

        REQUIRE(stmt != nullptr);
    }

    SECTION("ABS function") {
        libglot::Arena arena;
        SQLParser parser(arena, "SELECT ABS(balance) FROM accounts");
        auto stmt = parser.parse_top_level();

        REQUIRE(stmt != nullptr);
    }

    SECTION("POWER function") {
        libglot::Arena arena;
        SQLParser parser(arena, "SELECT POWER(2, 10) FROM dual");
        auto stmt = parser.parse_top_level();

        REQUIRE(stmt != nullptr);
    }

    SECTION("SQRT function") {
        libglot::Arena arena;
        SQLParser parser(arena, "SELECT SQRT(area) FROM shapes");
        auto stmt = parser.parse_top_level();

        REQUIRE(stmt != nullptr);
    }
}

TEST_CASE("Nested and complex scalar function usage", "[scalar][functions][complex]") {
    SECTION("Nested string functions") {
        libglot::Arena arena;
        SQLParser parser(arena, "SELECT UPPER(TRIM(SUBSTRING(name, 1, 10))) FROM users");
        auto stmt = parser.parse_top_level();

        REQUIRE(stmt != nullptr);
    }

    SECTION("String function in WHERE clause") {
        libglot::Arena arena;
        SQLParser parser(arena, "SELECT * FROM users WHERE LENGTH(name) > 10");
        auto stmt = parser.parse_top_level();

        REQUIRE(stmt != nullptr);
    }

    SECTION("Math function with expression") {
        libglot::Arena arena;
        SQLParser parser(arena, "SELECT ROUND(price * quantity, 2) FROM order_items");
        auto stmt = parser.parse_top_level();

        REQUIRE(stmt != nullptr);
    }

    SECTION("Function in ORDER BY") {
        libglot::Arena arena;
        SQLParser parser(arena, "SELECT name FROM users ORDER BY LOWER(name)");
        auto stmt = parser.parse_top_level();

        REQUIRE(stmt != nullptr);
    }

    SECTION("Function in GROUP BY") {
        libglot::Arena arena;
        SQLParser parser(arena, "SELECT UPPER(category), COUNT(*) FROM products GROUP BY UPPER(category)");
        auto stmt = parser.parse_top_level();

        REQUIRE(stmt != nullptr);
    }
}
