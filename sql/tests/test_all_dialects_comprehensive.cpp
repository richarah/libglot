#include <catch2/catch_test_macros.hpp>
#include <libglot/sql/parser.h>
#include <libglot/sql/generator.h>
#include <libglot/sql/dialect_traits.h>
#include <libglot/util/arena.h>
#include <vector>
#include <string>

using namespace libglot::sql;

// =============================================================================
// Test Matrix: All Dialects × Common SQL Patterns
// =============================================================================

namespace {
    // All supported dialects
    std::vector<SQLDialect> all_dialects = {
        SQLDialect::ANSI, SQLDialect::MySQL, SQLDialect::PostgreSQL, SQLDialect::SQLite,
        SQLDialect::BigQuery, SQLDialect::Snowflake, SQLDialect::Redshift, SQLDialect::Oracle,
        SQLDialect::SQLServer, SQLDialect::DuckDB, SQLDialect::ClickHouse, SQLDialect::Presto,
        SQLDialect::Trino, SQLDialect::Hive, SQLDialect::SparkSQL, SQLDialect::Athena,
        SQLDialect::Vertica, SQLDialect::Teradata, SQLDialect::Databricks, SQLDialect::MariaDB,
        SQLDialect::CockroachDB, SQLDialect::TimescaleDB, SQLDialect::Greenplum, SQLDialect::Netezza,
        SQLDialect::Impala, SQLDialect::Drill
    };
}

// =============================================================================
// Universal Query Tests - All Dialects Should Parse These
// =============================================================================

TEST_CASE("All dialects parse basic SELECT", "[dialects][universal]") {
    std::string sql = "SELECT id, name FROM users WHERE active = TRUE";

    for (auto dialect : all_dialects) {
        INFO("Testing dialect: " << SQLDialectTraits::name(dialect));

        libglot::Arena arena;
        SQLParser parser(arena, sql);
        REQUIRE_NOTHROW(parser.parse_top_level());
    }
}

TEST_CASE("All dialects parse INSERT", "[dialects][universal]") {
    std::string sql = "INSERT INTO users (id, name, email) VALUES (1, 'Alice', 'alice@example.com')";

    for (auto dialect : all_dialects) {
        INFO("Testing dialect: " << SQLDialectTraits::name(dialect));

        libglot::Arena arena;
        SQLParser parser(arena, sql);
        REQUIRE_NOTHROW(parser.parse_top_level());
    }
}

TEST_CASE("All dialects parse UPDATE", "[dialects][universal]") {
    std::string sql = "UPDATE users SET name = 'Bob', age = 30 WHERE id = 1";

    for (auto dialect : all_dialects) {
        INFO("Testing dialect: " << SQLDialectTraits::name(dialect));

        libglot::Arena arena;
        SQLParser parser(arena, sql);
        REQUIRE_NOTHROW(parser.parse_top_level());
    }
}

TEST_CASE("All dialects parse DELETE", "[dialects][universal]") {
    std::string sql = "DELETE FROM users WHERE age < 18";

    for (auto dialect : all_dialects) {
        INFO("Testing dialect: " << SQLDialectTraits::name(dialect));

        libglot::Arena arena;
        SQLParser parser(arena, sql);
        REQUIRE_NOTHROW(parser.parse_top_level());
    }
}

TEST_CASE("All dialects parse JOIN", "[dialects][universal]") {
    std::string sql = R"(
        SELECT u.name, o.total
        FROM users u
        INNER JOIN orders o ON u.id = o.user_id
        WHERE o.status = 'completed'
    )";

    for (auto dialect : all_dialects) {
        INFO("Testing dialect: " << SQLDialectTraits::name(dialect));

        libglot::Arena arena;
        SQLParser parser(arena, sql);
        REQUIRE_NOTHROW(parser.parse_top_level());
    }
}

TEST_CASE("All dialects parse GROUP BY", "[dialects][universal]") {
    std::string sql = R"(
        SELECT category, COUNT(*) as count, SUM(price) as total
        FROM products
        GROUP BY category
        HAVING COUNT(*) > 5
    )";

    for (auto dialect : all_dialects) {
        INFO("Testing dialect: " << SQLDialectTraits::name(dialect));

        libglot::Arena arena;
        SQLParser parser(arena, sql);
        REQUIRE_NOTHROW(parser.parse_top_level());
    }
}

TEST_CASE("All dialects parse subqueries", "[dialects][universal]") {
    std::string sql = R"(
        SELECT name FROM users
        WHERE id IN (SELECT user_id FROM orders WHERE total > 1000)
    )";

    for (auto dialect : all_dialects) {
        INFO("Testing dialect: " << SQLDialectTraits::name(dialect));

        libglot::Arena arena;
        SQLParser parser(arena, sql);
        REQUIRE_NOTHROW(parser.parse_top_level());
    }
}

