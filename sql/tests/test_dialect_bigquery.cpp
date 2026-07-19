// BigQuery conformance: promoting Google BigQuery from a partially-covered
// dialect to a first-class, test-backed one (GitHub issue #3).
//
// Covers, with exact-string round-trips and fixpoint checks:
//   - Backtick identifier quoting, no native ILIKE (LOWER() polyfill)
//   - SAFE_CAST(expr AS type) - round-trips distinctly from CAST (previously
//     silently downgraded to CAST, losing its error-suppressing semantics)
//   - SELECT * EXCEPT (col, ...) and SELECT * REPLACE (expr AS col, ...)
//   - QUALIFY (post-window-function filter)
//   - STRUCT(...) type constructor
//   - Array subscript functions: arr[OFFSET(n)] / arr[ORDINAL(n)] /
//     arr[SAFE_OFFSET(n)] (wave 2; covered here for conformance)
//   - Bracket array literal [1, 2, 3]

#include <catch2/catch_test_macros.hpp>
#include <libglot/sql/generator.h>
#include <libglot/sql/parser.h>
#include <libglot/util/arena.h>

#include <string>

using namespace libglot::sql;

namespace {

std::string transpile(const std::string& sql, SQLDialect dialect = SQLDialect::BigQuery) {
    libglot::Arena arena;
    SQLParser parser(arena, sql, dialect);
    auto* ast = parser.parse_top_level();
    SQLGenerator gen(dialect);
    return gen.generate(ast);
}

void require_fixpoint(const std::string& sql, SQLDialect dialect = SQLDialect::BigQuery) {
    const std::string g1 = transpile(sql, dialect);
    const std::string g2 = transpile(g1, dialect);
    REQUIRE(g1 == g2);
}

} // namespace

// ============================================================================
// Traits
// ============================================================================

TEST_CASE("BigQuery traits - backtick identifiers, no native ILIKE, LIMIT/OFFSET supported",
          "[dialect][bigquery][traits]") {
    const auto& f = SQLDialectTraits::get_features(SQLDialect::BigQuery);
    REQUIRE(f.identifier_quote == '`');
    REQUIRE(f.string_quote == '\'');
    REQUIRE_FALSE(f.supports_ilike);
    REQUIRE(f.supports_limit_offset);
    REQUIRE(std::string(f.true_literal) == "TRUE");
    REQUIRE(std::string(f.false_literal) == "FALSE");
}

// ============================================================================
// Identifier quoting / ILIKE polyfill
// ============================================================================

TEST_CASE("BigQuery - identifiers are backtick-quoted", "[dialect][bigquery]") {
    REQUIRE(transpile("SELECT id, name FROM users") == "SELECT `id`, `name` FROM `users`");
}

TEST_CASE("BigQuery - ILIKE polyfills to LOWER()..LIKE LOWER()", "[dialect][bigquery][ilike]") {
    REQUIRE(transpile("SELECT * FROM t WHERE a ILIKE 'x%'") ==
            "SELECT * FROM `t` WHERE LOWER(`a`) LIKE LOWER('x%')");
}

// ============================================================================
// SAFE_CAST
// ============================================================================

TEST_CASE("BigQuery - SAFE_CAST round-trips distinctly from CAST", "[dialect][bigquery][cast]") {
    REQUIRE(transpile("SELECT SAFE_CAST(a AS INT64) FROM t") ==
            "SELECT SAFE_CAST(`a` AS INT64) FROM `t`");
    REQUIRE(transpile("SELECT CAST(a AS INT64) FROM t") == "SELECT CAST(`a` AS INT64) FROM `t`");
}

TEST_CASE("BigQuery - SAFE_CAST fixpoint", "[dialect][bigquery][cast][roundtrip]") {
    require_fixpoint("SELECT SAFE_CAST(a AS INT64) FROM t");
}

TEST_CASE("BigQuery - SAFE_CAST throws for dialects with no error-suppressing equivalent",
          "[dialect][bigquery][cast][error]") {
    libglot::Arena arena;
    SQLParser parser(arena, "SELECT SAFE_CAST(a AS INT64) FROM t", SQLDialect::BigQuery);
    auto* ast = parser.parse_top_level();
    for (auto d : {SQLDialect::Oracle, SQLDialect::PostgreSQL, SQLDialect::MySQL}) {
        SQLGenerator gen(d);
        REQUIRE_THROWS_AS(gen.generate(ast), std::logic_error);
    }
}

// ============================================================================
// SELECT * EXCEPT (...) / SELECT * REPLACE (...)
// ============================================================================

TEST_CASE("BigQuery - SELECT * EXCEPT (...) round-trips", "[dialect][bigquery][star]") {
    REQUIRE(transpile("SELECT * EXCEPT (a, b) FROM t") == "SELECT * EXCEPT (`a`, `b`) FROM `t`");
}

