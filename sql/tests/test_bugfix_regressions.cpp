// Exact-string regression tests for the bug batch found by the roundtrip
// property tests (see test_roundtrip_property.cpp). One TEST_CASE per bug,
// pinning the exact generated SQL so regressions are caught as string
// diffs, not just property violations.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <libglot/sql/generator.h>
#include <libglot/sql/parser.h>
#include <libglot/util/arena.h>

#include <string>

using namespace libglot::sql;

namespace {

std::string transpile(const std::string& sql, SQLDialect d) {
    libglot::Arena arena;
    SQLParser parser(arena, sql, d);
    auto ast = parser.parse_top_level();
    SQLGenerator gen(d);
    return gen.generate(ast);
}

} // namespace

// ============================================================================
// Bug 1: quoted identifiers retained their quote characters on re-parse
// ============================================================================

TEST_CASE("Regression - quoted identifiers re-parse without doubling", "[regression][quoting]") {
    // Feeding the generator's own output back in must not double the quotes
    REQUIRE(transpile("SELECT \"id\" FROM \"users\"", SQLDialect::PostgreSQL) ==
            "SELECT \"id\" FROM \"users\"");
    REQUIRE(transpile("SELECT `id` FROM `users`", SQLDialect::MySQL) == "SELECT `id` FROM `users`");
    REQUIRE(transpile("SELECT [id] FROM [users]", SQLDialect::SQLServer) ==
            "SELECT [id] FROM [users]");
    // Cross-quoting: double quotes in, brackets out
    REQUIRE(transpile("SELECT \"id\" FROM \"users\"", SQLDialect::SQLServer) ==
            "SELECT [id] FROM [users]");
}

// ============================================================================
// Bug 2: doubled quotes inside quoted identifiers lexed as two identifiers
// ============================================================================

TEST_CASE("Regression - doubled quote inside quoted identifier is unescaped",
          "[regression][quoting]") {
    REQUIRE(transpile("SELECT \"emb\"\"edded\" FROM t", SQLDialect::PostgreSQL) ==
            "SELECT \"emb\"\"edded\" FROM \"t\"");
    // Bracket escaping: foo]bar -> [foo]]bar]
    REQUIRE(transpile("SELECT [foo]]bar] FROM t", SQLDialect::SQLServer) ==
            "SELECT [foo]]bar] FROM [t]");
}

// ============================================================================
// Bug 3: trailing tokens silently ignored; CARET missing; FOR UPDATE dropped
// ============================================================================

TEST_CASE("Regression - trailing input raises ParseError", "[regression][trailing]") {
    libglot::Arena arena;
    SQLParser parser(arena, "SELECT 1 SELECT 2", SQLDialect::ANSI);
    REQUIRE_THROWS_WITH(parser.parse_top_level(),
                        Catch::Matchers::ContainsSubstring("Unexpected trailing input"));
}

TEST_CASE("Regression - CARET operator parses at exponent precedence", "[regression][caret]") {
    // ^ binds tighter than * and looser than unary minus
    REQUIRE(transpile("SELECT 2 ^ 3", SQLDialect::PostgreSQL) == "SELECT 2 ^ 3");
    REQUIRE(transpile("SELECT 2 ^ 3 * 4", SQLDialect::PostgreSQL) == "SELECT 2 ^ 3 * 4");
    REQUIRE(transpile("SELECT 2 * (3 ^ 4)", SQLDialect::PostgreSQL) == "SELECT 2 * 3 ^ 4");
    REQUIRE(transpile("SELECT -2 ^ 3", SQLDialect::PostgreSQL) == "SELECT -2 ^ 3");
}

TEST_CASE("Regression - FOR UPDATE is parsed and regenerated", "[regression][for-update]") {
    REQUIRE(transpile("SELECT * FROM t FOR UPDATE", SQLDialect::PostgreSQL) ==
            "SELECT * FROM \"t\" FOR UPDATE");
    REQUIRE(transpile("SELECT * FROM t FOR UPDATE OF c NOWAIT", SQLDialect::PostgreSQL) ==
            "SELECT * FROM \"t\" FOR UPDATE OF \"c\" NOWAIT");
    REQUIRE(transpile("SELECT * FROM t FOR UPDATE OF a, b SKIP LOCKED", SQLDialect::PostgreSQL) ==
            "SELECT * FROM \"t\" FOR UPDATE OF \"a\", \"b\" SKIP LOCKED");
}

// ============================================================================
// Bug 4: CURRENT_* keywords and hex/binary literals emitted as strings
// ============================================================================

