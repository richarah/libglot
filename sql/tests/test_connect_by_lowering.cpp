// Lowering Oracle START WITH / CONNECT BY hierarchical queries into an
// equivalent WITH RECURSIVE CTE for dialects with no native CONNECT BY
// syntax (see sql/include/libglot/sql/transforms.h and the dispatch at the
// top of SQLGenerator::visit_select_stmt in generator.h).
//
// Native Oracle/Snowflake CONNECT BY generation is covered by
// test_connect_by.cpp and is untouched by this feature.

#include <catch2/catch_test_macros.hpp>
#include <libglot/sql/generator.h>
#include <libglot/sql/parser.h>
#include <libglot/util/arena.h>

#include <stdexcept>
#include <string>

using namespace libglot::sql;

namespace {

/// Parse `sql` as `parse_dialect` and generate it as `gen_dialect`, lowering
/// CONNECT BY when the target dialect doesn't understand it natively.
std::string lower(const std::string& sql, SQLDialect parse_dialect = SQLDialect::Oracle,
                  SQLDialect gen_dialect = SQLDialect::PostgreSQL) {
    libglot::Arena arena;
    SQLParser parser(arena, sql, parse_dialect);
    auto ast = parser.parse_top_level();

    libglot::Arena transform_arena;
    SQLGenerator gen(gen_dialect, &transform_arena);
    return gen.generate(ast);
}

} // namespace

// ============================================================================
// Canonical employees/manager_id hierarchy - without LEVEL
// ============================================================================

TEST_CASE("CONNECT BY lowering - canonical hierarchy without LEVEL", "[connect-by][lowering]") {
    REQUIRE(lower("SELECT employee_id, manager_id FROM employees "
                  "START WITH manager_id IS NULL "
                  "CONNECT BY PRIOR employee_id = manager_id") ==
            "WITH RECURSIVE \"hierarchy\" AS ("
            "SELECT \"employees\".* FROM \"employees\" WHERE \"manager_id\" IS NULL "
            "UNION ALL "
            "SELECT \"employees\".* FROM \"employees\" INNER JOIN \"hierarchy\" "
            "ON \"hierarchy\".\"employee_id\" = \"employees\".\"manager_id\""
            ") SELECT \"employee_id\", \"manager_id\" FROM \"hierarchy\"");
}

// ============================================================================
// Canonical employees/manager_id hierarchy - with LEVEL
// ============================================================================

TEST_CASE("CONNECT BY lowering - canonical hierarchy with LEVEL", "[connect-by][lowering][level]") {
    REQUIRE(lower("SELECT employee_id, LEVEL FROM employees "
                  "START WITH manager_id IS NULL "
                  "CONNECT BY PRIOR employee_id = manager_id") ==
            "WITH RECURSIVE \"hierarchy\" AS ("
            "SELECT \"employees\".*, 1 AS \"level\" FROM \"employees\" WHERE \"manager_id\" IS NULL "
            "UNION ALL "
            "SELECT \"employees\".*, \"hierarchy\".\"level\" + 1 FROM \"employees\" "
            "INNER JOIN \"hierarchy\" ON \"hierarchy\".\"employee_id\" = \"employees\".\"manager_id\""
            ") SELECT \"employee_id\", \"level\" FROM \"hierarchy\"");
}

// ============================================================================
// PRIOR placement and compound conditions
// ============================================================================

TEST_CASE("CONNECT BY lowering - PRIOR on the right side", "[connect-by][lowering][prior]") {
    // c reversed: `child = PRIOR parent` instead of `PRIOR child = parent`.
    REQUIRE(lower("SELECT employee_id FROM employees "
                  "START WITH manager_id IS NULL "
                  "CONNECT BY employee_id = PRIOR manager_id") ==
            "WITH RECURSIVE \"hierarchy\" AS ("
            "SELECT \"employees\".* FROM \"employees\" WHERE \"manager_id\" IS NULL "
            "UNION ALL "
            "SELECT \"employees\".* FROM \"employees\" INNER JOIN \"hierarchy\" "
            "ON \"employees\".\"employee_id\" = \"hierarchy\".\"manager_id\""
            ") SELECT \"employee_id\" FROM \"hierarchy\"");
}

