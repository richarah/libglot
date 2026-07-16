// Fixed-point property: for a query q and dialect d,
//
//     g1 = generate_d(parse_d(q));  g2 = generate_d(parse_d(g1));
//     REQUIRE(g1 == g2)
//
// i.e. generated SQL must be a fixed point of parse -> generate. This is the
// contract that makes transpilation idempotent and safe to re-run.
//
// The former KNOWN NON-FIXPOINT exclusion list is gone: quoted identifiers
// now round-trip (the parser uses the tokenizer's quote-stripped text,
// copied into the arena), so identifier-bearing statements participate in
// the fixed-point corpus below, alongside dedicated entries for every bug
// the exclusion list used to document (EXTRACT, CURRENT_* keywords, hex
// literals, LIMIT/OFFSET per dialect, derived-table aliases, procedural
// statements, the SQL Server FOR lowering, ...).
//
// SOLE REMAINING EXCLUSION (lexical, by design):
//   - "SELECT ? + ?" under PostgreSQL: '?' lexes as the jsonb key-exists
//     QUESTION operator (TokenizerConfig::question_is_operator), so the
//     positional-parameter form only round-trips in non-PostgreSQL
//     dialects (covered by a dedicated test below).
// ============================================================================

#include <catch2/catch_test_macros.hpp>
#include <libglot/sql/generator.h>
#include <libglot/sql/parser.h>
#include <libglot/util/arena.h>

#include <string>
#include <vector>

using namespace libglot::sql;