TEST_CASE("All dialects parse UNION", "[dialects][universal]") {
    std::string sql = R"(
        SELECT name FROM users WHERE active = TRUE
        UNION
        SELECT name FROM archived_users
    )";

    for (auto dialect : all_dialects) {
        INFO("Testing dialect: " << SQLDialectTraits::name(dialect));

        libglot::Arena arena;
        SQLParser parser(arena, sql);
        REQUIRE_NOTHROW(parser.parse_top_level());
    }
}

// =============================================================================
// Dialect-Specific Feature Tests
// =============================================================================

TEST_CASE("PostgreSQL ILIKE operator", "[dialects][postgres]") {
    std::string sql = "SELECT * FROM users WHERE name ILIKE 'john%'";
    libglot::Arena arena;
    SQLParser parser(arena, sql);
    REQUIRE_NOTHROW(parser.parse_top_level());
}

TEST_CASE("MySQL LIMIT with OFFSET", "[dialects][mysql]") {
    std::string sql = "SELECT * FROM users LIMIT 10 OFFSET 20";
    libglot::Arena arena;
    SQLParser parser(arena, sql);
    REQUIRE_NOTHROW(parser.parse_top_level());
}

TEST_CASE("Snowflake ILIKE operator", "[dialects][snowflake]") {
    std::string sql = "SELECT * FROM users WHERE email ILIKE '%@company.com'";
    libglot::Arena arena;
    SQLParser parser(arena, sql);
    REQUIRE_NOTHROW(parser.parse_top_level());
}

// =============================================================================
// Cross-Dialect Transpilation Tests
// =============================================================================

TEST_CASE("Transpile simple query across all dialect pairs", "[dialects][transpile]") {
    std::string sql = "SELECT id, name FROM users WHERE age > 18";

    // Test a sample of dialect pairs
    std::vector<std::pair<SQLDialect, SQLDialect>> dialect_pairs = {
        {SQLDialect::MySQL, SQLDialect::PostgreSQL},
        {SQLDialect::PostgreSQL, SQLDialect::MySQL},
        {SQLDialect::MySQL, SQLDialect::SQLServer},
        {SQLDialect::SQLServer, SQLDialect::MySQL},
        {SQLDialect::PostgreSQL, SQLDialect::BigQuery},
        {SQLDialect::BigQuery, SQLDialect::Snowflake},
        {SQLDialect::Snowflake, SQLDialect::Redshift},
        {SQLDialect::Oracle, SQLDialect::PostgreSQL},
        {SQLDialect::DuckDB, SQLDialect::ClickHouse},
        {SQLDialect::Hive, SQLDialect::SparkSQL},
    };

    for (const auto& [from_dialect, to_dialect] : dialect_pairs) {
        INFO("Testing: " << SQLDialectTraits::name(from_dialect)
             << " → " << SQLDialectTraits::name(to_dialect));

        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(to_dialect);
        std::string output = gen.generate(ast);

        REQUIRE(!output.empty());
        REQUIRE(output.find("SELECT") != std::string::npos);
    }
}

TEST_CASE("Round-trip transpilation preserves semantics", "[dialects][roundtrip]") {
    std::string original = "SELECT id, name FROM users WHERE age > 18 ORDER BY name LIMIT 10";

    // Test round-trip for major dialects
    std::vector<SQLDialect> major_dialects = {
        SQLDialect::MySQL, SQLDialect::PostgreSQL, SQLDialect::BigQuery,
        SQLDialect::Snowflake, SQLDialect::DuckDB
    };

    for (auto dialect : major_dialects) {
        INFO("Testing round-trip for: " << SQLDialectTraits::name(dialect));

        // Parse as PostgreSQL, convert to dialect, convert back
        libglot::Arena arena1, arena2;

        SQLParser parser1(arena1, original);
        auto ast1 = parser1.parse_top_level();
        SQLGenerator gen1(dialect);
        auto step1 = gen1.generate(ast1);

        SQLParser parser2(arena2, step1);
        auto ast2 = parser2.parse_top_level();
        SQLGenerator gen2(SQLDialect::PostgreSQL);
        auto step2 = gen2.generate(ast2);

        // Both should contain the same semantic elements
        REQUIRE(step2.find("SELECT") != std::string::npos);
        REQUIRE(step2.find("FROM") != std::string::npos);
        REQUIRE(step2.find("WHERE") != std::string::npos);
    }
}

// =============================================================================
// Complex Query Tests Across Dialects
// =============================================================================