TEST_CASE("BigQuery - SELECT * REPLACE (...) round-trips", "[dialect][bigquery][star]") {
    REQUIRE(transpile("SELECT * REPLACE (a + 1 AS a) FROM t") ==
            "SELECT * REPLACE (`a` + 1 AS `a`) FROM `t`");
}

TEST_CASE("BigQuery - SELECT * EXCEPT (...) REPLACE (...) combine", "[dialect][bigquery][star]") {
    REQUIRE(transpile("SELECT * EXCEPT (a) REPLACE (b + 1 AS b) FROM t") ==
            "SELECT * EXCEPT (`a`) REPLACE (`b` + 1 AS `b`) FROM `t`");
}

TEST_CASE("BigQuery - qualified t.* EXCEPT (...) round-trips", "[dialect][bigquery][star]") {
    REQUIRE(transpile("SELECT t.* EXCEPT (a) FROM t") == "SELECT `t`.* EXCEPT (`a`) FROM `t`");
}

TEST_CASE("BigQuery - star modifiers fixpoint", "[dialect][bigquery][star][roundtrip]") {
    require_fixpoint("SELECT * EXCEPT (a, b) FROM t");
    require_fixpoint("SELECT * REPLACE (a + 1 AS a) FROM t");
    require_fixpoint("SELECT * EXCEPT (a) REPLACE (b + 1 AS b) FROM t");
    require_fixpoint("SELECT t.* EXCEPT (a) FROM t");
}

TEST_CASE("BigQuery - SELECT * EXCEPT (...) throws outside BigQuery/DuckDB",
          "[dialect][bigquery][star][error]") {
    libglot::Arena arena;
    SQLParser parser(arena, "SELECT * EXCEPT (a) FROM t", SQLDialect::BigQuery);
    auto* ast = parser.parse_top_level();
    for (auto d : {SQLDialect::Oracle, SQLDialect::PostgreSQL, SQLDialect::MySQL}) {
        SQLGenerator gen(d);
        REQUIRE_THROWS_AS(gen.generate(ast), std::logic_error);
    }
}

// ============================================================================
// QUALIFY
// ============================================================================

TEST_CASE("BigQuery - QUALIFY round-trips", "[dialect][bigquery][qualify]") {
    REQUIRE(transpile("SELECT a FROM t QUALIFY ROW_NUMBER() OVER (ORDER BY a) = 1") ==
            "SELECT `a` FROM `t` QUALIFY ROW_NUMBER() OVER (ORDER BY `a`) = 1");
}

TEST_CASE("BigQuery - QUALIFY fixpoint", "[dialect][bigquery][qualify][roundtrip]") {
    require_fixpoint("SELECT a FROM t QUALIFY ROW_NUMBER() OVER (ORDER BY a) = 1");
}

// ============================================================================
// STRUCT(...) constructor
// ============================================================================

TEST_CASE("BigQuery - STRUCT(...) round-trips", "[dialect][bigquery][struct]") {
    REQUIRE(transpile("SELECT STRUCT(1 AS x, 'y' AS y)") ==
            "SELECT STRUCT(1 AS `x`, 'y' AS `y`)");
}

// ============================================================================
// Array subscript functions: OFFSET / ORDINAL / SAFE_OFFSET
// ============================================================================

TEST_CASE("BigQuery - arr[OFFSET(n)] round-trips", "[dialect][bigquery][array]") {
    REQUIRE(transpile("SELECT arr[OFFSET(0)] FROM t") == "SELECT `arr`[OFFSET(0)] FROM `t`");
}

TEST_CASE("BigQuery - arr[ORDINAL(n)] round-trips", "[dialect][bigquery][array]") {
    REQUIRE(transpile("SELECT arr[ORDINAL(1)] FROM t") == "SELECT `arr`[ORDINAL(1)] FROM `t`");
}

TEST_CASE("BigQuery - arr[SAFE_OFFSET(n)] round-trips", "[dialect][bigquery][array]") {
    REQUIRE(transpile("SELECT arr[SAFE_OFFSET(0)] FROM t") ==
            "SELECT `arr`[SAFE_OFFSET(0)] FROM `t`");
}

TEST_CASE("BigQuery - bracket array literal round-trips", "[dialect][bigquery][array]") {
    REQUIRE(transpile("SELECT [1, 2, 3]") == "SELECT [1, 2, 3]");
}

TEST_CASE("BigQuery - array subscript / literal fixpoint", "[dialect][bigquery][array][roundtrip]") {
    require_fixpoint("SELECT arr[OFFSET(0)] FROM t");
    require_fixpoint("SELECT arr[ORDINAL(1)] FROM t");
    require_fixpoint("SELECT arr[SAFE_OFFSET(0)] FROM t");
    require_fixpoint("SELECT [1, 2, 3]");
}