namespace {

std::string gen_once(const std::string& sql, SQLDialect d) {
    libglot::Arena arena;
    SQLParser parser(arena, sql, d);
    auto ast = parser.parse_top_level();
    SQLGenerator gen(d);
    return gen.generate(ast);
}

const SQLDialect kDialects[] = {
    SQLDialect::ANSI,
    SQLDialect::PostgreSQL,
    SQLDialect::MySQL,
    SQLDialect::SQLServer,
};

const char* dialect_label(SQLDialect d) {
    switch (d) {
    case SQLDialect::ANSI:
        return "ANSI";
    case SQLDialect::PostgreSQL:
        return "PostgreSQL";
    case SQLDialect::MySQL:
        return "MySQL";
    case SQLDialect::SQLServer:
        return "SQLServer";
    case SQLDialect::Oracle:
        return "Oracle";
    case SQLDialect::DB2:
        return "DB2";
    case SQLDialect::BigQuery:
        return "BigQuery";
    default:
        return "?";
    }
}

void require_fixpoint(const std::string& query, SQLDialect d) {
    INFO("dialect: " << dialect_label(d) << ", query: " << query);
    const std::string g1 = gen_once(query, d);
    const std::string g2 = gen_once(g1, d);
    REQUIRE(g1 == g2);
}

// Queries verified to satisfy the fixed-point property in ANSI, PostgreSQL,
// MySQL, and SQL Server. Identifier-bearing statements are first-class
// citizens now that quoted identifiers round-trip.
const std::vector<std::string>& fixpoint_corpus() {
    static const std::vector<std::string> corpus = {
        // Plain literals and arithmetic
        "SELECT 1",
        "SELECT 1 + 2 * 3",
        "SELECT (1 + 2) * 3",
        "SELECT -5",
        "SELECT 7 % 2",
        "SELECT 2 ^ 3",
        "SELECT 1.5e10",
        "SELECT 'hello'",
        "SELECT 'it''s'",
        "SELECT NULL",
        "SELECT 1, 2, 3",
        "SELECT 'a' || 'b'",
        // Hex / binary literals (regenerated verbatim, not as strings)
        "SELECT 0x1F",
        "SELECT 0b1010",
        // Datetime keyword expressions (not string literals)
        "SELECT CURRENT_TIMESTAMP",
        "SELECT CURRENT_DATE, CURRENT_TIME",
        // Aggregates and scalar functions over literals
        "SELECT COUNT(*)",
        "SELECT SUM(1)",
        "SELECT AVG(2), MIN(3), MAX(4)",
        "SELECT UPPER('abc')",
        "SELECT COALESCE(NULL, 1, 2)",
        "SELECT TRIM('  x  ')",
        "SELECT SUBSTRING('abc', 1, 2)",
        // CAST (including parenthesized target types)
        "SELECT CAST(1 AS INT)",
        "SELECT CAST('2024-01-01' AS DATE)",
        "SELECT CAST(EXTRACT(YEAR FROM d) AS VARCHAR(10)) FROM t",
        // EXTRACT: field as bare keyword, operand as expression
        "SELECT EXTRACT(YEAR FROM d) FROM t",
        "SELECT EXTRACT(YEAR FROM CURRENT_DATE)",
        // CASE
        "SELECT CASE WHEN 1 > 2 THEN 'a' ELSE 'b' END",
        "SELECT CASE WHEN 1 = 1 THEN 1 WHEN 2 = 2 THEN 2 ELSE 3 END",
        "SELECT CASE WHEN a > 1 THEN 'x' ELSE 'y' END FROM t",
        // Predicates: BETWEEN / IN / LIKE / IS / NOT
        "SELECT 1 IN (1, 2, 3)",
        "SELECT 1 NOT IN (2, 3)",
        "SELECT 1 BETWEEN 0 AND 2",
        "SELECT 1 NOT BETWEEN 2 AND 3",
        "SELECT 'abc' LIKE 'a%'",
        "SELECT 'x' NOT LIKE 'y%'",
        "SELECT 1 IS NOT NULL",
        "SELECT NOT 1 = 2",
        "SELECT 1 = 1 AND 2 = 2 OR 3 = 3",
        "SELECT * FROM t WHERE x BETWEEN 1 AND 10",
        "SELECT * FROM t WHERE name LIKE 'a%'",
        // ILIKE: native where supported, LOWER() polyfill elsewhere - both
        // forms are fixed points
        "SELECT * FROM t WHERE name ILIKE 'a%'",
        // Null-safe equality (MySQL / Spark)
        "SELECT a <=> b FROM t",
        // Subqueries and EXISTS
        "SELECT (SELECT 1)",
        "SELECT EXISTS (SELECT 1)",
        "SELECT 1 WHERE 1 IN (SELECT 1)",
        "SELECT 1 WHERE 1 = 1",
        "SELECT DISTINCT 1",
        // Identifier-bearing SELECTs (the former quote-retention bug)
        "SELECT id FROM users",
        "SELECT * FROM users WHERE age > 18 LIMIT 10",
        "SELECT u.id, o.total FROM users u INNER JOIN orders o ON u.id = o.user_id",
        "SELECT region, SUM(amount) FROM sales GROUP BY region HAVING SUM(amount) > 10",
        "SELECT a FROM t UNION SELECT b FROM u",
        // Quoted identifier with an escaped (doubled) quote character
        "SELECT \"emb\"\"edded\" FROM t",
        // Derived-table aliases
        "SELECT a FROM (SELECT a FROM t) x",
        "SELECT a FROM (SELECT a FROM t) AS x WHERE a > 1",
        // ASOF joins (DuckDB / ClickHouse)
        "SELECT * FROM t1 ASOF JOIN t2 ON t1.ts >= t2.ts",
        // LIMIT / OFFSET in every dialect strategy (LIMIT, TOP,
        // OFFSET..FETCH, FETCH FIRST)
        "SELECT * FROM users LIMIT 10 OFFSET 20",
        "SELECT * FROM users ORDER BY id LIMIT 10 OFFSET 20",
        // FOR UPDATE row locking
        "SELECT * FROM t FOR UPDATE",
        "SELECT * FROM t FOR UPDATE OF c NOWAIT",
        "SELECT * FROM t FOR UPDATE OF a, b SKIP LOCKED",
        // CTEs and window functions
        "WITH c AS (SELECT a FROM t) SELECT * FROM c",
        "SELECT ROW_NUMBER() OVER ()",
        "SELECT ROW_NUMBER() OVER (ORDER BY 1)",
        "SELECT ROW_NUMBER() OVER (PARTITION BY a ORDER BY b) FROM t",
        // Set operations
        "SELECT 1 UNION SELECT 2",
        "SELECT 1 UNION ALL SELECT 2",
        "SELECT 1 INTERSECT SELECT 2",
        "SELECT 1 EXCEPT SELECT 2",
        "SELECT 1 UNION SELECT 2 UNION ALL SELECT 3 INTERSECT SELECT 4 EXCEPT SELECT 5",
        // GROUP BY extensions (SQL:1999 T431)
        "SELECT a, SUM(b) FROM t GROUP BY ROLLUP(a, b)",
        "SELECT a, SUM(b) FROM t GROUP BY CUBE(a, b)",
        "SELECT a, b FROM t GROUP BY GROUPING SETS ((a, b), (a), ())",
        "SELECT a FROM t GROUP BY GROUPING SETS (ROLLUP(a, b), (c), ())",
        "SELECT a, b, c FROM t GROUP BY a, ROLLUP(b, c)",
        "SELECT GROUPING(a), SUM(b) FROM t GROUP BY ROLLUP(a)",
        // DML
        "INSERT INTO t (a, b) VALUES (1, 2)",
        "UPDATE t SET a = 1 WHERE b = 2",
        "DELETE FROM t WHERE a = 1",
        "MERGE INTO t USING u ON t.id = u.id WHEN MATCHED THEN UPDATE SET a = 1",
        // DDL
        "CREATE TABLE t (id INT PRIMARY KEY, name VARCHAR(255) NOT NULL)",
        "CREATE TABLE IF NOT EXISTS t (id INT)",
        "CREATE TABLE t (id INT, created TIMESTAMP DEFAULT CURRENT_TIMESTAMP)",
        "DROP TABLE t",
        "TRUNCATE TABLE t",
        // JSON operators on columns and parameters
        "SELECT data -> 'k' FROM t",
        "SELECT @a + @b",
        "SELECT $1 + $2",
        "SELECT :x * :y",
        "SELECT @data -> 'a'",
        "SELECT @data ->> 'b'",
        "SELECT @data @> '{}'",
        // GRANT / REVOKE (object names are not quoted by the generator)
        "GRANT SELECT ON users TO alice",
        "GRANT SELECT, INSERT, UPDATE, DELETE ON users TO alice",
        "GRANT ALL PRIVILEGES ON users TO alice",
        "GRANT SELECT ON users TO alice WITH GRANT OPTION",
        "GRANT SELECT ON users TO alice, bob, charlie",
        "GRANT SELECT ON users TO PUBLIC",
        "GRANT SELECT ON myschema.users TO alice",
        "REVOKE SELECT ON users FROM alice",
        "REVOKE ALL PRIVILEGES ON users FROM alice",
        // Transaction control and procedure calls
        "COMMIT",
        "ROLLBACK",
        "SAVEPOINT sp1",
        "CALL myproc()",
        "CALL myproc(1, 2)",
        "CALL myproc('a', 1 + 2)",
        // Procedural statements
        "SET x = 5",
        "DECLARE x INT",
        "DECLARE x INT DEFAULT 5",
        "OPEN cur",
        "OPEN cur(100, 'active')",
        "FETCH cur INTO x",
        "CLOSE cur",
        "BREAK",
        "CONTINUE",
        "RETURN 42",
        "RETURN",
        "RAISE EXCEPTION 'boom'",
        "RAISE EXCEPTION 'value is %', 5",
        "BEGIN SELECT 1; SELECT 2; END",
        "LOOP SELECT 1; END LOOP",
        "WHILE 1 = 1 LOOP BREAK; END LOOP",
        "IF 1 > 0 THEN SELECT 1; END IF",
        "IF 1 > 0 THEN SELECT 1; ELSE SELECT 2; END IF",
        // Wave 1: VALUES as a FROM-clause table source
        "SELECT * FROM (VALUES (1, 'a'), (2, 'b')) AS v(id, name)",
        "SELECT * FROM (VALUES (1), (2)) AS v",
        // Wave 1: USING / NATURAL joins
        "SELECT * FROM a JOIN b USING (id)",
        "SELECT * FROM a JOIN b USING (id, name)",
        "SELECT * FROM a NATURAL JOIN b",
        "SELECT * FROM a NATURAL LEFT JOIN b",
        // Wave 1: named windows
        "SELECT a, ROW_NUMBER() OVER w FROM t WINDOW w AS (PARTITION BY a ORDER BY b)",
        "SELECT RANK() OVER w, ROW_NUMBER() OVER w FROM t WINDOW w AS (ORDER BY a)",
        // Wave 1: INTERVAL literals (bare string form and value + unit form)
        "SELECT INTERVAL '1 day'",
        "SELECT INTERVAL '2' HOUR",
        "SELECT INTERVAL 7 DAY",
        "SELECT NOW() - INTERVAL '1 day'",
    };
    return corpus;
}

// OUTPUT / RETURNING (T-SQL emits OUTPUT, others RETURNING; both directions
// are fixed points for INSERTED-only / DELETE-DELETED combinations).
// Deliberately NOT run against MySQL like fixpoint_corpus() above is:
// MySQL has never supported RETURNING in any form (docs/ROADMAP.md stage
// 2 / test_dialect_mysql_family.cpp) - generator.h now throws
// std::logic_error for it instead of silently emitting invalid SQL, which
// is why these queries were split out of the shared corpus rather than
// weakening that new check to keep this file passing.
const std::vector<std::string>& output_returning_corpus() {
    static const std::vector<std::string> corpus = {
        "INSERT INTO t (a, b) OUTPUT INSERTED.a, INSERTED.b VALUES (1, 2)",
        "INSERT INTO t (a) VALUES (1) RETURNING id",
        "INSERT INTO t (a) VALUES (1) RETURNING *",
        "UPDATE t SET a = 1 OUTPUT INSERTED.a WHERE b = 2",
        "UPDATE t SET a = 1 WHERE b = 2 RETURNING a",
        "DELETE FROM t OUTPUT DELETED.* WHERE a = 1",
        "DELETE FROM t WHERE a = 1 RETURNING a",
    };
    return corpus;
}

} // namespace