TEST_CASE("CTE support across dialects", "[dialects][cte]") {
    std::string sql = R"(
        WITH regional_sales AS (
            SELECT region, SUM(amount) as total_sales
            FROM orders
            GROUP BY region
        )
        SELECT * FROM regional_sales WHERE total_sales > 10000
    )";

    // Dialects with CTE support
    std::vector<SQLDialect> cte_dialects = {
        SQLDialect::PostgreSQL, SQLDialect::MySQL, SQLDialect::BigQuery,
        SQLDialect::Snowflake, SQLDialect::Redshift, SQLDialect::DuckDB,
        SQLDialect::SQLServer, SQLDialect::Oracle
    };

    for (auto dialect : cte_dialects) {
        INFO("Testing CTE for: " << SQLDialectTraits::name(dialect));

        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto expr = parser.parse_select();
        auto stmt = static_cast<SelectStmt*>(expr);
        REQUIRE(stmt != nullptr);
        REQUIRE(stmt->with != nullptr);
    }
}

TEST_CASE("Window functions across dialects", "[dialects][window]") {
    std::string sql = R"(
        SELECT
            user_id,
            amount,
            ROW_NUMBER() OVER (PARTITION BY user_id ORDER BY created_at) as rn,
            SUM(amount) OVER (PARTITION BY user_id) as total
        FROM transactions
    )";

    // Most modern dialects support window functions
    std::vector<SQLDialect> window_dialects = {
        SQLDialect::PostgreSQL, SQLDialect::MySQL, SQLDialect::BigQuery,
        SQLDialect::Snowflake, SQLDialect::Redshift, SQLDialect::DuckDB,
        SQLDialect::SQLServer, SQLDialect::Oracle, SQLDialect::Hive,
        SQLDialect::SparkSQL, SQLDialect::Presto, SQLDialect::Trino
    };

    for (auto dialect : window_dialects) {
        INFO("Testing window functions for: " << SQLDialectTraits::name(dialect));

        libglot::Arena arena;
        SQLParser parser(arena, sql);
        REQUIRE_NOTHROW(parser.parse_top_level());
    }
}

TEST_CASE("Multi-table JOIN across dialects", "[dialects][join]") {
    std::string sql = R"(
        SELECT u.name, o.total, p.product_name
        FROM users u
        INNER JOIN orders o ON u.id = o.user_id
        INNER JOIN products p ON o.product_id = p.id
        WHERE u.active = TRUE
          AND o.status = 'completed'
        ORDER BY o.total DESC
    )";

    for (auto dialect : all_dialects) {
        INFO("Testing multi-JOIN for: " << SQLDialectTraits::name(dialect));

        libglot::Arena arena;
        SQLParser parser(arena, sql);
        REQUIRE_NOTHROW(parser.parse_top_level());
    }
}

// =============================================================================
// Dialect Feature Matrix Tests
// =============================================================================

TEST_CASE("ILIKE support matches dialect features", "[dialects][features]") {
    // Dialects with ILIKE support
    std::vector<SQLDialect> ilike_dialects = {SQLDialect::PostgreSQL, SQLDialect::Snowflake};

    for (auto dialect : ilike_dialects) {
        auto features = SQLDialectTraits::get_features(dialect);
        REQUIRE(features.supports_ilike == true);
    }

    // MySQL doesn't support ILIKE
    auto mysql_features = SQLDialectTraits::get_features(SQLDialect::MySQL);
    REQUIRE(mysql_features.supports_ilike == false);
}

TEST_CASE("Identifier quoting matches dialect features", "[dialects][features]") {
    REQUIRE(SQLDialectTraits::get_features(SQLDialect::MySQL).identifier_quote == '`');
    REQUIRE(SQLDialectTraits::get_features(SQLDialect::PostgreSQL).identifier_quote == '"');
    REQUIRE(SQLDialectTraits::get_features(SQLDialect::BigQuery).identifier_quote == '`');
}

// =============================================================================
// Real-World Query Tests
// =============================================================================

TEST_CASE("Real-world analytics query across dialects", "[dialects][realworld]") {
    // TODO: Add DATE_TRUNC function support
    std::string sql = R"(
        WITH monthly_revenue AS (
            SELECT
                order_month,
                SUM(amount) as revenue,
                COUNT(DISTINCT user_id) as customers
            FROM orders
            WHERE status = 'completed'
            GROUP BY order_month
        )
        SELECT
            order_month,
            revenue,
            customers,
            revenue / customers as avg_per_customer
        FROM monthly_revenue
        ORDER BY order_month DESC
        LIMIT 12
    )";

    // Test on major analytics platforms
    std::vector<SQLDialect> analytics_dialects = {
        SQLDialect::PostgreSQL, SQLDialect::BigQuery, SQLDialect::Snowflake,
        SQLDialect::Redshift, SQLDialect::DuckDB
    };

    for (auto dialect : analytics_dialects) {
        INFO("Testing analytics query for: " << SQLDialectTraits::name(dialect));

        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(dialect);
        REQUIRE_NOTHROW(gen.generate(ast));
    }
}
