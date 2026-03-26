#include <catch2/catch_test_macros.hpp>
#include <libglot/sql/parser.h>
#include <libglot/sql/generator.h>
#include <libglot/util/arena.h>

using namespace libglot::sql;

// ========================================================================
// PostgreSQL → Other Dialects
// ========================================================================

TEST_CASE("Transpile: PostgreSQL → MySQL", "[transpilation][postgres][mysql]") {
    std::string sql = "SELECT * FROM users WHERE active = TRUE LIMIT 10";

    libglot::Arena arena;
    SQLParser parser(arena, sql);
    auto ast = parser.parse_top_level();
    SQLGenerator gen(SQLDialect::MySQL);
    std::string output = gen.generate(ast);

    REQUIRE(!output.empty());
    REQUIRE(output.find("SELECT") != std::string::npos);
    REQUIRE(output.find("LIMIT") != std::string::npos);
}

TEST_CASE("Transpile: PostgreSQL → SQL Server (LIMIT to TOP)", "[transpilation][postgres][sqlserver]") {
    std::string sql = "SELECT * FROM users LIMIT 10";

    libglot::Arena arena;
    SQLParser parser(arena, sql);
    auto stmt = parser.parse_select();
    SQLGenerator gen(SQLDialect::SQLServer);
    std::string output = gen.generate(stmt);

    // LIMIT→TOP transformation implemented
    REQUIRE(!output.empty());
    REQUIRE(output.find("SELECT") != std::string::npos);
    REQUIRE(output.find("TOP") != std::string::npos);
    REQUIRE(output.find("users") != std::string::npos);
    REQUIRE(output.find("LIMIT") == std::string::npos);  // Should not contain LIMIT
}

TEST_CASE("Transpile: PostgreSQL → BigQuery", "[transpilation][postgres][bigquery]") {
    std::string sql = "SELECT id, name FROM users WHERE score > 100";

    libglot::Arena arena;
    SQLParser parser(arena, sql);
    auto ast = parser.parse_top_level();
    SQLGenerator gen(SQLDialect::BigQuery);
    std::string output = gen.generate(ast);

    REQUIRE(!output.empty());
    REQUIRE(output.find("SELECT") != std::string::npos);
}

TEST_CASE("Transpile: PostgreSQL → DuckDB", "[transpilation][postgres][duckdb]") {
    std::string sql = "SELECT * FROM users ORDER BY created_at DESC LIMIT 20";

    libglot::Arena arena;
    SQLParser parser(arena, sql);
    auto ast = parser.parse_top_level();
    SQLGenerator gen(SQLDialect::DuckDB);
    std::string output = gen.generate(ast);

    REQUIRE(!output.empty());
    REQUIRE(output.find("LIMIT") != std::string::npos);
}

TEST_CASE("Transpile: PostgreSQL → Snowflake", "[transpilation][postgres][snowflake]") {
    std::string sql = "SELECT COUNT(*) FROM orders WHERE status = 'completed'";

    libglot::Arena arena;
    SQLParser parser(arena, sql);
    auto ast = parser.parse_top_level();
    SQLGenerator gen(SQLDialect::Snowflake);
    std::string output = gen.generate(ast);

    REQUIRE(!output.empty());
    REQUIRE(output.find("SELECT") != std::string::npos);
    REQUIRE(output.find("orders") != std::string::npos);
}

// ========================================================================
// MySQL → Other Dialects
// ========================================================================

TEST_CASE("Transpile: MySQL → PostgreSQL", "[transpilation][mysql][postgres]") {
    std::string sql = "SELECT * FROM users LIMIT 10";

    libglot::Arena arena;
    SQLParser parser(arena, sql);
    auto ast = parser.parse_top_level();
    SQLGenerator gen(SQLDialect::PostgreSQL);
    std::string output = gen.generate(ast);

    REQUIRE(!output.empty());
    REQUIRE(output.find("LIMIT") != std::string::npos);
}