TEST_CASE("Regression - CURRENT_* are keyword expressions, not strings", "[regression][literal]") {
    REQUIRE(transpile("SELECT CURRENT_TIMESTAMP", SQLDialect::ANSI) == "SELECT CURRENT_TIMESTAMP");
    REQUIRE(transpile("SELECT CURRENT_DATE, CURRENT_TIME", SQLDialect::ANSI) ==
            "SELECT CURRENT_DATE, CURRENT_TIME");
    REQUIRE(transpile("CREATE TABLE t (created TIMESTAMP DEFAULT CURRENT_TIMESTAMP)",
                      SQLDialect::PostgreSQL) ==
            "CREATE TABLE \"t\" (\"created\" TIMESTAMP DEFAULT CURRENT_TIMESTAMP)");
}

TEST_CASE("Regression - hex and binary literals emitted verbatim", "[regression][literal]") {
    REQUIRE(transpile("SELECT 0x1F", SQLDialect::ANSI) == "SELECT 0x1F");
    REQUIRE(transpile("SELECT 0b1010", SQLDialect::ANSI) == "SELECT 0b1010");
}

// ============================================================================
// Bug 5: EXTRACT regenerated as EXTRACT('YEAR', 'CURRENT_DATE')
// ============================================================================

TEST_CASE("Regression - EXTRACT keeps field keyword and FROM form", "[regression][extract]") {
    REQUIRE(transpile("SELECT EXTRACT(YEAR FROM d) FROM t", SQLDialect::PostgreSQL) ==
            "SELECT EXTRACT(YEAR FROM \"d\") FROM \"t\"");
    REQUIRE(transpile("SELECT EXTRACT(YEAR FROM CURRENT_DATE)", SQLDialect::ANSI) ==
            "SELECT EXTRACT(YEAR FROM CURRENT_DATE)");
    // CAST around EXTRACT: paren-aware type capture must not eat the FROM clause
    REQUIRE(transpile("SELECT CAST(EXTRACT(YEAR FROM d) AS VARCHAR(10)) FROM t",
                      SQLDialect::PostgreSQL) ==
            "SELECT CAST(EXTRACT(YEAR FROM \"d\") AS VARCHAR(10)) FROM \"t\"");
}

// ============================================================================
// Bug 6: LIMIT/OFFSET dialect strategies (SQLFeatures::supports_limit_offset)
// ============================================================================

TEST_CASE("Regression - SQL Server OFFSET requires OFFSET..FETCH after ORDER BY",
          "[regression][limit]") {
    // No TOP + OFFSET mix
    REQUIRE(
        transpile("SELECT * FROM users ORDER BY id LIMIT 10 OFFSET 20", SQLDialect::SQLServer) ==
        "SELECT * FROM [users] ORDER BY [id] OFFSET 20 ROWS FETCH NEXT 10 ROWS ONLY");
    // Without ORDER BY there is no valid T-SQL offset form: TOP only
    REQUIRE(transpile("SELECT * FROM users LIMIT 10 OFFSET 20", SQLDialect::SQLServer) ==
            "SELECT TOP 10 * FROM [users]");
    // Plain limit stays TOP
    REQUIRE(transpile("SELECT * FROM users LIMIT 10", SQLDialect::SQLServer) ==
            "SELECT TOP 10 * FROM [users]");
}

TEST_CASE("Regression - Oracle and DB2 use FETCH FIRST / OFFSET..FETCH", "[regression][limit]") {
    REQUIRE(transpile("SELECT * FROM users LIMIT 10", SQLDialect::Oracle) ==
            "SELECT * FROM \"users\" FETCH FIRST 10 ROWS ONLY");
    REQUIRE(transpile("SELECT * FROM users LIMIT 10 OFFSET 5", SQLDialect::Oracle) ==
            "SELECT * FROM \"users\" OFFSET 5 ROWS FETCH NEXT 10 ROWS ONLY");
    REQUIRE(transpile("SELECT * FROM users LIMIT 10", SQLDialect::DB2) ==
            "SELECT * FROM \"users\" FETCH FIRST 10 ROWS ONLY");
    REQUIRE(transpile("SELECT * FROM users LIMIT 10 OFFSET 5", SQLDialect::DB2) ==
            "SELECT * FROM \"users\" OFFSET 5 ROWS FETCH NEXT 10 ROWS ONLY");
}

