// Oracle hierarchical queries: START WITH / CONNECT BY [NOCYCLE] with the
// PRIOR unary operator, LEVEL pseudo-column, and ORDER SIBLINGS BY.
//
// Design choice (documented in generator.h): only Oracle and Snowflake can
// generate CONNECT BY. For every other dialect the generator throws
// std::logic_error rather than emitting silently broken SQL - a correct,
// fixpoint-clean recursive-CTE transpilation is not attempted here.

#include <catch2/catch_test_macros.hpp>
#include <libglot/sql/parser.h>
#include <libglot/sql/generator.h>
#include <libglot/util/arena.h>

#include <stdexcept>
#include <string>

using namespace libglot::sql;

namespace {

std::string transpile(const std::string& sql,
                      SQLDialect parse_dialect,
                      SQLDialect gen_dialect) {
    libglot::Arena arena;
    SQLParser parser(arena, sql, parse_dialect);
    auto ast = parser.parse_top_level();
    SQLGenerator gen(gen_dialect);
    return gen.generate(ast);
}

std::string oracle(const std::string& sql) {
    return transpile(sql, SQLDialect::Oracle, SQLDialect::Oracle);
}

} // namespace

// ============================================================================
// Basic START WITH ... CONNECT BY
// ============================================================================

TEST_CASE("CONNECT BY - basic hierarchy with PRIOR", "[connect-by][oracle]") {
    REQUIRE(oracle("SELECT employee_id FROM employees "
                   "START WITH manager_id IS NULL "
                   "CONNECT BY PRIOR employee_id = manager_id")
            == "SELECT \"employee_id\" FROM \"employees\" "
               "START WITH \"manager_id\" IS NULL "
               "CONNECT BY PRIOR \"employee_id\" = \"manager_id\"");
}

TEST_CASE("CONNECT BY - both clause orders parse to the canonical form", "[connect-by][oracle]") {
    const std::string canonical =
        "SELECT \"id\" FROM \"t\" "
        "START WITH \"parent_id\" IS NULL "
        "CONNECT BY PRIOR \"id\" = \"parent_id\"";

    // START WITH first (canonical Oracle order)
    REQUIRE(oracle("SELECT id FROM t START WITH parent_id IS NULL "
                   "CONNECT BY PRIOR id = parent_id") == canonical);

    // CONNECT BY first - also legal in Oracle, normalized on output
    REQUIRE(oracle("SELECT id FROM t CONNECT BY PRIOR id = parent_id "
                   "START WITH parent_id IS NULL") == canonical);
}

TEST_CASE("CONNECT BY - without START WITH", "[connect-by][oracle]") {
    REQUIRE(oracle("SELECT id FROM t CONNECT BY PRIOR id = parent_id")
            == "SELECT \"id\" FROM \"t\" CONNECT BY PRIOR \"id\" = \"parent_id\"");
}

TEST_CASE("CONNECT BY - NOCYCLE", "[connect-by][oracle][nocycle]") {
    REQUIRE(oracle("SELECT id FROM t CONNECT BY NOCYCLE PRIOR id = parent_id")
            == "SELECT \"id\" FROM \"t\" CONNECT BY NOCYCLE PRIOR \"id\" = \"parent_id\"");
}

// ============================================================================
// PRIOR operator placement
// ============================================================================

TEST_CASE("PRIOR - on the right side of the comparison", "[connect-by][prior]") {
    REQUIRE(oracle("SELECT id FROM t CONNECT BY id = PRIOR parent_id")
            == "SELECT \"id\" FROM \"t\" CONNECT BY \"id\" = PRIOR \"parent_id\"");
}

TEST_CASE("PRIOR - inside a compound CONNECT BY condition", "[connect-by][prior]") {
    REQUIRE(oracle("SELECT id FROM t "
                   "CONNECT BY PRIOR id = parent_id AND status = 'active'")
            == "SELECT \"id\" FROM \"t\" "
               "CONNECT BY PRIOR \"id\" = \"parent_id\" AND \"status\" = 'active'");
}

// ============================================================================
// LEVEL pseudo-column and WHERE interaction
// ============================================================================

TEST_CASE("LEVEL pseudo-column parses as an identifier", "[connect-by][level]") {
    REQUIRE(oracle("SELECT LEVEL, id FROM t CONNECT BY PRIOR id = parent_id")
            == "SELECT \"LEVEL\", \"id\" FROM \"t\" CONNECT BY PRIOR \"id\" = \"parent_id\"");
    // LEVEL usable in conditions too
    REQUIRE(oracle("SELECT id FROM t CONNECT BY PRIOR id = parent_id AND LEVEL < 5")
            == "SELECT \"id\" FROM \"t\" CONNECT BY PRIOR \"id\" = \"parent_id\" AND \"LEVEL\" < 5");
}

