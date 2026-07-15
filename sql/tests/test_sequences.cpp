// Wave 2: CREATE/DROP/ALTER SEQUENCE and NEXTVAL/CURRVAL sequence references.
//
// Two surface spellings for sequence value access map onto the same AST
// (SequenceRefExpr): the function-style `nextval('seq')` / `currval('seq')`
// (PostgreSQL/DB2/MariaDB/...) and Oracle's member-style `seq.NEXTVAL` /
// `seq.CURRVAL`. Generation is dialect-driven regardless of which spelling
// was parsed; MySQL/SQLite have no sequence object at all and throw.

#include <catch2/catch_test_macros.hpp>
#include <libglot/sql/parser.h>
#include <libglot/sql/generator.h>
#include <libglot/util/arena.h>

#include <string>

using namespace libglot::sql;

namespace {

std::string transpile(const std::string& sql, SQLDialect parse_dialect, SQLDialect gen_dialect) {
    libglot::Arena arena;
    SQLParser parser(arena, sql, parse_dialect);
    auto* ast = parser.parse_top_level();
    SQLGenerator gen(gen_dialect);
    return gen.generate(ast);
}

std::string transpile(const std::string& sql, SQLDialect dialect) {
    return transpile(sql, dialect, dialect);
}

} // namespace

// ============================================================================
// CREATE SEQUENCE
// ============================================================================

TEST_CASE("CREATE SEQUENCE - minimal form", "[sequence][create]") {
    REQUIRE(transpile("CREATE SEQUENCE seq_a", SQLDialect::PostgreSQL)
            == "CREATE SEQUENCE \"seq_a\"");
}

TEST_CASE("CREATE SEQUENCE - IF NOT EXISTS", "[sequence][create]") {
    REQUIRE(transpile("CREATE SEQUENCE IF NOT EXISTS seq_a", SQLDialect::PostgreSQL)
            == "CREATE SEQUENCE IF NOT EXISTS \"seq_a\"");
}

TEST_CASE("CREATE SEQUENCE - every clause present", "[sequence][create]") {
    REQUIRE(transpile(
                "CREATE SEQUENCE seq_a START WITH 1 INCREMENT BY 1 "
                "MINVALUE 1 MAXVALUE 1000 CYCLE CACHE 20",
                SQLDialect::PostgreSQL)
            == "CREATE SEQUENCE \"seq_a\" START WITH 1 INCREMENT BY 1 "
               "MINVALUE 1 MAXVALUE 1000 CYCLE CACHE 20");
}

TEST_CASE("CREATE SEQUENCE - NO MINVALUE / NO MAXVALUE / NO CYCLE", "[sequence][create]") {
    REQUIRE(transpile("CREATE SEQUENCE seq_b NO MINVALUE NO MAXVALUE NO CYCLE",
                       SQLDialect::PostgreSQL)
            == "CREATE SEQUENCE \"seq_b\" NO MINVALUE NO MAXVALUE NO CYCLE");
}

TEST_CASE("CREATE SEQUENCE - START WITH without WITH keyword is also accepted", "[sequence][create]") {
    // Some dialects omit the WITH after START; both spellings parse to the
    // same AST, so both regenerate identically (canonical form always
    // includes WITH).
    libglot::Arena arena;
    SQLParser parser(arena, "CREATE SEQUENCE seq_a START 5", SQLDialect::PostgreSQL);
    auto* ast = parser.parse_top_level();
    REQUIRE(ast->type == SQLNodeKind::CREATE_SEQUENCE_STMT);
    SQLGenerator gen(SQLDialect::PostgreSQL);
    REQUIRE(gen.generate(ast) == "CREATE SEQUENCE \"seq_a\" START WITH 5");
}

TEST_CASE("CREATE SEQUENCE - fixed point", "[sequence][create][roundtrip]") {
    const std::string sql =
        "CREATE SEQUENCE \"seq_a\" START WITH 1 INCREMENT BY 1 "
        "MINVALUE 1 MAXVALUE 1000 CYCLE CACHE 20";
    REQUIRE(transpile(sql, SQLDialect::PostgreSQL) == sql);
}

// ============================================================================
// DROP SEQUENCE
// ============================================================================

TEST_CASE("DROP SEQUENCE - plain", "[sequence][drop]") {
    REQUIRE(transpile("DROP SEQUENCE seq_a", SQLDialect::PostgreSQL) == "DROP SEQUENCE \"seq_a\"");
}

TEST_CASE("DROP SEQUENCE - IF EXISTS", "[sequence][drop]") {
    REQUIRE(transpile("DROP SEQUENCE IF EXISTS seq_a", SQLDialect::PostgreSQL)
            == "DROP SEQUENCE IF EXISTS \"seq_a\"");
}