TEST_CASE("Regression - OFFSET..FETCH and FETCH FIRST forms parse everywhere",
          "[regression][limit]") {
    // Parsed as limit/offset, regenerated in the target dialect's strategy
    REQUIRE(transpile("SELECT * FROM users FETCH FIRST 10 ROWS ONLY", SQLDialect::PostgreSQL) ==
            "SELECT * FROM \"users\" LIMIT 10");
    REQUIRE(transpile("SELECT * FROM users OFFSET 5 ROWS FETCH NEXT 10 ROWS ONLY",
                      SQLDialect::MySQL) == "SELECT * FROM `users` LIMIT 10 OFFSET 5");
}

// ============================================================================
// Bug 7: derived-table alias dropped
// ============================================================================

TEST_CASE("Regression - derived-table alias survives", "[regression][alias]") {
    REQUIRE(transpile("SELECT a FROM (SELECT a FROM t) x", SQLDialect::PostgreSQL) ==
            "SELECT \"a\" FROM (SELECT \"a\" FROM \"t\") AS \"x\"");
    REQUIRE(transpile("SELECT a FROM (SELECT a FROM t) AS x WHERE a > 1", SQLDialect::MySQL) ==
            "SELECT `a` FROM (SELECT `a` FROM `t`) AS `x` WHERE `a` > 1");
}

// ============================================================================
// Bug 8: generator's SQL Server TOP output must re-parse
// ============================================================================

TEST_CASE("Regression - TOP n [PERCENT] [WITH TIES] round-trips", "[regression][top]") {
    REQUIRE(transpile("SELECT TOP 10 * FROM t", SQLDialect::SQLServer) ==
            "SELECT TOP 10 * FROM [t]");
    REQUIRE(transpile("SELECT TOP 10 PERCENT * FROM t", SQLDialect::SQLServer) ==
            "SELECT TOP 10 PERCENT * FROM [t]");
    REQUIRE(transpile("SELECT TOP 5 WITH TIES * FROM t ORDER BY a", SQLDialect::SQLServer) ==
            "SELECT TOP 5 WITH TIES * FROM [t] ORDER BY [a]");
}

// ============================================================================
// Bug 9: procedural generation per dialect (WHILE, RAISE, BEGIN..END semis)
// ============================================================================

TEST_CASE("Regression - WHILE emitted per dialect", "[regression][while]") {
    const std::string sql = "WHILE x < 10 DO SET x = x + 1; END WHILE";
    REQUIRE(transpile(sql, SQLDialect::MySQL) == "WHILE `x` < 10 DO SET `x` = `x` + 1; END WHILE");
    REQUIRE(transpile(sql, SQLDialect::PostgreSQL) ==
            "WHILE \"x\" < 10 LOOP SET \"x\" = \"x\" + 1; END LOOP");
    REQUIRE(transpile(sql, SQLDialect::Oracle) ==
            "WHILE \"x\" < 10 LOOP SET \"x\" = \"x\" + 1; END LOOP");
    REQUIRE(transpile(sql, SQLDialect::SQLServer) == "WHILE [x] < 10 BEGIN SET [x] = [x] + 1; END");
}

TEST_CASE("Regression - RAISE per dialect with format args preserved", "[regression][raise]") {
    REQUIRE(transpile("RAISE EXCEPTION 'value is %', 5", SQLDialect::PostgreSQL) ==
            "RAISE EXCEPTION 'value is %', 5");
    REQUIRE(transpile("RAISE EXCEPTION 'boom'", SQLDialect::MySQL) ==
            "SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'boom'");
    REQUIRE(transpile("RAISE EXCEPTION 'boom'", SQLDialect::SQLServer) ==
            "RAISERROR('boom', 16, 1)");
    REQUIRE(transpile("RAISE EXCEPTION 'value is %', 5", SQLDialect::SQLServer) ==
            "RAISERROR('value is %', 16, 1, 5)");
    // T-SQL RAISERROR round-trips verbatim
    REQUIRE(transpile("RAISERROR('boom', 16, 1)", SQLDialect::SQLServer) ==
            "RAISERROR('boom', 16, 1)");
}

TEST_CASE("Regression - BEGIN..END bodies keep statement semicolons", "[regression][begin-end]") {
    REQUIRE(transpile("BEGIN SELECT 1; SELECT 2; END", SQLDialect::PostgreSQL) ==
            "BEGIN SELECT 1; SELECT 2; END");
}

// ============================================================================
// Bug 10: FOR -> SQL Server lowering must re-parse (DECLARE @i INT = 1)
// ============================================================================