TEST_CASE("Roundtrip property - generated SQL is a fixed point (ANSI)",
          "[roundtrip-property][ansi]") {
    for (const auto& q : fixpoint_corpus()) {
        require_fixpoint(q, SQLDialect::ANSI);
    }
}

TEST_CASE("Roundtrip property - generated SQL is a fixed point (PostgreSQL)",
          "[roundtrip-property][postgresql]") {
    for (const auto& q : fixpoint_corpus()) {
        require_fixpoint(q, SQLDialect::PostgreSQL);
    }
}

TEST_CASE("Roundtrip property - generated SQL is a fixed point (MySQL)",
          "[roundtrip-property][mysql]") {
    for (const auto& q : fixpoint_corpus()) {
        require_fixpoint(q, SQLDialect::MySQL);
    }
}

TEST_CASE("Roundtrip property - generated SQL is a fixed point (SQLServer)",
          "[roundtrip-property][sqlserver]") {
    for (const auto& q : fixpoint_corpus()) {
        require_fixpoint(q, SQLDialect::SQLServer);
    }
}

TEST_CASE("Roundtrip property - OUTPUT/RETURNING is a fixed point (ANSI, PostgreSQL, "
          "SQLServer - not MySQL, which has no RETURNING at all)",
          "[roundtrip-property][output]") {
    for (const auto& q : output_returning_corpus()) {
        require_fixpoint(q, SQLDialect::ANSI);
        require_fixpoint(q, SQLDialect::PostgreSQL);
        require_fixpoint(q, SQLDialect::SQLServer);
    }
}

