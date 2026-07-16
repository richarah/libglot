// DB2 conformance: promoting IBM DB2 from a quote-only "partial" dialect to
// a first-class, test-backed one (GitHub issue #3).
//
// Covers, with exact-string round-trips and fixpoint checks:
//   - Double-quote identifier quoting, no native ILIKE (LOWER() polyfill)
//   - FETCH FIRST n ROWS ONLY (supports_limit_offset is false, so LIMIT n
//     OFFSET m lowers to OFFSET ... FETCH NEXT n ROWS ONLY)
//   - Standalone VALUES statement (VALUES (1, 2, 3) / bare VALUES 1)
//   - CURRENT DATE / CURRENT TIME / CURRENT TIMESTAMP two-word special
//     registers (in addition to the single-token CURRENT_DATE spelling)
//   - NEXT VALUE FOR seq / PREVIOUS VALUE FOR seq sequence expressions

#include <catch2/catch_test_macros.hpp>
#include <libglot/sql/generator.h>
#include <libglot/sql/parser.h>
#include <libglot/util/arena.h>

#include <string>

using namespace libglot::sql;

namespace {

std::string transpile(const std::string& sql, SQLDialect dialect = SQLDialect::DB2) {
    libglot::Arena arena;
    SQLParser parser(arena, sql, dialect);
    auto* ast = parser.parse_top_level();
    SQLGenerator gen(dialect);
    return gen.generate(ast);
}

void require_fixpoint(const std::string& sql, SQLDialect dialect = SQLDialect::DB2) {
    const std::string g1 = transpile(sql, dialect);
    const std::string g2 = transpile(g1, dialect);
    REQUIRE(g1 == g2);
}

} // namespace

// ============================================================================
// Traits
// ============================================================================

TEST_CASE("DB2 traits - double-quote identifiers, no native ILIKE, no classic "
          "LIMIT/OFFSET (uses FETCH FIRST)",
          "[dialect][db2][traits]") {
    const auto& f = SQLDialectTraits::get_features(SQLDialect::DB2);
    REQUIRE(f.identifier_quote == '"');
    REQUIRE(f.string_quote == '\'');
    REQUIRE_FALSE(f.supports_ilike);
    REQUIRE_FALSE(f.supports_limit_offset);
    REQUIRE(std::string(f.true_literal) == "TRUE");
    REQUIRE(std::string(f.false_literal) == "FALSE");
}

// ============================================================================
// Identifier quoting / ILIKE polyfill
// ============================================================================

TEST_CASE("DB2 - identifiers are double-quoted", "[dialect][db2]") {
    REQUIRE(transpile("SELECT id, name FROM users") ==
            "SELECT \"id\", \"name\" FROM \"users\"");
}

TEST_CASE("DB2 - ILIKE polyfills to LOWER()..LIKE LOWER()", "[dialect][db2][ilike]") {
    REQUIRE(transpile("SELECT * FROM t WHERE a ILIKE 'x%'") ==
            "SELECT * FROM \"t\" WHERE LOWER(\"a\") LIKE LOWER('x%')");
}

// ============================================================================
// FETCH FIRST n ROWS ONLY
// ============================================================================

TEST_CASE("DB2 - FETCH FIRST n ROWS ONLY round-trips", "[dialect][db2][limit]") {
    REQUIRE(transpile("SELECT * FROM t FETCH FIRST 10 ROWS ONLY") ==
            "SELECT * FROM \"t\" FETCH FIRST 10 ROWS ONLY");
}

TEST_CASE("DB2 - LIMIT/OFFSET lowers to OFFSET ... FETCH NEXT", "[dialect][db2][limit]") {
    REQUIRE(transpile("SELECT * FROM t ORDER BY id LIMIT 10 OFFSET 20") ==
            "SELECT * FROM \"t\" ORDER BY \"id\" OFFSET 20 ROWS FETCH NEXT 10 ROWS ONLY");
}

TEST_CASE("DB2 - FETCH FIRST fixpoint", "[dialect][db2][limit][roundtrip]") {
    require_fixpoint("SELECT * FROM t FETCH FIRST 10 ROWS ONLY");
    require_fixpoint("SELECT * FROM t ORDER BY id OFFSET 20 ROWS FETCH NEXT 10 ROWS ONLY");
}

// ============================================================================
// Standalone VALUES statement
// ============================================================================

TEST_CASE("DB2 - standalone VALUES (row) round-trips", "[dialect][db2][values]") {
    REQUIRE(transpile("VALUES (1, 2, 3)") == "VALUES (1, 2, 3)");
}

TEST_CASE("DB2 - bare VALUES (no parens) canonicalizes to a parenthesized row",
          "[dialect][db2][values]") {
    REQUIRE(transpile("VALUES 1") == "VALUES (1)");
}

