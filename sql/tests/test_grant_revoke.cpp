/**
 * Comprehensive tests for GRANT and REVOKE statements
 *
 * Covers all 60+ privilege types across:
 * - SQL Standard privileges
 * - PostgreSQL-specific privileges
 * - MySQL-specific privileges
 * - Oracle-specific privileges
 * - SQL Server-specific privileges
 * - BigQuery-specific privileges
 * - Snowflake-specific privileges
 * - Column-level privileges
 * - Role grants
 * - All options (WITH GRANT OPTION, WITH ADMIN OPTION, CASCADE, RESTRICT, etc.)
 */

#include <catch2/catch_test_macros.hpp>
#include <libglot/sql/generator.h>
#include <libglot/sql/parser.h>
#include <string>

using namespace libglot::sql;

// Helper function to test parse → generate round-trip
std::string test_round_trip(const std::string& sql) {
    libglot::Arena arena;
    SQLParser parser(arena, sql);
    auto ast = parser.parse_top_level();
    SQLGenerator gen(SQLDialect::ANSI);
    return gen.generate(ast);
}

// ============================================================================
// SQL Standard Privileges
// ============================================================================

TEST_CASE("GRANT - Basic SELECT privilege", "[grant][sql-standard]") {
    std::string sql = "GRANT SELECT, INSERT, UPDATE, DELETE ON users TO alice";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "GRANT SELECT, INSERT, UPDATE, DELETE ON users TO alice");
}

TEST_CASE("GRANT - ALL PRIVILEGES", "[grant][sql-standard]") {
    std::string sql = "GRANT ALL PRIVILEGES ON users TO alice";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "GRANT ALL PRIVILEGES ON users TO alice");
}

TEST_CASE("GRANT - ALL (shorthand)", "[grant][sql-standard]") {
    std::string sql = "GRANT ALL ON users TO alice";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "GRANT ALL ON users TO alice");
}

TEST_CASE("GRANT - WITH GRANT OPTION", "[grant][sql-standard]") {
    std::string sql = "GRANT SELECT ON users TO alice WITH GRANT OPTION";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "GRANT SELECT ON users TO alice WITH GRANT OPTION");
}

TEST_CASE("GRANT - Multiple grantees", "[grant][sql-standard]") {
    std::string sql = "GRANT SELECT ON users TO alice, bob, charlie";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "GRANT SELECT ON users TO alice, bob, charlie");
}

TEST_CASE("GRANT - PUBLIC grantee", "[grant][sql-standard]") {
    std::string sql = "GRANT SELECT ON users TO PUBLIC";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "GRANT SELECT ON users TO PUBLIC");
}

TEST_CASE("GRANT - Schema-qualified object", "[grant][sql-standard]") {
    std::string sql = "GRANT SELECT ON myschema.users TO alice";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "GRANT SELECT ON myschema.users TO alice");
}

TEST_CASE("GRANT - Multiple objects", "[grant][sql-standard]") {
    std::string sql = "GRANT SELECT ON users, orders, products TO alice";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "GRANT SELECT ON users, orders, products TO alice");
}

// ============================================================================
// Column-Level Privileges
// ============================================================================

TEST_CASE("GRANT - Column-level UPDATE privilege", "[grant][column-level]") {
    std::string sql = "GRANT UPDATE(email, phone) ON users TO alice";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "GRANT UPDATE(email, phone) ON users TO alice");
}

TEST_CASE("GRANT - Column-level REFERENCES privilege", "[grant][column-level]") {
    std::string sql = "GRANT REFERENCES(user_id) ON users TO alice";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "GRANT REFERENCES(user_id) ON users TO alice");
}

TEST_CASE("GRANT - Multiple column-level privileges", "[grant][column-level]") {
    std::string sql = "GRANT UPDATE(email), REFERENCES(user_id) ON users TO alice";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "GRANT UPDATE(email), REFERENCES(user_id) ON users TO alice");
}