TEST_CASE("Roundtrip property - FOR loop is a fixed point in every dialect",
          "[roundtrip-property][for]") {
    // Includes SQL Server: the FOR -> DECLARE/WHILE lowering is wrapped in
    // BEGIN..END and re-parses to the identical form.
    const std::string q = "FOR i IN 1..10 LOOP SELECT 1; END LOOP";
    for (auto d : kDialects) {
        require_fixpoint(q, d);
    }
}

TEST_CASE("Roundtrip property - SQL Server specific forms", "[roundtrip-property][sqlserver]") {
    // The generator's own T-SQL output must re-parse to a fixed point.
    require_fixpoint("SELECT TOP 10 * FROM t", SQLDialect::SQLServer);
    require_fixpoint("SELECT TOP 10 PERCENT * FROM t", SQLDialect::SQLServer);
    require_fixpoint("SELECT TOP 5 WITH TIES * FROM t ORDER BY a", SQLDialect::SQLServer);
    require_fixpoint("SELECT * FROM t ORDER BY a OFFSET 5 ROWS FETCH NEXT 3 ROWS ONLY",
                     SQLDialect::SQLServer);
    require_fixpoint("SELECT * INTO #tmp FROM users", SQLDialect::SQLServer);
    require_fixpoint("DECLARE @i INT = 1", SQLDialect::SQLServer);
    require_fixpoint("SET @i = @i + 1", SQLDialect::SQLServer);
    require_fixpoint("RAISERROR('boom', 16, 1)", SQLDialect::SQLServer);
    require_fixpoint("WHILE @i <= 10 BEGIN SELECT 1; END", SQLDialect::SQLServer);
}

