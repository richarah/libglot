// Dialect family inheritance (GitHub issue #5): dialect_traits.h expresses
// each of the 45 SQLDialect rows as `with(<family>_base(), {delta})` instead
// of a fully hand-written feature vector, and tags every row with the
// SQLDialectFamily it belongs to. This suite locks that mechanism down so a
// future edit to a base profile or a row's delta cannot silently drift
// without a test noticing:
//
//   - every dialect's family() is what docs/ROADMAP.md says it should be
//     (the stage-2 promotion plan is keyed off these family assignments);
//   - is_family() agrees with family() for both members and non-members;
//   - a representative feature is demonstrably inherited from the family
//     base rather than restated per-row;
//   - a full table-driven spot-check reproduces the exact traits
//     (identifier_quote, supports_limit_offset, supports_ilike,
//     true_literal, false_literal) that the pre-refactor hand-written table
//     had for all 45 dialects - so a future edit that nudges a base profile
//     cannot silently change a dialect's generated SQL.

#include <catch2/catch_test_macros.hpp>
#include <libglot/sql/dialect_traits.h>

#include <string>

using namespace libglot::sql;

namespace {

struct ExpectedRow {
    SQLDialect dialect;
    SQLDialectFamily family;
    char identifier_quote;
    bool supports_limit_offset;
    bool supports_ilike;
    const char* true_literal;
    const char* false_literal;
};

// One row per dialect, in enum declaration order - this is the exact
// content of the pre-refactor hand-written kFeatures table (see the
// dump-and-diff verification described in the issue #5 writeup), now
// asserted permanently instead of just eyeballed once.
constexpr ExpectedRow kExpected[] = {
    {SQLDialect::ANSI, SQLDialectFamily::Standard, '"', true, false, "TRUE", "FALSE"},
    {SQLDialect::PostgreSQL, SQLDialectFamily::PostgreSQL, '"', true, true, "TRUE", "FALSE"},
    {SQLDialect::MySQL, SQLDialectFamily::MySQL, '`', true, false, "1", "0"},
    {SQLDialect::SQLite, SQLDialectFamily::SQLite, '"', true, false, "1", "0"},
    {SQLDialect::SQLServer, SQLDialectFamily::TSQL, '[', false, false, "1", "0"},
    {SQLDialect::Oracle, SQLDialectFamily::Oracle, '"', false, false, "TRUE", "FALSE"},

    {SQLDialect::DB2, SQLDialectFamily::DB2, '"', false, false, "TRUE", "FALSE"},
    {SQLDialect::Teradata, SQLDialectFamily::Standard, '"', true, false, "TRUE", "FALSE"},
    {SQLDialect::MariaDB, SQLDialectFamily::MySQL, '`', true, false, "1", "0"},
    {SQLDialect::Informix, SQLDialectFamily::Standard, '"', false, false, "TRUE", "FALSE"},
    {SQLDialect::Firebird, SQLDialectFamily::Standard, '"', true, false, "TRUE", "FALSE"},
    {SQLDialect::SAPHANA, SQLDialectFamily::Standard, '"', true, false, "TRUE", "FALSE"},

    {SQLDialect::Snowflake, SQLDialectFamily::Snowflake, '"', true, true, "TRUE", "FALSE"},
    {SQLDialect::Redshift, SQLDialectFamily::PostgreSQL, '"', true, true, "TRUE", "FALSE"},
    {SQLDialect::BigQuery, SQLDialectFamily::BigQuery, '`', true, false, "TRUE", "FALSE"},
    {SQLDialect::AzureSynapse, SQLDialectFamily::TSQL, '"', false, false, "1", "0"},
    {SQLDialect::Athena, SQLDialectFamily::Standard, '"', true, false, "TRUE", "FALSE"},

    {SQLDialect::DuckDB, SQLDialectFamily::DuckDB, '"', true, true, "TRUE", "FALSE"},
    {SQLDialect::ClickHouse, SQLDialectFamily::Standard, '`', true, true, "1", "0"},
    {SQLDialect::Presto, SQLDialectFamily::Standard, '"', true, false, "TRUE", "FALSE"},
    {SQLDialect::Trino, SQLDialectFamily::Standard, '"', true, false, "TRUE", "FALSE"},
    {SQLDialect::Hive, SQLDialectFamily::Standard, '`', true, false, "TRUE", "FALSE"},
    {SQLDialect::Impala, SQLDialectFamily::Standard, '`', true, false, "TRUE", "FALSE"},
    {SQLDialect::Drill, SQLDialectFamily::Standard, '`', true, false, "TRUE", "FALSE"},
    {SQLDialect::SparkSQL, SQLDialectFamily::Standard, '`', true, false, "TRUE", "FALSE"},
    {SQLDialect::Databricks, SQLDialectFamily::Standard, '`', true, false, "TRUE", "FALSE"},
    {SQLDialect::Dremio, SQLDialectFamily::Standard, '"', true, false, "TRUE", "FALSE"},

    {SQLDialect::Vertica, SQLDialectFamily::Standard, '"', true, true, "TRUE", "FALSE"},
    {SQLDialect::Greenplum, SQLDialectFamily::PostgreSQL, '"', true, true, "TRUE", "FALSE"},
    {SQLDialect::Netezza, SQLDialectFamily::Standard, '"', true, false, "TRUE", "FALSE"},
    {SQLDialect::Exasol, SQLDialectFamily::Standard, '"', true, false, "TRUE", "FALSE"},
    {SQLDialect::MonetDB, SQLDialectFamily::Standard, '"', true, true, "TRUE", "FALSE"},

    {SQLDialect::CockroachDB, SQLDialectFamily::PostgreSQL, '"', true, true, "TRUE", "FALSE"},
    {SQLDialect::YugabyteDB, SQLDialectFamily::PostgreSQL, '"', true, true, "TRUE", "FALSE"},
    {SQLDialect::TiDB, SQLDialectFamily::MySQL, '`', true, false, "TRUE", "FALSE"},
    {SQLDialect::Spanner, SQLDialectFamily::Standard, '`', true, false, "TRUE", "FALSE"},
    {SQLDialect::Citus, SQLDialectFamily::PostgreSQL, '"', true, true, "TRUE", "FALSE"},

    {SQLDialect::TimescaleDB, SQLDialectFamily::PostgreSQL, '"', true, true, "TRUE", "FALSE"},
    {SQLDialect::QuestDB, SQLDialectFamily::Standard, '"', true, false, "TRUE", "FALSE"},
    {SQLDialect::SingleStore, SQLDialectFamily::MySQL, '`', true, false, "TRUE", "FALSE"},

    {SQLDialect::RisingWave, SQLDialectFamily::PostgreSQL, '"', true, true, "TRUE", "FALSE"},
    {SQLDialect::Materialize, SQLDialectFamily::PostgreSQL, '"', true, true, "TRUE", "FALSE"},

    {SQLDialect::H2, SQLDialectFamily::Standard, '"', true, false, "TRUE", "FALSE"},
    {SQLDialect::HSQLDB, SQLDialectFamily::Standard, '"', true, false, "TRUE", "FALSE"},
    {SQLDialect::Derby, SQLDialectFamily::Standard, '"', false, false, "TRUE", "FALSE"},
};

} // namespace

