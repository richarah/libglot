#include <catch2/catch_test_macros.hpp>
#include <libglot/sql/parser.h>
#include <libglot/util/arena.h>

using namespace libglot::sql;

TEST_CASE("EXTRACT function parsing", "[parser][extract]") {
    SECTION("EXTRACT YEAR") {
        libglot::Arena arena;
        SQLParser parser(arena, "SELECT EXTRACT(YEAR FROM order_date) FROM orders");
        REQUIRE_NOTHROW(parser.parse_top_level());
    }

    SECTION("EXTRACT MONTH") {
        libglot::Arena arena;
        SQLParser parser(arena, "SELECT EXTRACT(MONTH FROM order_date) FROM orders");
        REQUIRE_NOTHROW(parser.parse_top_level());
    }

    SECTION("EXTRACT DAY") {
        libglot::Arena arena;
        SQLParser parser(arena, "SELECT EXTRACT(DAY FROM order_date) FROM orders");
        REQUIRE_NOTHROW(parser.parse_top_level());
    }

    SECTION("EXTRACT in CAST") {
        libglot::Arena arena;
        SQLParser parser(arena, "SELECT CAST(EXTRACT(YEAR FROM order_date) AS VARCHAR(10)) FROM orders");
        REQUIRE_NOTHROW(parser.parse_top_level());
    }

    SECTION("Multiple EXTRACT calls") {
        libglot::Arena arena;
        SQLParser parser(arena, "SELECT EXTRACT(YEAR FROM order_date), EXTRACT(MONTH FROM order_date) FROM orders");
        REQUIRE_NOTHROW(parser.parse_top_level());
    }

    SECTION("EXTRACT in WHERE clause") {
        libglot::Arena arena;
        SQLParser parser(arena, "SELECT * FROM orders WHERE EXTRACT(YEAR FROM order_date) = 2024");
        REQUIRE_NOTHROW(parser.parse_top_level());
    }
}