TEST_CASE("Roundtrip property - Oracle hierarchical queries", "[roundtrip-property][connect-by]") {
    // CONNECT BY only generates for Oracle/Snowflake (other dialects throw),
    // so these run outside the shared corpus.
    const std::string queries[] = {
        "SELECT id FROM t START WITH parent_id IS NULL CONNECT BY PRIOR id = parent_id",
        "SELECT id FROM t CONNECT BY NOCYCLE PRIOR id = parent_id",
        "SELECT LEVEL, id FROM t CONNECT BY PRIOR id = parent_id ORDER SIBLINGS BY id",
    };
    for (auto d : {SQLDialect::Oracle, SQLDialect::Snowflake}) {
        for (const auto& q : queries) {
            require_fixpoint(q, d);
        }
    }
}

TEST_CASE("Roundtrip property - mixed INSERTED/DELETED OUTPUT (SQL Server only)",
          "[roundtrip-property][output]") {
    // Mixing row images is only expressible in T-SQL; other dialects throw.
    require_fixpoint("UPDATE t SET a = 1 OUTPUT INSERTED.a, DELETED.a WHERE b = 2",
                     SQLDialect::SQLServer);
    require_fixpoint("UPDATE t SET a = 1 OUTPUT INSERTED.a AS new_a, DELETED.a AS old_a",
                     SQLDialect::SQLServer);
}

TEST_CASE("Roundtrip property - FETCH FIRST dialects (Oracle, DB2)",
          "[roundtrip-property][fetch-first]") {
    // supports_limit_offset=false without TOP: FETCH FIRST / OFFSET..FETCH
    for (auto d : {SQLDialect::Oracle, SQLDialect::DB2}) {
        require_fixpoint("SELECT * FROM users LIMIT 10", d);
        require_fixpoint("SELECT * FROM users LIMIT 10 OFFSET 5", d);
        require_fixpoint("SELECT * FROM users FETCH FIRST 10 ROWS ONLY", d);
        require_fixpoint("SELECT * FROM users OFFSET 5 ROWS FETCH NEXT 10 ROWS ONLY", d);
    }
}

TEST_CASE("Roundtrip property - ILIKE polyfill dialects", "[roundtrip-property][ilike]") {
    // Dialects without native ILIKE route through the LOWER() polyfill,
    // which is itself a fixed point.
    for (auto d : {SQLDialect::BigQuery, SQLDialect::MySQL, SQLDialect::SQLServer, SQLDialect::ANSI,
                   SQLDialect::Oracle}) {
        require_fixpoint("SELECT * FROM t WHERE name ILIKE 'a%'", d);
    }
    // Native ILIKE stays ILIKE
    REQUIRE(gen_once("SELECT * FROM t WHERE name ILIKE 'a%'", SQLDialect::PostgreSQL) ==
            "SELECT * FROM \"t\" WHERE \"name\" ILIKE 'a%'");
}