// ============================================================================
// The expectation table itself covers every dialect exactly once
// ============================================================================

TEST_CASE("dialect families - expectation table covers every dialect exactly once",
          "[dialect][families]") {
    REQUIRE(sizeof(kExpected) / sizeof(kExpected[0]) == static_cast<size_t>(SQLDialect::COUNT));
    for (size_t i = 0; i < static_cast<size_t>(SQLDialect::COUNT); ++i) {
        REQUIRE(static_cast<size_t>(kExpected[i].dialect) == i);
    }
}

// ============================================================================
// Table-driven spot-check: every one of the 45 rows has the exact traits
// the pre-refactor hand-written table had.
// ============================================================================

TEST_CASE("dialect families - every dialect's traits match the pre-refactor table exactly",
          "[dialect][families][traits]") {
    for (const auto& row : kExpected) {
        INFO("dialect = " << SQLDialectTraits::name(row.dialect));
        const auto& f = SQLDialectTraits::get_features(row.dialect);
        CHECK(f.identifier_quote == row.identifier_quote);
        CHECK(f.string_quote == '\'');
        CHECK(f.supports_limit_offset == row.supports_limit_offset);
        CHECK(f.supports_ilike == row.supports_ilike);
        CHECK(std::string(f.true_literal) == row.true_literal);
        CHECK(std::string(f.false_literal) == row.false_literal);
    }
}