TEST_CASE("Transpile: MySQL → SQL Server (LIMIT to TOP)", "[transpilation][mysql][sqlserver]") {
    std::string sql = "SELECT * FROM products LIMIT 5";

    libglot::Arena arena;
    SQLParser parser(arena, sql);
    auto stmt = parser.parse_select();
    SQLGenerator gen(SQLDialect::SQLServer);
    std::string output = gen.generate(stmt);

    // TODO: Implement LIMIT→TOP transformation for SQL Server
    REQUIRE(!output.empty());
    REQUIRE(output.find("SELECT") != std::string::npos);
    REQUIRE(output.find("products") != std::string::npos);
}

TEST_CASE("Transpile: MySQL → BigQuery", "[transpilation][mysql][bigquery]") {
    std::string sql = "SELECT user_id, SUM(amount) as total FROM transactions GROUP BY user_id";

    libglot::Arena arena;
    SQLParser parser(arena, sql);
    auto ast = parser.parse_top_level();
    SQLGenerator gen(SQLDialect::BigQuery);
    std::string output = gen.generate(ast);

    REQUIRE(!output.empty());
    REQUIRE(output.find("SELECT") != std::string::npos);
    REQUIRE(output.find("GROUP BY") != std::string::npos);
    REQUIRE(output.find("transactions") != std::string::npos);
}

TEST_CASE("Transpile: MySQL → DuckDB", "[transpilation][mysql][duckdb]") {
    std::string sql = "SELECT * FROM sales WHERE sale_date >= '2024-01-01'";

    libglot::Arena arena;
    SQLParser parser(arena, sql);
    auto ast = parser.parse_top_level();
    SQLGenerator gen(SQLDialect::DuckDB);
    std::string output = gen.generate(ast);

    REQUIRE(!output.empty());
    REQUIRE(output.find("SELECT") != std::string::npos);
}

// ========================================================================
// SQL Server → Other Dialects
// ========================================================================

TEST_CASE("Transpile: SQL Server → PostgreSQL", "[transpilation][sqlserver][postgres]") {
    std::string sql = "SELECT * FROM users WHERE id IN (1, 2, 3)";

    libglot::Arena arena;
    SQLParser parser(arena, sql);
    auto stmt = parser.parse_select();
    SQLGenerator gen(SQLDialect::PostgreSQL);
    std::string output = gen.generate(stmt);

    REQUIRE(!output.empty());
    REQUIRE(output.find("SELECT") != std::string::npos);
    REQUIRE(output.find("users") != std::string::npos);
    REQUIRE(output.find("IN") != std::string::npos);
}

TEST_CASE("Transpile: SQL Server → MySQL", "[transpilation][sqlserver][mysql]") {
    std::string sql = "SELECT COUNT(*) FROM orders";

    libglot::Arena arena;
    SQLParser parser(arena, sql);
    auto stmt = parser.parse_select();
    SQLGenerator gen(SQLDialect::MySQL);
    std::string output = gen.generate(stmt);

    REQUIRE(!output.empty());
    REQUIRE(output.find("SELECT") != std::string::npos);
    REQUIRE(output.find("orders") != std::string::npos);
}

// ========================================================================
// BigQuery → Other Dialects
// ========================================================================

TEST_CASE("Transpile: BigQuery → PostgreSQL", "[transpilation][bigquery][postgres]") {
    std::string sql = "SELECT * FROM users LIMIT 50";

    libglot::Arena arena;
    SQLParser parser(arena, sql);
    auto ast = parser.parse_top_level();
    SQLGenerator gen(SQLDialect::PostgreSQL);
    std::string output = gen.generate(ast);

    REQUIRE(!output.empty());
    REQUIRE(output.find("LIMIT") != std::string::npos);
}

TEST_CASE("Transpile: BigQuery → MySQL", "[transpilation][bigquery][mysql]") {
    std::string sql = "SELECT user_id, name FROM users WHERE active = TRUE";

    libglot::Arena arena;
    SQLParser parser(arena, sql);
    auto stmt = parser.parse_select();
    SQLGenerator gen(SQLDialect::MySQL);
    std::string output = gen.generate(stmt);

    REQUIRE(!output.empty());
    REQUIRE(output.find("SELECT") != std::string::npos);
}