TEST_CASE("Roundtrip property - positional '?' parameters (non-PostgreSQL)",
          "[roundtrip-property][params]") {
    // Sole remaining exclusion: under PostgreSQL '?' lexes as the jsonb
    // QUESTION operator (question_is_operator), so this form is only a
    // fixed point in the other dialects.
    for (auto d : {SQLDialect::ANSI, SQLDialect::MySQL, SQLDialect::SQLServer}) {
        require_fixpoint("SELECT ? + ?", d);
    }
}

TEST_CASE("Roundtrip property - trailing input is rejected, not dropped",
          "[roundtrip-property][trailing]") {
    // These used to parse "successfully" by silently discarding the tail.
    for (auto d : kDialects) {
        INFO("dialect: " << dialect_label(d));
        libglot::Arena a1;
        SQLParser p1(a1, "SELECT 1 SELECT 2", d);
        REQUIRE_THROWS_AS(p1.parse_top_level(), libglot::ParseError);

        libglot::Arena a2;
        SQLParser p2(a2, "SELECT 1; DROP TABLE users; --", d);
        REQUIRE_THROWS_AS(p2.parse_top_level(), libglot::ParseError);
    }

    // Trailing semicolons remain fine
    for (auto d : kDialects) {
        libglot::Arena arena;
        SQLParser parser(arena, "SELECT 1;", d);
        REQUIRE(parser.parse_top_level() != nullptr);
    }
}

TEST_CASE("Roundtrip property - ORDER BY NULLS FIRST/LAST", "[roundtrip-property][nulls]") {
    // No native syntax in MySQL/MariaDB or T-SQL (see test_order_by_nulls.cpp),
    // so this only runs where it is a fixed point.
    for (auto d :
         {SQLDialect::ANSI, SQLDialect::PostgreSQL, SQLDialect::Snowflake, SQLDialect::SQLite}) {
        require_fixpoint("SELECT a FROM t ORDER BY a NULLS FIRST", d);
        require_fixpoint("SELECT a FROM t ORDER BY a DESC NULLS LAST", d);
    }
}

TEST_CASE("Roundtrip property - DISTINCT ON (PostgreSQL only)",
          "[roundtrip-property][distinct-on]") {
    require_fixpoint("SELECT DISTINCT ON (a) a, b FROM t", SQLDialect::PostgreSQL);
    require_fixpoint("SELECT DISTINCT ON (a, b) a, b, c FROM t ORDER BY a, b",
                     SQLDialect::PostgreSQL);
}

TEST_CASE("Roundtrip property - TABLESAMPLE (PG/ANSI; MySQL throws)",
          "[roundtrip-property][tablesample]") {
    for (auto d : {SQLDialect::ANSI, SQLDialect::PostgreSQL}) {
        require_fixpoint("SELECT * FROM t TABLESAMPLE BERNOULLI(10)", d);
        require_fixpoint("SELECT * FROM t AS x TABLESAMPLE SYSTEM(20) REPEATABLE(7)", d);
    }
}

TEST_CASE("Roundtrip property - QUALIFY (Snowflake/BigQuery/DuckDB)",
          "[roundtrip-property][qualify]") {
    for (auto d : {SQLDialect::Snowflake, SQLDialect::BigQuery, SQLDialect::DuckDB}) {
        require_fixpoint("SELECT a FROM t QUALIFY ROW_NUMBER() OVER (ORDER BY a) = 1", d);
    }
}