TEST_CASE("CONNECT BY lowering - compound condition (AND of two comparisons)",
         "[connect-by][lowering][prior]") {
    // Only one side of the AND has a PRIOR; the other is a plain
    // child-row comparison and should be qualified with the source alias.
    REQUIRE(lower("SELECT employee_id FROM employees "
                  "CONNECT BY PRIOR employee_id = manager_id AND status = 'active'") ==
            "WITH RECURSIVE \"hierarchy\" AS ("
            "SELECT \"employees\".* FROM \"employees\" "
            "UNION ALL "
            "SELECT \"employees\".* FROM \"employees\" INNER JOIN \"hierarchy\" "
            "ON \"hierarchy\".\"employee_id\" = \"employees\".\"manager_id\" "
            "AND \"employees\".\"status\" = 'active'"
            ") SELECT \"employee_id\" FROM \"hierarchy\"");
}

// ============================================================================
// WHERE placement: applied on the outer SELECT, not inside the anchor
// ============================================================================

TEST_CASE("CONNECT BY lowering - WHERE is applied outside the hierarchy",
         "[connect-by][lowering][where]") {
    const std::string result = lower("SELECT employee_id FROM employees WHERE active = 1 "
                                    "START WITH manager_id IS NULL "
                                    "CONNECT BY PRIOR employee_id = manager_id");
    REQUIRE(result == "WITH RECURSIVE \"hierarchy\" AS ("
                      "SELECT \"employees\".* FROM \"employees\" WHERE \"manager_id\" IS NULL "
                      "UNION ALL "
                      "SELECT \"employees\".* FROM \"employees\" INNER JOIN \"hierarchy\" "
                      "ON \"hierarchy\".\"employee_id\" = \"employees\".\"manager_id\""
                      ") SELECT \"employee_id\" FROM \"hierarchy\" WHERE \"active\" = 1");

    // The anchor member's own WHERE (inside the CTE, before the first
    // UNION ALL) is only the START WITH condition - "active" never appears
    // there.
    const auto anchor_end = result.find("UNION ALL");
    REQUIRE(anchor_end != std::string::npos);
    REQUIRE(result.substr(0, anchor_end).find("\"active\"") == std::string::npos);
}

// ============================================================================
// Alias handling
// ============================================================================

TEST_CASE("CONNECT BY lowering - FROM employees e (explicit alias)",
         "[connect-by][lowering][alias]") {
    REQUIRE(lower("SELECT e.employee_id FROM employees e "
                  "START WITH e.manager_id IS NULL "
                  "CONNECT BY PRIOR e.employee_id = e.manager_id") ==
            "WITH RECURSIVE \"hierarchy\" AS ("
            "SELECT \"e\".* FROM \"employees\" AS \"e\" WHERE \"e\".\"manager_id\" IS NULL "
            "UNION ALL "
            "SELECT \"e\".* FROM \"employees\" AS \"e\" INNER JOIN \"hierarchy\" "
            "ON \"hierarchy\".\"employee_id\" = \"e\".\"manager_id\""
            ") SELECT \"hierarchy\".\"employee_id\" FROM \"hierarchy\"");
}

// ============================================================================
// Fixed point: the lowered PostgreSQL output re-parses and regenerates
// identically (it's a plain recursive CTE, so this must hold exactly).
// ============================================================================

TEST_CASE("CONNECT BY lowering - lowered output is a fixed point under PostgreSQL",
         "[connect-by][lowering][fixpoint]") {
    const std::string oracle_query = "SELECT employee_id FROM employees "
                                    "START WITH manager_id IS NULL "
                                    "CONNECT BY PRIOR employee_id = manager_id";

    const std::string g1 = lower(oracle_query, SQLDialect::Oracle, SQLDialect::PostgreSQL);

    libglot::Arena arena2;
    SQLParser parser2(arena2, g1, SQLDialect::PostgreSQL);
    auto ast2 = parser2.parse_top_level();
    SQLGenerator gen2(SQLDialect::PostgreSQL);
    const std::string g2 = gen2.generate(ast2);

    REQUIRE(g2 == g1);
}

