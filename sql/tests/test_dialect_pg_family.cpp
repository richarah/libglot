// PostgreSQL family conformance (docs/ROADMAP.md stage 2, issue #3
// follow-on): promoting the 8 PostgreSQL-family forks/extensions to
// first-class now that dialect family inheritance exists (stage 1, commit
// 4c5f58e) - Redshift, Greenplum, TimescaleDB, CockroachDB, YugabyteDB,
// Citus, RisingWave, Materialize.
//
// All 8 already inherit `postgres_base()` in dialect_traits.h with *no*
// delta (double-quote identifiers, native ILIKE, LIMIT/OFFSET, TRUE/FALSE
// literals) - verified accurate for every member here, not just assumed:
//   - Redshift: real PostgreSQL fork; ILIKE is native (confirmed).
//   - Greenplum: PostgreSQL fork (predates the Cloudberry/Broadcom split).
//   - TimescaleDB: a PostgreSQL *extension* (not a fork) - it IS PostgreSQL
//     with added functions/hypertables, so it is quoting/literal-identical
//     by construction.
//   - CockroachDB: PostgreSQL wire-compatible from the ground up.
//   - YugabyteDB: PostgreSQL wire-compatible (YSQL layer).
//   - Citus: a PostgreSQL *extension* (distributed-table functions bolted
//     onto an unmodified PostgreSQL parser).
//   - RisingWave: PostgreSQL wire-compatible streaming database.
//   - Materialize: PostgreSQL wire-compatible streaming database.
//
// Real, testable deltas encoded here:
//   - Redshift: DISTSTYLE/DISTKEY/SORTKEY table options (already handled by
//     the generic, non-dialect-gated table-options mechanism - covered with
//     exact-string tests here) and the SUPER column type (falls out of the
//     existing raw-span column-type capture, likewise just needs a test).
//   - Greenplum: DISTRIBUTED BY table option (same generic mechanism).
//   - CockroachDB: `AS OF SYSTEM TIME <expr>` historical-read clause (new
//     TableRef::as_of_system_time, CockroachDB-only) and `UPSERT INTO ...`
//     (new InsertStmt::is_upsert, CockroachDB-only - previously silently
//     downgraded to a plain INSERT on regeneration, which was not even a
//     fixed point; fixed here).
//   - RisingWave: `SELECT ... EMIT CHANGES` streaming modifier (new
//     SelectStmt::emit_changes) and `CREATE MATERIALIZED VIEW` (new
//     CreateViewStmt/DropViewStmt::materialized, restricted to the whole
//     PostgreSQL family since every member's docs confirm this exact
//     syntax).
//   - Materialize: `TAIL`/`SUBSCRIBE` streaming statements (new
//     ShowStmt::is_tail/is_subscribe - this also fixes a pre-existing bug:
//     TAIL had no dedicated generator branch at all and silently
//     regenerated as `SHOW <name>`, not even the same statement) and
//     `CREATE MATERIALIZED VIEW`.
//   - Citus: create_distributed_table(...) needs no code at all - it is an
//     ordinary function call syntactically. Covered with a test proving
//     that, not just asserted.
//
// Deliberately NOT encoded (uncertain, so left inherited rather than
// guessed at - see the rules of honesty in docs/ROADMAP.md):
//   - Redshift's "limited JSON functions" - the `->`/`->>`/`#>`/`#>>` JSON
//     operators are tokenized and generated generically for every dialect,
//     never gated. Whether Redshift's operator support exactly matches
//     PostgreSQL's was not verified either way (Redshift added JSON
//     operator support in a 2022 release train, but the exact version/
//     operator-set boundary is not something this suite asserts) - a test
//     below proves the operators are inherited, unrestricted, on purpose.
//   - DISTINCT ON is (and remains) gated to literally `SQLDialect::
//     PostgreSQL` only, pre-dating this stage - not extended to the family
//     here. Redshift is documented not to support it, but whether
//     CockroachDB/Citus/TimescaleDB/RisingWave/Materialize do was not
//     verified, so the existing single-dialect gate is left untouched.
//   - YugabyteDB's own distributed-timestamp read story was not verified to
//     use the identical `AS OF SYSTEM TIME` syntax, so it was NOT added to
//     the CockroachDB-only check - a test below proves it still throws.

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