TEST_CASE("DB2 - multi-row VALUES round-trips", "[dialect][db2][values]") {
    REQUIRE(transpile("VALUES (1), (2), (3)") == "VALUES (1), (2), (3)");
}

TEST_CASE("DB2 - VALUES fixpoint", "[dialect][db2][values][roundtrip]") {
    require_fixpoint("VALUES (1, 2, 3)");
    require_fixpoint("VALUES 1");
    require_fixpoint("VALUES (1), (2), (3)");
}

// ============================================================================
// CURRENT DATE / CURRENT TIME / CURRENT TIMESTAMP special registers
// ============================================================================

TEST_CASE("DB2 - two-word CURRENT DATE canonicalizes to CURRENT_DATE", "[dialect][db2][current]") {
    REQUIRE(transpile("SELECT CURRENT DATE FROM t") == "SELECT CURRENT_DATE FROM \"t\"");
}

TEST_CASE("DB2 - two-word CURRENT TIME canonicalizes to CURRENT_TIME", "[dialect][db2][current]") {
    REQUIRE(transpile("SELECT CURRENT TIME FROM t") == "SELECT CURRENT_TIME FROM \"t\"");
}

TEST_CASE("DB2 - two-word CURRENT TIMESTAMP canonicalizes to CURRENT_TIMESTAMP",
          "[dialect][db2][current]") {
    REQUIRE(transpile("SELECT CURRENT TIMESTAMP FROM t") ==
            "SELECT CURRENT_TIMESTAMP FROM \"t\"");
}

TEST_CASE("DB2 - single-token CURRENT_DATE still works", "[dialect][db2][current]") {
    REQUIRE(transpile("SELECT CURRENT_DATE FROM t") == "SELECT CURRENT_DATE FROM \"t\"");
}

TEST_CASE("DB2 - CURRENT special registers fixpoint", "[dialect][db2][current][roundtrip]") {
    require_fixpoint("SELECT CURRENT DATE FROM t");
    require_fixpoint("SELECT CURRENT TIME FROM t");
    require_fixpoint("SELECT CURRENT TIMESTAMP FROM t");
}

// ============================================================================
// NEXT VALUE FOR / PREVIOUS VALUE FOR sequences
// ============================================================================

TEST_CASE("DB2 - NEXT VALUE FOR seq round-trips", "[dialect][db2][sequence]") {
    REQUIRE(transpile("SELECT NEXT VALUE FOR seq FROM t") ==
            "SELECT NEXT VALUE FOR \"seq\" FROM \"t\"");
}

TEST_CASE("DB2 - PREVIOUS VALUE FOR seq round-trips", "[dialect][db2][sequence]") {
    REQUIRE(transpile("SELECT PREVIOUS VALUE FOR seq FROM t") ==
            "SELECT PREVIOUS VALUE FOR \"seq\" FROM \"t\"");
}

TEST_CASE("DB2 - NEXT VALUE FOR inside a standalone VALUES statement", "[dialect][db2][sequence]") {
    REQUIRE(transpile("VALUES NEXT VALUE FOR seq") == "VALUES (NEXT VALUE FOR \"seq\")");
}

TEST_CASE("DB2 - function-style NEXTVAL('seq') also parses and lowers to NEXT VALUE FOR",
          "[dialect][db2][sequence]") {
    // Both surface spellings map onto the same SequenceRefExpr AST node;
    // DB2 always regenerates the SQL:2003 NEXT VALUE FOR form.
    REQUIRE(transpile("SELECT NEXTVAL('seq')") == "SELECT NEXT VALUE FOR \"seq\"");
}

TEST_CASE("DB2 - sequence expression fixpoint", "[dialect][db2][sequence][roundtrip]") {
    require_fixpoint("SELECT NEXT VALUE FOR seq FROM t");
    require_fixpoint("SELECT PREVIOUS VALUE FOR seq FROM t");
    require_fixpoint("VALUES NEXT VALUE FOR seq");
}

TEST_CASE("SQL Server - NEXT VALUE FOR seq round-trips too (shared SQL:2003 syntax)",
          "[dialect][db2][sequence]") {
    REQUIRE(transpile("SELECT NEXT VALUE FOR seq FROM t", SQLDialect::SQLServer) ==
            "SELECT NEXT VALUE FOR [seq] FROM [t]");
}

TEST_CASE("SQL Server - PREVIOUS VALUE FOR (CURRVAL) has no equivalent and throws cleanly",
          "[dialect][db2][sequence][error]") {
    libglot::Arena arena;
    SQLParser parser(arena, "SELECT PREVIOUS VALUE FOR seq FROM t", SQLDialect::SQLServer);
    auto* ast = parser.parse_top_level();
    SQLGenerator gen(SQLDialect::SQLServer);
    REQUIRE_THROWS_AS(gen.generate(ast), std::logic_error);
}
