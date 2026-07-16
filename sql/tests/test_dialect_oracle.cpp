// Oracle conformance: promoting Oracle from a quote-only "partial" dialect
// to a first-class, test-backed one (GitHub issue #3).
//
// Covers, with exact-string round-trips and fixpoint checks:
//   - Double-quote identifier quoting (SQLFeatures::identifier_quote)
//   - FETCH FIRST n ROWS ONLY (12c+ OFFSET/FETCH; supports_limit_offset is
//     false so the generic OFFSET/FETCH generator path is used)
//   - DUAL pseudo-table and ROWNUM pseudo-column
//   - NVL / NVL2 / DECODE pass through as ordinary function calls
//   - String concatenation with ||
//   - Sequences: seq.NEXTVAL / seq.CURRVAL member-style syntax
//   - CONNECT BY / START WITH hierarchical queries, including PRIOR
//
// Deliberately out of scope (documented, not silently mishandled):
//   - Unquoted-identifier case folding to uppercase (would break
//     round-trip; see docs/FEATURE_MATRIX.md)

#include <catch2/catch_test_macros.hpp>
#include <libglot/sql/generator.h>
#include <libglot/sql/parser.h>
#include <libglot/util/arena.h>

#include <string>

using namespace libglot::sql;

namespace {

std::string transpile(const std::string& sql, SQLDialect dialect = SQLDialect::Oracle) {
    libglot::Arena arena;
    SQLParser parser(arena, sql, dialect);
    auto* ast = parser.parse_top_level();
    SQLGenerator gen(dialect);
    return gen.generate(ast);
}

void require_fixpoint(const std::string& sql, SQLDialect dialect = SQLDialect::Oracle) {
    const std::string g1 = transpile(sql, dialect);
    const std::string g2 = transpile(g1, dialect);
    REQUIRE(g1 == g2);
}

} // namespace

// ============================================================================
// Traits
// ============================================================================

TEST_CASE("Oracle traits - double-quote identifiers, no native ILIKE, "
          "OFFSET/FETCH (not the classic LIMIT/OFFSET form)",
          "[dialect][oracle][traits]") {
    const auto& f = SQLDialectTraits::get_features(SQLDialect::Oracle);
    REQUIRE(f.identifier_quote == '"');
    REQUIRE(f.string_quote == '\'');
    REQUIRE_FALSE(f.supports_ilike);
    REQUIRE_FALSE(f.supports_limit_offset);
    REQUIRE(std::string(f.true_literal) == "TRUE");
    REQUIRE(std::string(f.false_literal) == "FALSE");
}

// ============================================================================
// Identifier quoting
// ============================================================================

TEST_CASE("Oracle - identifiers are double-quoted", "[dialect][oracle]") {
    REQUIRE(transpile("SELECT id, name FROM users") ==
            "SELECT \"id\", \"name\" FROM \"users\"");
}

// ============================================================================
// FETCH FIRST n ROWS ONLY (12c+)
// ============================================================================

TEST_CASE("Oracle - FETCH FIRST n ROWS ONLY round-trips", "[dialect][oracle][limit]") {
    REQUIRE(transpile("SELECT * FROM t FETCH FIRST 10 ROWS ONLY") ==
            "SELECT * FROM \"t\" FETCH FIRST 10 ROWS ONLY");
}

TEST_CASE("Oracle - LIMIT/OFFSET lowers to OFFSET ... FETCH NEXT", "[dialect][oracle][limit]") {
    REQUIRE(transpile("SELECT * FROM t ORDER BY id LIMIT 10 OFFSET 20") ==
            "SELECT * FROM \"t\" ORDER BY \"id\" OFFSET 20 ROWS FETCH NEXT 10 ROWS ONLY");
}

TEST_CASE("Oracle - FETCH FIRST fixpoint", "[dialect][oracle][limit][roundtrip]") {
    require_fixpoint("SELECT * FROM t FETCH FIRST 10 ROWS ONLY");
    require_fixpoint("SELECT * FROM t ORDER BY id OFFSET 20 ROWS FETCH NEXT 10 ROWS ONLY");
}

// ============================================================================
// DUAL / ROWNUM
// ============================================================================

TEST_CASE("Oracle - DUAL pseudo-table round-trips", "[dialect][oracle][dual]") {
    REQUIRE(transpile("SELECT 1 FROM DUAL") == "SELECT 1 FROM \"DUAL\"");
}

TEST_CASE("Oracle - ROWNUM in the select list round-trips", "[dialect][oracle][rownum]") {
    REQUIRE(transpile("SELECT ROWNUM FROM DUAL") == "SELECT \"ROWNUM\" FROM \"DUAL\"");
}

TEST_CASE("Oracle - ROWNUM in a WHERE predicate round-trips", "[dialect][oracle][rownum]") {
    REQUIRE(transpile("SELECT * FROM t WHERE ROWNUM <= 5") ==
            "SELECT * FROM \"t\" WHERE \"ROWNUM\" <= 5");
}

TEST_CASE("Oracle - ROWNUM / DUAL fixpoint", "[dialect][oracle][rownum][roundtrip]") {
    require_fixpoint("SELECT ROWNUM FROM DUAL");
    require_fixpoint("SELECT * FROM t WHERE ROWNUM <= 5");
}