void require_fixpoint(const std::string& sql, SQLDialect dialect) {
    const std::string g1 = transpile(sql, dialect);
    const std::string g2 = transpile(g1, dialect);
    REQUIRE(g1 == g2);
}

constexpr SQLDialect kPgFamily[] = {
    SQLDialect::Redshift,   SQLDialect::Greenplum,  SQLDialect::TimescaleDB, SQLDialect::CockroachDB,
    SQLDialect::YugabyteDB, SQLDialect::Citus,      SQLDialect::RisingWave,  SQLDialect::Materialize,
};

} // namespace

// ============================================================================
// Traits: every member is a true PostgreSQL-family row with no delta
// ============================================================================

TEST_CASE("PG family - every member's family() is PostgreSQL", "[dialect][pgfamily][traits]") {
    for (auto d : kPgFamily) {
        INFO("dialect = " << SQLDialectTraits::name(d));
        CHECK(SQLDialectTraits::is_family(d, SQLDialectFamily::PostgreSQL));
    }
}

TEST_CASE("PG family - every member inherits postgres_base() verbatim (no delta)",
          "[dialect][pgfamily][traits]") {
    for (auto d : kPgFamily) {
        INFO("dialect = " << SQLDialectTraits::name(d));
        const auto& f = SQLDialectTraits::get_features(d);
        CHECK(f.identifier_quote == '"');
        CHECK(f.string_quote == '\'');
        CHECK(f.supports_limit_offset);
        CHECK(f.supports_ilike);
        CHECK(std::string(f.true_literal) == "TRUE");
        CHECK(std::string(f.false_literal) == "FALSE");
    }
}

// ============================================================================
// Common inherited behavior: quoting, ILIKE, LIMIT/OFFSET, booleans
// ============================================================================

TEST_CASE("PG family - identifiers double-quoted for every member",
          "[dialect][pgfamily][quoting]") {
    for (auto d : kPgFamily) {
        INFO("dialect = " << SQLDialectTraits::name(d));
        CHECK(transpile("SELECT id, name FROM users", d) ==
              "SELECT \"id\", \"name\" FROM \"users\"");
    }
}

TEST_CASE("PG family - native ILIKE (no LOWER() polyfill) for every member",
          "[dialect][pgfamily][ilike]") {
    for (auto d : kPgFamily) {
        INFO("dialect = " << SQLDialectTraits::name(d));
        CHECK(transpile("SELECT * FROM t WHERE a ILIKE 'x%'", d) ==
              "SELECT * FROM \"t\" WHERE \"a\" ILIKE 'x%'");
    }
}

TEST_CASE("PG family - LIMIT/OFFSET round-trips for every member", "[dialect][pgfamily][limit]") {
    for (auto d : kPgFamily) {
        INFO("dialect = " << SQLDialectTraits::name(d));
        CHECK(transpile("SELECT * FROM t LIMIT 10 OFFSET 5", d) ==
              "SELECT * FROM \"t\" LIMIT 10 OFFSET 5");
    }
}

TEST_CASE("PG family - TRUE/FALSE boolean literals for every member",
          "[dialect][pgfamily][boolean]") {
    for (auto d : kPgFamily) {
        INFO("dialect = " << SQLDialectTraits::name(d));
        CHECK(transpile("SELECT true, false", d) == "SELECT TRUE, FALSE");
    }
}

TEST_CASE("PG family - common inherited behavior is a fixed point",
          "[dialect][pgfamily][roundtrip]") {
    for (auto d : kPgFamily) {
        require_fixpoint("SELECT * FROM t WHERE a ILIKE 'x%' LIMIT 10 OFFSET 5", d);
    }
}

// ============================================================================
// Redshift: DISTSTYLE/DISTKEY/SORTKEY table options, SUPER column type,
// JSON operators inherited unrestricted (deliberately not gated further)
// ============================================================================

