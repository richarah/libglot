#include <catch2/catch_test_macros.hpp>
#include <libglot/sql/parser.h>
#include <libglot/sql/generator.h>

using namespace libglot::sql;

static std::string test_round_trip(const std::string& sql, SQLDialect dialect = SQLDialect::PostgreSQL) {
    libglot::Arena arena;
    SQLParser parser(arena, sql);
    auto ast = parser.parse_top_level();
    SQLGenerator gen(dialect);
    return gen.generate(ast);
}

TEST_CASE("Temporary tables - PostgreSQL", "[temp][postgresql]") {
    SECTION("CREATE TEMP TABLE") {
        std::string sql = "CREATE TEMP TABLE temp_users (id INT, name VARCHAR(100))";
        std::string result = test_round_trip(sql);
        REQUIRE(result.find("CREATE") != std::string::npos);
        REQUIRE(result.find("TEMP") != std::string::npos);
        REQUIRE(result.find("TABLE") != std::string::npos);
    }

    SECTION("CREATE TEMPORARY TABLE") {
        std::string sql = "CREATE TEMPORARY TABLE session_data (user_id INT, session_key VARCHAR(255))";
        std::string result = test_round_trip(sql);
        REQUIRE(result.find("TEMPORARY") != std::string::npos);
    }

    SECTION("CREATE TEMP TABLE with SELECT") {
        std::string sql = "CREATE TEMP TABLE active_users AS SELECT * FROM users WHERE status = 'active'";
        std::string result = test_round_trip(sql);
        REQUIRE(result.find("TEMP") != std::string::npos);
        REQUIRE(result.find("AS SELECT") != std::string::npos);
    }

    SECTION("DROP TEMPORARY TABLE") {
        std::string sql = "DROP TABLE IF EXISTS temp_users";
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        REQUIRE(ast != nullptr);
    }
}

TEST_CASE("Temporary tables - MySQL", "[temp][mysql]") {
    SECTION("CREATE TEMPORARY TABLE") {
        std::string sql = "CREATE TEMPORARY TABLE temp_results (id INT, value DECIMAL(10,2))";
        std::string result = test_round_trip(sql, SQLDialect::MySQL);
        REQUIRE(result.find("TEMPORARY") != std::string::npos);
    }

    SECTION("CREATE TEMPORARY TABLE with ENGINE") {
        std::string sql = "CREATE TEMPORARY TABLE cache (key_name VARCHAR(100), value TEXT) ENGINE=MEMORY";
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        REQUIRE(ast != nullptr);
    }

    SECTION("CREATE TEMPORARY TABLE AS SELECT") {
        std::string sql = "CREATE TEMPORARY TABLE recent_orders AS SELECT * FROM orders WHERE created_at > NOW() - INTERVAL 7 DAY";
        std::string result = test_round_trip(sql, SQLDialect::MySQL);
        REQUIRE(result.find("TEMPORARY") != std::string::npos);
    }
}

TEST_CASE("Temporary tables - SQL Server", "[temp][sqlserver]") {
    SECTION("Local temporary table with # prefix") {
        std::string sql = "CREATE TABLE #temp_data (id INT, value VARCHAR(100))";
        libglot::Arena arena;
        SQLParser parser(arena, sql, SQLDialect::SQLServer);
        auto ast = parser.parse_top_level();
        REQUIRE(ast != nullptr);
    }

    SECTION("Global temporary table with ## prefix") {
        std::string sql = "CREATE TABLE ##global_temp (session_id INT, data TEXT)";
        libglot::Arena arena;
        SQLParser parser(arena, sql, SQLDialect::SQLServer);
        auto ast = parser.parse_top_level();
        REQUIRE(ast != nullptr);
    }

    SECTION("SELECT INTO temporary table") {
        std::string sql = "SELECT * INTO #temp_users FROM users WHERE active = 1";
        libglot::Arena arena;
        SQLParser parser(arena, sql, SQLDialect::SQLServer);
        auto ast = parser.parse_top_level();
        REQUIRE(ast != nullptr);
    }
}

