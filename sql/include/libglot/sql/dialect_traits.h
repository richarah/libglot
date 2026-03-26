#pragma once

#include "../../../../core/include/libglot/dialect/traits.h"
#include <string_view>

namespace libglot::sql {

/// ============================================================================
/// SQL Dialect Enumeration
/// ============================================================================

enum class SQLDialect : uint8_t {
    // Core SQL Standards & Major Databases
    ANSI,         // ANSI SQL standard
    PostgreSQL,   // PostgreSQL
    MySQL,        // MySQL
    SQLite,       // SQLite
    SQLServer,    // Microsoft SQL Server (T-SQL)
    Oracle,       // Oracle Database (PL/SQL)

    // Enterprise Databases
    DB2,          // IBM DB2
    Teradata,     // Teradata
    MariaDB,      // MariaDB
    Informix,     // IBM Informix
    Firebird,     // Firebird
    SAPHANA,      // SAP HANA

    // Cloud Data Warehouses
    Snowflake,    // Snowflake
    Redshift,     // Amazon Redshift
    BigQuery,     // Google BigQuery
    AzureSynapse, // Azure Synapse Analytics
    Athena,       // AWS Athena

    // Modern Analytics Databases
    DuckDB,       // DuckDB
    ClickHouse,   // ClickHouse
    Presto,       // Presto
    Trino,        // Trino (formerly PrestoSQL)
    Hive,         // Apache Hive
    Impala,       // Apache Impala
    Drill,        // Apache Drill
    SparkSQL,     // Apache Spark SQL
    Databricks,   // Databricks SQL
    Dremio,       // Dremio

    // MPP & Columnar Databases
    Vertica,      // Vertica
    Greenplum,    // Greenplum
    Netezza,      // IBM Netezza
    Exasol,       // Exasol
    MonetDB,      // MonetDB

    // Distributed SQL Databases
    CockroachDB,  // CockroachDB
    YugabyteDB,   // YugabyteDB
    TiDB,         // TiDB
    Spanner,      // Google Cloud Spanner
    Citus,        // Citus (PostgreSQL extension)

    // Time-Series & Real-Time Databases
    TimescaleDB,  // TimescaleDB (PostgreSQL extension)
    QuestDB,      // QuestDB
    SingleStore,  // SingleStore (formerly MemSQL)

    // Streaming & Materialized Views
    RisingWave,   // RisingWave
    Materialize,  // Materialize

    // Embedded & Lightweight
    H2,           // H2 Database
    HSQLDB,       // HSQLDB
    Derby,        // Apache Derby

    COUNT
};

/// ============================================================================
/// SQL Dialect Features
/// ============================================================================

struct SQLFeatures {
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
/// SQL Dialect Traits - Satisfies DialectTraits Concept
/// ============================================================================

struct SQLDialectTraits {
    using DialectId = SQLDialect;
    using Features = SQLFeatures;

