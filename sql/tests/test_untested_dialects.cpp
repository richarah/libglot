#include <catch2/catch_test_macros.hpp>
#include <libglot/sql/parser.h>
#include <libglot/sql/generator.h>

using namespace libglot::sql;

// Helper function for round-trip testing
static std::string test_round_trip(const std::string& sql, SQLDialect dialect) {
    libglot::Arena arena;
    SQLParser parser(arena, sql);
    auto ast = parser.parse_top_level();
    SQLGenerator gen(dialect);
    return gen.generate(ast);
}

// Helper to verify basic parsing works
static void verify_basic_sql(SQLDialect dialect, const char* dialect_name) {
    libglot::Arena arena;

    // Test 1: Basic SELECT
    {
        SQLParser parser(arena, "SELECT id, name FROM users WHERE age > 21");
        auto ast = parser.parse_top_level();
        REQUIRE(ast != nullptr);
        SQLGenerator gen(dialect);
        std::string result = gen.generate(ast);
        REQUIRE(result.find("SELECT") != std::string::npos);
        REQUIRE(result.find("FROM") != std::string::npos);
        REQUIRE(result.find("WHERE") != std::string::npos);
    }

    // Test 2: Basic INSERT
    {
        SQLParser parser(arena, "INSERT INTO users (id, name) VALUES (1, 'Alice')");
        auto ast = parser.parse_top_level();
        REQUIRE(ast != nullptr);
        SQLGenerator gen(dialect);
        std::string result = gen.generate(ast);
        REQUIRE(result.find("INSERT") != std::string::npos);
    }

    // Test 3: Basic UPDATE
    {
        SQLParser parser(arena, "UPDATE users SET name = 'Bob' WHERE id = 1");
        auto ast = parser.parse_top_level();
        REQUIRE(ast != nullptr);
    }

    // Test 4: Basic DELETE
    {
        SQLParser parser(arena, "DELETE FROM users WHERE id = 1");
        auto ast = parser.parse_top_level();
        REQUIRE(ast != nullptr);
    }

    // Test 5: CREATE TABLE
    {
        SQLParser parser(arena, "CREATE TABLE users (id INT, name VARCHAR(100))");
        auto ast = parser.parse_top_level();
        REQUIRE(ast != nullptr);
    }
}

TEST_CASE("Azure Synapse Analytics - Basic SQL support", "[dialect][synapse]") {
    verify_basic_sql(SQLDialect::AzureSynapse, "Azure Synapse");

    SECTION("Synapse-specific: DISTRIBUTION") {
        std::string sql = "CREATE TABLE sales (id INT, amount DECIMAL) DISTRIBUTED BY HASH(id)";
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        // Just verify it parses without error
        auto ast = parser.parse_top_level();
        REQUIRE(ast != nullptr);
    }
}

TEST_CASE("Citus - Basic SQL support", "[dialect][citus]") {
    verify_basic_sql(SQLDialect::Citus, "Citus");

    SECTION("Citus distributed tables") {
        // Citus is a PostgreSQL extension, should support PostgreSQL syntax
        std::string sql = "SELECT * FROM users WHERE id > 10 LIMIT 5";
        std::string result = test_round_trip(sql, SQLDialect::Citus);
        REQUIRE(result.find("LIMIT") != std::string::npos);
    }
}

TEST_CASE("DB2 - Basic SQL support", "[dialect][db2]") {
    verify_basic_sql(SQLDialect::DB2, "DB2");

    SECTION("DB2 FETCH FIRST syntax") {
        std::string sql = "SELECT * FROM users FETCH FIRST 10 ROWS ONLY";
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        REQUIRE(ast != nullptr);
    }
}

TEST_CASE("Apache Derby - Basic SQL support", "[dialect][derby]") {
    verify_basic_sql(SQLDialect::Derby, "Derby");

    SECTION("Derby boolean literals") {
        std::string sql = "SELECT * FROM users WHERE active = true";
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        REQUIRE(ast != nullptr);
    }
}

TEST_CASE("Dremio - Basic SQL support", "[dialect][dremio]") {
    verify_basic_sql(SQLDialect::Dremio, "Dremio");

    SECTION("Dremio reflection support") {
        // CREATE REFLECTION tested elsewhere, just verify basic queries
        std::string sql = "SELECT COUNT(*) FROM sales WHERE year = 2024";
        std::string result = test_round_trip(sql, SQLDialect::Dremio);
        REQUIRE(result.find("COUNT") != std::string::npos);
    }
}

TEST_CASE("Exasol - Basic SQL support", "[dialect][exasol]") {
    verify_basic_sql(SQLDialect::Exasol, "Exasol");

    SECTION("Exasol LIMIT syntax") {
        std::string sql = "SELECT * FROM users LIMIT 10";
        std::string result = test_round_trip(sql, SQLDialect::Exasol);
        REQUIRE(result.find("SELECT") != std::string::npos);
    }
}

TEST_CASE("Firebird - Basic SQL support", "[dialect][firebird]") {
    verify_basic_sql(SQLDialect::Firebird, "Firebird");

    SECTION("Firebird FIRST/SKIP syntax") {
        std::string sql = "SELECT FIRST 10 SKIP 5 * FROM users";
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        REQUIRE(ast != nullptr);
    }
}

