#include <catch2/catch_test_macros.hpp>
#include <libglot/sql/parser.h>
#include <libglot/sql/generator.h>
#include <libsqlglot/dialects.h>

using namespace libglot::sql;

// ========================================================================
// PostgreSQL → Other Dialects
// ========================================================================

TEST_CASE("Transpile: PostgreSQL → MySQL", "[transpilation][postgres][mysql]") {
    std::string sql = "SELECT * FROM users WHERE active = TRUE LIMIT 10";
    std::string output =     libglot::Arena arena;
    SQLParser parser(arena, sql);
    auto ast = parser.parse_top_level();
    SQLGenerator gen(SQLDialect::MySQL);
    std::string output = gen.generate(ast);;

    REQUIRE(!output.empty());
    REQUIRE(output.find("SELECT") != std::string::npos);
    REQUIRE(output.find("LIMIT") != std::string::npos);
}

TEST_CASE("Transpile: PostgreSQL → SQL Server (LIMIT to TOP)", "[transpilation][postgres][sqlserver]") {
    std::string sql = "SELECT * FROM users LIMIT 10";

    libglot::Arena arena;
    SQLParser parser(arena, sql);
    auto stmt = parser.parse_select();
    std::string output =     SQLGenerator gen(SQLDialect::SQLServer);
    std::string output = gen.generate(stmt);;

    REQUIRE(output.find("TOP") != std::string::npos);
    REQUIRE(output.find("LIMIT") == std::string::npos);
}

TEST_CASE("Transpile: PostgreSQL → BigQuery", "[transpilation][postgres][bigquery]") {
    std::string sql = "SELECT id, name FROM users WHERE score > 100";
    std::string output =     libglot::Arena arena;
    SQLParser parser(arena, sql);
    auto ast = parser.parse_top_level();
    SQLGenerator gen(SQLDialect::BigQuery);
    std::string output = gen.generate(ast);;

    REQUIRE(!output.empty());
    REQUIRE(output.find("SELECT") != std::string::npos);
}

TEST_CASE("Transpile: PostgreSQL → DuckDB", "[transpilation][postgres][duckdb]") {
    std::string sql = "SELECT * FROM users ORDER BY created_at DESC LIMIT 20";
    std::string output =     libglot::Arena arena;
    SQLParser parser(arena, sql);
    auto ast = parser.parse_top_level();
    SQLGenerator gen(SQLDialect::DuckDB);
    std::string output = gen.generate(ast);;

    REQUIRE(!output.empty());
    REQUIRE(output.find("LIMIT") != std::string::npos);
}

TEST_CASE("Transpile: PostgreSQL → Snowflake", "[transpilation][postgres][snowflake]") {
    std::string sql = "SELECT COUNT(*) FROM orders WHERE status = 'completed'";
    std::string output =     libglot::Arena arena;
    SQLParser parser(arena, sql);
    auto ast = parser.parse_top_level();
    SQLGenerator gen(SQLDialect::Snowflake);
    std::string output = gen.generate(ast);;

    REQUIRE(!output.empty());
    REQUIRE(output.find("COUNT") != std::string::npos);
}

// ========================================================================
// MySQL → Other Dialects
// ========================================================================

TEST_CASE("Transpile: MySQL → PostgreSQL", "[transpilation][mysql][postgres]") {
    std::string sql = "SELECT * FROM users LIMIT 10";
    std::string output =     libglot::Arena arena;
    SQLParser parser(arena, sql);
    auto ast = parser.parse_top_level();
    SQLGenerator gen(SQLDialect::PostgreSQL);
    std::string output = gen.generate(ast);;

    REQUIRE(!output.empty());
    REQUIRE(output.find("LIMIT") != std::string::npos);
}

TEST_CASE("Transpile: MySQL → SQL Server (LIMIT to TOP)", "[transpilation][mysql][sqlserver]") {
    std::string sql = "SELECT * FROM products LIMIT 5";

    libglot::Arena arena;
    SQLParser parser(arena, sql);
    auto stmt = parser.parse_select();
    std::string output =     SQLGenerator gen(SQLDialect::SQLServer);
    std::string output = gen.generate(stmt);;

    REQUIRE(output.find("TOP") != std::string::npos);
    REQUIRE(output.find("LIMIT") == std::string::npos);
}

TEST_CASE("Transpile: MySQL → BigQuery", "[transpilation][mysql][bigquery]") {
    std::string sql = "SELECT user_id, SUM(amount) as total FROM transactions GROUP BY user_id";
    std::string output =     libglot::Arena arena;
    SQLParser parser(arena, sql);
    auto ast = parser.parse_top_level();
    SQLGenerator gen(SQLDialect::BigQuery);
    std::string output = gen.generate(ast);;

    REQUIRE(!output.empty());
    REQUIRE(output.find("SUM") != std::string::npos);
    REQUIRE(output.find("GROUP BY") != std::string::npos);
}