    /// Get feature flags for a dialect (compile-time lookup table)
    static constexpr const Features& get_features(DialectId id) noexcept {
        // Compile-time lookup table (zero runtime overhead)
        static constexpr Features features[] = {
            // Core SQL Standards & Major Databases
            {.identifier_quote = '"', .string_quote = '\'', .supports_limit_offset = true, .supports_ilike = false, .true_literal = "TRUE", .false_literal = "FALSE"}, // ANSI
            {.identifier_quote = '"', .string_quote = '\'', .supports_limit_offset = true, .supports_ilike = true, .true_literal = "TRUE", .false_literal = "FALSE"},  // PostgreSQL
            {.identifier_quote = '`', .string_quote = '\'', .supports_limit_offset = true, .supports_ilike = false, .true_literal = "1", .false_literal = "0"},        // MySQL
            {.identifier_quote = '"', .string_quote = '\'', .supports_limit_offset = true, .supports_ilike = false, .true_literal = "1", .false_literal = "0"},        // SQLite
            {.identifier_quote = '[', .string_quote = '\'', .supports_limit_offset = false, .supports_ilike = false, .true_literal = "1", .false_literal = "0"},       // SQLServer
            {.identifier_quote = '"', .string_quote = '\'', .supports_limit_offset = false, .supports_ilike = false, .true_literal = "TRUE", .false_literal = "FALSE"},// Oracle

            // Enterprise Databases
            {.identifier_quote = '"', .string_quote = '\'', .supports_limit_offset = false, .supports_ilike = false, .true_literal = "TRUE", .false_literal = "FALSE"},// DB2
            {.identifier_quote = '"', .string_quote = '\'', .supports_limit_offset = true, .supports_ilike = false, .true_literal = "TRUE", .false_literal = "FALSE"}, // Teradata
            {.identifier_quote = '`', .string_quote = '\'', .supports_limit_offset = true, .supports_ilike = false, .true_literal = "1", .false_literal = "0"},        // MariaDB
            {.identifier_quote = '"', .string_quote = '\'', .supports_limit_offset = false, .supports_ilike = false, .true_literal = "TRUE", .false_literal = "FALSE"},// Informix
            {.identifier_quote = '"', .string_quote = '\'', .supports_limit_offset = true, .supports_ilike = false, .true_literal = "TRUE", .false_literal = "FALSE"}, // Firebird
            {.identifier_quote = '"', .string_quote = '\'', .supports_limit_offset = true, .supports_ilike = false, .true_literal = "TRUE", .false_literal = "FALSE"}, // SAPHANA

            // Cloud Data Warehouses
            {.identifier_quote = '"', .string_quote = '\'', .supports_limit_offset = true, .supports_ilike = true, .true_literal = "TRUE", .false_literal = "FALSE"},  // Snowflake
            {.identifier_quote = '"', .string_quote = '\'', .supports_limit_offset = true, .supports_ilike = true, .true_literal = "TRUE", .false_literal = "FALSE"},  // Redshift
            {.identifier_quote = '`', .string_quote = '\'', .supports_limit_offset = true, .supports_ilike = false, .true_literal = "TRUE", .false_literal = "FALSE"}, // BigQuery
            {.identifier_quote = '"', .string_quote = '\'', .supports_limit_offset = false, .supports_ilike = false, .true_literal = "1", .false_literal = "0"},       // AzureSynapse
            {.identifier_quote = '"', .string_quote = '\'', .supports_limit_offset = true, .supports_ilike = false, .true_literal = "TRUE", .false_literal = "FALSE"}, // Athena

            // Modern Analytics Databases
            {.identifier_quote = '"', .string_quote = '\'', .supports_limit_offset = true, .supports_ilike = true, .true_literal = "TRUE", .false_literal = "FALSE"},  // DuckDB
            {.identifier_quote = '`', .string_quote = '\'', .supports_limit_offset = true, .supports_ilike = true, .true_literal = "1", .false_literal = "0"},         // ClickHouse
            {.identifier_quote = '"', .string_quote = '\'', .supports_limit_offset = true, .supports_ilike = false, .true_literal = "TRUE", .false_literal = "FALSE"}, // Presto
            {.identifier_quote = '"', .string_quote = '\'', .supports_limit_offset = true, .supports_ilike = false, .true_literal = "TRUE", .false_literal = "FALSE"}, // Trino
            {.identifier_quote = '`', .string_quote = '\'', .supports_limit_offset = true, .supports_ilike = false, .true_literal = "TRUE", .false_literal = "FALSE"}, // Hive
            {.identifier_quote = '`', .string_quote = '\'', .supports_limit_offset = true, .supports_ilike = false, .true_literal = "TRUE", .false_literal = "FALSE"}, // Impala
            {.identifier_quote = '`', .string_quote = '\'', .supports_limit_offset = true, .supports_ilike = false, .true_literal = "TRUE", .false_literal = "FALSE"}, // Drill
            {.identifier_quote = '`', .string_quote = '\'', .supports_limit_offset = true, .supports_ilike = false, .true_literal = "TRUE", .false_literal = "FALSE"}, // SparkSQL
            {.identifier_quote = '`', .string_quote = '\'', .supports_limit_offset = true, .supports_ilike = false, .true_literal = "TRUE", .false_literal = "FALSE"}, // Databricks
            {.identifier_quote = '"', .string_quote = '\'', .supports_limit_offset = true, .supports_ilike = false, .true_literal = "TRUE", .false_literal = "FALSE"}, // Dremio

            // MPP & Columnar Databases
            {.identifier_quote = '"', .string_quote = '\'', .supports_limit_offset = true, .supports_ilike = true, .true_literal = "TRUE", .false_literal = "FALSE"},  // Vertica
            {.identifier_quote = '"', .string_quote = '\'', .supports_limit_offset = true, .supports_ilike = true, .true_literal = "TRUE", .false_literal = "FALSE"},  // Greenplum
            {.identifier_quote = '"', .string_quote = '\'', .supports_limit_offset = true, .supports_ilike = false, .true_literal = "TRUE", .false_literal = "FALSE"}, // Netezza
            {.identifier_quote = '"', .string_quote = '\'', .supports_limit_offset = true, .supports_ilike = false, .true_literal = "TRUE", .false_literal = "FALSE"}, // Exasol
            {.identifier_quote = '"', .string_quote = '\'', .supports_limit_offset = true, .supports_ilike = true, .true_literal = "TRUE", .false_literal = "FALSE"},  // MonetDB

            // Distributed SQL Databases
            {.identifier_quote = '"', .string_quote = '\'', .supports_limit_offset = true, .supports_ilike = true, .true_literal = "TRUE", .false_literal = "FALSE"},  // CockroachDB
            {.identifier_quote = '"', .string_quote = '\'', .supports_limit_offset = true, .supports_ilike = true, .true_literal = "TRUE", .false_literal = "FALSE"},  // YugabyteDB
            {.identifier_quote = '`', .string_quote = '\'', .supports_limit_offset = true, .supports_ilike = false, .true_literal = "TRUE", .false_literal = "FALSE"}, // TiDB
            {.identifier_quote = '`', .string_quote = '\'', .supports_limit_offset = true, .supports_ilike = false, .true_literal = "TRUE", .false_literal = "FALSE"}, // Spanner
            {.identifier_quote = '"', .string_quote = '\'', .supports_limit_offset = true, .supports_ilike = true, .true_literal = "TRUE", .false_literal = "FALSE"},  // Citus

            // Time-Series & Real-Time Databases
            {.identifier_quote = '"', .string_quote = '\'', .supports_limit_offset = true, .supports_ilike = true, .true_literal = "TRUE", .false_literal = "FALSE"},  // TimescaleDB
            {.identifier_quote = '"', .string_quote = '\'', .supports_limit_offset = true, .supports_ilike = false, .true_literal = "TRUE", .false_literal = "FALSE"}, // QuestDB
            {.identifier_quote = '`', .string_quote = '\'', .supports_limit_offset = true, .supports_ilike = false, .true_literal = "TRUE", .false_literal = "FALSE"}, // SingleStore

            // Streaming & Materialized Views
            {.identifier_quote = '"', .string_quote = '\'', .supports_limit_offset = true, .supports_ilike = true, .true_literal = "TRUE", .false_literal = "FALSE"},  // RisingWave
            {.identifier_quote = '"', .string_quote = '\'', .supports_limit_offset = true, .supports_ilike = true, .true_literal = "TRUE", .false_literal = "FALSE"},  // Materialize

            // Embedded & Lightweight
            {.identifier_quote = '"', .string_quote = '\'', .supports_limit_offset = true, .supports_ilike = false, .true_literal = "TRUE", .false_literal = "FALSE"}, // H2
            {.identifier_quote = '"', .string_quote = '\'', .supports_limit_offset = true, .supports_ilike = false, .true_literal = "TRUE", .false_literal = "FALSE"}, // HSQLDB
            {.identifier_quote = '"', .string_quote = '\'', .supports_limit_offset = false, .supports_ilike = false, .true_literal = "TRUE", .false_literal = "FALSE"} // Derby
        };

        return features[static_cast<size_t>(id)];
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
            "DuckDB", "ClickHouse", "Presto", "Trino", "Hive", "Impala", "Drill", "Spark SQL", "Databricks", "Dremio",
            // MPP & Columnar Databases
            "Vertica", "Greenplum", "Netezza", "Exasol", "MonetDB",
            // Distributed SQL Databases
            "CockroachDB", "YugabyteDB", "TiDB", "Spanner", "Citus",
            // Time-Series & Real-Time Databases
            "TimescaleDB", "QuestDB", "SingleStore",
            // Streaming & Materialized Views
            "RisingWave", "Materialize",
            // Embedded & Lightweight
            "H2", "HSQLDB", "Derby"
        };
        return names[static_cast<size_t>(id)];
    }
};

// ============================================================================
/// Verify Concept Satisfaction
/// ============================================================================

static_assert(libglot::DialectTraits<SQLDialectTraits>,
    "SQLDialectTraits must satisfy libglot::DialectTraits concept");

} // namespace libglot::sql