TEST_CASE("Redshift - DISTSTYLE KEY DISTKEY(...) SORTKEY(...) trailing table options round-trip",
          "[dialect][redshift][tableoptions]") {
    REQUIRE(transpile("CREATE TABLE t (id INT) DISTSTYLE KEY DISTKEY(id) SORTKEY(ts)",
                      SQLDialect::Redshift) ==
            "CREATE TABLE \"t\" (\"id\" INT) DISTSTYLE KEY DISTKEY(id) SORTKEY(ts)");
}

TEST_CASE("Redshift - column-level DISTKEY/SORTKEY and the SUPER type round-trip",
          "[dialect][redshift][super]") {
    REQUIRE(transpile("CREATE TABLE users (id INT DISTKEY, name VARCHAR(100) SORTKEY, data SUPER)",
                      SQLDialect::Redshift) ==
            "CREATE TABLE \"users\" (\"id\" INT DISTKEY, \"name\" VARCHAR(100) SORTKEY, \"data\" "
            "SUPER)");
}

TEST_CASE("Redshift - table options fixpoint", "[dialect][redshift][roundtrip]") {
    require_fixpoint("CREATE TABLE t (id INT) DISTSTYLE KEY DISTKEY(id) SORTKEY(ts)",
                     SQLDialect::Redshift);
    require_fixpoint(
        "CREATE TABLE users (id INT DISTKEY, name VARCHAR(100) SORTKEY, data SUPER)",
        SQLDialect::Redshift);
}

TEST_CASE("Redshift - JSON arrow operators are inherited, not restricted (uncertainty "
          "deliberately not guessed at - see file header)",
          "[dialect][redshift][json]") {
    REQUIRE(transpile("SELECT data -> 'key' FROM t", SQLDialect::Redshift) ==
            "SELECT \"data\"->'key' FROM \"t\"");
    REQUIRE(transpile("SELECT data ->> 'key' FROM t", SQLDialect::Redshift) ==
            "SELECT \"data\"->>'key' FROM \"t\"");
}

// ============================================================================
// Greenplum: DISTRIBUTED BY table option
// ============================================================================

TEST_CASE("Greenplum - DISTRIBUTED BY (column list) round-trips",
          "[dialect][greenplum][tableoptions]") {
    REQUIRE(transpile("CREATE TABLE sales (id INT, amount DECIMAL) DISTRIBUTED BY (id)",
                      SQLDialect::Greenplum) ==
            "CREATE TABLE \"sales\" (\"id\" INT, \"amount\" DECIMAL) DISTRIBUTED BY(id)");
}

TEST_CASE("Greenplum - DISTRIBUTED RANDOMLY round-trips", "[dialect][greenplum][tableoptions]") {
    REQUIRE(transpile("CREATE TABLE t (id INT) DISTRIBUTED RANDOMLY", SQLDialect::Greenplum) ==
            "CREATE TABLE \"t\" (\"id\" INT) DISTRIBUTED RANDOMLY");
}

TEST_CASE("Greenplum - DISTRIBUTED BY fixpoint", "[dialect][greenplum][roundtrip]") {
    require_fixpoint("CREATE TABLE sales (id INT, amount DECIMAL) DISTRIBUTED BY (id)",
                     SQLDialect::Greenplum);
    require_fixpoint("CREATE TABLE t (id INT) DISTRIBUTED RANDOMLY", SQLDialect::Greenplum);
}

// ============================================================================
// TimescaleDB: essentially PostgreSQL - time_bucket() etc. are ordinary
// function calls, no code needed
// ============================================================================

TEST_CASE("TimescaleDB - time_bucket() round-trips as an ordinary function call",
          "[dialect][timescaledb]") {
    REQUIRE(transpile("SELECT time_bucket('1 hour', ts) FROM events", SQLDialect::TimescaleDB) ==
            "SELECT time_bucket('1 hour', \"ts\") FROM \"events\"");
}

TEST_CASE("TimescaleDB - basic PostgreSQL-compatible query is a fixed point",
          "[dialect][timescaledb][roundtrip]") {
    require_fixpoint("SELECT time_bucket('1 hour', ts), avg(val) FROM events GROUP BY 1",
                     SQLDialect::TimescaleDB);
}

// ============================================================================
// CockroachDB: AS OF SYSTEM TIME, UPSERT INTO
// ============================================================================

