#pragma once

#include <libglot/dialect/traits.h>
#include <optional>
#include <string_view>

namespace libglot::sql {

/// ============================================================================
/// SQL Dialect Enumeration
/// ============================================================================

enum class SQLDialect : uint8_t {
    // Core SQL Standards & Major Databases
    ANSI,       // ANSI SQL standard
    PostgreSQL, // PostgreSQL
    MySQL,      // MySQL
    SQLite,     // SQLite
    SQLServer,  // Microsoft SQL Server (T-SQL)
    Oracle,     // Oracle Database (PL/SQL)

    // Enterprise Databases
    DB2,      // IBM DB2
    Teradata, // Teradata
    MariaDB,  // MariaDB
    Informix, // IBM Informix
    Firebird, // Firebird
    SAPHANA,  // SAP HANA

    // Cloud Data Warehouses
    Snowflake,    // Snowflake
    Redshift,     // Amazon Redshift
    BigQuery,     // Google BigQuery
    AzureSynapse, // Azure Synapse Analytics
    Athena,       // AWS Athena

    // Modern Analytics Databases
    DuckDB,     // DuckDB
    ClickHouse, // ClickHouse
    Presto,     // Presto
    Trino,      // Trino (formerly PrestoSQL)
    Hive,       // Apache Hive
    Impala,     // Apache Impala
    Drill,      // Apache Drill
    SparkSQL,   // Apache Spark SQL
    Databricks, // Databricks SQL
    Dremio,     // Dremio

    // MPP & Columnar Databases
    Vertica,   // Vertica
    Greenplum, // Greenplum
    Netezza,   // IBM Netezza
    Exasol,    // Exasol
    MonetDB,   // MonetDB

    // Distributed SQL Databases
    CockroachDB, // CockroachDB
    YugabyteDB,  // YugabyteDB
    TiDB,        // TiDB
    Spanner,     // Google Cloud Spanner
    Citus,       // Citus (PostgreSQL extension)

    // Time-Series & Real-Time Databases
    TimescaleDB, // TimescaleDB (PostgreSQL extension)
    QuestDB,     // QuestDB
    SingleStore, // SingleStore (formerly MemSQL)

    // Streaming & Materialized Views
    RisingWave,  // RisingWave
    Materialize, // Materialize

    // Embedded & Lightweight
    H2,     // H2 Database
    HSQLDB, // HSQLDB
    Derby,  // Apache Derby

    COUNT
};

/// ============================================================================
/// SQL Dialect Families
/// ============================================================================
///
/// A family groups dialects that share a real syntax/engine lineage - not
/// merely dialects that happen to have identical SQLFeatures values today.
/// Only families the code actually distinguishes are represented here (see
/// docs/ROADMAP.md stage 1/2): ANSI-like dialects with no special quoting or
/// literal conventions fall into `Standard`; PostgreSQL, MySQL, and T-SQL
/// each have several real forks/wire-compatible engines in the enum above;
/// Oracle, DB2, BigQuery, DuckDB, Snowflake, and SQLite are each currently a
/// family of one (the flagship engine itself) with no promoted member yet,
/// but are still broken out because generator.h/parser.h already gate
/// dialect-specific syntax on that single dialect by name. Hive and Presto
/// were considered (per the original design sketch) but dropped: nothing in
/// generator.h or parser.h branches on them today, so a dedicated family
/// would be unused scaffolding rather than something "the current code
/// actually distinguishes".
enum class SQLDialectFamily : uint8_t {
    Standard,   // ANSI and every dialect with no distinguishing lineage below
    PostgreSQL, // PostgreSQL and its wire/syntax-compatible forks
    MySQL,      // MySQL and its forks
    TSQL,       // Microsoft SQL Server and its derivatives
    Oracle,
    DB2,
    BigQuery,
    DuckDB,
    Snowflake,
    SQLite,
};

/// ============================================================================
/// SQL Dialect Features
/// ============================================================================

struct SQLFeatures {
    /// Family this dialect belongs to (see SQLDialectFamily).
    SQLDialectFamily family = SQLDialectFamily::Standard;

