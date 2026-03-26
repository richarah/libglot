/**
 * Comprehensive tests for utility statements: DO, ANALYZE, VACUUM
 *
 * Covers:
 * - PostgreSQL DO blocks (anonymous code blocks)
 * - ANALYZE statements (PostgreSQL, MySQL, Oracle)
 * - VACUUM statements (PostgreSQL)
 */

#include <catch2/catch_test_macros.hpp>
#include <libglot/sql/parser.h>
#include <libglot/sql/generator.h>
#include <libglot/util/arena.h>
#include <string>

using namespace libglot::sql;

namespace {
    // Helper function to test parse → generate round-trip
    std::string test_round_trip(const std::string& sql) {
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        return gen.generate(ast);
    }
}

// ============================================================================
// DO Statement Tests (PostgreSQL)
// ============================================================================

TEST_CASE("DO - Simple anonymous block", "[do][postgresql]") {
    std::string sql = "DO LANGUAGE plpgsql $$ BEGIN RAISE NOTICE 'Test'; END $$";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "DO LANGUAGE plpgsql $$ BEGIN RAISE NOTICE 'Test'; END $$");
}

TEST_CASE("DO - With variable declarations", "[do][postgresql]") {
    std::string sql = "DO $$ DECLARE v_count INTEGER; BEGIN SELECT COUNT(*) INTO v_count FROM users; END $$";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "DO $$ DECLARE v_count INTEGER; BEGIN SELECT COUNT(*) INTO v_count FROM users; END $$");
}

TEST_CASE("DO - With IF statement", "[do][postgresql]") {
    std::string sql = "DO $$ BEGIN IF EXISTS (SELECT 1 FROM users WHERE id = 1) THEN RAISE NOTICE 'Found'; END IF; END $$";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "DO $$ BEGIN IF EXISTS (SELECT 1 FROM users WHERE id = 1) THEN RAISE NOTICE 'Found'; END IF; END $$");
}

TEST_CASE("DO - With LOOP", "[do][postgresql]") {
    std::string sql = "DO $$ DECLARE i INTEGER := 0; BEGIN LOOP i := i + 1; EXIT WHEN i > 10; END LOOP; END $$";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "DO $$ DECLARE i INTEGER := 0; BEGIN LOOP i := i + 1; EXIT WHEN i > 10; END LOOP; END $$");
}

TEST_CASE("DO - With FOR loop", "[do][postgresql]") {
    std::string sql = "DO $$ BEGIN FOR i IN 1..10 LOOP RAISE NOTICE '%', i; END LOOP; END $$";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "DO $$ BEGIN FOR i IN 1..10 LOOP RAISE NOTICE '%', i; END LOOP; END $$");
}

TEST_CASE("DO - With WHILE loop", "[do][postgresql]") {
    std::string sql = "DO $$ DECLARE i INTEGER := 0; BEGIN WHILE i < 10 LOOP i := i + 1; END LOOP; END $$";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "DO $$ DECLARE i INTEGER := 0; BEGIN WHILE i < 10 LOOP i := i + 1; END LOOP; END $$");
}

TEST_CASE("DO - With exception handling", "[do][postgresql]") {
    std::string sql = "DO $$ BEGIN INSERT INTO users VALUES (1, 'test'); EXCEPTION WHEN unique_violation THEN RAISE NOTICE 'Duplicate'; END $$";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "DO $$ BEGIN INSERT INTO users VALUES (1, 'test'); EXCEPTION WHEN unique_violation THEN RAISE NOTICE 'Duplicate'; END $$");
}

TEST_CASE("DO - With multiple statements", "[do][postgresql]") {
    std::string sql = "DO $$ BEGIN UPDATE users SET active = true; DELETE FROM sessions WHERE expired = true; COMMIT; END $$";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "DO $$ BEGIN UPDATE users SET active = true; DELETE FROM sessions WHERE expired = true; COMMIT; END $$");
}

TEST_CASE("DO - With dynamic SQL (EXECUTE)", "[do][postgresql]") {
    std::string sql = "DO $$ BEGIN EXECUTE 'CREATE TABLE IF NOT EXISTS temp_table (id INT)'; END $$";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "DO $$ BEGIN EXECUTE 'CREATE TABLE IF NOT EXISTS temp_table (id INT)'; END $$");
}

TEST_CASE("DO - Nested blocks", "[do][postgresql]") {
    std::string sql = "DO $$ BEGIN BEGIN RAISE NOTICE 'Nested'; END; END $$";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "DO $$ BEGIN BEGIN RAISE NOTICE 'Nested'; END; END $$");
}

// ============================================================================
// ANALYZE Statement Tests (PostgreSQL)
// ============================================================================

TEST_CASE("ANALYZE - Single table", "[analyze][postgresql]") {
    std::string sql = "ANALYZE users";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "ANALYZE users");
}

TEST_CASE("ANALYZE - Multiple tables", "[analyze][postgresql]") {
    std::string sql = "ANALYZE users, orders, products";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "ANALYZE users, orders, products");
}