// ========================================================================
// Snowflake → Other Dialects
// ========================================================================

TEST_CASE("Transpile: Snowflake → PostgreSQL", "[transpilation][snowflake][postgres]") {
    std::string sql = "SELECT * FROM products ORDER BY price DESC";

    libglot::Arena arena;
    SQLParser parser(arena, sql);
    auto ast = parser.parse_top_level();
    SQLGenerator gen(SQLDialect::PostgreSQL);
    std::string output = gen.generate(ast);

    REQUIRE(!output.empty());
    REQUIRE(output.find("ORDER BY") != std::string::npos);
}

TEST_CASE("Transpile: Snowflake → DuckDB", "[transpilation][snowflake][duckdb]") {
    std::string sql = "SELECT region, COUNT(*) as cnt FROM sales GROUP BY region";

    libglot::Arena arena;
    SQLParser parser(arena, sql);
    auto ast = parser.parse_top_level();
    SQLGenerator gen(SQLDialect::DuckDB);
    std::string output = gen.generate(ast);

    REQUIRE(!output.empty());
    REQUIRE(output.find("SELECT") != std::string::npos);
    REQUIRE(output.find("GROUP BY") != std::string::npos);
}

// ========================================================================
// DuckDB → Other Dialects
// ========================================================================

TEST_CASE("Transpile: DuckDB → PostgreSQL", "[transpilation][duckdb][postgres]") {
    std::string sql = "SELECT * FROM events WHERE event_timestamp > '2024-01-01'";

    libglot::Arena arena;
    SQLParser parser(arena, sql);
    auto ast = parser.parse_top_level();
    SQLGenerator gen(SQLDialect::PostgreSQL);
    std::string output = gen.generate(ast);

    REQUIRE(!output.empty());
    REQUIRE(output.find("SELECT") != std::string::npos);
}

TEST_CASE("Transpile: DuckDB → MySQL", "[transpilation][duckdb][mysql]") {
    std::string sql = "SELECT id, name FROM users LIMIT 100";

    libglot::Arena arena;
    SQLParser parser(arena, sql);
    auto ast = parser.parse_top_level();
    SQLGenerator gen(SQLDialect::MySQL);
    std::string output = gen.generate(ast);

    REQUIRE(!output.empty());
    REQUIRE(output.find("LIMIT") != std::string::npos);
}

// ========================================================================
// Complex Queries Across Dialects
// ========================================================================

TEST_CASE("Transpile: Complex CTE query across dialects", "[transpilation][complex]") {
    std::string sql = R"(
        WITH regional_sales AS (
            SELECT region, SUM(amount) as total
            FROM sales
            GROUP BY region
        )
        SELECT * FROM regional_sales WHERE total > 10000
    )";

    // Test multiple target dialects
    libglot::Arena arena1, arena2, arena3;

    SQLParser parser1(arena1, sql);
    auto ast1 = parser1.parse_top_level();
    SQLGenerator gen1(SQLDialect::PostgreSQL);
    std::string pg = gen1.generate(ast1);

    SQLParser parser2(arena2, sql);
    auto ast2 = parser2.parse_top_level();
    SQLGenerator gen2(SQLDialect::MySQL);
    std::string mysql = gen2.generate(ast2);

    SQLParser parser3(arena3, sql);
    auto ast3 = parser3.parse_top_level();
    SQLGenerator gen3(SQLDialect::BigQuery);
    std::string bigquery = gen3.generate(ast3);

    REQUIRE(!pg.empty());
    REQUIRE(!mysql.empty());
    REQUIRE(!bigquery.empty());

    REQUIRE(pg.find("WITH") != std::string::npos);
    REQUIRE(mysql.find("WITH") != std::string::npos);
    REQUIRE(bigquery.find("WITH") != std::string::npos);
}