TEST_CASE("Roundtrip property - upsert forms (each dialect's own syntax only)",
          "[roundtrip-property][upsert]") {
    require_fixpoint("INSERT INTO t (id) VALUES (1) ON CONFLICT (id) DO NOTHING",
                     SQLDialect::PostgreSQL);
    require_fixpoint(
        "INSERT INTO t (id, c) VALUES (1, 1) ON CONFLICT (id) DO UPDATE SET c = EXCLUDED.c",
        SQLDialect::PostgreSQL);
    require_fixpoint("INSERT INTO t (id, c) VALUES (1, 1) ON DUPLICATE KEY UPDATE c = VALUES(c)",
                     SQLDialect::MySQL);
}

// ============================================================================
// Wave 2
// ============================================================================

TEST_CASE("Roundtrip property - sequences (CREATE/DROP/ALTER SEQUENCE, NEXTVAL/CURRVAL)",
          "[roundtrip-property][sequence]") {
    require_fixpoint("CREATE SEQUENCE seq_a START WITH 1 INCREMENT BY 1 MINVALUE 1 MAXVALUE 1000 "
                     "CYCLE CACHE 20",
                     SQLDialect::PostgreSQL);
    require_fixpoint("CREATE SEQUENCE seq_a NO MINVALUE NO MAXVALUE NO CYCLE",
                     SQLDialect::PostgreSQL);
    require_fixpoint("DROP SEQUENCE IF EXISTS seq_a", SQLDialect::PostgreSQL);
    require_fixpoint("ALTER SEQUENCE seq_a RESTART WITH 5", SQLDialect::PostgreSQL);
    require_fixpoint("SELECT NEXTVAL('seq_a')", SQLDialect::PostgreSQL);
    require_fixpoint("SELECT seq_a.NEXTVAL FROM t", SQLDialect::Oracle);
}

TEST_CASE("Roundtrip property - temporal tables (T-SQL / MariaDB FOR SYSTEM_TIME)",
          "[roundtrip-property][temporal]") {
    for (auto d : {SQLDialect::SQLServer, SQLDialect::AzureSynapse, SQLDialect::MariaDB}) {
        require_fixpoint("SELECT * FROM t FOR SYSTEM_TIME AS OF '2020-01-01'", d);
        require_fixpoint("SELECT * FROM t FOR SYSTEM_TIME ALL", d);
    }
}

TEST_CASE("Roundtrip property - MySQL fulltext MATCH ... AGAINST",
          "[roundtrip-property][fulltext]") {
    for (auto d : {SQLDialect::MySQL, SQLDialect::MariaDB}) {
        require_fixpoint("SELECT * FROM t WHERE MATCH (a) AGAINST ('x' IN BOOLEAN MODE)", d);
    }
}

TEST_CASE("Roundtrip property - Snowflake LATERAL FLATTEN", "[roundtrip-property][flatten]") {
    require_fixpoint(
        "SELECT * FROM t, LATERAL FLATTEN(INPUT => t.col, PATH => 'a.b', OUTER => TRUE) f",
        SQLDialect::Snowflake);
}

TEST_CASE("Roundtrip property - BigQuery STRUCT literal and array subscript functions",
          "[roundtrip-property][bigquery]") {
    require_fixpoint("SELECT STRUCT(1 AS a, 'x' AS b)", SQLDialect::BigQuery);
    require_fixpoint("SELECT arr[OFFSET(0)]", SQLDialect::BigQuery);
    require_fixpoint("SELECT arr[ORDINAL(1)]", SQLDialect::BigQuery);
}

TEST_CASE("Roundtrip property - FOR record/REVERSE loop forms", "[roundtrip-property][for]") {
    require_fixpoint("FOR i IN REVERSE 10..1 LOOP SELECT 1; END LOOP", SQLDialect::PostgreSQL);
    require_fixpoint("FOR i IN REVERSE 10..1 LOOP SELECT 1; END LOOP", SQLDialect::Oracle);
    require_fixpoint("FOR i IN REVERSE 10..1 LOOP SELECT 1; END LOOP", SQLDialect::SQLServer);
    require_fixpoint("FOR rec IN SELECT id FROM users LOOP SELECT 1; END LOOP",
                     SQLDialect::PostgreSQL);
    require_fixpoint("FOR rec IN (SELECT id FROM users) LOOP SELECT 1; END LOOP",
                     SQLDialect::Oracle);
}