TEST_CASE("CockroachDB - AS OF SYSTEM TIME with a string literal round-trips",
          "[dialect][cockroachdb][asof]") {
    REQUIRE(transpile("SELECT * FROM t AS OF SYSTEM TIME '-1m'", SQLDialect::CockroachDB) ==
            "SELECT * FROM \"t\" AS OF SYSTEM TIME '-1m'");
}

TEST_CASE("CockroachDB - AS OF SYSTEM TIME with a function-call expression round-trips",
          "[dialect][cockroachdb][asof]") {
    REQUIRE(transpile("SELECT * FROM t AS OF SYSTEM TIME follower_read_timestamp()",
                      SQLDialect::CockroachDB) ==
            "SELECT * FROM \"t\" AS OF SYSTEM TIME follower_read_timestamp()");
}

TEST_CASE("CockroachDB - AS OF SYSTEM TIME composes with an alias and a JOIN",
          "[dialect][cockroachdb][asof]") {
    REQUIRE(transpile("SELECT * FROM t AS OF SYSTEM TIME '-1m' AS x", SQLDialect::CockroachDB) ==
            "SELECT * FROM \"t\" AS OF SYSTEM TIME '-1m' AS \"x\"");
    REQUIRE(transpile(
                "SELECT * FROM t1 AS OF SYSTEM TIME '-1m' JOIN t2 ON t1.id = t2.id",
                SQLDialect::CockroachDB) ==
            "SELECT * FROM \"t1\" AS OF SYSTEM TIME '-1m' INNER JOIN \"t2\" ON \"t1\".\"id\" = "
            "\"t2\".\"id\"");
}

TEST_CASE("CockroachDB - AS OF SYSTEM TIME throws for every other dialect, including the rest "
          "of the PostgreSQL family (YugabyteDB's support for this exact clause was never "
          "verified - see file header)",
          "[dialect][cockroachdb][asof][error]") {
    libglot::Arena arena;
    SQLParser parser(arena, "SELECT * FROM t AS OF SYSTEM TIME '-1m'", SQLDialect::CockroachDB);
    auto* ast = parser.parse_top_level();
    for (auto d : {SQLDialect::PostgreSQL, SQLDialect::YugabyteDB, SQLDialect::Citus,
                   SQLDialect::MySQL}) {
        SQLGenerator gen(d);
        REQUIRE_THROWS_AS(gen.generate(ast), std::logic_error);
    }
}

TEST_CASE("CockroachDB - AS OF SYSTEM TIME fixpoint", "[dialect][cockroachdb][roundtrip]") {
    require_fixpoint("SELECT * FROM t AS OF SYSTEM TIME '-1m'", SQLDialect::CockroachDB);
    require_fixpoint("SELECT * FROM t AS OF SYSTEM TIME follower_read_timestamp() AS x",
                     SQLDialect::CockroachDB);
}

TEST_CASE("CockroachDB - UPSERT INTO round-trips (VALUES and INSERT ... SELECT forms)",
          "[dialect][cockroachdb][upsert]") {
    REQUIRE(transpile("UPSERT INTO t (a, b) VALUES (1, 2)", SQLDialect::CockroachDB) ==
            "UPSERT INTO \"t\" (\"a\", \"b\") VALUES (1, 2)");
    REQUIRE(transpile("UPSERT INTO t SELECT a FROM u", SQLDialect::CockroachDB) ==
            "UPSERT INTO \"t\" SELECT \"a\" FROM \"u\"");
}

TEST_CASE("CockroachDB - UPSERT INTO throws for every other dialect (not silently downgraded "
          "to a plain INSERT, which would change the statement's meaning)",
          "[dialect][cockroachdb][upsert][error]") {
    libglot::Arena arena;
    SQLParser parser(arena, "UPSERT INTO t (a) VALUES (1)", SQLDialect::CockroachDB);
    auto* ast = parser.parse_top_level();
    for (auto d : {SQLDialect::PostgreSQL, SQLDialect::MySQL, SQLDialect::YugabyteDB}) {
        SQLGenerator gen(d);
        REQUIRE_THROWS_AS(gen.generate(ast), std::logic_error);
    }
}

