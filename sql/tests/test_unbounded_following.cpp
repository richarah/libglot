#include <catch2/catch_test_macros.hpp>
#include <libglot/sql/generator.h>
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

// ============================================================================
// Window frame regeneration: the generator must emit the actual parsed
// frame, not a hardcoded BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW.
// ============================================================================

TEST_CASE("Window frame regeneration", "[generator][window]") {
    SECTION("ROWS 3 PRECEDING (short form, exact roundtrip)") {
        const char* sql = "SELECT SUM(x) OVER (ORDER BY d ROWS 3 PRECEDING) FROM t";
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto* stmt = parser.parse_top_level();

        SQLGenerator gen(SQLDialect::ANSI);
        REQUIRE(gen.generate(stmt) ==
                "SELECT SUM(\"x\") OVER (ORDER BY \"d\" ROWS 3 PRECEDING) FROM \"t\"");
    }

    SECTION("RANGE BETWEEN UNBOUNDED PRECEDING AND UNBOUNDED FOLLOWING (exact roundtrip)") {
        const char* sql = "SELECT AVG(x) OVER (ORDER BY d RANGE BETWEEN UNBOUNDED PRECEDING AND "
                          "UNBOUNDED FOLLOWING) FROM t";
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto* stmt = parser.parse_top_level();

        SQLGenerator gen(SQLDialect::ANSI);
        REQUIRE(gen.generate(stmt) ==
                "SELECT AVG(\"x\") OVER (ORDER BY \"d\" "
                "RANGE BETWEEN UNBOUNDED PRECEDING AND UNBOUNDED FOLLOWING) FROM \"t\"");
    }

    SECTION("ROWS BETWEEN 2 PRECEDING AND 3 FOLLOWING (offset bounds)") {
        const char* sql = "SELECT SUM(x) OVER (ROWS BETWEEN 2 PRECEDING AND 3 FOLLOWING) FROM t";
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto* stmt = parser.parse_top_level();

        SQLGenerator gen(SQLDialect::ANSI);
        REQUIRE(gen.generate(stmt) ==
                "SELECT SUM(\"x\") OVER (ROWS BETWEEN 2 PRECEDING AND 3 FOLLOWING) FROM \"t\"");
    }

    SECTION("Parsed frame AST carries the real bounds") {
        const char* sql = "SELECT SUM(x) OVER (ROWS 3 PRECEDING) FROM t";
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto* stmt = static_cast<SelectStmt*>(parser.parse_top_level());

        REQUIRE(stmt->columns[0]->type == SQLNodeKind::WINDOW_FUNCTION);
        auto* wf = static_cast<WindowFunction*>(stmt->columns[0]);
        REQUIRE(wf->over != nullptr);
        REQUIRE(wf->over->frame != nullptr);
        REQUIRE(wf->over->frame->frame_type == FrameType::ROWS);
        REQUIRE(wf->over->frame->between_form == false);
        REQUIRE(wf->over->frame->start_bound == FrameBound::PRECEDING);
        REQUIRE(wf->over->frame->start_offset != nullptr);
        REQUIRE(static_cast<Literal*>(wf->over->frame->start_offset)->value == "3");
    }
}