TEST_CASE("GRANT - Mixed table and column privileges", "[grant][column-level]") {
    std::string sql = "GRANT SELECT, UPDATE(email, phone) ON users TO alice";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "GRANT SELECT, UPDATE(email, phone) ON users TO alice");
}

// ============================================================================
// Role Grants
// ============================================================================

TEST_CASE("GRANT - Simple role grant", "[grant][role]") {
    std::string sql = "GRANT admin_role TO alice";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "GRANT admin_role TO alice");
}

TEST_CASE("GRANT - Multiple roles to user", "[grant][role]") {
    std::string sql = "GRANT admin_role, developer_role TO alice";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "GRANT admin_role, developer_role TO alice");
}

TEST_CASE("GRANT - Role with WITH ADMIN OPTION", "[grant][role]") {
    std::string sql = "GRANT admin_role TO alice WITH ADMIN OPTION";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "GRANT admin_role TO alice WITH ADMIN OPTION");
}

TEST_CASE("GRANT - Multiple roles to multiple users", "[grant][role]") {
    std::string sql = "GRANT admin_role, developer_role TO alice, bob";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "GRANT admin_role, developer_role TO alice, bob");
}

// ============================================================================
// PostgreSQL-Specific Privileges
// ============================================================================

TEST_CASE("GRANT - PostgreSQL TRUNCATE privilege", "[grant][postgresql]") {
    std::string sql = "GRANT TRUNCATE ON users TO alice";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "GRANT TRUNCATE ON users TO alice");
}

TEST_CASE("GRANT - PostgreSQL TRIGGER privilege", "[grant][postgresql]") {
    std::string sql = "GRANT TRIGGER ON users TO alice";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "GRANT TRIGGER ON users TO alice");
}

TEST_CASE("GRANT - PostgreSQL USAGE on schema", "[grant][postgresql]") {
    std::string sql = "GRANT USAGE ON SCHEMA myschema TO alice";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "GRANT USAGE ON SCHEMA myschema TO alice");
}

TEST_CASE("GRANT - PostgreSQL CREATE on database", "[grant][postgresql]") {
    std::string sql = "GRANT CREATE ON DATABASE mydb TO alice";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "GRANT CREATE ON DATABASE mydb TO alice");
}

TEST_CASE("GRANT - PostgreSQL CONNECT privilege", "[grant][postgresql]") {
    std::string sql = "GRANT CONNECT ON DATABASE mydb TO alice";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "GRANT CONNECT ON DATABASE mydb TO alice");
}

TEST_CASE("GRANT - PostgreSQL TEMPORARY privilege", "[grant][postgresql]") {
    std::string sql = "GRANT TEMPORARY ON DATABASE mydb TO alice";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "GRANT TEMPORARY ON DATABASE mydb TO alice");
}

TEST_CASE("GRANT - PostgreSQL TEMP privilege (alias)", "[grant][postgresql]") {
    std::string sql = "GRANT TEMP ON DATABASE mydb TO alice";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "GRANT TEMP ON DATABASE mydb TO alice");
}

TEST_CASE("GRANT - PostgreSQL EXECUTE on function", "[grant][postgresql]") {
    std::string sql = "GRANT EXECUTE ON FUNCTION my_function TO alice";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "GRANT EXECUTE ON FUNCTION my_function TO alice");
}

TEST_CASE("GRANT - PostgreSQL role administrative privileges", "[grant][postgresql]") {
    std::string sql = "GRANT CREATEROLE, CREATEDB, REPLICATION, BYPASSRLS TO alice";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "GRANT CREATEROLE, CREATEDB, REPLICATION, BYPASSRLS TO alice");
}

// ============================================================================
// MySQL-Specific Privileges
// ============================================================================

TEST_CASE("GRANT - MySQL SHOW VIEW privilege", "[grant][mysql]") {
    std::string sql = "GRANT SHOW VIEW ON mydb.* TO alice";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "GRANT SHOW VIEW ON mydb.* TO alice");
}