TEST_CASE("Transpile: MySQL → DuckDB", "[transpilation][mysql][duckdb]") {
    std::string sql = "SELECT * FROM sales WHERE sale_date >= '2024-01-01'";
    std::string output =     libglot::Arena arena;
    SQLParser parser(arena, sql);
    auto ast = parser.parse_top_level();
    SQLGenerator gen(SQLDialect::DuckDB);
    std::string output = gen.generate(ast);;

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
    std::string output =     SQLGenerator gen(SQLDialect::PostgreSQL);
    std::string output = gen.generate(stmt);;

    REQUIRE(!output.empty());
    REQUIRE(output.find("SELECT") != std::string::npos);
}

TEST_CASE("Transpile: SQL Server → MySQL", "[transpilation][sqlserver][mysql]") {
    std::string sql = "SELECT COUNT(*) FROM orders";

    libglot::Arena arena;
    SQLParser parser(arena, sql);
    auto stmt = parser.parse_select();
    std::string output =     SQLGenerator gen(SQLDialect::MySQL);
    std::string output = gen.generate(stmt);;

    REQUIRE(!output.empty());
    REQUIRE(output.find("COUNT") != std::string::npos);
}

// ========================================================================
// BigQuery → Other Dialects
// ========================================================================

TEST_CASE("Transpile: BigQuery → PostgreSQL", "[transpilation][bigquery][postgres]") {
    std::string sql = "SELECT * FROM users LIMIT 50";
    std::string output =     libglot::Arena arena;
    SQLParser parser(arena, sql);
    auto ast = parser.parse_top_level();
    SQLGenerator gen(SQLDialect::PostgreSQL);
    std::string output = gen.generate(ast);;

    REQUIRE(!output.empty());
    REQUIRE(output.find("LIMIT") != std::string::npos);
}

TEST_CASE("Transpile: BigQuery → MySQL", "[transpilation][bigquery][mysql]") {
    std::string sql = "SELECT user_id, name FROM users WHERE active = TRUE";

    libglot::Arena arena;
    SQLParser parser(arena, sql);
    auto stmt = parser.parse_select();
    std::string output =     SQLGenerator gen(SQLDialect::MySQL);
    std::string output = gen.generate(stmt);;

    REQUIRE(!output.empty());
    REQUIRE(output.find("SELECT") != std::string::npos);
}

// ========================================================================
// Snowflake → Other Dialects
// ========================================================================

TEST_CASE("Transpile: Snowflake → PostgreSQL", "[transpilation][snowflake][postgres]") {
    std::string sql = "SELECT * FROM products ORDER BY price DESC";
    std::string output =     libglot::Arena arena;
    SQLParser parser(arena, sql);
    auto ast = parser.parse_top_level();
    SQLGenerator gen(SQLDialect::PostgreSQL);
    std::string output = gen.generate(ast);;

    REQUIRE(!output.empty());
    REQUIRE(output.find("ORDER BY") != std::string::npos);
}

TEST_CASE("Transpile: Snowflake → DuckDB", "[transpilation][snowflake][duckdb]") {
    std::string sql = "SELECT region, COUNT(*) as cnt FROM sales GROUP BY region";
    std::string output =     libglot::Arena arena;
    SQLParser parser(arena, sql);
    auto ast = parser.parse_top_level();
    SQLGenerator gen(SQLDialect::DuckDB);
    std::string output = gen.generate(ast);;

    REQUIRE(!output.empty());
    REQUIRE(output.find("COUNT") != std::string::npos);
}

// ========================================================================
// DuckDB → Other Dialects
// ========================================================================

TEST_CASE("Transpile: DuckDB → PostgreSQL", "[transpilation][duckdb][postgres]") {
    std::string sql = "SELECT * FROM events WHERE event_timestamp > '2024-01-01'";
    std::string output =     libglot::Arena arena;
    SQLParser parser(arena, sql);
    auto ast = parser.parse_top_level();
    SQLGenerator gen(SQLDialect::PostgreSQL);
    std::string output = gen.generate(ast);;

    REQUIRE(!output.empty());
    REQUIRE(output.find("SELECT") != std::string::npos);
}