// ============================================================================
// family() matches docs/ROADMAP.md's stage-2 promotion plan
// ============================================================================

TEST_CASE("dialect families - family() matches the expectation table", "[dialect][families]") {
    for (const auto& row : kExpected) {
        INFO("dialect = " << SQLDialectTraits::name(row.dialect));
        CHECK(SQLDialectTraits::family(row.dialect) == row.family);
    }
}

// ============================================================================
// is_family() agrees with family() for members and non-members alike
// ============================================================================

TEST_CASE("dialect families - is_family() is consistent with family() for every dialect against "
          "every family",
          "[dialect][families]") {
    constexpr SQLDialectFamily kAllFamilies[] = {
        SQLDialectFamily::Standard,   SQLDialectFamily::PostgreSQL, SQLDialectFamily::MySQL,
        SQLDialectFamily::TSQL,       SQLDialectFamily::Oracle,     SQLDialectFamily::DB2,
        SQLDialectFamily::BigQuery,   SQLDialectFamily::DuckDB,     SQLDialectFamily::Snowflake,
        SQLDialectFamily::SQLite,
    };
    for (const auto& row : kExpected) {
        for (auto fam : kAllFamilies) {
            CHECK(SQLDialectTraits::is_family(row.dialect, fam) == (row.family == fam));
        }
    }
}

// ============================================================================
// PostgreSQL family membership matches docs/ROADMAP.md stage 2 exactly
// ============================================================================

TEST_CASE("dialect families - PostgreSQL family is PostgreSQL + its documented forks",
          "[dialect][families][postgresql]") {
    constexpr SQLDialect kMembers[] = {
        SQLDialect::PostgreSQL,  SQLDialect::Redshift,    SQLDialect::Greenplum,
        SQLDialect::CockroachDB, SQLDialect::YugabyteDB,  SQLDialect::Citus,
        SQLDialect::TimescaleDB, SQLDialect::RisingWave,  SQLDialect::Materialize,
    };
    size_t member_count = 0;
    for (size_t i = 0; i < static_cast<size_t>(SQLDialect::COUNT); ++i) {
        const auto d = static_cast<SQLDialect>(i);
        bool expected_member = false;
        for (auto m : kMembers) {
            expected_member = expected_member || (m == d);
        }
        if (expected_member) {
            ++member_count;
        }
        CHECK(SQLDialectTraits::is_family(d, SQLDialectFamily::PostgreSQL) == expected_member);
    }
    REQUIRE(member_count == sizeof(kMembers) / sizeof(kMembers[0]));
}

// ============================================================================
// MySQL family membership matches docs/ROADMAP.md stage 2 exactly
// ============================================================================

TEST_CASE("dialect families - MySQL family is MySQL + its documented forks",
          "[dialect][families][mysql]") {
    constexpr SQLDialect kMembers[] = {
        SQLDialect::MySQL,
        SQLDialect::MariaDB,
        SQLDialect::TiDB,
        SQLDialect::SingleStore,
    };
    for (size_t i = 0; i < static_cast<size_t>(SQLDialect::COUNT); ++i) {
        const auto d = static_cast<SQLDialect>(i);
        bool expected_member = false;
        for (auto m : kMembers) {
            expected_member = expected_member || (m == d);
        }
        CHECK(SQLDialectTraits::is_family(d, SQLDialectFamily::MySQL) == expected_member);
    }
}

// ============================================================================
// T-SQL family membership matches docs/ROADMAP.md stage 2 exactly
// ============================================================================

TEST_CASE("dialect families - TSQL family is exactly SQLServer + AzureSynapse",
          "[dialect][families][tsql]") {
    for (size_t i = 0; i < static_cast<size_t>(SQLDialect::COUNT); ++i) {
        const auto d = static_cast<SQLDialect>(i);
        const bool expected_member = (d == SQLDialect::SQLServer || d == SQLDialect::AzureSynapse);
        CHECK(SQLDialectTraits::is_family(d, SQLDialectFamily::TSQL) == expected_member);
    }
}

