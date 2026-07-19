// DuckDB conformance: promoting DuckDB from a partially-covered dialect to a
// first-class, test-backed one (GitHub issue #3).
//
// Covers, with exact-string round-trips and fixpoint checks:
//   - Double-quote identifier quoting, NATIVE ILIKE (no LOWER() polyfill)
//   - SELECT * EXCLUDE (col, ...) and SELECT * REPLACE (expr AS col, ...);
//     DuckDB also accepts the BigQuery EXCEPT spelling
//   - QUALIFY (post-window-function filter)
//   - Array subscripting (arr[1], plain 1-based indexing - no
//     OFFSET/ORDINAL wrapper, which is BigQuery-only) and bracket array
//     literals ([1, 2, 3] / ARRAY[1, 2, 3])
//   - ASOF JOIN, LIMIT/OFFSET
//
// This file also documents (and regression-tests, via the "fixed" bugs
// below) a real lexical bug found while promoting this dialect: before a
// dedicated TokenizerConfig::duckdb() existed, DuckDB fell back to
// default_config() (bracket_identifiers = true), which mis-tokenized every
// "[...]" as a single SQL-Server/Access-style bracket-quoted identifier -
// silently breaking both array subscripting (arr[1]) and bracket array
// literals ([1, 2, 3]) for this dialect.
//
// Deliberately out of scope (documented, not silently mishandled):
//   - DuckDB's `{'key': value}` dict/struct literal syntax
//   - Named-argument function calls (STRUCT_PACK(a := 1)); positional-arg
//     STRUCT_PACK(1, 2) works as an ordinary function call

#include <catch2/catch_test_macros.hpp>
#include <libglot/sql/generator.h>
#include <libglot/sql/parser.h>
#include <libglot/util/arena.h>

#include <string>

using namespace libglot::sql;

namespace {

std::string transpile(const std::string& sql, SQLDialect dialect = SQLDialect::DuckDB) {
    libglot::Arena arena;
    SQLParser parser(arena, sql, dialect);
    auto* ast = parser.parse_top_level();
    SQLGenerator gen(dialect);
    return gen.generate(ast);
}

void require_fixpoint(const std::string& sql, SQLDialect dialect = SQLDialect::DuckDB) {
    const std::string g1 = transpile(sql, dialect);
    const std::string g2 = transpile(g1, dialect);
    REQUIRE(g1 == g2);
}

} // namespace

// ============================================================================
// Traits
// ============================================================================

TEST_CASE("DuckDB traits - double-quote identifiers, NATIVE ILIKE, LIMIT/OFFSET supported",
          "[dialect][duckdb][traits]") {
    const auto& f = SQLDialectTraits::get_features(SQLDialect::DuckDB);
    REQUIRE(f.identifier_quote == '"');
    REQUIRE(f.string_quote == '\'');
    REQUIRE(f.supports_ilike);
    REQUIRE(f.supports_limit_offset);
    REQUIRE(std::string(f.true_literal) == "TRUE");
    REQUIRE(std::string(f.false_literal) == "FALSE");
}

// ============================================================================
// Identifier quoting / native ILIKE (no polyfill)
// ============================================================================

TEST_CASE("DuckDB - identifiers are double-quoted", "[dialect][duckdb]") {
    REQUIRE(transpile("SELECT id, name FROM users") ==
            "SELECT \"id\", \"name\" FROM \"users\"");
}

TEST_CASE("DuckDB - ILIKE is native (not LOWER()-polyfilled)", "[dialect][duckdb][ilike]") {
    REQUIRE(transpile("SELECT * FROM t WHERE a ILIKE 'x%'") ==
            "SELECT * FROM \"t\" WHERE \"a\" ILIKE 'x%'");
}

// ============================================================================
// SELECT * EXCLUDE (...) / SELECT * REPLACE (...) / SELECT * EXCEPT (...)
// ============================================================================

TEST_CASE("DuckDB - SELECT * EXCLUDE (...) round-trips", "[dialect][duckdb][star]") {
    REQUIRE(transpile("SELECT * EXCLUDE (a, b) FROM t") ==
            "SELECT * EXCLUDE (\"a\", \"b\") FROM \"t\"");
}

TEST_CASE("DuckDB - SELECT * REPLACE (...) round-trips", "[dialect][duckdb][star]") {
    REQUIRE(transpile("SELECT * REPLACE (a + 1 AS a) FROM t") ==
            "SELECT * REPLACE (\"a\" + 1 AS \"a\") FROM \"t\"");
}

TEST_CASE("DuckDB - SELECT * EXCLUDE (...) REPLACE (...) combine", "[dialect][duckdb][star]") {
    REQUIRE(transpile("SELECT * EXCLUDE (a) REPLACE (b + 1 AS b) FROM t") ==
            "SELECT * EXCLUDE (\"a\") REPLACE (\"b\" + 1 AS \"b\") FROM \"t\"");
}

TEST_CASE("DuckDB - qualified t.* EXCLUDE (...) round-trips", "[dialect][duckdb][star]") {
    REQUIRE(transpile("SELECT t.* EXCLUDE (a) FROM t") ==
            "SELECT \"t\".* EXCLUDE (\"a\") FROM \"t\"");
}

TEST_CASE("DuckDB - also accepts the BigQuery EXCEPT spelling", "[dialect][duckdb][star]") {
    REQUIRE(transpile("SELECT * EXCEPT (a, b) FROM t") ==
            "SELECT * EXCEPT (\"a\", \"b\") FROM \"t\"");
}