    /// Identifier quoting character (" for standard, ` for MySQL, [ for SQL Server)
    char identifier_quote = '"';

    /// String literal quote character (always ' in SQL)
    char string_quote = '\'';

    /// Supports LIMIT/OFFSET syntax
    bool supports_limit_offset = true;

    /// Supports ILIKE (case-insensitive LIKE)
    bool supports_ilike = false;

    /// TRUE/FALSE literal representation
    const char* true_literal = "TRUE";
    const char* false_literal = "FALSE";
};

/// ============================================================================
/// Family base profiles + delta combinator
/// ============================================================================
///
/// Each base() function is the exact feature vector of the family's
/// flagship dialect. Every one of the 45 rows in kFeatures below is then
/// expressed as `with(some_base(), {only the fields that differ})`, so a
/// row states only its delta from family - not a full copy of every field.
/// `with()` uses std::optional deltas (rather than requiring every row to
/// restate every field) so "no override" and "override to a falsy/zero
/// value" are both expressible; std::optional is fully constexpr-usable for
/// the field types used here, so this remains a zero-runtime-cost,
/// compile-time-evaluated table exactly like the hand-written version it
/// replaces.
struct SQLFeaturesDelta {
    std::optional<char> identifier_quote = std::nullopt;
    std::optional<char> string_quote = std::nullopt;
    std::optional<bool> supports_limit_offset = std::nullopt;
    std::optional<bool> supports_ilike = std::nullopt;
    std::optional<const char*> true_literal = std::nullopt;
    std::optional<const char*> false_literal = std::nullopt;
};

constexpr SQLFeatures with(SQLFeatures base, SQLFeaturesDelta delta) noexcept {
    if (delta.identifier_quote) {
        base.identifier_quote = *delta.identifier_quote;
    }
    if (delta.string_quote) {
        base.string_quote = *delta.string_quote;
    }
    if (delta.supports_limit_offset) {
        base.supports_limit_offset = *delta.supports_limit_offset;
    }
    if (delta.supports_ilike) {
        base.supports_ilike = *delta.supports_ilike;
    }
    if (delta.true_literal) {
        base.true_literal = *delta.true_literal;
    }
    if (delta.false_literal) {
        base.false_literal = *delta.false_literal;
    }
    return base;
}

/// ANSI SQL / no distinguishing lineage: " identifiers, LIMIT/OFFSET, no
/// ILIKE, TRUE/FALSE literals. This is deliberately just the SQLFeatures
/// default member initializers plus the family tag.
constexpr SQLFeatures standard_base() noexcept {
    return SQLFeatures{.family = SQLDialectFamily::Standard};
}

/// PostgreSQL and its forks: " identifiers, LIMIT/OFFSET, ILIKE, TRUE/FALSE.
constexpr SQLFeatures postgres_base() noexcept {
    return SQLFeatures{.family = SQLDialectFamily::PostgreSQL, .supports_ilike = true};
}

/// MySQL and its forks: ` identifiers, LIMIT/OFFSET, no ILIKE, 1/0 literals.
constexpr SQLFeatures mysql_base() noexcept {
    return SQLFeatures{.family = SQLDialectFamily::MySQL,
                       .identifier_quote = '`',
                       .true_literal = "1",
                       .false_literal = "0"};
}

/// T-SQL (SQL Server / Azure Synapse): [ identifiers, no LIMIT/OFFSET
/// (TOP / OFFSET-FETCH instead), no ILIKE, 1/0 literals.
constexpr SQLFeatures tsql_base() noexcept {
    return SQLFeatures{.family = SQLDialectFamily::TSQL,
                       .identifier_quote = '[',
                       .supports_limit_offset = false,
                       .true_literal = "1",
                       .false_literal = "0"};
}

/// Oracle: " identifiers, no LIMIT/OFFSET (FETCH FIRST instead), TRUE/FALSE.
constexpr SQLFeatures oracle_base() noexcept {
    return SQLFeatures{.family = SQLDialectFamily::Oracle, .supports_limit_offset = false};
}

/// DB2: same shape as Oracle's vector today (both use FETCH FIRST-style
/// pagination), but kept a distinct family per docs/ROADMAP.md - DB2 is not
/// an Oracle fork, the two just happen to share this one convention.
constexpr SQLFeatures db2_base() noexcept {
    return SQLFeatures{.family = SQLDialectFamily::DB2, .supports_limit_offset = false};
}

/// BigQuery: ` identifiers, LIMIT/OFFSET, no ILIKE, TRUE/FALSE.
constexpr SQLFeatures bigquery_base() noexcept {
    return SQLFeatures{.family = SQLDialectFamily::BigQuery, .identifier_quote = '`'};
}

/// DuckDB: " identifiers, LIMIT/OFFSET, ILIKE, TRUE/FALSE.
constexpr SQLFeatures duckdb_base() noexcept {
    return SQLFeatures{.family = SQLDialectFamily::DuckDB, .supports_ilike = true};
}

/// Snowflake: " identifiers, LIMIT/OFFSET, ILIKE, TRUE/FALSE.
constexpr SQLFeatures snowflake_base() noexcept {
    return SQLFeatures{.family = SQLDialectFamily::Snowflake, .supports_ilike = true};
}

/// SQLite: " identifiers, LIMIT/OFFSET, no ILIKE, 1/0 literals.
constexpr SQLFeatures sqlite_base() noexcept {
    return SQLFeatures{
        .family = SQLDialectFamily::SQLite, .true_literal = "1", .false_literal = "0"};
}

/// ============================================================================
/// SQL Dialect Traits - Satisfies DialectTraits Concept
/// ============================================================================

struct SQLDialectTraits {
    using DialectId = SQLDialect;
    using Features = SQLFeatures;

private:
    /// Compile-time lookup table, indexed by SQLDialect enum value. Each row
    /// is `with(<family>_base(), {only the fields that differ from that
    /// family's flagship})` - see docs/ROADMAP.md stage 2 for which of these
    /// are slated to become first-class promoted family members (currently
    /// just quoting/traits rows for everything past the flagship dialect).
    static constexpr Features kFeatures[] = {
        // Core SQL Standards & Major Databases
        with(standard_base(), {}), // ANSI
        with(postgres_base(), {}), // PostgreSQL
        with(mysql_base(), {}),    // MySQL
        with(sqlite_base(), {}),   // SQLite
        with(tsql_base(), {}),     // SQLServer
        with(oracle_base(), {}),   // Oracle

        // Enterprise Databases
        with(db2_base(), {}),                                     // DB2
        with(standard_base(), {}),                                // Teradata
        with(mysql_base(), {}),                                   // MariaDB
        with(standard_base(), {.supports_limit_offset = false}),  // Informix
        with(standard_base(), {}),                                // Firebird
        with(standard_base(), {}),                                // SAPHANA

        // Cloud Data Warehouses
        with(snowflake_base(), {}),                   // Snowflake
        with(postgres_base(), {}),                    // Redshift (PostgreSQL fork)
        with(bigquery_base(), {}),                    // BigQuery
        with(tsql_base(), {.identifier_quote = '"'}), // AzureSynapse
        with(standard_base(), {}),                    // Athena

        // Modern Analytics Databases
        with(duckdb_base(), {}), // DuckDB
        with(standard_base(),
             {.identifier_quote = '`',
              .supports_ilike = true,
              .true_literal = "1",
              .false_literal = "0"}),                 // ClickHouse
        with(standard_base(), {}),                    // Presto
        with(standard_base(), {}),                    // Trino
        with(standard_base(), {.identifier_quote = '`'}), // Hive
        with(standard_base(), {.identifier_quote = '`'}), // Impala
        with(standard_base(), {.identifier_quote = '`'}), // Drill
        with(standard_base(), {.identifier_quote = '`'}), // SparkSQL
        with(standard_base(), {.identifier_quote = '`'}), // Databricks
        with(standard_base(), {}),                        // Dremio

        // MPP & Columnar Databases
        with(standard_base(), {.supports_ilike = true}), // Vertica
        with(postgres_base(), {}),                       // Greenplum (PostgreSQL fork)
        with(standard_base(), {}),                       // Netezza
        with(standard_base(), {}),                       // Exasol
        with(standard_base(), {.supports_ilike = true}), // MonetDB

        // Distributed SQL Databases
        //
        // TiDB and SingleStore previously overrode true_literal/false_literal
        // to "TRUE"/"FALSE" (issue #5's illustrative example of the `with()`
        // delta mechanism overriding a base value). Verified while promoting
        // both to first-class (docs/ROADMAP.md stage 2, issue #3 follow-on):
        // both are MySQL wire-compatible forks with no confirmed boolean-
        // literal display difference from MySQL (MySQL/TiDB/SingleStore all
        // lack a real BOOLEAN literal - TRUE/FALSE are accepted as input but
        // are just aliases for 1/0). Since this could not be confirmed as a
        // real delta, per the honesty rule it now inherits mysql_base()'s
        // 1/0 unchanged rather than restating an unverified guess.
        with(postgres_base(), {}), // CockroachDB (PostgreSQL wire-compatible)
        with(postgres_base(), {}), // YugabyteDB (PostgreSQL wire-compatible)
        with(mysql_base(), {}),    // TiDB (MySQL wire-compatible; see note below)
        with(standard_base(), {.identifier_quote = '`'}),                      // Spanner
        with(postgres_base(), {}), // Citus (PostgreSQL extension)

        // Time-Series & Real-Time Databases
        with(postgres_base(), {}), // TimescaleDB (PostgreSQL extension)
        with(standard_base(), {}), // QuestDB
        with(mysql_base(), {}),    // SingleStore (MySQL wire-compatible; see note below)

        // Streaming & Materialized Views
        with(postgres_base(), {}), // RisingWave (PostgreSQL wire-compatible)
        with(postgres_base(), {}), // Materialize (PostgreSQL wire-compatible)

        // Embedded & Lightweight
        with(standard_base(), {}),                               // H2
        with(standard_base(), {}),                               // HSQLDB
        with(standard_base(), {.supports_limit_offset = false})  // Derby
    };

public:
    /// Get feature flags for a dialect (compile-time lookup table)
    static constexpr const Features& get_features(DialectId id) noexcept {
        return kFeatures[static_cast<size_t>(id)];
    }