TEST_CASE("Transpile: Window functions across dialects", "[transpilation][complex]") {
    std::string sql = R"(
        SELECT
            user_id,
            ROW_NUMBER() OVER (ORDER BY score DESC) as rank
        FROM leaderboard
    )";

    libglot::Arena arena1, arena2, arena3;

    SQLParser parser1(arena1, sql);
    auto stmt1 = parser1.parse_select();
    SQLGenerator gen1(SQLDialect::PostgreSQL);
    std::string pg = gen1.generate(stmt1);

    SQLParser parser2(arena2, sql);
    auto stmt2 = parser2.parse_select();
    SQLGenerator gen2(SQLDialect::BigQuery);
    std::string bigquery = gen2.generate(stmt2);

    SQLParser parser3(arena3, sql);
    auto stmt3 = parser3.parse_select();
    SQLGenerator gen3(SQLDialect::Snowflake);
    std::string snowflake = gen3.generate(stmt3);

    REQUIRE(pg.find("ROW_NUMBER") != std::string::npos);
    REQUIRE(bigquery.find("ROW_NUMBER") != std::string::npos);
    REQUIRE(snowflake.find("ROW_NUMBER") != std::string::npos);
}

TEST_CASE("Transpile: JOIN queries across dialects", "[transpilation][complex]") {
    std::string sql = R"(
        SELECT u.id, u.name, o.total
        FROM users u
        INNER JOIN orders o ON u.id = o.user_id
        WHERE o.status = 'completed'
    )";

    libglot::Arena arena1, arena2, arena3;

    SQLParser parser1(arena1, sql);
    auto ast1 = parser1.parse_top_level();
    SQLGenerator gen1(SQLDialect::MySQL);
    std::string mysql = gen1.generate(ast1);

    SQLParser parser2(arena2, sql);
    auto ast2 = parser2.parse_top_level();
    SQLGenerator gen2(SQLDialect::PostgreSQL);
    std::string postgres = gen2.generate(ast2);

    SQLParser parser3(arena3, sql);
    auto ast3 = parser3.parse_top_level();
    SQLGenerator gen3(SQLDialect::DuckDB);
    std::string duckdb = gen3.generate(ast3);

    REQUIRE(!mysql.empty());
    REQUIRE(!postgres.empty());
    REQUIRE(!duckdb.empty());

    REQUIRE(mysql.find("INNER JOIN") != std::string::npos);
    REQUIRE(postgres.find("INNER JOIN") != std::string::npos);
    REQUIRE(duckdb.find("INNER JOIN") != std::string::npos);
}

// ========================================================================
// Boolean Literal Transformations
// ========================================================================

TEST_CASE("Transpile: Boolean TRUE to PostgreSQL", "[transpilation][boolean]") {
    std::string sql = "SELECT * FROM users WHERE active = TRUE";

    libglot::Arena arena;
    SQLParser parser(arena, sql);
    auto stmt = parser.parse_select();
    SQLGenerator gen(SQLDialect::PostgreSQL);
    std::string output = gen.generate(stmt);

    REQUIRE(output.find("TRUE") != std::string::npos);
}

TEST_CASE("Transpile: Boolean TRUE to SQL Server", "[transpilation][boolean]") {
    std::string sql = "SELECT * FROM users WHERE active = TRUE";

    libglot::Arena arena;
    SQLParser parser(arena, sql);
    auto stmt = parser.parse_select();
    SQLGenerator gen(SQLDialect::SQLServer);
    std::string output = gen.generate(stmt);

    // SQL Server converts TRUE to 1
    REQUIRE(output.find("= 1") != std::string::npos);
}

// ========================================================================
// Round-trip Verification
// ========================================================================