// ============================================================================
// NVL / NVL2 / DECODE - pass through as ordinary function calls
// ============================================================================

TEST_CASE("Oracle - NVL round-trips as a function call", "[dialect][oracle][functions]") {
    REQUIRE(transpile("SELECT NVL(a, 0) FROM t") == "SELECT NVL(\"a\", 0) FROM \"t\"");
}

TEST_CASE("Oracle - NVL2 round-trips as a function call", "[dialect][oracle][functions]") {
    REQUIRE(transpile("SELECT NVL2(a, 1, 0) FROM t") == "SELECT NVL2(\"a\", 1, 0) FROM \"t\"");
}

TEST_CASE("Oracle - DECODE round-trips as a function call", "[dialect][oracle][functions]") {
    REQUIRE(transpile("SELECT DECODE(a, 1, 'x', 'y') FROM t") ==
            "SELECT DECODE(\"a\", 1, 'x', 'y') FROM \"t\"");
}

TEST_CASE("Oracle - NVL/NVL2/DECODE fixpoint", "[dialect][oracle][functions][roundtrip]") {
    require_fixpoint("SELECT NVL(a, 0) FROM t");
    require_fixpoint("SELECT NVL2(a, 1, 0) FROM t");
    require_fixpoint("SELECT DECODE(a, 1, 'x', 'y') FROM t");
}

// ============================================================================
// String concatenation with ||
// ============================================================================

TEST_CASE("Oracle - || string concatenation round-trips", "[dialect][oracle][concat]") {
    REQUIRE(transpile("SELECT a || b FROM t") == "SELECT \"a\" || \"b\" FROM \"t\"");
    require_fixpoint("SELECT first_name || ' ' || last_name FROM t");
}

// ============================================================================
// Sequences: seq.NEXTVAL / seq.CURRVAL (Oracle member-style)
// ============================================================================

TEST_CASE("Oracle - seq.NEXTVAL round-trips", "[dialect][oracle][sequence]") {
    REQUIRE(transpile("SELECT seq_a.NEXTVAL FROM t") == "SELECT \"seq_a\".NEXTVAL FROM \"t\"");
}

TEST_CASE("Oracle - seq.CURRVAL round-trips", "[dialect][oracle][sequence]") {
    REQUIRE(transpile("SELECT seq_a.CURRVAL FROM t") == "SELECT \"seq_a\".CURRVAL FROM \"t\"");
}

TEST_CASE("Oracle - sequence member-style fixpoint", "[dialect][oracle][sequence][roundtrip]") {
    require_fixpoint("SELECT seq_a.NEXTVAL FROM t");
    require_fixpoint("SELECT seq_a.CURRVAL FROM t");
}

TEST_CASE("Oracle - function-style NEXTVAL('seq') also parses and lowers to member-style",
          "[dialect][oracle][sequence]") {
    // Both surface spellings map onto the same SequenceRefExpr AST node;
    // Oracle always regenerates the member-style form.
    REQUIRE(transpile("SELECT NEXTVAL('seq_a')") == "SELECT \"seq_a\".NEXTVAL");
}

// ============================================================================
// CONNECT BY / START WITH hierarchical queries
// ============================================================================

TEST_CASE("Oracle - CONNECT BY PRIOR round-trips", "[dialect][oracle][connectby]") {
    REQUIRE(transpile("SELECT id FROM t CONNECT BY PRIOR id = parent_id") ==
            "SELECT \"id\" FROM \"t\" CONNECT BY PRIOR \"id\" = \"parent_id\"");
}

TEST_CASE("Oracle - START WITH ... CONNECT BY PRIOR round-trips", "[dialect][oracle][connectby]") {
    REQUIRE(transpile("SELECT id FROM t START WITH id = 1 CONNECT BY PRIOR id = parent_id") ==
            "SELECT \"id\" FROM \"t\" START WITH \"id\" = 1 CONNECT BY PRIOR \"id\" = "
            "\"parent_id\"");
}

TEST_CASE("Oracle - CONNECT BY NOCYCLE round-trips", "[dialect][oracle][connectby]") {
    REQUIRE(transpile("SELECT id FROM t CONNECT BY NOCYCLE PRIOR id = parent_id") ==
            "SELECT \"id\" FROM \"t\" CONNECT BY NOCYCLE PRIOR \"id\" = \"parent_id\"");
}

TEST_CASE("Oracle - CONNECT BY throws for a non-hierarchical dialect", "[dialect][oracle][error]") {
    libglot::Arena arena;
    SQLParser parser(arena, "SELECT id FROM t CONNECT BY PRIOR id = parent_id", SQLDialect::Oracle);
    auto* ast = parser.parse_top_level();
    SQLGenerator gen(SQLDialect::MySQL);
    REQUIRE_THROWS_AS(gen.generate(ast), std::logic_error);
}

TEST_CASE("Oracle - CONNECT BY fixpoint", "[dialect][oracle][connectby][roundtrip]") {
    require_fixpoint("SELECT id FROM t CONNECT BY PRIOR id = parent_id");
    require_fixpoint("SELECT id FROM t START WITH id = 1 CONNECT BY PRIOR id = parent_id");
    require_fixpoint("SELECT id FROM t CONNECT BY NOCYCLE PRIOR id = parent_id");
}