    /// Get the family a dialect belongs to (compile-time; reads the tag
    /// carried on that dialect's SQLFeatures row).
    static constexpr SQLDialectFamily family(DialectId id) noexcept {
        return get_features(id).family;
    }

    /// Is `id` a member of family `fam`?
    static constexpr bool is_family(DialectId id, SQLDialectFamily fam) noexcept {
        return family(id) == fam;
    }

    /// Get human-readable dialect name
    static constexpr std::string_view name(DialectId id) noexcept {
        constexpr std::string_view names[] = {
            // Core SQL Standards & Major Databases
            "ANSI SQL", "PostgreSQL", "MySQL", "SQLite", "SQL Server", "Oracle",
            // Enterprise Databases
            "DB2", "Teradata", "MariaDB", "Informix", "Firebird", "SAP HANA",
            // Cloud Data Warehouses
            "Snowflake", "Redshift", "BigQuery", "Azure Synapse", "Athena",
            // Modern Analytics Databases
            "DuckDB", "ClickHouse", "Presto", "Trino", "Hive", "Impala", "Drill", "Spark SQL",
            "Databricks", "Dremio",
            // MPP & Columnar Databases
            "Vertica", "Greenplum", "Netezza", "Exasol", "MonetDB",
            // Distributed SQL Databases
            "CockroachDB", "YugabyteDB", "TiDB", "Spanner", "Citus",
            // Time-Series & Real-Time Databases
            "TimescaleDB", "QuestDB", "SingleStore",
            // Streaming & Materialized Views
            "RisingWave", "Materialize",
            // Embedded & Lightweight
            "H2", "HSQLDB", "Derby"};
        return names[static_cast<size_t>(id)];
    }
};

// ============================================================================
/// Verify Concept Satisfaction
/// ============================================================================

static_assert(libglot::DialectTraits<SQLDialectTraits>,
              "SQLDialectTraits must satisfy libglot::DialectTraits concept");

} // namespace libglot::sql