TEST_CASE("GRANT - MySQL CREATE VIEW privilege", "[grant][mysql]") {
    std::string sql = "GRANT CREATE VIEW ON mydb.* TO alice";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "GRANT CREATE VIEW ON mydb.* TO alice");
}

TEST_CASE("GRANT - MySQL EVENT privilege", "[grant][mysql]") {
    std::string sql = "GRANT EVENT ON mydb.* TO alice";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "GRANT EVENT ON mydb.* TO alice");
}

TEST_CASE("GRANT - MySQL LOCK TABLES privilege", "[grant][mysql]") {
    std::string sql = "GRANT LOCK TABLES ON mydb.* TO alice";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "GRANT LOCK TABLES ON mydb.* TO alice");
}

TEST_CASE("GRANT - MySQL administrative privileges", "[grant][mysql]") {
    std::string sql = "GRANT RELOAD, SHUTDOWN, PROCESS, FILE, SUPER ON *.* TO alice";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "GRANT RELOAD, SHUTDOWN, PROCESS, FILE, SUPER ON *.* TO alice");
}

TEST_CASE("GRANT - MySQL GRANT OPTION privilege", "[grant][mysql]") {
    std::string sql = "GRANT GRANT OPTION ON mydb.* TO alice";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "GRANT GRANT OPTION ON mydb.* TO alice");
}

// ============================================================================
// Oracle-Specific Privileges
// ============================================================================

TEST_CASE("GRANT - Oracle INDEX privilege", "[grant][oracle]") {
    std::string sql = "GRANT INDEX ON users TO alice";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "GRANT INDEX ON users TO alice");
}

TEST_CASE("GRANT - Oracle DEBUG privilege", "[grant][oracle]") {
    std::string sql = "GRANT DEBUG ON users TO alice";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "GRANT DEBUG ON users TO alice");
}

TEST_CASE("GRANT - Oracle FLASHBACK privilege", "[grant][oracle]") {
    std::string sql = "GRANT FLASHBACK ON users TO alice";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "GRANT FLASHBACK ON users TO alice");
}

// ============================================================================
// SQL Server-Specific Privileges
// ============================================================================

TEST_CASE("GRANT - SQL Server CONTROL privilege", "[grant][sqlserver]") {
    std::string sql = "GRANT CONTROL ON users TO alice";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "GRANT CONTROL ON users TO alice");
}

TEST_CASE("GRANT - SQL Server TAKE OWNERSHIP privilege", "[grant][sqlserver]") {
    std::string sql = "GRANT TAKE OWNERSHIP ON users TO alice";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "GRANT TAKE OWNERSHIP ON users TO alice");
}

TEST_CASE("GRANT - SQL Server IMPERSONATE privilege", "[grant][sqlserver]") {
    std::string sql = "GRANT IMPERSONATE ON LOGIN::sa TO alice";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "GRANT IMPERSONATE ON LOGIN::sa TO alice");
}

TEST_CASE("GRANT - SQL Server VIEW DEFINITION privilege", "[grant][sqlserver]") {
    std::string sql = "GRANT VIEW DEFINITION ON users TO alice";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "GRANT VIEW DEFINITION ON users TO alice");
}

TEST_CASE("GRANT - SQL Server ALTER ANY USER privilege", "[grant][sqlserver]") {
    std::string sql = "GRANT ALTER ANY USER TO alice";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "GRANT ALTER ANY USER TO alice");
}

TEST_CASE("GRANT - SQL Server ALTER ANY ROLE privilege", "[grant][sqlserver]") {
    std::string sql = "GRANT ALTER ANY ROLE TO alice";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "GRANT ALTER ANY ROLE TO alice");
}

// ============================================================================
// BigQuery-Specific Privileges
// ============================================================================

TEST_CASE("GRANT - BigQuery predefined roles", "[grant][bigquery]") {
    std::string sql = "GRANT BIGQUERY READER ON TABLE myproject.dataset.table TO alice";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "GRANT BIGQUERY READER ON TABLE myproject.dataset.table TO alice");
}