TEST_CASE("CockroachDB - UPSERT INTO fixpoint", "[dialect][cockroachdb][roundtrip]") {
    require_fixpoint("UPSERT INTO t (a, b) VALUES (1, 2)", SQLDialect::CockroachDB);
    require_fixpoint("UPSERT INTO t SELECT a FROM u", SQLDialect::CockroachDB);
}

// ============================================================================
// YugabyteDB: PostgreSQL-compatible, no additional verified delta
// ============================================================================

TEST_CASE("YugabyteDB - basic PostgreSQL-compatible query is a fixed point",
          "[dialect][yugabytedb][roundtrip]") {
    require_fixpoint("SELECT id, name FROM users WHERE id > 10 ORDER BY id LIMIT 5",
                     SQLDialect::YugabyteDB);
}

// ============================================================================
// Citus: create_distributed_table(...) is an ordinary function call
// ============================================================================

TEST_CASE("Citus - create_distributed_table(...) round-trips as an ordinary function call",
          "[dialect][citus]") {
    REQUIRE(transpile("SELECT create_distributed_table('events', 'device_id')",
                      SQLDialect::Citus) ==
            "SELECT create_distributed_table('events', 'device_id')");
}

TEST_CASE("Citus - create_distributed_table(...) fixpoint", "[dialect][citus][roundtrip]") {
    require_fixpoint("SELECT create_distributed_table('events', 'device_id')", SQLDialect::Citus);
}

// ============================================================================
// RisingWave: EMIT CHANGES, CREATE MATERIALIZED VIEW
// ============================================================================

TEST_CASE("RisingWave - SELECT ... EMIT CHANGES round-trips", "[dialect][risingwave][emit]") {
    REQUIRE(transpile("SELECT * FROM t EMIT CHANGES", SQLDialect::RisingWave) ==
            "SELECT * FROM \"t\" EMIT CHANGES");
}

TEST_CASE("RisingWave - EMIT CHANGES composes after WHERE/GROUP BY",
          "[dialect][risingwave][emit]") {
    REQUIRE(transpile("SELECT a, COUNT(*) FROM t WHERE a > 1 GROUP BY a EMIT CHANGES",
                      SQLDialect::RisingWave) ==
            "SELECT \"a\", COUNT(*) FROM \"t\" WHERE \"a\" > 1 GROUP BY \"a\" EMIT CHANGES");
}

TEST_CASE("RisingWave - a table literally aliased 'emit' is unaffected",
          "[dialect][risingwave][emit]") {
    REQUIRE(transpile("SELECT * FROM t emit", SQLDialect::RisingWave) ==
            "SELECT * FROM \"t\" AS \"emit\"");
}

TEST_CASE("RisingWave - EMIT CHANGES throws for every other dialect",
          "[dialect][risingwave][emit][error]") {
    libglot::Arena arena;
    SQLParser parser(arena, "SELECT * FROM t EMIT CHANGES", SQLDialect::RisingWave);
    auto* ast = parser.parse_top_level();
    for (auto d : {SQLDialect::PostgreSQL, SQLDialect::MySQL, SQLDialect::Materialize}) {
        SQLGenerator gen(d);
        REQUIRE_THROWS_AS(gen.generate(ast), std::logic_error);
    }
}

TEST_CASE("RisingWave - EMIT CHANGES fixpoint", "[dialect][risingwave][roundtrip]") {
    require_fixpoint("SELECT * FROM t EMIT CHANGES", SQLDialect::RisingWave);
    require_fixpoint("SELECT a, COUNT(*) FROM t WHERE a > 1 GROUP BY a EMIT CHANGES",
                     SQLDialect::RisingWave);
}

TEST_CASE("RisingWave - CREATE MATERIALIZED VIEW round-trips",
          "[dialect][risingwave][materialized]") {
    REQUIRE(transpile("CREATE MATERIALIZED VIEW v AS SELECT a FROM t", SQLDialect::RisingWave) ==
            "CREATE MATERIALIZED VIEW \"v\" AS SELECT \"a\" FROM \"t\"");
    REQUIRE(transpile("CREATE OR REPLACE MATERIALIZED VIEW v AS SELECT a FROM t",
                      SQLDialect::RisingWave) ==
            "CREATE OR REPLACE MATERIALIZED VIEW \"v\" AS SELECT \"a\" FROM \"t\"");
}