TEST_CASE("ANALYZE - Schema-qualified table", "[analyze][postgresql]") {
    std::string sql = "ANALYZE myschema.users";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "ANALYZE myschema.users");
}

TEST_CASE("ANALYZE - With VERBOSE option", "[analyze][postgresql]") {
    std::string sql = "ANALYZE VERBOSE users";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "ANALYZE VERBOSE users");
}

TEST_CASE("ANALYZE - Specific columns", "[analyze][postgresql]") {
    std::string sql = "ANALYZE users(email, created_at)";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "ANALYZE users(email, created_at)");
}

TEST_CASE("ANALYZE - VERBOSE with specific columns", "[analyze][postgresql]") {
    std::string sql = "ANALYZE VERBOSE users(email, phone)";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "ANALYZE VERBOSE users(email, phone)");
}

TEST_CASE("ANALYZE - All tables (no table specified)", "[analyze][postgresql]") {
    std::string sql = "ANALYZE";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "ANALYZE");
}

TEST_CASE("ANALYZE - VERBOSE without table", "[analyze][postgresql]") {
    std::string sql = "ANALYZE VERBOSE";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "ANALYZE VERBOSE");
}

// ============================================================================
// ANALYZE Statement Tests (MySQL)
// ============================================================================

TEST_CASE("ANALYZE - MySQL TABLE syntax", "[analyze][mysql]") {
    std::string sql = "ANALYZE TABLE users";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "ANALYZE TABLE users");
}

TEST_CASE("ANALYZE - MySQL multiple tables with TABLE", "[analyze][mysql]") {
    std::string sql = "ANALYZE TABLE users, orders, products";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "ANALYZE TABLE users, orders, products");
}

TEST_CASE("ANALYZE - MySQL LOCAL option", "[analyze][mysql]") {
    std::string sql = "ANALYZE LOCAL TABLE users";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "ANALYZE LOCAL TABLE users");
}

TEST_CASE("ANALYZE - MySQL NO_WRITE_TO_BINLOG", "[analyze][mysql]") {
    std::string sql = "ANALYZE NO_WRITE_TO_BINLOG TABLE users";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "ANALYZE NO_WRITE_TO_BINLOG TABLE users");
}

// ============================================================================
// VACUUM Statement Tests (PostgreSQL)
// ============================================================================

TEST_CASE("VACUUM - Simple vacuum", "[vacuum][postgresql]") {
    std::string sql = "VACUUM users";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "VACUUM users");
}

TEST_CASE("VACUUM - Multiple tables", "[vacuum][postgresql]") {
    std::string sql = "VACUUM users, orders, products";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "VACUUM users, orders, products");
}

TEST_CASE("VACUUM - Schema-qualified table", "[vacuum][postgresql]") {
    std::string sql = "VACUUM myschema.users";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "VACUUM myschema.users");
}

TEST_CASE("VACUUM - FULL option", "[vacuum][postgresql]") {
    std::string sql = "VACUUM FULL users";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "VACUUM FULL users");
}

TEST_CASE("VACUUM - FREEZE option", "[vacuum][postgresql]") {
    std::string sql = "VACUUM FREEZE users";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "VACUUM FREEZE users");
}

TEST_CASE("VACUUM - VERBOSE option", "[vacuum][postgresql]") {
    std::string sql = "VACUUM VERBOSE users";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "VACUUM VERBOSE users");
}

TEST_CASE("VACUUM - ANALYZE option", "[vacuum][postgresql]") {
    std::string sql = "VACUUM ANALYZE users";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "VACUUM ANALYZE users");
}

TEST_CASE("VACUUM - Multiple options", "[vacuum][postgresql]") {
    std::string sql = "VACUUM FULL VERBOSE ANALYZE users";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "VACUUM FULL VERBOSE ANALYZE users");
}

TEST_CASE("VACUUM - All tables (no table specified)", "[vacuum][postgresql]") {
    std::string sql = "VACUUM";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "VACUUM");
}

TEST_CASE("VACUUM - FULL without table", "[vacuum][postgresql]") {
    std::string sql = "VACUUM FULL";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "VACUUM FULL");
}

TEST_CASE("VACUUM - Specific columns", "[vacuum][postgresql]") {
    std::string sql = "VACUUM users(email, created_at)";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "VACUUM users(email, created_at)");
}

TEST_CASE("VACUUM - ANALYZE with columns", "[vacuum][postgresql]") {
    std::string sql = "VACUUM ANALYZE users(email, phone)";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "VACUUM ANALYZE users(email, phone)");
}

TEST_CASE("VACUUM - Parenthesized options (PostgreSQL 9.0+)", "[vacuum][postgresql]") {
    std::string sql = "VACUUM (FULL, VERBOSE) users";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "VACUUM (FULL, VERBOSE) users");
}

TEST_CASE("VACUUM - Parenthesized options with values", "[vacuum][postgresql]") {
    std::string sql = "VACUUM (PARALLEL 4) users";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "VACUUM (PARALLEL 4) users");
}

// ============================================================================
// Real-World Scenarios
// ============================================================================