TEST_CASE("GRANT - BigQuery EDITOR role", "[grant][bigquery]") {
    std::string sql = "GRANT BIGQUERY EDITOR ON DATASET myproject.dataset TO alice";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "GRANT BIGQUERY EDITOR ON DATASET myproject.dataset TO alice");
}

TEST_CASE("GRANT - BigQuery OWNER role", "[grant][bigquery]") {
    std::string sql = "GRANT BIGQUERY OWNER ON DATASET myproject.dataset TO alice";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "GRANT BIGQUERY OWNER ON DATASET myproject.dataset TO alice");
}

TEST_CASE("GRANT - BigQuery VIEWER role", "[grant][bigquery]") {
    std::string sql = "GRANT BIGQUERY VIEWER ON DATASET myproject.dataset TO alice";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "GRANT BIGQUERY VIEWER ON DATASET myproject.dataset TO alice");
}

// ============================================================================
// Snowflake-Specific Privileges
// ============================================================================

TEST_CASE("GRANT - Snowflake OWNERSHIP privilege", "[grant][snowflake]") {
    std::string sql = "GRANT OWNERSHIP ON TABLE users TO alice";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "GRANT OWNERSHIP ON TABLE users TO alice");
}

TEST_CASE("GRANT - Snowflake OPERATE privilege", "[grant][snowflake]") {
    std::string sql = "GRANT OPERATE ON WAREHOUSE my_wh TO alice";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "GRANT OPERATE ON WAREHOUSE my_wh TO alice");
}

TEST_CASE("GRANT - Snowflake MONITOR privilege", "[grant][snowflake]") {
    std::string sql = "GRANT MONITOR ON WAREHOUSE my_wh TO alice";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "GRANT MONITOR ON WAREHOUSE my_wh TO alice");
}

TEST_CASE("GRANT - Snowflake MODIFY privilege", "[grant][snowflake]") {
    std::string sql = "GRANT MODIFY ON TABLE users TO alice";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "GRANT MODIFY ON TABLE users TO alice");
}

TEST_CASE("GRANT - Snowflake READ and WRITE privileges", "[grant][snowflake]") {
    std::string sql = "GRANT READ, WRITE ON STAGE my_stage TO alice";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "GRANT READ, WRITE ON STAGE my_stage TO alice");
}

// ============================================================================
// REVOKE Statements - SQL Standard
// ============================================================================

TEST_CASE("REVOKE - Basic SELECT privilege", "[revoke][sql-standard]") {
    std::string sql = "REVOKE SELECT ON users FROM alice";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "REVOKE SELECT ON users FROM alice");
}

TEST_CASE("REVOKE - Multiple privileges", "[revoke][sql-standard]") {
    std::string sql = "REVOKE SELECT, INSERT, UPDATE ON users FROM alice";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "REVOKE SELECT, INSERT, UPDATE ON users FROM alice");
}

TEST_CASE("REVOKE - ALL PRIVILEGES", "[revoke][sql-standard]") {
    std::string sql = "REVOKE ALL PRIVILEGES ON users FROM alice";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "REVOKE ALL PRIVILEGES ON users FROM alice");
}

TEST_CASE("REVOKE - From PUBLIC", "[revoke][sql-standard]") {
    std::string sql = "REVOKE SELECT ON users FROM PUBLIC";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "REVOKE SELECT ON users FROM PUBLIC");
}

TEST_CASE("REVOKE - Multiple grantees", "[revoke][sql-standard]") {
    std::string sql = "REVOKE SELECT ON users FROM alice, bob, charlie";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "REVOKE SELECT ON users FROM alice, bob, charlie");
}

TEST_CASE("REVOKE - With CASCADE", "[revoke][sql-standard]") {
    std::string sql = "REVOKE SELECT ON users FROM alice CASCADE";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "REVOKE SELECT ON users FROM alice CASCADE");
}

