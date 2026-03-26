#include <catch2/catch_test_macros.hpp>
#include <libglot/sql/parser.h>
#include <libglot/sql/generator.h>
#include <libglot/sql/dialect_traits.h>
#include <libglot/util/arena.h>

using namespace libglot::sql;

// ============================================================================
// CTE (WITH CLAUSE) TESTS
// ============================================================================

TEST_CASE("CTE - Simple WITH clause", "[phase3][cte]") {
    libglot::Arena arena;
    SQLParser parser(arena,
        "WITH high_scorers AS (SELECT * FROM users WHERE score > 100) "
        "SELECT * FROM high_scorers");

    auto stmt = static_cast<SelectStmt*>(parser.parse_top_level());

    REQUIRE(stmt != nullptr);
    REQUIRE(stmt->with != nullptr);
    REQUIRE(stmt->with->ctes.size() == 1);
    REQUIRE(stmt->with->ctes[0]->name == "high_scorers");
    REQUIRE(stmt->with->ctes[0]->query != nullptr);

    SQLGenerator gen(SQLDialect::ANSI);
    std::string sql = gen.generate(stmt);
    REQUIRE(sql.find("WITH") != std::string::npos);
    REQUIRE(sql.find("high_scorers") != std::string::npos);
}

TEST_CASE("CTE - Multiple CTEs", "[phase3][cte]") {
    libglot::Arena arena;
    SQLParser parser(arena,
        "WITH users_active AS (SELECT * FROM users WHERE active = 1), "
        "users_premium AS (SELECT * FROM users_active WHERE premium = 1) "
        "SELECT * FROM users_premium");

    auto stmt = static_cast<SelectStmt*>(parser.parse_top_level());

    REQUIRE(stmt->with != nullptr);
    REQUIRE(stmt->with->ctes.size() == 2);
    REQUIRE(stmt->with->ctes[0]->name == "users_active");
    REQUIRE(stmt->with->ctes[1]->name == "users_premium");

    SQLGenerator gen(SQLDialect::ANSI);
    std::string sql = gen.generate(stmt);
    REQUIRE(sql.find("WITH") != std::string::npos);
    REQUIRE(sql.find("users_active") != std::string::npos);
    REQUIRE(sql.find("users_premium") != std::string::npos);
}

TEST_CASE("CTE - With column list", "[phase3][cte]") {
    libglot::Arena arena;
    SQLParser parser(arena,
        "WITH top_users (user_id, user_name, user_score) AS (SELECT id, name, score FROM users ORDER BY score DESC LIMIT 10) "
        "SELECT * FROM top_users");

    auto stmt = static_cast<SelectStmt*>(parser.parse_top_level());

    REQUIRE(stmt->with->ctes.size() == 1);
    REQUIRE(stmt->with->ctes[0]->columns.size() == 3);
    REQUIRE(stmt->with->ctes[0]->columns[0] == "user_id");
    REQUIRE(stmt->with->ctes[0]->columns[1] == "user_name");
    REQUIRE(stmt->with->ctes[0]->columns[2] == "user_score");

    SQLGenerator gen(SQLDialect::ANSI);
    std::string sql = gen.generate(stmt);
    REQUIRE(sql.find("top_users") != std::string::npos);
    REQUIRE(sql.find("user_id") != std::string::npos);
}

// ============================================================================
// WINDOW FUNCTION TESTS
// ============================================================================

TEST_CASE("Window - ROW_NUMBER with PARTITION BY", "[phase3][window]") {
    libglot::Arena arena;
    SQLParser parser(arena, "SELECT ROW_NUMBER() OVER (PARTITION BY department ORDER BY salary) FROM employees");

    auto stmt = static_cast<SelectStmt*>(parser.parse_top_level());

    REQUIRE(stmt->columns.size() == 1);
    REQUIRE(stmt->columns[0]->type == SQLNodeKind::WINDOW_FUNCTION);
    auto* win_func = static_cast<WindowFunction*>(stmt->columns[0]);
    REQUIRE(win_func->function_name == "ROW_NUMBER");
    REQUIRE(win_func->over != nullptr);
    REQUIRE(win_func->over->partition_by.size() == 1);
    REQUIRE(win_func->over->order_by.size() == 1);

    SQLGenerator gen(SQLDialect::ANSI);
    std::string sql = gen.generate(stmt);
    REQUIRE(sql.find("ROW_NUMBER") != std::string::npos);
    REQUIRE(sql.find("OVER") != std::string::npos);
    REQUIRE(sql.find("PARTITION BY") != std::string::npos);
}

TEST_CASE("Window - RANK with ORDER BY", "[phase3][window]") {
    libglot::Arena arena;
    SQLParser parser(arena, "SELECT RANK() OVER (ORDER BY score DESC) FROM users");

    auto stmt = static_cast<SelectStmt*>(parser.parse_top_level());

    REQUIRE(stmt->columns[0]->type == SQLNodeKind::WINDOW_FUNCTION);
    auto* win_func = static_cast<WindowFunction*>(stmt->columns[0]);
    REQUIRE(win_func->function_name == "RANK");
    REQUIRE(win_func->over->partition_by.empty());
    REQUIRE(win_func->over->order_by.size() == 1);

    SQLGenerator gen(SQLDialect::ANSI);
    std::string sql = gen.generate(stmt);
    REQUIRE(sql.find("RANK") != std::string::npos);
    REQUIRE(sql.find("OVER") != std::string::npos);
}