TEST_CASE("DO - Real-world: Conditional table creation", "[do][real-world]") {
    std::string sql = "DO $$ BEGIN IF NOT EXISTS (SELECT 1 FROM pg_tables WHERE tablename = 'audit_log') THEN CREATE TABLE audit_log (id SERIAL, action TEXT, created_at TIMESTAMP DEFAULT NOW()); END IF; END $$";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "DO $$ BEGIN IF NOT EXISTS (SELECT 1 FROM pg_tables WHERE tablename = 'audit_log') THEN CREATE TABLE audit_log (id SERIAL, action TEXT, created_at TIMESTAMP DEFAULT NOW()); END IF; END $$");
}

TEST_CASE("DO - Real-world: Bulk update with logging", "[do][real-world]") {
    std::string sql = "DO $$ DECLARE affected INTEGER; BEGIN UPDATE users SET active = false WHERE last_login < NOW() - INTERVAL '90 days'; GET DIAGNOSTICS affected = ROW_COUNT; RAISE NOTICE 'Deactivated % users', affected; END $$";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "DO $$ DECLARE affected INTEGER; BEGIN UPDATE users SET active = false WHERE last_login < NOW() - INTERVAL '90 days'; GET DIAGNOSTICS affected = ROW_COUNT; RAISE NOTICE 'Deactivated % users', affected; END $$");
}

TEST_CASE("ANALYZE - Real-world: Maintenance after bulk load", "[analyze][real-world]") {
    std::string sql = "ANALYZE VERBOSE users, orders, order_items";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "ANALYZE VERBOSE users, orders, order_items");
}

TEST_CASE("VACUUM - Real-world: Full vacuum after major delete", "[vacuum][real-world]") {
    std::string sql = "VACUUM FULL VERBOSE ANALYZE users";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "VACUUM FULL VERBOSE ANALYZE users");
}

TEST_CASE("VACUUM - Real-world: Regular maintenance job", "[vacuum][real-world]") {
    std::string sql = "VACUUM ANALYZE users, sessions, audit_log";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "VACUUM ANALYZE users, sessions, audit_log");
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_CASE("DO - Empty block", "[do][edge-case]") {
    std::string sql = "DO $$ BEGIN END $$";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "DO $$ BEGIN END $$");
}

TEST_CASE("DO - With single quote delimiters", "[do][edge-case]") {
    std::string sql = "DO ' BEGIN RAISE NOTICE ''Test''; END '";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "DO ' BEGIN RAISE NOTICE ''Test''; END '");
}

TEST_CASE("DO - With custom delimiter", "[do][edge-case]") {
    std::string sql = "DO $custom$ BEGIN RAISE NOTICE 'Test'; END $custom$";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "DO $custom$ BEGIN RAISE NOTICE 'Test'; END $custom$");
}

TEST_CASE("ANALYZE - Empty column list should fail", "[analyze][edge-case]") {
    std::string sql = "ANALYZE users()";

    libglot::Arena arena;
    SQLParser parser(arena, sql);

    // Expect parsing error for empty column list
    REQUIRE_THROWS(parser.parse_top_level());
}

TEST_CASE("VACUUM - Conflicting options", "[vacuum][edge-case]") {
    // FULL and FREEZE are mutually exclusive in some PostgreSQL versions
    // SQLParser should still accept it (validation happens at execution)
    std::string sql = "VACUUM FULL FREEZE users";
    std::string result = test_round_trip(sql);
    REQUIRE(result == "VACUUM FULL FREEZE users");
}

// ============================================================================
// Security Tests
// ============================================================================

TEST_CASE("DO - SQL injection via delimiter", "[do][security]") {
    // Malicious input attempting to break out of DO block
    std::string sql = "DO $$ BEGIN RAISE NOTICE 'test'; END $$; DROP TABLE users; --";

    libglot::Arena arena;
    SQLParser parser(arena, sql);

    // Should parse only the DO statement, treating the rest as error or second statement
    auto expr = parser.parse_top_level();
    REQUIRE(expr != nullptr);
    // TODO: Verify node kind is DO_BLOCK
    // REQUIRE(expr->kind == SQLNodeKind::DO_BLOCK);
}

TEST_CASE("ANALYZE - Very long table list", "[analyze][security]") {
    // Stress test with many tables
    std::string tables;
    for (int i = 0; i < 100; ++i) {
        if (i > 0) tables += ", ";
        tables += "table" + std::to_string(i);
    }

    std::string sql = "ANALYZE " + tables;

    // Should not crash or hang
    std::string result = test_round_trip(sql);
    REQUIRE(!result.empty());
    REQUIRE(result.find("ANALYZE") != std::string::npos);
}

TEST_CASE("VACUUM - Very long table list", "[vacuum][security]") {
    // Stress test with many tables
    std::string tables;
    for (int i = 0; i < 100; ++i) {
        if (i > 0) tables += ", ";
        tables += "table" + std::to_string(i);
    }

    std::string sql = "VACUUM " + tables;

    // Should not crash or hang
    std::string result = test_round_trip(sql);
    REQUIRE(!result.empty());
    REQUIRE(result.find("VACUUM") != std::string::npos);
}