// ============================================================================
// Flagship-only families (currently a family of one; no promoted member)
// ============================================================================

TEST_CASE("dialect families - Oracle/DB2/BigQuery/DuckDB/Snowflake/SQLite are each a family of "
          "one today",
          "[dialect][families][flagship]") {
    struct FlagshipOnly {
        SQLDialect dialect;
        SQLDialectFamily family;
    };
    constexpr FlagshipOnly kFlagships[] = {
        {SQLDialect::Oracle, SQLDialectFamily::Oracle},
        {SQLDialect::DB2, SQLDialectFamily::DB2},
        {SQLDialect::BigQuery, SQLDialectFamily::BigQuery},
        {SQLDialect::DuckDB, SQLDialectFamily::DuckDB},
        {SQLDialect::Snowflake, SQLDialectFamily::Snowflake},
        {SQLDialect::SQLite, SQLDialectFamily::SQLite},
    };
    for (const auto& flagship : kFlagships) {
        size_t member_count = 0;
        for (size_t i = 0; i < static_cast<size_t>(SQLDialect::COUNT); ++i) {
            if (SQLDialectTraits::is_family(static_cast<SQLDialect>(i), flagship.family)) {
                ++member_count;
            }
        }
        INFO("family = " << static_cast<int>(flagship.family));
        CHECK(member_count == 1);
        CHECK(SQLDialectTraits::is_family(flagship.dialect, flagship.family));
    }
}

// ============================================================================
// A representative feature is genuinely inherited from the family base,
// not restated per-row: MariaDB and TiDB/SingleStore all pick up MySQL's
// backtick identifier quoting from mysql_base() even though only MariaDB
// also inherits the 1/0 literal convention (TiDB/SingleStore override it).
// ============================================================================

TEST_CASE("dialect families - MySQL family members inherit backtick quoting from the family base",
          "[dialect][families][inheritance]") {
    CHECK(SQLDialectTraits::get_features(SQLDialect::MySQL).identifier_quote == '`');
    CHECK(SQLDialectTraits::get_features(SQLDialect::MariaDB).identifier_quote == '`');
    CHECK(SQLDialectTraits::get_features(SQLDialect::TiDB).identifier_quote == '`');
    CHECK(SQLDialectTraits::get_features(SQLDialect::SingleStore).identifier_quote == '`');

    // MariaDB inherits the 1/0 literal convention unchanged from
    // mysql_base(); TiDB and SingleStore override it back to TRUE/FALSE -
    // demonstrating a delta actually overriding a base value.
    CHECK(std::string(SQLDialectTraits::get_features(SQLDialect::MariaDB).true_literal) == "1");
    CHECK(std::string(SQLDialectTraits::get_features(SQLDialect::TiDB).true_literal) == "TRUE");
    CHECK(std::string(SQLDialectTraits::get_features(SQLDialect::SingleStore).true_literal) ==
          "TRUE");
}

TEST_CASE("dialect families - PostgreSQL family members inherit ILIKE support from the family base",
          "[dialect][families][inheritance]") {
    CHECK(SQLDialectTraits::get_features(SQLDialect::PostgreSQL).supports_ilike);
    CHECK(SQLDialectTraits::get_features(SQLDialect::Redshift).supports_ilike);
    CHECK(SQLDialectTraits::get_features(SQLDialect::CockroachDB).supports_ilike);
    CHECK(SQLDialectTraits::get_features(SQLDialect::Citus).supports_ilike);
}

TEST_CASE("dialect families - T-SQL family members inherit no-LIMIT/OFFSET from the family base, "
          "but AzureSynapse overrides the identifier quote",
          "[dialect][families][inheritance]") {
    CHECK_FALSE(SQLDialectTraits::get_features(SQLDialect::SQLServer).supports_limit_offset);
    CHECK_FALSE(SQLDialectTraits::get_features(SQLDialect::AzureSynapse).supports_limit_offset);
    CHECK(SQLDialectTraits::get_features(SQLDialect::SQLServer).identifier_quote == '[');
    CHECK(SQLDialectTraits::get_features(SQLDialect::AzureSynapse).identifier_quote == '"');
}