TEST_CASE("REVOKE - With RESTRICT", "[revoke][sql-standard]") {
    std::string sql = "REVOKE SELECT ON users FROM alice RESTRICT";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "REVOKE SELECT ON users FROM alice RESTRICT");
}

// ============================================================================
// REVOKE - GRANT OPTION FOR
// ============================================================================

TEST_CASE("REVOKE - GRANT OPTION FOR", "[revoke][grant-option]") {
    std::string sql = "REVOKE GRANT OPTION FOR SELECT ON users FROM alice";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "REVOKE GRANT OPTION FOR SELECT ON users FROM alice");
}

TEST_CASE("REVOKE - GRANT OPTION FOR with CASCADE", "[revoke][grant-option]") {
    std::string sql = "REVOKE GRANT OPTION FOR SELECT ON users FROM alice CASCADE";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "REVOKE GRANT OPTION FOR SELECT ON users FROM alice CASCADE");
}

TEST_CASE("REVOKE - GRANT OPTION FOR multiple privileges", "[revoke][grant-option]") {
    std::string sql = "REVOKE GRANT OPTION FOR SELECT, INSERT ON users FROM alice";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "REVOKE GRANT OPTION FOR SELECT, INSERT ON users FROM alice");
}

// ============================================================================
// REVOKE - ADMIN OPTION FOR (Role Revokes)
// ============================================================================

TEST_CASE("REVOKE - ADMIN OPTION FOR", "[revoke][admin-option]") {
    std::string sql = "REVOKE ADMIN OPTION FOR admin_role FROM alice";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "REVOKE ADMIN OPTION FOR admin_role FROM alice");
}

TEST_CASE("REVOKE - ADMIN OPTION FOR with CASCADE", "[revoke][admin-option]") {
    std::string sql = "REVOKE ADMIN OPTION FOR admin_role FROM alice CASCADE";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "REVOKE ADMIN OPTION FOR admin_role FROM alice CASCADE");
}

// ============================================================================
// REVOKE - Role Revokes
// ============================================================================

TEST_CASE("REVOKE - Simple role revoke", "[revoke][role]") {
    std::string sql = "REVOKE admin_role FROM alice";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "REVOKE admin_role FROM alice");
}

TEST_CASE("REVOKE - Multiple roles", "[revoke][role]") {
    std::string sql = "REVOKE admin_role, developer_role FROM alice";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "REVOKE admin_role, developer_role FROM alice");
}

TEST_CASE("REVOKE - Role from multiple users", "[revoke][role]") {
    std::string sql = "REVOKE admin_role FROM alice, bob, charlie";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "REVOKE admin_role FROM alice, bob, charlie");
}

// ============================================================================
// REVOKE - Column-Level Privileges
// ============================================================================

TEST_CASE("REVOKE - Column-level UPDATE privilege", "[revoke][column-level]") {
    std::string sql = "REVOKE UPDATE(email, phone) ON users FROM alice";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "REVOKE UPDATE(email, phone) ON users FROM alice");
}

TEST_CASE("REVOKE - Column-level REFERENCES privilege", "[revoke][column-level]") {
    std::string sql = "REVOKE REFERENCES(user_id) ON users FROM alice";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "REVOKE REFERENCES(user_id) ON users FROM alice");
}

TEST_CASE("REVOKE - Mixed table and column privileges", "[revoke][column-level]") {
    std::string sql = "REVOKE SELECT, UPDATE(email) ON users FROM alice";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "REVOKE SELECT, UPDATE(email) ON users FROM alice");
}

// ============================================================================
// Complex Real-World Scenarios
// ============================================================================

TEST_CASE("GRANT - Real-world: Database administrator role", "[grant][real-world]") {
    std::string sql = "GRANT ALL PRIVILEGES ON DATABASE production TO dba_team WITH GRANT OPTION";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "GRANT ALL PRIVILEGES ON DATABASE production TO dba_team WITH GRANT OPTION");
}