// ============================================================================
// ALTER SEQUENCE ... RESTART [WITH n]
// ============================================================================

TEST_CASE("ALTER SEQUENCE - RESTART bare", "[sequence][alter]") {
    REQUIRE(transpile("ALTER SEQUENCE seq_a RESTART", SQLDialect::PostgreSQL)
            == "ALTER SEQUENCE \"seq_a\" RESTART");
}

TEST_CASE("ALTER SEQUENCE - RESTART WITH n", "[sequence][alter]") {
    REQUIRE(transpile("ALTER SEQUENCE seq_a RESTART WITH 5", SQLDialect::PostgreSQL)
            == "ALTER SEQUENCE \"seq_a\" RESTART WITH 5");
}

TEST_CASE("ALTER SEQUENCE - missing RESTART is a clean ParseError", "[sequence][alter][error]") {
    libglot::Arena arena;
    SQLParser parser(arena, "ALTER SEQUENCE seq_a", SQLDialect::PostgreSQL);
    REQUIRE_THROWS_AS(parser.parse_top_level(), libglot::ParseError);
}

// ============================================================================
// NEXTVAL / CURRVAL - function-style (PostgreSQL/DB2/MariaDB/...)
// ============================================================================

TEST_CASE("NEXTVAL/CURRVAL - function-style round-trip", "[sequence][nextval]") {
    REQUIRE(transpile("SELECT NEXTVAL('seq_a')", SQLDialect::PostgreSQL)
            == "SELECT NEXTVAL('seq_a')");
    REQUIRE(transpile("SELECT CURRVAL('seq_a')", SQLDialect::PostgreSQL)
            == "SELECT CURRVAL('seq_a')");
}

TEST_CASE("NEXTVAL/CURRVAL - AST shape", "[sequence][nextval]") {
    libglot::Arena arena;
    SQLParser parser(arena, "SELECT NEXTVAL('seq_a')", SQLDialect::PostgreSQL);
    auto* ast = static_cast<SelectStmt*>(parser.parse_top_level());
    REQUIRE(ast->columns.size() == 1);
    REQUIRE(ast->columns[0]->type == SQLNodeKind::SEQUENCE_REF_EXPR);
    auto* seq = static_cast<SequenceRefExpr*>(ast->columns[0]);
    REQUIRE(seq->sequence_name == "seq_a");
    REQUIRE(seq->is_next == true);
}

// ============================================================================
// seq.NEXTVAL / seq.CURRVAL - Oracle member-style
// ============================================================================

TEST_CASE("Sequence - Oracle member-style round-trip", "[sequence][oracle]") {
    REQUIRE(transpile("SELECT seq_a.NEXTVAL FROM t", SQLDialect::Oracle)
            == "SELECT \"seq_a\".NEXTVAL FROM \"t\"");
    REQUIRE(transpile("SELECT seq_a.CURRVAL FROM t", SQLDialect::Oracle)
            == "SELECT \"seq_a\".CURRVAL FROM \"t\"");
}

TEST_CASE("Sequence - Oracle member-style transpiles to function-style for PostgreSQL",
          "[sequence][oracle][transpile]") {
    REQUIRE(transpile("SELECT seq_a.NEXTVAL FROM t", SQLDialect::Oracle, SQLDialect::PostgreSQL)
            == "SELECT NEXTVAL('seq_a') FROM \"t\"");
}

TEST_CASE("Sequence - member-style syntax is Oracle-only at parse time", "[sequence][oracle]") {
    // Outside Oracle, "seq.NEXTVAL" parses as an ordinary qualified column
    // reference, not a sequence access - so a column genuinely named
    // NEXTVAL still works everywhere else.
    libglot::Arena arena;
    SQLParser parser(arena, "SELECT seq_a.NEXTVAL FROM t", SQLDialect::PostgreSQL);
    auto* ast = static_cast<SelectStmt*>(parser.parse_top_level());
    REQUIRE(ast->columns[0]->type == SQLNodeKind::COLUMN);
}

// ============================================================================
// Unsupported dialects: MySQL/SQLite have no sequence object
// ============================================================================

TEST_CASE("Sequence - MySQL has no sequence object (clean std::logic_error)",
          "[sequence][error]") {
    REQUIRE_THROWS_AS(transpile("CREATE SEQUENCE seq_a", SQLDialect::MySQL), std::logic_error);
    REQUIRE_THROWS_AS(transpile("DROP SEQUENCE seq_a", SQLDialect::MySQL), std::logic_error);
    REQUIRE_THROWS_AS(transpile("ALTER SEQUENCE seq_a RESTART", SQLDialect::MySQL), std::logic_error);
    REQUIRE_THROWS_AS(transpile("SELECT NEXTVAL('seq_a')", SQLDialect::MySQL), std::logic_error);
}