TEST_CASE("Transpile: Round-trip preserves semantics", "[transpilation][roundtrip]") {
    std::string original = "SELECT id, name FROM users WHERE score > 100 LIMIT 50";

    // PostgreSQL → MySQL → PostgreSQL
    libglot::Arena arena1;
    SQLParser parser1(arena1, original);
    auto ast1 = parser1.parse_top_level();
    SQLGenerator gen1(SQLDialect::MySQL);
    std::string mysql_version = gen1.generate(ast1);

    libglot::Arena arena2;
    SQLParser parser2(arena2, mysql_version);
    auto ast2 = parser2.parse_top_level();
    SQLGenerator gen2(SQLDialect::PostgreSQL);
    std::string back_to_pg = gen2.generate(ast2);

    // Both should contain the same semantic elements
    REQUIRE(back_to_pg.find("SELECT") != std::string::npos);
    REQUIRE(back_to_pg.find("WHERE") != std::string::npos);
    REQUIRE(back_to_pg.find("LIMIT") != std::string::npos);
}

// ========================================================================
// Multi-target Generation (Parse Once, Generate Many)
// ========================================================================

TEST_CASE("Transpile: Single parse, multiple targets", "[transpilation][multitarget]") {
    std::string sql = "SELECT name, email FROM users WHERE age >= 18";

    libglot::Arena arena;
    SQLParser parser(arena, sql);
    auto ast = parser.parse_top_level();

    // Generate for multiple dialects from single AST
    SQLGenerator gen_pg(SQLDialect::PostgreSQL);
    std::string pg = gen_pg.generate(ast);

    SQLGenerator gen_mysql(SQLDialect::MySQL);
    std::string mysql = gen_mysql.generate(ast);

    SQLGenerator gen_bigquery(SQLDialect::BigQuery);
    std::string bigquery = gen_bigquery.generate(ast);

    SQLGenerator gen_duckdb(SQLDialect::DuckDB);
    std::string duckdb = gen_duckdb.generate(ast);

    SQLGenerator gen_snowflake(SQLDialect::Snowflake);
    std::string snowflake = gen_snowflake.generate(ast);

    // All should be valid and non-empty
    REQUIRE(!pg.empty());
    REQUIRE(!mysql.empty());
    REQUIRE(!bigquery.empty());
    REQUIRE(!duckdb.empty());
    REQUIRE(!snowflake.empty());

    // All should contain core SELECT elements
    REQUIRE(pg.find("SELECT") != std::string::npos);
    REQUIRE(mysql.find("SELECT") != std::string::npos);
    REQUIRE(bigquery.find("SELECT") != std::string::npos);
    REQUIRE(duckdb.find("SELECT") != std::string::npos);
    REQUIRE(snowflake.find("SELECT") != std::string::npos);
}

// ========================================================================
// ILIKE Transformation
// ========================================================================

TEST_CASE("Transpile: ILIKE native support (PostgreSQL)", "[transpilation][ilike]") {
    std::string sql = "SELECT * FROM users WHERE name ILIKE 'john%'";

    libglot::Arena arena;
    SQLParser parser(arena, sql);
    auto stmt = parser.parse_select();
    SQLGenerator gen(SQLDialect::PostgreSQL);
    std::string output = gen.generate(stmt);

    // PostgreSQL supports ILIKE natively
    REQUIRE(output.find("ILIKE") != std::string::npos);
}

TEST_CASE("Transpile: ILIKE polyfill (MySQL)", "[transpilation][ilike]") {
    std::string sql = "SELECT * FROM users WHERE name ILIKE 'john%'";

    libglot::Arena arena;
    SQLParser parser(arena, sql);
    auto stmt = parser.parse_select();
    SQLGenerator gen(SQLDialect::MySQL);
    std::string output = gen.generate(stmt);

    // ILIKE→LOWER+LIKE polyfill implemented for MySQL
    REQUIRE(!output.empty());
    REQUIRE(output.find("SELECT") != std::string::npos);
    REQUIRE(output.find("LOWER") != std::string::npos);  // Should contain LOWER()
    REQUIRE(output.find("LIKE") != std::string::npos);   // Should use LIKE not ILIKE
    REQUIRE(output.find("ILIKE") == std::string::npos);  // Should NOT contain ILIKE
}