TEST_CASE("GRANT - Real-world: Read-only analyst", "[grant][real-world]") {
    std::string sql = "GRANT SELECT ON analytics.users, analytics.orders, analytics.products TO data_analyst";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "GRANT SELECT ON analytics.users, analytics.orders, analytics.products TO data_analyst");
}

TEST_CASE("GRANT - Real-world: Application service account", "[grant][real-world]") {
    std::string sql = "GRANT SELECT, INSERT, UPDATE, DELETE ON app.users, app.sessions TO app_service";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "GRANT SELECT, INSERT, UPDATE, DELETE ON app.users, app.sessions TO app_service");
}

TEST_CASE("GRANT - Real-world: Column-level PII restrictions", "[grant][real-world]") {
    std::string sql = "GRANT SELECT, UPDATE(last_login, status) ON users TO support_team";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "GRANT SELECT, UPDATE(last_login, status) ON users TO support_team");
}

TEST_CASE("REVOKE - Real-world: Emergency access removal", "[revoke][real-world]") {
    std::string sql = "REVOKE ALL PRIVILEGES ON production.* FROM former_employee CASCADE";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "REVOKE ALL PRIVILEGES ON production.* FROM former_employee CASCADE");
}

TEST_CASE("REVOKE - Real-world: Downgrade admin to regular user", "[revoke][real-world]") {
    std::string sql = "REVOKE ADMIN OPTION FOR admin_role FROM alice CASCADE";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "REVOKE ADMIN OPTION FOR admin_role FROM alice CASCADE");
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_CASE("GRANT - Custom/Unrecognized privilege", "[grant][edge-case]") {
    std::string sql = "GRANT CUSTOM_PRIVILEGE ON users TO alice";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "GRANT CUSTOM_PRIVILEGE ON users TO alice");
}

TEST_CASE("GRANT - Schema-qualified with wildcard", "[grant][edge-case]") {
    std::string sql = "GRANT SELECT ON myschema.* TO alice";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "GRANT SELECT ON myschema.* TO alice");
}

TEST_CASE("GRANT - WITH HIERARCHY OPTION (SQL Server)", "[grant][edge-case]") {
    std::string sql = "GRANT SELECT ON users TO alice WITH HIERARCHY OPTION";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "GRANT SELECT ON users TO alice WITH HIERARCHY OPTION");
}

TEST_CASE("GRANT - Empty column list (should be handled gracefully)", "[grant][edge-case]") {
    // This should fail gracefully in parsing
    std::string sql = "GRANT UPDATE() ON users TO alice";

    libglot::Arena arena;
    SQLParser parser(arena, sql);

    // Expect this to throw a parsing error
    REQUIRE_THROWS(parser.parse());
}

TEST_CASE("REVOKE - Empty grantee list (should fail)", "[revoke][edge-case]") {
    std::string sql = "REVOKE SELECT ON users FROM";

    libglot::Arena arena;
    SQLParser parser(arena, sql);

    // Expect this to throw a parsing error
    REQUIRE_THROWS(parser.parse());
}

// ============================================================================
// Security Tests
// ============================================================================

TEST_CASE("GRANT - SQL injection via identifier", "[grant][security]") {
    // Malicious input attempting SQL injection
    std::string sql = "GRANT SELECT ON users TO alice'; DROP TABLE users; --";

    libglot::Arena arena;
    SQLParser parser(arena, sql);

    // Should parse the entire string as an identifier, not execute injection
    auto expr = parser.parse();
    REQUIRE(expr != nullptr);
    REQUIRE(expr->type == SQLNodeKind::GRANT_STMT);
}

TEST_CASE("REVOKE - Extremely long privilege list", "[revoke][security]") {
    // Test handling of many privileges (stress test)
    std::string privileges;
    for (int i = 0; i < 100; ++i) {
        if (i > 0) privileges += ", ";
        privileges += "SELECT";
    }

    std::string sql = "REVOKE " + privileges + " ON users FROM alice";

    // Should not crash or hang
    std::string result = test_round_trip(sql);
    REQUIRE(!result.empty());
}