TEST_CASE("DuckDB - star modifiers fixpoint", "[dialect][duckdb][star][roundtrip]") {
    require_fixpoint("SELECT * EXCLUDE (a, b) FROM t");
    require_fixpoint("SELECT * REPLACE (a + 1 AS a) FROM t");
    require_fixpoint("SELECT * EXCLUDE (a) REPLACE (b + 1 AS b) FROM t");
    require_fixpoint("SELECT t.* EXCLUDE (a) FROM t");
    require_fixpoint("SELECT * EXCEPT (a, b) FROM t");
}

TEST_CASE("DuckDB - SELECT * EXCLUDE (...) throws outside DuckDB (including BigQuery, which "
          "only has EXCEPT)",
          "[dialect][duckdb][star][error]") {
    libglot::Arena arena;
    SQLParser parser(arena, "SELECT * EXCLUDE (a) FROM t", SQLDialect::DuckDB);
    auto* ast = parser.parse_top_level();
    for (auto d : {SQLDialect::BigQuery, SQLDialect::Oracle, SQLDialect::PostgreSQL}) {
        SQLGenerator gen(d);
        REQUIRE_THROWS_AS(gen.generate(ast), std::logic_error);
    }
}

// ============================================================================
// QUALIFY
// ============================================================================

TEST_CASE("DuckDB - QUALIFY round-trips", "[dialect][duckdb][qualify]") {
    REQUIRE(transpile("SELECT a FROM t QUALIFY ROW_NUMBER() OVER (ORDER BY a) = 1") ==
            "SELECT \"a\" FROM \"t\" QUALIFY ROW_NUMBER() OVER (ORDER BY \"a\") = 1");
}

TEST_CASE("DuckDB - QUALIFY fixpoint", "[dialect][duckdb][qualify][roundtrip]") {
    require_fixpoint("SELECT a FROM t QUALIFY ROW_NUMBER() OVER (ORDER BY a) = 1");
}

// ============================================================================
// Array subscripting and literals (regression coverage for the
// bracket_identifiers tokenizer bug fixed while promoting this dialect)
// ============================================================================

TEST_CASE("DuckDB - arr[n] plain subscript round-trips (1-based, no OFFSET/ORDINAL wrapper)",
          "[dialect][duckdb][array]") {
    REQUIRE(transpile("SELECT arr[1] FROM t") == "SELECT \"arr\"[1] FROM \"t\"");
}

TEST_CASE("DuckDB - arr[OFFSET(0)] (BigQuery-only subscript function) throws",
          "[dialect][duckdb][array][error]") {
    libglot::Arena arena;
    SQLParser parser(arena, "SELECT arr[OFFSET(0)] FROM t", SQLDialect::DuckDB);
    auto* ast = parser.parse_top_level();
    SQLGenerator gen(SQLDialect::DuckDB);
    REQUIRE_THROWS_AS(gen.generate(ast), std::logic_error);
}

TEST_CASE("DuckDB - bracket array literal [1, 2, 3] round-trips", "[dialect][duckdb][array]") {
    REQUIRE(transpile("SELECT [1, 2, 3]") == "SELECT [1, 2, 3]");
}

TEST_CASE("DuckDB - ARRAY[1, 2, 3] constructor round-trips to the same bracket literal",
          "[dialect][duckdb][array]") {
    REQUIRE(transpile("SELECT ARRAY[1, 2, 3]") == "SELECT [1, 2, 3]");
}

TEST_CASE("DuckDB - array subscript / literal fixpoint", "[dialect][duckdb][array][roundtrip]") {
    require_fixpoint("SELECT arr[1] FROM t");
    require_fixpoint("SELECT [1, 2, 3]");
}

// ============================================================================
// ASOF JOIN / LIMIT-OFFSET
// ============================================================================

TEST_CASE("DuckDB - ASOF JOIN round-trips", "[dialect][duckdb][join]") {
    REQUIRE(transpile("SELECT * FROM t1 ASOF JOIN t2 ON t1.ts >= t2.ts") ==
            "SELECT * FROM \"t1\" ASOF JOIN \"t2\" ON \"t1\".\"ts\" >= \"t2\".\"ts\"");
}

TEST_CASE("DuckDB - LIMIT/OFFSET round-trips (native, not FETCH FIRST)",
          "[dialect][duckdb][limit]") {
    REQUIRE(transpile("SELECT * FROM t LIMIT 10 OFFSET 5") ==
            "SELECT * FROM \"t\" LIMIT 10 OFFSET 5");
}

// ============================================================================
// STRUCT_PACK positional-arg function call (STRUCT(...) BigQuery constructor
// and named-argument STRUCT_PACK(a := 1) are out of scope; see file header)
// ============================================================================

TEST_CASE("DuckDB - STRUCT_PACK(...) with positional args round-trips as an ordinary "
          "function call",
          "[dialect][duckdb][struct]") {
    REQUIRE(transpile("SELECT STRUCT_PACK(1, 2)") == "SELECT STRUCT_PACK(1, 2)");
}

TEST_CASE("DuckDB - STRUCT(...) BigQuery constructor has no DuckDB equivalent and throws",
          "[dialect][duckdb][struct][error]") {
    libglot::Arena arena;
    SQLParser parser(arena, "SELECT STRUCT(1 AS x)", SQLDialect::BigQuery);
    auto* ast = parser.parse_top_level();
    SQLGenerator gen(SQLDialect::DuckDB);
    REQUIRE_THROWS_AS(gen.generate(ast), std::logic_error);
}