TEST_CASE("RisingWave - DROP MATERIALIZED VIEW round-trips", "[dialect][risingwave][materialized]") {
    REQUIRE(transpile("DROP MATERIALIZED VIEW v", SQLDialect::RisingWave) ==
            "DROP MATERIALIZED VIEW \"v\"");
    REQUIRE(transpile("DROP MATERIALIZED VIEW IF EXISTS v", SQLDialect::RisingWave) ==
            "DROP MATERIALIZED VIEW IF EXISTS \"v\"");
}

TEST_CASE("RisingWave - CREATE/DROP MATERIALIZED VIEW throws for MySQL/SQLServer/Oracle "
          "(each has its own unmodeled materialized-view syntax, see file header)",
          "[dialect][risingwave][materialized][error]") {
    libglot::Arena arena;
    SQLParser parser(arena, "CREATE MATERIALIZED VIEW v AS SELECT a FROM t",
                     SQLDialect::RisingWave);
    auto* ast = parser.parse_top_level();
    for (auto d : {SQLDialect::MySQL, SQLDialect::SQLServer, SQLDialect::Oracle}) {
        SQLGenerator gen(d);
        REQUIRE_THROWS_AS(gen.generate(ast), std::logic_error);
    }
}

TEST_CASE("RisingWave - CREATE MATERIALIZED VIEW fixpoint", "[dialect][risingwave][roundtrip]") {
    require_fixpoint("CREATE MATERIALIZED VIEW v AS SELECT a FROM t", SQLDialect::RisingWave);
    require_fixpoint("DROP MATERIALIZED VIEW IF EXISTS v", SQLDialect::RisingWave);
}

// ============================================================================
// Materialize: TAIL/SUBSCRIBE, CREATE MATERIALIZED VIEW
// ============================================================================

TEST_CASE("Materialize - TAIL round-trips (deprecated spelling, preserved exactly)",
          "[dialect][materialize][tail]") {
    REQUIRE(transpile("TAIL my_view", SQLDialect::Materialize) == "TAIL \"my_view\"");
}

TEST_CASE("Materialize - SUBSCRIBE round-trips (current spelling, preserved exactly)",
          "[dialect][materialize][tail]") {
    REQUIRE(transpile("SUBSCRIBE my_view", SQLDialect::Materialize) == "SUBSCRIBE \"my_view\"");
}

TEST_CASE("Materialize - TAIL/SUBSCRIBE throw for every other dialect (this also fixes a "
          "pre-existing bug: TAIL used to silently regenerate as SHOW - see file header)",
          "[dialect][materialize][tail][error]") {
    libglot::Arena arena;
    SQLParser parser(arena, "TAIL my_view", SQLDialect::Materialize);
    auto* ast = parser.parse_top_level();
    for (auto d : {SQLDialect::PostgreSQL, SQLDialect::MySQL}) {
        SQLGenerator gen(d);
        REQUIRE_THROWS_AS(gen.generate(ast), std::logic_error);
    }
}

TEST_CASE("Materialize - TAIL/SUBSCRIBE fixpoint", "[dialect][materialize][roundtrip]") {
    require_fixpoint("TAIL my_view", SQLDialect::Materialize);
    require_fixpoint("SUBSCRIBE my_view", SQLDialect::Materialize);
}

TEST_CASE("Materialize - CREATE MATERIALIZED VIEW round-trips",
          "[dialect][materialize][materialized]") {
    REQUIRE(transpile("CREATE MATERIALIZED VIEW v AS SELECT a FROM t", SQLDialect::Materialize) ==
            "CREATE MATERIALIZED VIEW \"v\" AS SELECT \"a\" FROM \"t\"");
}

TEST_CASE("Materialize - CREATE MATERIALIZED VIEW fixpoint", "[dialect][materialize][roundtrip]") {
    require_fixpoint("CREATE MATERIALIZED VIEW v AS SELECT a FROM t", SQLDialect::Materialize);
}