TEST_CASE("Window - LAG with arguments", "[phase3][window]") {
    libglot::Arena arena;
    SQLParser parser(arena, "SELECT LAG(price, 1) OVER (ORDER BY trade_date) FROM stocks");

    auto stmt = static_cast<SelectStmt*>(parser.parse_top_level());

    REQUIRE(stmt->columns[0]->type == SQLNodeKind::WINDOW_FUNCTION);
    auto* win_func = static_cast<WindowFunction*>(stmt->columns[0]);
    REQUIRE(win_func->function_name == "LAG");
    REQUIRE(win_func->args.size() == 2);

    SQLGenerator gen(SQLDialect::ANSI);
    std::string sql = gen.generate(stmt);
    REQUIRE(sql.find("LAG") != std::string::npos);
}

TEST_CASE("Window - SUM with PARTITION BY and ORDER BY", "[phase3][window]") {
    libglot::Arena arena;
    SQLParser parser(arena, "SELECT SUM(amount) OVER (PARTITION BY user_id ORDER BY transaction_date) FROM transactions");

    auto stmt = static_cast<SelectStmt*>(parser.parse_top_level());

    REQUIRE(stmt->columns[0]->type == SQLNodeKind::WINDOW_FUNCTION);
    auto* win_func = static_cast<WindowFunction*>(stmt->columns[0]);
    REQUIRE(win_func->function_name == "SUM");
    REQUIRE(win_func->over->partition_by.size() == 1);
    REQUIRE(win_func->over->order_by.size() == 1);

    SQLGenerator gen(SQLDialect::ANSI);
    std::string sql = gen.generate(stmt);
    REQUIRE(sql.find("SUM") != std::string::npos);
    REQUIRE(sql.find("OVER") != std::string::npos);
}

// ============================================================================
// SUBQUERY TESTS
// ============================================================================

TEST_CASE("Subquery - In FROM clause", "[phase3][subquery]") {
    libglot::Arena arena;
    SQLParser parser(arena, "SELECT * FROM (SELECT id, name FROM users WHERE active = 1) AS active_users");

    auto stmt = static_cast<SelectStmt*>(parser.parse_top_level());

    REQUIRE(stmt->from != nullptr);
    REQUIRE(stmt->from->type == SQLNodeKind::SUBQUERY_EXPR);

    SQLGenerator gen(SQLDialect::ANSI);
    std::string sql = gen.generate(stmt);
    REQUIRE(sql.find("SELECT") != std::string::npos);
}

TEST_CASE("Subquery - JOIN with subquery", "[phase3][subquery]") {
    libglot::Arena arena;
    SQLParser parser(arena,
        "SELECT * FROM users u INNER JOIN (SELECT user_id, COUNT(*) AS order_count FROM orders GROUP BY user_id) o ON u.id = o.user_id");

    auto stmt = static_cast<SelectStmt*>(parser.parse_top_level());

    REQUIRE(stmt->from != nullptr);
    REQUIRE(stmt->from->type == SQLNodeKind::JOIN_CLAUSE);

    SQLGenerator gen(SQLDialect::ANSI);
    std::string sql = gen.generate(stmt);
    REQUIRE(!sql.empty());
    REQUIRE(sql.find("INNER JOIN") != std::string::npos);
}

TEST_CASE("Subquery - Nested subqueries", "[phase3][subquery]") {
    libglot::Arena arena;
    SQLParser parser(arena,
        "SELECT * FROM (SELECT * FROM (SELECT id, name FROM users) AS inner_query WHERE id > 10) AS outer_query");

    auto stmt = static_cast<SelectStmt*>(parser.parse_top_level());

    REQUIRE(stmt->from != nullptr);
    REQUIRE(stmt->from->type == SQLNodeKind::SUBQUERY_EXPR);

    SQLGenerator gen(SQLDialect::ANSI);
    std::string sql = gen.generate(stmt);
    REQUIRE(!sql.empty());
}

// ============================================================================
// COMBINED FEATURES TESTS
// ============================================================================

TEST_CASE("Combined - CTE with window function", "[phase3][combined]") {
    libglot::Arena arena;
    SQLParser parser(arena,
        "WITH ranked_users AS (SELECT *, ROW_NUMBER() OVER (ORDER BY score DESC) AS rank FROM users) "
        "SELECT * FROM ranked_users WHERE rank <= 10");

    auto stmt = static_cast<SelectStmt*>(parser.parse_top_level());

    REQUIRE(stmt->with != nullptr);
    REQUIRE(stmt->with->ctes.size() == 1);

    SQLGenerator gen(SQLDialect::ANSI);
    std::string sql = gen.generate(stmt);
    REQUIRE(sql.find("WITH") != std::string::npos);
    REQUIRE(sql.find("ranked_users") != std::string::npos);
    REQUIRE(sql.find("ROW_NUMBER") != std::string::npos);
}

TEST_CASE("Combined - CTE with subquery in FROM", "[phase3][combined]") {
    libglot::Arena arena;
    SQLParser parser(arena,
        "WITH active_users AS (SELECT * FROM users WHERE active = 1) "
        "SELECT * FROM (SELECT * FROM active_users WHERE premium = 1) AS premium_users");

    auto stmt = static_cast<SelectStmt*>(parser.parse_top_level());

    REQUIRE(stmt->with != nullptr);
    REQUIRE(stmt->from != nullptr);

    SQLGenerator gen(SQLDialect::ANSI);
    std::string sql = gen.generate(stmt);
    REQUIRE(!sql.empty());
}

TEST_CASE("Combined - INSERT with SELECT", "[phase3][combined]") {
    libglot::Arena arena;
    SQLParser parser(arena,
        "INSERT INTO users_backup SELECT * FROM users WHERE active = 1");

    auto stmt = static_cast<InsertStmt*>(parser.parse_top_level());

    REQUIRE(stmt->select_query != nullptr);

    SQLGenerator gen(SQLDialect::ANSI);
    std::string sql = gen.generate(stmt);
    REQUIRE(sql.find("INSERT INTO") != std::string::npos);
}