TEST_CASE("Regression - T-SQL DECLARE initializer form parses", "[regression][declare]") {
    REQUIRE(transpile("DECLARE @i INT = 1", SQLDialect::SQLServer) == "DECLARE @i INT = 1");
    // Non-T-SQL dialects keep DEFAULT
    REQUIRE(transpile("DECLARE x INT DEFAULT 5", SQLDialect::PostgreSQL) ==
            "DECLARE x INT DEFAULT 5");
}

TEST_CASE("Regression - FOR lowering for SQL Server is re-parseable", "[regression][for]") {
    const std::string lowered =
        "BEGIN DECLARE @i INT = 1; WHILE @i <= 10 BEGIN SELECT 1; SET @i = @i + 1; END; END";
    REQUIRE(transpile("FOR i IN 1..10 LOOP SELECT 1; END LOOP", SQLDialect::SQLServer) == lowered);
    // And the lowering is a fixed point of parse -> generate
    REQUIRE(transpile(lowered, SQLDialect::SQLServer) == lowered);
}

// ============================================================================
// Bug 11: CREATE TABLE IF NOT EXISTS; ILIKE polyfill routing
// ============================================================================

TEST_CASE("Regression - CREATE TABLE IF NOT EXISTS round-trips", "[regression][ddl]") {
    REQUIRE(transpile("CREATE TABLE IF NOT EXISTS t (id INT)", SQLDialect::PostgreSQL) ==
            "CREATE TABLE IF NOT EXISTS \"t\" (\"id\" INT)");
}

TEST_CASE("Regression - ILIKE routed through LOWER() polyfill where unsupported",
          "[regression][ilike]") {
    // BigQuery has no ILIKE
    REQUIRE(transpile("SELECT * FROM t WHERE name ILIKE 'a%'", SQLDialect::BigQuery) ==
            "SELECT * FROM `t` WHERE LOWER(`name`) LIKE LOWER('a%')");
    // Other supports_ilike=false dialects also polyfill instead of raw passthrough
    REQUIRE(transpile("SELECT * FROM t WHERE name ILIKE 'a%'", SQLDialect::SQLServer) ==
            "SELECT * FROM [t] WHERE LOWER([name]) LIKE LOWER('a%')");
    REQUIRE(transpile("SELECT * FROM t WHERE name ILIKE 'a%'", SQLDialect::ANSI) ==
            "SELECT * FROM \"t\" WHERE LOWER(\"name\") LIKE LOWER('a%')");
    // Native ILIKE untouched
    REQUIRE(transpile("SELECT * FROM t WHERE name ILIKE 'a%'", SQLDialect::PostgreSQL) ==
            "SELECT * FROM \"t\" WHERE \"name\" ILIKE 'a%'");
}

// ============================================================================
// Collateral fixes surfaced by strict trailing-input parsing
// ============================================================================

TEST_CASE("Regression - MERGE parses both WHEN clauses", "[regression][merge]") {
    const std::string sql = "MERGE INTO t USING u ON t.id = u.id "
                            "WHEN MATCHED THEN UPDATE SET a = 1 "
                            "WHEN NOT MATCHED THEN INSERT (a) VALUES (1)";
    REQUIRE(transpile(sql, SQLDialect::ANSI) ==
            "MERGE INTO \"t\" USING \"u\" ON \"t\".\"id\" = \"u\".\"id\" "
            "WHEN MATCHED THEN UPDATE SET \"a\" = 1 "
            "WHEN NOT MATCHED THEN INSERT (\"a\") VALUES (1)");
}

TEST_CASE("Regression - OPEN cursor arguments preserved", "[regression][cursor]") {
    REQUIRE(transpile("OPEN cur(100, 'active')", SQLDialect::PostgreSQL) ==
            "OPEN cur(100, 'active')");
}

TEST_CASE("Regression - SELECT INTO target preserved", "[regression][select-into]") {
    REQUIRE(transpile("SELECT * INTO #tmp FROM users", SQLDialect::SQLServer) ==
            "SELECT * INTO [#tmp] FROM [users]");
}

TEST_CASE("Regression - null-safe equality and ASOF joins", "[regression][dialect-ops]") {
    REQUIRE(transpile("SELECT a <=> b FROM t", SQLDialect::MySQL) == "SELECT `a` <=> `b` FROM `t`");
    REQUIRE(transpile("SELECT * FROM t1 ASOF JOIN t2 ON t1.ts >= t2.ts", SQLDialect::ANSI) ==
            "SELECT * FROM \"t1\" ASOF JOIN \"t2\" ON \"t1\".\"ts\" >= \"t2\".\"ts\"");
}
