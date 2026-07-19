// Wave 2: BigQuery STRUCT literal and ARRAY subscript functions.
//
//   STRUCT(1 AS a, 'x' AS b)              - BigQuery type constructor
//   arr[OFFSET(0)]   - 0-based subscript, errors if out of range
//   arr[ORDINAL(1)]  - 1-based subscript, errors if out of range
//   arr[SAFE_OFFSET(0)] - 0-based, NULL instead of an error if out of range
//
// Plain arr[0] is unchanged (and unrestricted) in every dialect; the
// subscript functions and STRUCT(...) are BigQuery-only at generation time.

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

// ============================================================================
// STRUCT(...) literal
// ============================================================================

TEST_CASE("STRUCT literal - round-trips for BigQuery", "[struct][bigquery]") {
    REQUIRE(transpile("SELECT STRUCT(1 AS a, 'x' AS b)", SQLDialect::BigQuery) ==
            "SELECT STRUCT(1 AS `a`, 'x' AS `b`)");
}

TEST_CASE("STRUCT literal - throws for non-BigQuery dialects", "[struct][error]") {
    for (auto d :
         {SQLDialect::PostgreSQL, SQLDialect::MySQL, SQLDialect::ANSI, SQLDialect::Snowflake}) {
        REQUIRE_THROWS_AS(transpile("SELECT STRUCT(1 AS a, 'x' AS b)", d), std::logic_error);
    }
}

// ============================================================================
// arr[OFFSET(n)] / arr[ORDINAL(n)] / arr[SAFE_OFFSET(n)]
// ============================================================================

TEST_CASE("Array subscript - OFFSET round-trips for BigQuery", "[array][bigquery]") {
    REQUIRE(transpile("SELECT arr[OFFSET(0)] FROM t", SQLDialect::BigQuery) ==
            "SELECT `arr`[OFFSET(0)] FROM `t`");
}

TEST_CASE("Array subscript - ORDINAL round-trips for BigQuery", "[array][bigquery]") {
    REQUIRE(transpile("SELECT arr[ORDINAL(1)] FROM t", SQLDialect::BigQuery) ==
            "SELECT `arr`[ORDINAL(1)] FROM `t`");
}

TEST_CASE("Array subscript - SAFE_OFFSET round-trips for BigQuery", "[array][bigquery]") {
    REQUIRE(transpile("SELECT arr[SAFE_OFFSET(0)] FROM t", SQLDialect::BigQuery) ==
            "SELECT `arr`[SAFE_OFFSET(0)] FROM `t`");
}

TEST_CASE("Array subscript - AST shape", "[array][bigquery]") {
    libglot::Arena arena;
    SQLParser parser(arena, "SELECT arr[OFFSET(0)]", SQLDialect::BigQuery);
    auto* ast = static_cast<SelectStmt*>(parser.parse_top_level());
    REQUIRE(ast->columns[0]->type == SQLNodeKind::ARRAY_INDEX);
    auto* idx = static_cast<ArrayIndex*>(ast->columns[0]);
    REQUIRE(idx->subscript == ArraySubscript::OFFSET);
}

// NOTE: `identifier[...]` subscripting only lexes as array indexing in
// dialects whose TokenizerConfig has bracket_identifiers == false
// (Snowflake, and now BigQuery - see lex/tokenizer.h). Every other
// configured dialect here (PostgreSQL, MySQL, ANSI, ...) lexes a bare
// `[...]` immediately after an identifier as a *bracket-quoted identifier*
// instead (asserted directly in test_tokenizer.cpp, "Default config:
// '[0]' is a bracket-quoted identifier") - a pre-existing lexical
// limitation out of scope for this feature, so subscripting after a plain
// identifier is only exercised here for the dialects that actually lex it.
TEST_CASE("Array subscript - plain arr[0] is unrestricted in every dialect that lexes it",
          "[array]") {
    struct Case {
        SQLDialect dialect;
        const char* expected;
    };
    const Case cases[] = {
        {SQLDialect::BigQuery, "SELECT `arr`[0]"},
        {SQLDialect::Snowflake, "SELECT \"arr\"[0]"},
    };
    for (const auto& c : cases) {
        libglot::Arena arena;
        SQLParser parser(arena, "SELECT arr[0]", c.dialect);
        auto* ast = static_cast<SelectStmt*>(parser.parse_top_level());
        REQUIRE(ast->columns[0]->type == SQLNodeKind::ARRAY_INDEX);
        REQUIRE(static_cast<ArrayIndex*>(ast->columns[0])->subscript == ArraySubscript::NONE);
        SQLGenerator gen(c.dialect);
        REQUIRE(gen.generate(ast) == c.expected);
    }
}

TEST_CASE("Array subscript - OFFSET/ORDINAL throw for non-BigQuery dialects", "[array][error]") {
    // Snowflake lexes the subscript-function form fine (bracket_identifiers
    // == false there too) but BigQuery is the only dialect this generates
    // for.
    REQUIRE_THROWS_AS(transpile("SELECT arr[OFFSET(0)] FROM t", SQLDialect::Snowflake),
                      std::logic_error);
    REQUIRE_THROWS_AS(transpile("SELECT arr[ORDINAL(1)] FROM t", SQLDialect::Snowflake),
                      std::logic_error);
}

TEST_CASE("Array subscript - fixed point (BigQuery)", "[array][roundtrip]") {
    const std::string queries[] = {
        "SELECT arr[0]",
        "SELECT arr[OFFSET(0)]",
        "SELECT arr[ORDINAL(1)]",
        "SELECT arr[SAFE_OFFSET(0)]",
    };
    for (const auto& q : queries) {
        const std::string g1 = transpile(q, SQLDialect::BigQuery);
        REQUIRE(transpile(g1, SQLDialect::BigQuery) == g1);
    }
}
