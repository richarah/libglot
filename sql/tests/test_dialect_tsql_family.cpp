// T-SQL family conformance (docs/ROADMAP.md stage 2, issue #3 follow-on):
// promoting Azure Synapse to first-class now that dialect family
// inheritance exists (stage 1, commit 4c5f58e) - the TSQL family is
// {SQLServer, AzureSynapse} (test_dialect_families.cpp already locks that
// membership down).
//
// Azure Synapse inherits `tsql_base()` with one pre-existing delta this
// pass did not touch: identifier_quote is '"' rather than SQLServer's '['
// (dialect_traits.h, predates this stage-2 pass). Whether Synapse actually
// needs double-quote identifiers over the bracket form both accept in
// standard T-SQL, or this was simply an illustrative "delta overrides a
// base value" example from issue #5, was not re-verified here - it is left
// as-is (extensively tested elsewhere: test_dialect_families.cpp,
// test_merge_extended.cpp, test_temporal_tables.cpp, test_output_clause.cpp
// indirectly via OUTPUT item quoting) rather than churned without new
// evidence either way.
//
// This suite's job is the genuinely new part: proving Azure Synapse
// actually inherits the *behaviors* that are gated on `is_family(d, TSQL)`
// / an explicit {SQLServer, AzureSynapse} check in generator.h - TOP n,
// the OUTPUT clause, MERGE ... WHEN NOT MATCHED BY SOURCE, FOR SYSTEM_TIME
// temporal tables, and the DECLARE @x TYPE = value initializer form - with
// exact-string round-trips and fixpoints, not just "it doesn't crash".
//
// Per the task's explicit conservatism instruction: Azure Synapse is
// reported to lack some T-SQL features in some SKUs/versions (no MERGE in
// older Synapse; no OUTPUT INTO in some configurations). Neither of those
// was confirmed with enough confidence to encode as a restriction here -
// guessing wrong would be worse than staying identical to SQL Server (see
// docs/ROADMAP.md's rules of honesty), so MERGE and OUTPUT are
// deliberately left fully inherited from the TSQL family, exactly as
// SQLServer has them. This is recorded in docs/FEATURE_MATRIX.md as an
// explicit uncertainty, not silently assumed away.

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

} // namespace

// ============================================================================
// Traits
// ============================================================================

TEST_CASE("Azure Synapse - family() is TSQL, no LIMIT/OFFSET, 1/0 booleans",
          "[dialect][azuresynapse][traits]") {
    const auto& f = SQLDialectTraits::get_features(SQLDialect::AzureSynapse);
    CHECK(SQLDialectTraits::is_family(SQLDialect::AzureSynapse, SQLDialectFamily::TSQL));
    CHECK_FALSE(f.supports_limit_offset);
    CHECK_FALSE(f.supports_ilike);
    CHECK(std::string(f.true_literal) == "1");
    CHECK(std::string(f.false_literal) == "0");
}

// ============================================================================
// TOP n (inherited from is_tsql_dialect(), not SQLServer-only)
// ============================================================================

TEST_CASE("Azure Synapse - TOP n round-trips like SQL Server (double-quoted instead of "
          "bracketed)",
          "[dialect][azuresynapse][top]") {
    REQUIRE(transpile("SELECT TOP 10 id FROM t", SQLDialect::AzureSynapse) ==
            "SELECT TOP 10 \"id\" FROM \"t\"");
    REQUIRE(transpile("SELECT TOP 10 id FROM t", SQLDialect::SQLServer) ==
            "SELECT TOP 10 [id] FROM [t]");
}

TEST_CASE("Azure Synapse - LIMIT/OFFSET lowers to TOP (no native LIMIT support, same as "
          "SQL Server)",
          "[dialect][azuresynapse][top]") {
    REQUIRE(transpile("SELECT id FROM t LIMIT 10", SQLDialect::AzureSynapse) ==
            "SELECT TOP 10 \"id\" FROM \"t\"");
}

TEST_CASE("Azure Synapse - TOP n fixpoint", "[dialect][azuresynapse][roundtrip]") {
    require_fixpoint("SELECT TOP 10 id FROM t", SQLDialect::AzureSynapse);
}

// ============================================================================
// OUTPUT clause (inherited from is_tsql_dialect())
// ============================================================================