TEST_CASE("CONNECT BY - after a WHERE clause", "[connect-by][oracle]") {
    REQUIRE(oracle("SELECT id FROM t WHERE active = 1 "
                   "START WITH parent_id IS NULL CONNECT BY PRIOR id = parent_id")
            == "SELECT \"id\" FROM \"t\" WHERE \"active\" = 1 "
               "START WITH \"parent_id\" IS NULL CONNECT BY PRIOR \"id\" = \"parent_id\"");
}

// ============================================================================
// ORDER SIBLINGS BY
// ============================================================================

TEST_CASE("ORDER SIBLINGS BY", "[connect-by][siblings]") {
    REQUIRE(oracle("SELECT id, name FROM t "
                   "START WITH parent_id IS NULL "
                   "CONNECT BY PRIOR id = parent_id "
                   "ORDER SIBLINGS BY name")
            == "SELECT \"id\", \"name\" FROM \"t\" "
               "START WITH \"parent_id\" IS NULL "
               "CONNECT BY PRIOR \"id\" = \"parent_id\" "
               "ORDER SIBLINGS BY \"name\"");

    REQUIRE(oracle("SELECT id FROM t CONNECT BY PRIOR id = parent_id "
                   "ORDER SIBLINGS BY name DESC")
            == "SELECT \"id\" FROM \"t\" CONNECT BY PRIOR \"id\" = \"parent_id\" "
               "ORDER SIBLINGS BY \"name\" DESC");
}

TEST_CASE("Plain ORDER BY is unaffected", "[connect-by][siblings]") {
    REQUIRE(oracle("SELECT id FROM t ORDER BY id")
            == "SELECT \"id\" FROM \"t\" ORDER BY \"id\"");
}

// ============================================================================
// Fixed point of the generator's own output
// ============================================================================

TEST_CASE("CONNECT BY - generated Oracle SQL is a fixed point", "[connect-by][fixpoint]") {
    const std::string queries[] = {
        "SELECT id FROM t START WITH parent_id IS NULL CONNECT BY PRIOR id = parent_id",
        "SELECT id FROM t CONNECT BY NOCYCLE PRIOR id = parent_id",
        "SELECT LEVEL, id FROM t CONNECT BY PRIOR id = parent_id ORDER SIBLINGS BY id",
    };
    for (const auto& q : queries) {
        const std::string g1 = oracle(q);
        REQUIRE(oracle(g1) == g1);
    }
}

// ============================================================================
// Snowflake also supports CONNECT BY
// ============================================================================

TEST_CASE("CONNECT BY - Snowflake generation", "[connect-by][snowflake]") {
    REQUIRE(transpile("SELECT id FROM t START WITH parent_id IS NULL "
                      "CONNECT BY PRIOR id = parent_id",
                      SQLDialect::Snowflake, SQLDialect::Snowflake)
            == "SELECT \"id\" FROM \"t\" START WITH \"parent_id\" IS NULL "
               "CONNECT BY PRIOR \"id\" = \"parent_id\"");
}

// ============================================================================
// Unsupported dialects throw instead of emitting broken SQL
// ============================================================================

TEST_CASE("CONNECT BY - unsupported dialects throw std::logic_error", "[connect-by][error]") {
    const std::string sql =
        "SELECT id FROM t START WITH parent_id IS NULL CONNECT BY PRIOR id = parent_id";
    for (auto d : {SQLDialect::PostgreSQL, SQLDialect::MySQL,
                   SQLDialect::SQLServer, SQLDialect::ANSI}) {
        libglot::Arena arena;
        SQLParser parser(arena, sql, SQLDialect::Oracle);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(d);
        REQUIRE_THROWS_AS(gen.generate(ast), std::logic_error);
    }
}

// ============================================================================
// Strictness: trailing input after hierarchical clauses is still an error
// ============================================================================

TEST_CASE("CONNECT BY does not relax trailing-input checking", "[connect-by][strict]") {
    libglot::Arena arena;
    SQLParser parser(arena, "SELECT id FROM t CONNECT BY PRIOR id = parent_id SELECT 2",
                     SQLDialect::Oracle);
    REQUIRE_THROWS_AS(parser.parse_top_level(), libglot::ParseError);
}