TEST_CASE("H2 Database - Basic SQL support", "[dialect][h2]") {
    verify_basic_sql(SQLDialect::H2, "H2");

    SECTION("H2 AUTO_INCREMENT") {
        std::string sql = "CREATE TABLE users (id INT AUTO_INCREMENT, name VARCHAR(100))";
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        REQUIRE(ast != nullptr);
    }
}

TEST_CASE("HSQLDB - Basic SQL support", "[dialect][hsqldb]") {
    verify_basic_sql(SQLDialect::HSQLDB, "HSQLDB");

    SECTION("HSQLDB LIMIT syntax") {
        std::string sql = "SELECT * FROM users LIMIT 10 OFFSET 5";
        std::string result = test_round_trip(sql, SQLDialect::HSQLDB);
        REQUIRE(result.find("SELECT") != std::string::npos);
    }
}

TEST_CASE("Informix - Basic SQL support", "[dialect][informix]") {
    verify_basic_sql(SQLDialect::Informix, "Informix");

    SECTION("Informix FIRST syntax") {
        std::string sql = "SELECT FIRST 10 * FROM users";
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        REQUIRE(ast != nullptr);
    }
}

TEST_CASE("Materialize - Basic SQL support", "[dialect][materialize]") {
    verify_basic_sql(SQLDialect::Materialize, "Materialize");

    SECTION("Materialize streaming queries") {
        // Materialize supports PostgreSQL-compatible syntax
        std::string sql = "SELECT * FROM events WHERE timestamp > NOW() - INTERVAL '1 hour'";
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        REQUIRE(ast != nullptr);
    }
}

TEST_CASE("MonetDB - Basic SQL support", "[dialect][monetdb]") {
    verify_basic_sql(SQLDialect::MonetDB, "MonetDB");

    SECTION("MonetDB LIMIT syntax") {
        std::string sql = "SELECT * FROM users LIMIT 10";
        std::string result = test_round_trip(sql, SQLDialect::MonetDB);
        REQUIRE(result.find("SELECT") != std::string::npos);
    }
}

TEST_CASE("QuestDB - Basic SQL support", "[dialect][questdb]") {
    verify_basic_sql(SQLDialect::QuestDB, "QuestDB");

    SECTION("QuestDB time-series queries") {
        std::string sql = "SELECT * FROM measurements WHERE timestamp > '2024-01-01'";
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        REQUIRE(ast != nullptr);
    }
}

TEST_CASE("RisingWave - Basic SQL support", "[dialect][risingwave]") {
    verify_basic_sql(SQLDialect::RisingWave, "RisingWave");

    SECTION("RisingWave streaming") {
        // RisingWave supports PostgreSQL-compatible syntax
        std::string sql = "SELECT COUNT(*) FROM events";
        std::string result = test_round_trip(sql, SQLDialect::RisingWave);
        REQUIRE(result.find("COUNT") != std::string::npos);
    }
}

TEST_CASE("SAP HANA - Basic SQL support", "[dialect][saphana]") {
    verify_basic_sql(SQLDialect::SAPHANA, "SAP HANA");

    SECTION("SAP HANA LIMIT syntax") {
        std::string sql = "SELECT * FROM users LIMIT 10";
        std::string result = test_round_trip(sql, SQLDialect::SAPHANA);
        REQUIRE(result.find("SELECT") != std::string::npos);
    }
}

TEST_CASE("SingleStore - Basic SQL support", "[dialect][singlestore]") {
    verify_basic_sql(SQLDialect::SingleStore, "SingleStore");

    SECTION("SingleStore distributed queries") {
        // SingleStore (formerly MemSQL) uses MySQL-compatible syntax
        std::string sql = "SELECT * FROM users WHERE id > 10 LIMIT 5";
        std::string result = test_round_trip(sql, SQLDialect::SingleStore);
        REQUIRE(result.find("LIMIT") != std::string::npos);
    }
}

TEST_CASE("Google Cloud Spanner - Basic SQL support", "[dialect][spanner]") {
    verify_basic_sql(SQLDialect::Spanner, "Spanner");

    SECTION("Spanner LIMIT syntax") {
        std::string sql = "SELECT * FROM users LIMIT 10";
        std::string result = test_round_trip(sql, SQLDialect::Spanner);
        REQUIRE(result.find("SELECT") != std::string::npos);
    }
}

TEST_CASE("TiDB - Basic SQL support", "[dialect][tidb]") {
    verify_basic_sql(SQLDialect::TiDB, "TiDB");

    SECTION("TiDB MySQL compatibility") {
        // TiDB is MySQL-compatible
        std::string sql = "SELECT * FROM users WHERE id > 10 LIMIT 5";
        std::string result = test_round_trip(sql, SQLDialect::TiDB);
        REQUIRE(result.find("LIMIT") != std::string::npos);
    }
}

TEST_CASE("YugabyteDB - Basic SQL support", "[dialect][yugabyte]") {
    verify_basic_sql(SQLDialect::YugabyteDB, "YugabyteDB");

    SECTION("YugabyteDB PostgreSQL compatibility") {
        // YugabyteDB is PostgreSQL-compatible
        std::string sql = "SELECT * FROM users WHERE id > 10 LIMIT 5";
        std::string result = test_round_trip(sql, SQLDialect::YugabyteDB);
        REQUIRE(result.find("LIMIT") != std::string::npos);
    }
}