TEST_CASE("Azure Synapse - OUTPUT clause round-trips like SQL Server",
          "[dialect][azuresynapse][output]") {
    REQUIRE(transpile("INSERT INTO t (a, b) OUTPUT INSERTED.a, INSERTED.b VALUES (1, 2)",
                      SQLDialect::AzureSynapse) ==
            "INSERT INTO \"t\" (\"a\", \"b\") OUTPUT INSERTED.\"a\", INSERTED.\"b\" VALUES (1, 2)");
    REQUIRE(transpile("DELETE FROM t OUTPUT DELETED.* WHERE a = 1", SQLDialect::AzureSynapse) ==
            "DELETE FROM \"t\" OUTPUT DELETED.* WHERE \"a\" = 1");
}

TEST_CASE("Azure Synapse - OUTPUT clause fixpoint", "[dialect][azuresynapse][roundtrip]") {
    require_fixpoint("INSERT INTO t (a) OUTPUT INSERTED.a VALUES (1)", SQLDialect::AzureSynapse);
    require_fixpoint("DELETE FROM t OUTPUT DELETED.* WHERE a = 1", SQLDialect::AzureSynapse);
}

// ============================================================================
// MERGE ... WHEN NOT MATCHED BY SOURCE (inherited - see file header for the
// deliberate non-restriction on older-Synapse MERGE support)
// ============================================================================

TEST_CASE("Azure Synapse - MERGE WHEN NOT MATCHED BY SOURCE round-trips like SQL Server",
          "[dialect][azuresynapse][merge]") {
    const std::string sql = "MERGE INTO t USING u ON t.id = u.id "
                            "WHEN NOT MATCHED BY SOURCE THEN UPDATE SET a = 0";
    REQUIRE(transpile(sql, SQLDialect::AzureSynapse) ==
            "MERGE INTO \"t\" USING \"u\" ON \"t\".\"id\" = \"u\".\"id\" "
            "WHEN NOT MATCHED BY SOURCE THEN UPDATE SET \"a\" = 0");
}

TEST_CASE("Azure Synapse - MERGE fixpoint", "[dialect][azuresynapse][roundtrip]") {
    require_fixpoint(
        "MERGE INTO t USING u ON t.id = u.id WHEN NOT MATCHED BY SOURCE THEN UPDATE SET a = 0",
        SQLDialect::AzureSynapse);
}

// ============================================================================
// FOR SYSTEM_TIME temporal tables (inherited - explicit dialect list in
// generator.h, not a family query, but SQLServer/AzureSynapse/MariaDB are
// exactly the three it names)
// ============================================================================

TEST_CASE("Azure Synapse - FOR SYSTEM_TIME AS OF round-trips like SQL Server",
          "[dialect][azuresynapse][temporal]") {
    REQUIRE(transpile("SELECT * FROM t FOR SYSTEM_TIME AS OF '2020-01-01'",
                      SQLDialect::AzureSynapse) ==
            "SELECT * FROM \"t\" FOR SYSTEM_TIME AS OF '2020-01-01'");
}

TEST_CASE("Azure Synapse - FOR SYSTEM_TIME fixpoint", "[dialect][azuresynapse][roundtrip]") {
    require_fixpoint("SELECT * FROM t FOR SYSTEM_TIME AS OF '2020-01-01'", SQLDialect::AzureSynapse);
    require_fixpoint("SELECT * FROM t FOR SYSTEM_TIME ALL", SQLDialect::AzureSynapse);
}

// ============================================================================
// DECLARE @x TYPE = value initializer form (is_tsql_dialect())
// ============================================================================

TEST_CASE("Azure Synapse - DECLARE @x INT = 5 uses the T-SQL initializer form",
          "[dialect][azuresynapse][declare]") {
    REQUIRE(transpile("DECLARE @x INT = 5", SQLDialect::AzureSynapse) == "DECLARE @x INT = 5");
}

// ============================================================================
// NULLS FIRST/LAST still throws (T-SQL family has no such syntax at all -
// lacks_nulls_ordering() already expresses this as is_family(d, TSQL))
// ============================================================================

TEST_CASE("Azure Synapse - explicit NULLS FIRST/LAST still throws, same as SQL Server",
          "[dialect][azuresynapse][error]") {
    libglot::Arena arena;
    SQLParser parser(arena, "SELECT * FROM t ORDER BY a NULLS FIRST", SQLDialect::PostgreSQL);
    auto* ast = parser.parse_top_level();
    SQLGenerator gen(SQLDialect::AzureSynapse);
    REQUIRE_THROWS_AS(gen.generate(ast), std::logic_error);
}