TEST_CASE("Transpile: DuckDB → MySQL", "[transpilation][duckdb][mysql]") {
    std::string sql = "SELECT id, name FROM users LIMIT 100";
    std::string output =     libglot::Arena arena;
    SQLParser parser(arena, sql);
    auto ast = parser.parse_top_level();
    SQLGenerator gen(SQLDialect::MySQL);
    std::string output = gen.generate(ast);;

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
    std::string pg =     libglot::Arena arena;
    SQLParser parser(arena, sql);
    auto ast = parser.parse_top_level();
    SQLGenerator gen(SQLDialect::PostgreSQL);
    std::string pg = gen.generate(ast);;
    std::string mysql =     libglot::Arena arena;
    SQLParser parser(arena, sql);
    auto ast = parser.parse_top_level();
    SQLGenerator gen(SQLDialect::MySQL);
    std::string mysql = gen.generate(ast);;
    std::string bigquery =     libglot::Arena arena;
    SQLParser parser(arena, sql);
    auto ast = parser.parse_top_level();
    SQLGenerator gen(SQLDialect::BigQuery);
    std::string bigquery = gen.generate(ast);;

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

    Arena arena1, arena2, arena3;

    Parser parser1(arena1, sql);
    auto stmt1 = parser1.parse_select();
    std::string pg =     SQLGenerator gen(SQLDialect::PostgreSQL);
    std::string pg = gen.generate(stmt1);;

    Parser parser2(arena2, sql);
    auto stmt2 = parser2.parse_select();
    std::string bigquery =     SQLGenerator gen(SQLDialect::BigQuery);
    std::string bigquery = gen.generate(stmt2);;

    Parser parser3(arena3, sql);
    auto stmt3 = parser3.parse_select();
    std::string snowflake =     SQLGenerator gen(SQLDialect::Snowflake);
    std::string snowflake = gen.generate(stmt3);;

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

    std::string mysql =     libglot::Arena arena;
    SQLParser parser(arena, sql);
    auto ast = parser.parse_top_level();
    SQLGenerator gen(SQLDialect::MySQL);
    std::string mysql = gen.generate(ast);;
    std::string postgres =     libglot::Arena arena;
    SQLParser parser(arena, sql);
    auto ast = parser.parse_top_level();
    SQLGenerator gen(SQLDialect::PostgreSQL);
    std::string postgres = gen.generate(ast);;
    std::string duckdb =     libglot::Arena arena;
    SQLParser parser(arena, sql);
    auto ast = parser.parse_top_level();
    SQLGenerator gen(SQLDialect::DuckDB);
    std::string duckdb = gen.generate(ast);;

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
    std::string output =     SQLGenerator gen(SQLDialect::PostgreSQL);
    std::string output = gen.generate(stmt);;

    REQUIRE(output.find("TRUE") != std::string::npos);
}

TEST_CASE("Transpile: Boolean TRUE to SQL Server", "[transpilation][boolean]") {
    std::string sql = "SELECT * FROM users WHERE active = TRUE";

    libglot::Arena arena;
    SQLParser parser(arena, sql);
    auto stmt = parser.parse_select();
    std::string output =     SQLGenerator gen(SQLDialect::SQLServer);
    std::string output = gen.generate(stmt);;

    // SQL Server converts TRUE to 1
    REQUIRE(output.find("= 1") != std::string::npos);
}

// ========================================================================
// Round-trip Verification
// ========================================================================

TEST_CASE("Transpile: Round-trip preserves semantics", "[transpilation][roundtrip]") {
    std::string original = "SELECT id, name FROM users WHERE score > 100 LIMIT 50";

    // PostgreSQL → MySQL → PostgreSQL
    std::string mysql_version =     libglot::Arena arena;
    SQLParser parser(arena, original);
    auto ast = parser.parse_top_level();
    SQLGenerator gen(SQLDialect::MySQL);
    std::string mysql_version = gen.generate(ast);;
    std::string back_to_pg =     libglot::Arena arena;
    SQLParser parser(arena, mysql_version);
    auto ast = parser.parse_top_level();
    SQLGenerator gen(SQLDialect::PostgreSQL);
    std::string back_to_pg = gen.generate(ast);;

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
    auto ast = [&]() { libglot::sql::SQLParser p(arena, sql); return p.parse_top_level(); }();

    // Generate for multiple dialects from single AST
    std::string pg =     SQLGenerator gen(SQLDialect::PostgreSQL);
    std::string pg = gen.generate(ast);;
    std::string mysql =     SQLGenerator gen(SQLDialect::MySQL);
    std::string mysql = gen.generate(ast);;
    std::string bigquery =     SQLGenerator gen(SQLDialect::BigQuery);
    std::string bigquery = gen.generate(ast);;
    std::string duckdb =     SQLGenerator gen(SQLDialect::DuckDB);
    std::string duckdb = gen.generate(ast);;
    std::string snowflake =     SQLGenerator gen(SQLDialect::Snowflake);
    std::string snowflake = gen.generate(ast);;

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
    std::string output =     SQLGenerator gen(SQLDialect::PostgreSQL);
    std::string output = gen.generate(stmt);;

    // PostgreSQL supports ILIKE natively
    REQUIRE(output.find("ILIKE") != std::string::npos);
}

TEST_CASE("Transpile: ILIKE polyfill (MySQL)", "[transpilation][ilike]") {
    std::string sql = "SELECT * FROM users WHERE name ILIKE 'john%'";

    libglot::Arena arena;
    SQLParser parser(arena, sql);
    auto stmt = parser.parse_select();
    std::string output =     SQLGenerator gen(SQLDialect::MySQL);
    std::string output = gen.generate(stmt);;

    // MySQL should transform ILIKE
    REQUIRE(output.find("LOWER") != std::string::npos);
    REQUIRE(output.find("LIKE") != std::string::npos);
}
