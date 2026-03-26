#include <catch2/catch_test_macros.hpp>
#include <libglot/sql/parser.h>
#include <libglot/util/arena.h>

using namespace libglot::sql;

TEST_CASE("UNBOUNDED FOLLOWING frame bound", "[parser][window]") {
    SECTION("ROWS BETWEEN UNBOUNDED PRECEDING AND UNBOUNDED FOLLOWING") {
        const char* sql = R"(
              SELECT
                SUM(amount) OVER (
                  ORDER BY created_at
                  ROWS BETWEEN UNBOUNDED PRECEDING AND UNBOUNDED FOLLOWING
                ) AS total
              FROM transactions
        )";
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        REQUIRE_NOTHROW(parser.parse_top_level());
    }

    SECTION("RANGE BETWEEN UNBOUNDED PRECEDING AND UNBOUNDED FOLLOWING") {
        const char* sql = R"(
              SELECT
                AVG(score) OVER (
                  PARTITION BY team_id
                  ORDER BY match_day
                  RANGE BETWEEN UNBOUNDED PRECEDING AND UNBOUNDED FOLLOWING
                ) AS season_avg
              FROM games
        )";
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        REQUIRE_NOTHROW(parser.parse_top_level());
    }
}
