// Wave 2: Snowflake LATERAL FLATTEN table function.
//
//   FROM t, LATERAL FLATTEN(INPUT => t.col [, PATH => '...'] [, OUTER => TRUE]) f
//
// The '=>' named-argument token (FAT_ARROW) is new lexically; FLATTEN is
// Snowflake-only at generation time, everything else throws.

#include <catch2/catch_test_macros.hpp>
#include <libglot/sql/generator.h>
#include <libglot/sql/parser.h>
#include <libglot/util/arena.h>

#include <string>

using namespace libglot::sql;

namespace {

std::string transpile(const std::string& sql, SQLDialect dialect) {
    libglot::Arena arena;
    SQLParser parser(arena, sql, dialect);
    auto* ast = parser.parse_top_level();
    SQLGenerator gen(dialect);
    return gen.generate(ast);
}

} // namespace

// Comma-separated FROM items are implicit CROSS JOINs and are always
// regenerated as an explicit "CROSS JOIN" (existing, pre-wave-2 generator
// behavior - not specific to FLATTEN).

TEST_CASE("LATERAL FLATTEN - INPUT only", "[flatten]") {
    REQUIRE(
        transpile("SELECT * FROM t, LATERAL FLATTEN(INPUT => t.col) f", SQLDialect::Snowflake) ==
        "SELECT * FROM \"t\" CROSS JOIN LATERAL FLATTEN(INPUT => \"t\".\"col\") \"f\"");
}

TEST_CASE("LATERAL FLATTEN - INPUT, PATH, OUTER", "[flatten]") {
    REQUIRE(transpile(
                "SELECT * FROM t, LATERAL FLATTEN(INPUT => t.col, PATH => 'a.b', OUTER => TRUE) f",
                SQLDialect::Snowflake) ==
            "SELECT * FROM \"t\" CROSS JOIN LATERAL FLATTEN(INPUT => \"t\".\"col\", "
            "PATH => 'a.b', OUTER => TRUE) \"f\"");
}

TEST_CASE("LATERAL FLATTEN - no alias", "[flatten]") {
    REQUIRE(transpile("SELECT * FROM t, LATERAL FLATTEN(INPUT => t.col)", SQLDialect::Snowflake) ==
            "SELECT * FROM \"t\" CROSS JOIN LATERAL FLATTEN(INPUT => \"t\".\"col\")");
}

TEST_CASE("LATERAL FLATTEN - AST shape", "[flatten]") {
    libglot::Arena arena;
    SQLParser parser(arena, "SELECT * FROM t, LATERAL FLATTEN(INPUT => t.col, PATH => 'p') f",
                     SQLDialect::Snowflake);
    auto* ast = static_cast<SelectStmt*>(parser.parse_top_level());
    REQUIRE(ast->from->type == SQLNodeKind::JOIN_CLAUSE);
    auto* join = static_cast<JoinClause*>(ast->from);
    REQUIRE(join->right_table->type == SQLNodeKind::LATERAL_JOIN);
    auto* lateral = static_cast<LateralJoin*>(join->right_table);
    REQUIRE(lateral->table_expr->type == SQLNodeKind::FLATTEN_CLAUSE);
    auto* flatten = static_cast<FlattenClause*>(lateral->table_expr);
    REQUIRE(flatten->input != nullptr);
    REQUIRE(flatten->path != nullptr);
    REQUIRE(flatten->outer == nullptr);
    REQUIRE(flatten->alias == "f");
}

TEST_CASE("LATERAL FLATTEN - fixed point (Snowflake)", "[flatten][roundtrip]") {
    const std::string queries[] = {
        "SELECT * FROM t, LATERAL FLATTEN(INPUT => t.col) f",
        "SELECT * FROM t, LATERAL FLATTEN(INPUT => t.col, PATH => 'a.b', OUTER => TRUE) f",
    };
    for (const auto& q : queries) {
        const std::string g1 = transpile(q, SQLDialect::Snowflake);
        REQUIRE(transpile(g1, SQLDialect::Snowflake) == g1);
    }
}

TEST_CASE("LATERAL FLATTEN - unsupported dialects throw a clean std::logic_error",
          "[flatten][error]") {
    for (auto d : {SQLDialect::PostgreSQL, SQLDialect::BigQuery, SQLDialect::MySQL}) {
        REQUIRE_THROWS_AS(transpile("SELECT * FROM t, LATERAL FLATTEN(INPUT => t.col) f", d),
                          std::logic_error);
    }
}

TEST_CASE("LATERAL FLATTEN - missing INPUT is a clean ParseError", "[flatten][error]") {
    libglot::Arena arena;
    SQLParser parser(arena, "SELECT * FROM t, LATERAL FLATTEN(PATH => 'a.b') f",
                     SQLDialect::Snowflake);
    REQUIRE_THROWS_AS(parser.parse_top_level(), libglot::ParseError);
}

TEST_CASE("LATERAL FLATTEN - '=>' is required, not '='", "[flatten][error]") {
    libglot::Arena arena;
    SQLParser parser(arena, "SELECT * FROM t, LATERAL FLATTEN(INPUT = t.col) f",
                     SQLDialect::Snowflake);
    REQUIRE_THROWS_AS(parser.parse_top_level(), libglot::ParseError);
}
