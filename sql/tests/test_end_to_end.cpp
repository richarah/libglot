#include <catch2/catch_test_macros.hpp>
#include <libglot/util/arena.h>
#include <libglot/sql/ast_nodes.h>
#include <libglot/sql/generator.h>
#include <libglot/sql/tokens.h>

using namespace libglot::sql;

TEST_CASE("End-to-end - Simple SELECT *", "[e2e]") {
    libglot::Arena arena;

    // Build AST: SELECT * FROM users
    auto stmt = arena.create<SelectStmt>();
    stmt->columns.push_back(arena.create<Star>());
    stmt->from = arena.create<TableRef>("users");

    // Generate SQL
    SQLGenerator gen(SQLDialect::ANSI);
    std::string sql = gen.generate(stmt);

    REQUIRE(sql == "SELECT * FROM \"users\"");
}

TEST_CASE("End-to-end - SELECT with columns", "[e2e]") {
    libglot::Arena arena;

    // Build AST: SELECT id, name FROM users
    auto stmt = arena.create<SelectStmt>();
    stmt->columns.push_back(arena.create<Column>("id"));
    stmt->columns.push_back(arena.create<Column>("name"));
    stmt->from = arena.create<TableRef>("users");

    SQLGenerator gen(SQLDialect::ANSI);
    std::string sql = gen.generate(stmt);

    REQUIRE(sql == "SELECT \"id\", \"name\" FROM \"users\"");
}

TEST_CASE("End-to-end - SELECT with WHERE", "[e2e]") {
    libglot::Arena arena;

    // Build AST: SELECT * FROM users WHERE age > 18
    auto stmt = arena.create<SelectStmt>();
    stmt->columns.push_back(arena.create<Star>());
    stmt->from = arena.create<TableRef>("users");

    auto age_col = arena.create<Column>("age");
    auto eighteen = arena.create<Literal>("18");
    stmt->where = arena.create<BinaryOp>(libsqlglot::TokenType::GT, age_col, eighteen);

    SQLGenerator gen(SQLDialect::ANSI);
    std::string sql = gen.generate(stmt);

    REQUIRE(sql == "SELECT * FROM \"users\" WHERE \"age\" > 18");
}

TEST_CASE("End-to-end - SELECT with table alias", "[e2e]") {
    libglot::Arena arena;

    // Build AST: SELECT u.name FROM users AS u
    auto stmt = arena.create<SelectStmt>();
    stmt->columns.push_back(arena.create<Column>("u", "name"));
    auto table_ref = arena.create<TableRef>("users");
    table_ref->alias = "u";
    stmt->from = table_ref;

    SQLGenerator gen(SQLDialect::ANSI);
    std::string sql = gen.generate(stmt);

    REQUIRE(sql == "SELECT \"u\".\"name\" FROM \"users\" AS \"u\"");
}

TEST_CASE("End-to-end - SELECT with JOIN", "[e2e]") {
    libglot::Arena arena;

    // Build AST: SELECT * FROM users u JOIN orders o ON u.id = o.user_id
    auto stmt = arena.create<SelectStmt>();
    stmt->columns.push_back(arena.create<Star>());

    auto users = arena.create<TableRef>("users");
    users->alias = "u";
    auto orders = arena.create<TableRef>("orders");
    orders->alias = "o";

    auto u_id = arena.create<Column>("u", "id");
    auto o_user_id = arena.create<Column>("o", "user_id");
    auto join_condition = arena.create<BinaryOp>(libsqlglot::TokenType::EQ, u_id, o_user_id);

    stmt->from = arena.create<JoinClause>(JoinType::INNER, users, orders, join_condition);

    SQLGenerator gen(SQLDialect::ANSI);
    std::string sql = gen.generate(stmt);

    REQUIRE(sql == "SELECT * FROM \"users\" AS \"u\" INNER JOIN \"orders\" AS \"o\" ON \"u\".\"id\" = \"o\".\"user_id\"");
}

TEST_CASE("End-to-end - SELECT with multiple conditions", "[e2e]") {
    libglot::Arena arena;

    // Build AST: SELECT * FROM users WHERE age > 18 AND active = 'true'
    auto stmt = arena.create<SelectStmt>();
    stmt->columns.push_back(arena.create<Star>());
    stmt->from = arena.create<TableRef>("users");

    auto age_col = arena.create<Column>("age");
    auto eighteen = arena.create<Literal>("18");
    auto age_condition = arena.create<BinaryOp>(libsqlglot::TokenType::GT, age_col, eighteen);

    auto active_col = arena.create<Column>("active");
    auto true_val = arena.create<Literal>("'true'");  // String literal, not boolean
    auto active_condition = arena.create<BinaryOp>(libsqlglot::TokenType::EQ, active_col, true_val);

    stmt->where = arena.create<BinaryOp>(libsqlglot::TokenType::AND, age_condition, active_condition);

    SQLGenerator gen(SQLDialect::ANSI);
    std::string sql = gen.generate(stmt);

    REQUIRE(sql == "SELECT * FROM \"users\" WHERE \"age\" > 18 AND \"active\" = 'true'");
}

TEST_CASE("End-to-end - SELECT with column alias", "[e2e]") {
    libglot::Arena arena;

    // Build AST: SELECT name AS user_name FROM users
    auto stmt = arena.create<SelectStmt>();

    auto name_col = arena.create<Column>("name");
    auto aliased = arena.create<Alias>(name_col, "user_name");
    stmt->columns.push_back(aliased);
    stmt->from = arena.create<TableRef>("users");

    SQLGenerator gen(SQLDialect::ANSI);
    std::string sql = gen.generate(stmt);

    REQUIRE(sql == "SELECT \"name\" AS \"user_name\" FROM \"users\"");
}

TEST_CASE("End-to-end - SELECT with LIMIT", "[e2e]") {
    libglot::Arena arena;

    // Build AST: SELECT * FROM users LIMIT 10
    auto stmt = arena.create<SelectStmt>();
    stmt->columns.push_back(arena.create<Star>());
    stmt->from = arena.create<TableRef>("users");
    stmt->limit = arena.create<Literal>("10");

    SQLGenerator gen(SQLDialect::ANSI);
    std::string sql = gen.generate(stmt);

    REQUIRE(sql == "SELECT * FROM \"users\" LIMIT 10");
}