TEST_CASE("CONNECT BY lowering - lowered output with LEVEL is also a fixed point",
         "[connect-by][lowering][fixpoint][level]") {
    const std::string oracle_query = "SELECT employee_id, LEVEL FROM employees "
                                    "START WITH manager_id IS NULL "
                                    "CONNECT BY PRIOR employee_id = manager_id";

    const std::string g1 = lower(oracle_query, SQLDialect::Oracle, SQLDialect::PostgreSQL);

    libglot::Arena arena2;
    SQLParser parser2(arena2, g1, SQLDialect::PostgreSQL);
    auto ast2 = parser2.parse_top_level();
    SQLGenerator gen2(SQLDialect::PostgreSQL);
    const std::string g2 = gen2.generate(ast2);

    REQUIRE(g2 == g1);
}

// ============================================================================
// Negative cases: forms with no clean lowering
// ============================================================================

namespace {

/// Runs `gen.generate(ast)`, requires it throws std::logic_error, and
/// requires the message contains `needle`.
void require_throws_with(SQLGenerator& gen, SQLNode* ast, std::string_view needle) {
    try {
        gen.generate(ast);
        FAIL("expected std::logic_error containing '" << needle << "'");
    } catch (const std::logic_error& e) {
        INFO("exception message: " << e.what());
        REQUIRE(std::string(e.what()).find(needle) != std::string::npos);
    }
}

} // namespace

TEST_CASE("CONNECT BY lowering - NOCYCLE throws std::logic_error",
         "[connect-by][lowering][error]") {
    libglot::Arena arena;
    SQLParser parser(arena, "SELECT id FROM t CONNECT BY NOCYCLE PRIOR id = parent_id",
                     SQLDialect::Oracle);
    auto ast = parser.parse_top_level();

    libglot::Arena transform_arena;
    SQLGenerator gen(SQLDialect::PostgreSQL, &transform_arena);
    require_throws_with(gen, ast, "NOCYCLE");
}

TEST_CASE("CONNECT BY lowering - ORDER SIBLINGS BY throws std::logic_error",
         "[connect-by][lowering][error]") {
    libglot::Arena arena;
    SQLParser parser(arena,
                     "SELECT id FROM t CONNECT BY PRIOR id = parent_id ORDER SIBLINGS BY id",
                     SQLDialect::Oracle);
    auto ast = parser.parse_top_level();

    libglot::Arena transform_arena;
    SQLGenerator gen(SQLDialect::PostgreSQL, &transform_arena);
    require_throws_with(gen, ast, "ORDER SIBLINGS BY");
}

TEST_CASE("CONNECT BY lowering - joined FROM throws std::logic_error",
         "[connect-by][lowering][error]") {
    libglot::Arena arena;
    SQLParser parser(arena,
                     "SELECT t.id FROM t JOIN u ON t.id = u.id "
                     "CONNECT BY PRIOR t.id = t.parent_id",
                     SQLDialect::Oracle);
    auto ast = parser.parse_top_level();

    libglot::Arena transform_arena;
    SQLGenerator gen(SQLDialect::PostgreSQL, &transform_arena);
    require_throws_with(gen, ast, "single-table FROM");
}

TEST_CASE("CONNECT BY lowering - no transform arena still throws, message mentions the arena",
         "[connect-by][lowering][error]") {
    libglot::Arena arena;
    SQLParser parser(arena, "SELECT id FROM t CONNECT BY PRIOR id = parent_id", SQLDialect::Oracle);
    auto ast = parser.parse_top_level();

    SQLGenerator gen(SQLDialect::PostgreSQL); // no transform arena
    require_throws_with(gen, ast, "transform arena");
}