TEST_CASE("Roundtrip property - CREATE TABLE trailing table options",
          "[roundtrip-property][table-options]") {
    require_fixpoint(
        "CREATE TABLE t (id INT) ENGINE=InnoDB AUTO_INCREMENT=10 DEFAULT CHARSET=utf8mb4 "
        "COMMENT='hi'",
        SQLDialect::MySQL);
    require_fixpoint("CREATE TABLE t (id INT) DISTSTYLE KEY DISTKEY(id) SORTKEY(ts)",
                     SQLDialect::Redshift);
}

TEST_CASE("Roundtrip property - MERGE WHEN NOT MATCHED BY SOURCE (T-SQL)",
          "[roundtrip-property][merge]") {
    require_fixpoint("MERGE INTO t USING u ON t.id = u.id "
                     "WHEN MATCHED THEN UPDATE SET a = 1 "
                     "WHEN NOT MATCHED THEN INSERT (a) VALUES (1) "
                     "WHEN NOT MATCHED BY SOURCE THEN DELETE",
                     SQLDialect::SQLServer);
}

// ============================================================================
// Stage 2 (docs/ROADMAP.md, issue #3 follow-on): promoted PostgreSQL/MySQL/
// T-SQL family members. Full per-dialect exact-string coverage lives in
// test_dialect_pg_family.cpp / test_dialect_mysql_family.cpp /
// test_dialect_tsql_family.cpp - these are corpus-style fixpoint-only
// entries for the property suite.
// ============================================================================

TEST_CASE("Roundtrip property - CockroachDB AS OF SYSTEM TIME / UPSERT",
          "[roundtrip-property][cockroachdb]") {
    require_fixpoint("SELECT * FROM t AS OF SYSTEM TIME '-1m' WHERE a = 1", SQLDialect::CockroachDB);
    require_fixpoint("UPSERT INTO t (a, b) VALUES (1, 2)", SQLDialect::CockroachDB);
}

TEST_CASE("Roundtrip property - RisingWave EMIT CHANGES / CREATE MATERIALIZED VIEW",
          "[roundtrip-property][risingwave]") {
    require_fixpoint("SELECT a, b FROM t WHERE a > 1 EMIT CHANGES", SQLDialect::RisingWave);
    require_fixpoint("CREATE MATERIALIZED VIEW v AS SELECT a FROM t WHERE a > 1",
                     SQLDialect::RisingWave);
}

TEST_CASE("Roundtrip property - Materialize TAIL / SUBSCRIBE / CREATE MATERIALIZED VIEW",
          "[roundtrip-property][materialize]") {
    require_fixpoint("TAIL my_view", SQLDialect::Materialize);
    require_fixpoint("SUBSCRIBE my_view", SQLDialect::Materialize);
    require_fixpoint("CREATE MATERIALIZED VIEW v AS SELECT a FROM t", SQLDialect::Materialize);
}

TEST_CASE("Roundtrip property - Redshift/Greenplum trailing table options",
          "[roundtrip-property][table-options]") {
    require_fixpoint(
        "CREATE TABLE users (id INT DISTKEY, name VARCHAR(100) SORTKEY, data SUPER)",
        SQLDialect::Redshift);
    require_fixpoint("CREATE TABLE sales (id INT, amount DECIMAL) DISTRIBUTED BY (id)",
                     SQLDialect::Greenplum);
}

TEST_CASE("Roundtrip property - MariaDB NEXTVAL/LASTVAL sequences and INSERT/DELETE RETURNING",
          "[roundtrip-property][mariadb]") {
    require_fixpoint("SELECT NEXTVAL(seq_a)", SQLDialect::MariaDB);
    require_fixpoint("SELECT LASTVAL(seq_a)", SQLDialect::MariaDB);
    require_fixpoint("INSERT INTO t (a) VALUES (1) RETURNING id", SQLDialect::MariaDB);
    require_fixpoint("DELETE FROM t WHERE a = 1 RETURNING id", SQLDialect::MariaDB);
}