TEST_CASE("Temporary tables - Oracle", "[temp][oracle]") {
    SECTION("CREATE GLOBAL TEMPORARY TABLE") {
        std::string sql = "CREATE GLOBAL TEMPORARY TABLE temp_session (user_id NUMBER, session_data CLOB)";
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        REQUIRE(ast != nullptr);
    }

    SECTION("CREATE GLOBAL TEMPORARY TABLE ON COMMIT DELETE ROWS") {
        std::string sql = "CREATE GLOBAL TEMPORARY TABLE temp_calc (result NUMBER) ON COMMIT DELETE ROWS";
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        REQUIRE(ast != nullptr);
    }

    SECTION("CREATE GLOBAL TEMPORARY TABLE ON COMMIT PRESERVE ROWS") {
        std::string sql = "CREATE GLOBAL TEMPORARY TABLE temp_staging (data VARCHAR2(4000)) ON COMMIT PRESERVE ROWS";
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        REQUIRE(ast != nullptr);
    }
}

TEST_CASE("Temporary tables - BigQuery", "[temp][bigquery]") {
    SECTION("CREATE TEMP TABLE") {
        std::string sql = "CREATE TEMP TABLE temp_results AS SELECT id, name FROM users WHERE active = TRUE";
        std::string result = test_round_trip(sql, SQLDialect::BigQuery);
        REQUIRE(result.find("TEMP") != std::string::npos);
    }

    SECTION("CREATE TEMPORARY TABLE") {
        std::string sql = "CREATE TEMPORARY TABLE session_data (user_id INT64, timestamp TIMESTAMP)";
        std::string result = test_round_trip(sql, SQLDialect::BigQuery);
        REQUIRE(result.find("TEMPORARY") != std::string::npos);
    }
}

TEST_CASE("Temporary tables - Snowflake", "[temp][snowflake]") {
    SECTION("CREATE TEMPORARY TABLE") {
        std::string sql = "CREATE TEMPORARY TABLE temp_calculations (id NUMBER, result FLOAT)";
        std::string result = test_round_trip(sql, SQLDialect::Snowflake);
        REQUIRE(result.find("TEMPORARY") != std::string::npos);
    }

    SECTION("CREATE TEMP TABLE") {
        std::string sql = "CREATE TEMP TABLE staging_data AS SELECT * FROM source_data";
        std::string result = test_round_trip(sql, SQLDialect::Snowflake);
        REQUIRE(result.find("TEMP") != std::string::npos);
    }
}

TEST_CASE("Temporary tables - DuckDB", "[temp][duckdb]") {
    SECTION("CREATE TEMP TABLE") {
        std::string sql = "CREATE TEMP TABLE analysis_results (category VARCHAR, count BIGINT)";
        std::string result = test_round_trip(sql, SQLDialect::DuckDB);
        REQUIRE(result.find("TEMP") != std::string::npos);
    }
}

TEST_CASE("Temporary tables - Common operations", "[temp][operations]") {
    SECTION("Insert into temporary table") {
        std::string sql = "INSERT INTO temp_users SELECT * FROM users WHERE created_at > '2024-01-01'";
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        REQUIRE(ast != nullptr);
    }

    SECTION("Update temporary table") {
        std::string sql = "UPDATE temp_results SET processed = TRUE WHERE id < 100";
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        REQUIRE(ast != nullptr);
    }

    SECTION("Delete from temporary table") {
        std::string sql = "DELETE FROM temp_staging WHERE error_flag = 1";
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        REQUIRE(ast != nullptr);
    }

    SECTION("Join with temporary table") {
        std::string sql = "SELECT u.*, t.score FROM users u JOIN temp_scores t ON u.id = t.user_id";
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto ast = parser.parse_top_level();
        REQUIRE(ast != nullptr);
    }
}
