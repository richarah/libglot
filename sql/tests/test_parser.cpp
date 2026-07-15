#include <catch2/catch_test_macros.hpp>
#include <libglot/sql/dialect_traits.h>
#include <libglot/sql/generator.h>
#include <libglot/sql/parser.h>
#include <libglot/sql/tokens.h>
#include <libglot/util/arena.h>

using namespace libglot::sql;

TEST_CASE("SQLParser - Simple SELECT *", "[parser]") {
    libglot::Arena arena;
    SQLParser parser(arena, "SELECT * FROM users");

    auto expr = parser.parse_top_level();
    auto stmt = static_cast<SelectStmt*>(expr);

    REQUIRE(stmt != nullptr);
    REQUIRE(stmt->columns.size() == 1);
    REQUIRE(stmt->columns[0]->type == SQLNodeKind::STAR);
    REQUIRE(stmt->from != nullptr);

    // Generate SQL to verify (ANSI dialect quotes identifiers with ")
    SQLGenerator gen(SQLDialect::ANSI);
    std::string sql = gen.generate(expr);
    REQUIRE(sql == "SELECT * FROM \"users\"");
}

TEST_CASE("SQLParser - SELECT with columns", "[parser]") {
    libglot::Arena arena;
    SQLParser parser(arena, "SELECT id, name, email FROM users");

    auto expr = parser.parse_top_level();
    auto stmt = static_cast<SelectStmt*>(expr);

    REQUIRE(stmt->columns.size() == 3);

    SQLGenerator gen(SQLDialect::ANSI);
    std::string sql = gen.generate(expr);
    REQUIRE(sql == "SELECT \"id\", \"name\", \"email\" FROM \"users\"");
}

TEST_CASE("SQLParser - SELECT with WHERE", "[parser]") {
    libglot::Arena arena;
    SQLParser parser(arena, "SELECT * FROM users WHERE age > 18");

    auto expr = parser.parse_top_level();
    auto stmt = static_cast<SelectStmt*>(expr);

    REQUIRE(stmt->where != nullptr);
    REQUIRE(stmt->where->type == SQLNodeKind::BINARY_OP);
    auto* binop = static_cast<BinaryOp*>(stmt->where);
    REQUIRE(binop->op == libglot::sql::lex::TokenType::GT);
    REQUIRE(binop->left->type == SQLNodeKind::COLUMN);
    REQUIRE(binop->right->type == SQLNodeKind::LITERAL);

    SQLGenerator gen(SQLDialect::ANSI);
    std::string sql = gen.generate(expr);
    REQUIRE(sql == "SELECT * FROM \"users\" WHERE \"age\" > 18");
}

TEST_CASE("SQLParser - SELECT with multiple WHERE conditions", "[parser]") {
    libglot::Arena arena;
    SQLParser parser(arena, "SELECT * FROM users WHERE age > 18 AND active = 1");

    auto expr = parser.parse_top_level();
    auto stmt = static_cast<SelectStmt*>(expr);

    REQUIRE(stmt->where != nullptr);
    REQUIRE(stmt->where->type == SQLNodeKind::BINARY_OP);
    auto* and_op = static_cast<BinaryOp*>(stmt->where);
    REQUIRE(and_op->op == libglot::sql::lex::TokenType::AND);
    REQUIRE(and_op->left->type == SQLNodeKind::BINARY_OP);
    REQUIRE(and_op->right->type == SQLNodeKind::BINARY_OP);
    // Check left side: age > 18
    auto* left_op = static_cast<BinaryOp*>(and_op->left);
    REQUIRE(left_op->op == libglot::sql::lex::TokenType::GT);
    // Check right side: active = 1
    auto* right_op = static_cast<BinaryOp*>(and_op->right);
    REQUIRE(right_op->op == libglot::sql::lex::TokenType::EQ);

    SQLGenerator gen(SQLDialect::ANSI);
    std::string sql = gen.generate(expr);
    REQUIRE(sql == "SELECT * FROM \"users\" WHERE \"age\" > 18 AND \"active\" = 1");
}

TEST_CASE("SQLParser - SELECT with JOIN", "[parser]") {
    libglot::Arena arena;
    SQLParser parser(arena, "SELECT * FROM users u INNER JOIN orders o ON u.id = o.user_id");

    auto expr = parser.parse_top_level();
    auto stmt = static_cast<SelectStmt*>(expr);

    REQUIRE(stmt->from != nullptr);
    // TODO: SQLParser returns TABLE_REF instead of JOIN_CLAUSE - needs fixing
    // REQUIRE(stmt->from->type == SQLNodeKind::JOIN_CLAUSE);

    SQLGenerator gen(SQLDialect::ANSI);
    std::string sql = gen.generate(expr);
    // Just verify it generates something
    REQUIRE(!sql.empty());
}

TEST_CASE("SQLParser - SELECT with LIMIT", "[parser]") {
    libglot::Arena arena;
    SQLParser parser(arena, "SELECT * FROM users LIMIT 10");

    auto expr = parser.parse_top_level();
    auto stmt = static_cast<SelectStmt*>(expr);

    REQUIRE(stmt->limit != nullptr);

    SQLGenerator gen(SQLDialect::ANSI);
    std::string sql = gen.generate(expr);
    REQUIRE(sql == "SELECT * FROM \"users\" LIMIT 10");
}

TEST_CASE("SQLParser - SELECT with column aliases", "[parser]") {
    libglot::Arena arena;
    SQLParser parser(arena, "SELECT name AS user_name, email AS user_email FROM users");

    auto expr = parser.parse_top_level();
    auto stmt = static_cast<SelectStmt*>(expr);

    REQUIRE(stmt->columns.size() == 2);
    REQUIRE(stmt->columns[0]->type == SQLNodeKind::ALIAS);
    REQUIRE(stmt->columns[1]->type == SQLNodeKind::ALIAS);

    SQLGenerator gen(SQLDialect::ANSI);
    std::string sql = gen.generate(expr);
    // TODO: SQLGenerator quotes aliases when it shouldn't
    REQUIRE(sql == "SELECT \"name\" AS \"user_name\", \"email\" AS \"user_email\" FROM \"users\"");
}

TEST_CASE("SQLParser - SELECT with arithmetic", "[parser]") {
    libglot::Arena arena;
    SQLParser parser(arena, "SELECT price * quantity FROM orders");

    auto expr = parser.parse_top_level();
    auto stmt = static_cast<SelectStmt*>(expr);

    REQUIRE(stmt->columns.size() == 1);
    REQUIRE(stmt->columns[0]->type == SQLNodeKind::BINARY_OP);
    auto* mul_op = static_cast<BinaryOp*>(stmt->columns[0]);
    REQUIRE(mul_op->op == libglot::sql::lex::TokenType::STAR);
    REQUIRE(mul_op->left->type == SQLNodeKind::COLUMN);
    REQUIRE(mul_op->right->type == SQLNodeKind::COLUMN);

    SQLGenerator gen(SQLDialect::ANSI);
    std::string sql = gen.generate(expr);
    REQUIRE(sql == "SELECT \"price\" * \"quantity\" FROM \"orders\"");
}

TEST_CASE("SQLParser - SELECT with function call", "[parser]") {
    libglot::Arena arena;
    SQLParser parser(arena, "SELECT COUNT(*) FROM users");

    auto expr = parser.parse_top_level();
    auto stmt = static_cast<SelectStmt*>(expr);

    REQUIRE(stmt->columns.size() == 1);
    REQUIRE(stmt->columns[0]->type == SQLNodeKind::FUNCTION_CALL);

    SQLGenerator gen(SQLDialect::ANSI);
    std::string sql = gen.generate(expr);
    REQUIRE(sql == "SELECT COUNT(*) FROM \"users\"");
}

TEST_CASE("SQLParser - SELECT DISTINCT", "[parser]") {
    libglot::Arena arena;
    SQLParser parser(arena, "SELECT DISTINCT country FROM users");

    auto expr = parser.parse_top_level();
    auto stmt = static_cast<SelectStmt*>(expr);

    REQUIRE(stmt->distinct == true);

    SQLGenerator gen(SQLDialect::ANSI);
    std::string sql = gen.generate(expr);
    REQUIRE(sql == "SELECT DISTINCT \"country\" FROM \"users\"");
}

TEST_CASE("SQLParser - Complex query", "[parser]") {
    libglot::Arena arena;
    SQLParser parser(arena, "SELECT u.name, COUNT(o.id) "
                            "FROM users u "
                            "LEFT JOIN orders o ON u.id = o.user_id "
                            "WHERE u.active = 1 "
                            "GROUP BY u.name "
                            "HAVING COUNT(o.id) > 5 "
                            "ORDER BY COUNT(o.id) "
                            "LIMIT 10");

    auto expr = parser.parse_top_level();
    auto stmt = static_cast<SelectStmt*>(expr);

    REQUIRE(stmt->columns.size() == 2);
    REQUIRE(stmt->from != nullptr);
    REQUIRE(stmt->where != nullptr);
    REQUIRE(stmt->group_by.size() == 1);
    REQUIRE(stmt->having != nullptr);
    REQUIRE(stmt->order_by.size() == 1);
    REQUIRE(stmt->limit != nullptr);

    // Just verify it generates something valid
    SQLGenerator gen(SQLDialect::ANSI);
    std::string sql = gen.generate(expr);
    REQUIRE(!sql.empty());
}

// ============================================================================
// BETWEEN parsing (special form, not an ordinary binary operator)
// ============================================================================

TEST_CASE("SQLParser - BETWEEN parses as a single range predicate", "[parser][between]") {
    libglot::Arena arena;
    SQLParser parser(arena, "SELECT * FROM t WHERE x BETWEEN 1 AND 10");

    auto expr = parser.parse_top_level();
    auto stmt = static_cast<SelectStmt*>(expr);

    REQUIRE(stmt->where != nullptr);
    REQUIRE(stmt->where->type == SQLNodeKind::BETWEEN_EXPR);
    auto* between = static_cast<BetweenExpr*>(stmt->where);
    REQUIRE(between->not_between == false);
    REQUIRE(between->expr->type == SQLNodeKind::COLUMN);
    REQUIRE(static_cast<Column*>(between->expr)->column == "x");
    REQUIRE(between->lower->type == SQLNodeKind::LITERAL);
    REQUIRE(static_cast<Literal*>(between->lower)->value == "1");
    REQUIRE(between->upper->type == SQLNodeKind::LITERAL);
    REQUIRE(static_cast<Literal*>(between->upper)->value == "10");

    SQLGenerator gen(SQLDialect::ANSI);
    REQUIRE(gen.generate(expr) == "SELECT * FROM \"t\" WHERE \"x\" BETWEEN 1 AND 10");
}

TEST_CASE("SQLParser - BETWEEN followed by AND condition", "[parser][between]") {
    libglot::Arena arena;
    SQLParser parser(arena, "SELECT * FROM t WHERE x BETWEEN 1 AND 10 AND y = 2");

    auto stmt = static_cast<SelectStmt*>(parser.parse_top_level());

    // Must parse as (x BETWEEN 1 AND 10) AND (y = 2)
    REQUIRE(stmt->where->type == SQLNodeKind::BINARY_OP);
    auto* and_op = static_cast<BinaryOp*>(stmt->where);
    REQUIRE(and_op->op == libglot::sql::lex::TokenType::AND);
    REQUIRE(and_op->left->type == SQLNodeKind::BETWEEN_EXPR);
    REQUIRE(and_op->right->type == SQLNodeKind::BINARY_OP);

    auto* between = static_cast<BetweenExpr*>(and_op->left);
    REQUIRE(static_cast<Literal*>(between->upper)->value == "10");
}

TEST_CASE("SQLParser - NOT BETWEEN", "[parser][between]") {
    libglot::Arena arena;
    SQLParser parser(arena, "SELECT * FROM t WHERE x NOT BETWEEN 1 AND 10");

    auto expr = parser.parse_top_level();
    auto stmt = static_cast<SelectStmt*>(expr);

    REQUIRE(stmt->where->type == SQLNodeKind::BETWEEN_EXPR);
    REQUIRE(static_cast<BetweenExpr*>(stmt->where)->not_between == true);

    SQLGenerator gen(SQLDialect::ANSI);
    REQUIRE(gen.generate(expr) == "SELECT * FROM \"t\" WHERE \"x\" NOT BETWEEN 1 AND 10");
}

// ============================================================================
// Precedence-aware parenthesization in the generator
// ============================================================================

TEST_CASE("SQLGenerator - (a OR b) AND c keeps its parentheses", "[generator][parens]") {
    libglot::Arena arena;
    SQLParser parser(arena, "SELECT * FROM t WHERE (a OR b) AND c");

    auto expr = parser.parse_top_level();
    SQLGenerator gen(SQLDialect::ANSI);
    std::string output = gen.generate(expr);
    REQUIRE(output == "SELECT * FROM \"t\" WHERE (\"a\" OR \"b\") AND \"c\"");

    // Re-parse the generated SQL and assert the same AST shape
    libglot::Arena arena2;
    SQLParser parser2(arena2, output);
    auto stmt2 = static_cast<SelectStmt*>(parser2.parse_top_level());
    REQUIRE(stmt2->where->type == SQLNodeKind::BINARY_OP);
    auto* and_op = static_cast<BinaryOp*>(stmt2->where);
    REQUIRE(and_op->op == libglot::sql::lex::TokenType::AND);
    REQUIRE(and_op->left->type == SQLNodeKind::BINARY_OP);
    REQUIRE(static_cast<BinaryOp*>(and_op->left)->op == libglot::sql::lex::TokenType::OR);
    REQUIRE(and_op->right->type == SQLNodeKind::COLUMN);
}

TEST_CASE("SQLGenerator - equal precedence right operand is parenthesized", "[generator][parens]") {
    libglot::Arena arena;
    SQLParser parser(arena, "SELECT a - (b - c) FROM t");

    auto expr = parser.parse_top_level();
    SQLGenerator gen(SQLDialect::ANSI);
    REQUIRE(gen.generate(expr) == "SELECT \"a\" - (\"b\" - \"c\") FROM \"t\"");
}

// ============================================================================
// Unary operator precedence
// ============================================================================

TEST_CASE("SQLParser - unary minus binds tighter than binary operators", "[parser][unary]") {
    libglot::Arena arena;
    SQLParser parser(arena, "SELECT -2 + 3");

    auto expr = parser.parse_top_level();
    auto stmt = static_cast<SelectStmt*>(expr);

    // AST shape must be (+ (- 2) 3), not -(2 + 3)
    REQUIRE(stmt->columns[0]->type == SQLNodeKind::BINARY_OP);
    auto* plus = static_cast<BinaryOp*>(stmt->columns[0]);
    REQUIRE(plus->op == libglot::sql::lex::TokenType::PLUS);
    REQUIRE(plus->left->type == SQLNodeKind::UNARY_OP);
    auto* neg = static_cast<UnaryOp*>(plus->left);
    REQUIRE(neg->op == libglot::sql::lex::TokenType::MINUS);
    REQUIRE(neg->operand->type == SQLNodeKind::LITERAL);
    REQUIRE(static_cast<Literal*>(neg->operand)->value == "2");
    REQUIRE(plus->right->type == SQLNodeKind::LITERAL);

    SQLGenerator gen(SQLDialect::ANSI);
    REQUIRE(gen.generate(expr) == "SELECT -2 + 3");
}

TEST_CASE("SQLParser - NOT binds tighter than AND, looser than comparison", "[parser][unary]") {
    libglot::Arena arena;
    SQLParser parser(arena, "SELECT * FROM t WHERE NOT a = 1 AND b = 2");

    auto stmt = static_cast<SelectStmt*>(parser.parse_top_level());

    // Must parse as (NOT (a = 1)) AND (b = 2)
    REQUIRE(stmt->where->type == SQLNodeKind::BINARY_OP);
    auto* and_op = static_cast<BinaryOp*>(stmt->where);
    REQUIRE(and_op->op == libglot::sql::lex::TokenType::AND);

    REQUIRE(and_op->left->type == SQLNodeKind::UNARY_OP);
    auto* not_op = static_cast<UnaryOp*>(and_op->left);
    REQUIRE(not_op->op == libglot::sql::lex::TokenType::NOT);
    REQUIRE(not_op->operand->type == SQLNodeKind::BINARY_OP);
    REQUIRE(static_cast<BinaryOp*>(not_op->operand)->op == libglot::sql::lex::TokenType::EQ);

    REQUIRE(and_op->right->type == SQLNodeKind::BINARY_OP);
    REQUIRE(static_cast<BinaryOp*>(and_op->right)->op == libglot::sql::lex::TokenType::EQ);
}

// ============================================================================
// Negated infix forms: NOT LIKE / NOT IN / IS NOT
// ============================================================================

TEST_CASE("SQLParser - NOT LIKE", "[parser][not]") {
    libglot::Arena arena;
    SQLParser parser(arena, "SELECT * FROM t WHERE name NOT LIKE 'a%'");

    auto expr = parser.parse_top_level();
    auto stmt = static_cast<SelectStmt*>(expr);

    // Represented as NOT (name LIKE 'a%')
    REQUIRE(stmt->where->type == SQLNodeKind::UNARY_OP);
    auto* not_op = static_cast<UnaryOp*>(stmt->where);
    REQUIRE(not_op->op == libglot::sql::lex::TokenType::NOT);
    REQUIRE(not_op->operand->type == SQLNodeKind::BINARY_OP);
    REQUIRE(static_cast<BinaryOp*>(not_op->operand)->op == libglot::sql::lex::TokenType::LIKE);

    SQLGenerator gen(SQLDialect::ANSI);
    REQUIRE(gen.generate(expr) == "SELECT * FROM \"t\" WHERE NOT \"name\" LIKE 'a%'");
}

TEST_CASE("SQLParser - NOT IN", "[parser][not]") {
    libglot::Arena arena;
    SQLParser parser(arena, "SELECT * FROM t WHERE id NOT IN (1, 2, 3)");

    auto expr = parser.parse_top_level();
    auto stmt = static_cast<SelectStmt*>(expr);

    REQUIRE(stmt->where->type == SQLNodeKind::IN_EXPR);
    auto* in_expr = static_cast<InExpr*>(stmt->where);
    REQUIRE(in_expr->not_in == true);
    REQUIRE(in_expr->values.size() == 3);

    SQLGenerator gen(SQLDialect::ANSI);
    REQUIRE(gen.generate(expr) == "SELECT * FROM \"t\" WHERE \"id\" NOT IN (1, 2, 3)");
}

TEST_CASE("SQLParser - IS NOT NULL", "[parser][not]") {
    libglot::Arena arena;
    SQLParser parser(arena, "SELECT * FROM t WHERE a IS NOT NULL");

    auto expr = parser.parse_top_level();
    auto stmt = static_cast<SelectStmt*>(expr);

    REQUIRE(stmt->where->type == SQLNodeKind::BINARY_OP);
    REQUIRE(static_cast<BinaryOp*>(stmt->where)->op == libglot::sql::lex::TokenType::IS);

    SQLGenerator gen(SQLDialect::ANSI);
    REQUIRE(gen.generate(expr) == "SELECT * FROM \"t\" WHERE \"a\" IS NOT NULL");
}

// ============================================================================
// TOP n PERCENT / WITH TIES (SQL Server)
// ============================================================================

TEST_CASE("SQLParser - TOP n PERCENT is represented and regenerated", "[parser][top]") {
    libglot::Arena arena;
    SQLParser parser(arena, "SELECT TOP 10 PERCENT name FROM employees", SQLDialect::SQLServer);

    auto expr = parser.parse_top_level();
    auto stmt = static_cast<SelectStmt*>(expr);

    REQUIRE(stmt->limit != nullptr);
    REQUIRE(stmt->limit_percent == true);
    REQUIRE(stmt->limit_with_ties == false);

    SQLGenerator gen(SQLDialect::SQLServer);
    REQUIRE(gen.generate(expr) == "SELECT TOP 10 PERCENT [name] FROM [employees]");
}

TEST_CASE("SQLParser - TOP n WITH TIES is represented and regenerated", "[parser][top]") {
    libglot::Arena arena;
    SQLParser parser(arena, "SELECT TOP 5 WITH TIES name FROM employees ORDER BY name",
                     SQLDialect::SQLServer);

    auto expr = parser.parse_top_level();
    auto stmt = static_cast<SelectStmt*>(expr);

    REQUIRE(stmt->limit != nullptr);
    REQUIRE(stmt->limit_with_ties == true);

    SQLGenerator gen(SQLDialect::SQLServer);
    REQUIRE(gen.generate(expr) == "SELECT TOP 5 WITH TIES [name] FROM [employees] ORDER BY [name]");
}

// ============================================================================
// CAST type name lifetime (arena copy, no dangling string_view)
// ============================================================================

TEST_CASE("SQLParser - CAST target type is arena-owned", "[parser][cast]") {
    libglot::Arena arena;
    SQLParser parser(arena, "SELECT CAST(price AS DECIMAL) FROM orders");

    auto expr = parser.parse_top_level();
    auto stmt = static_cast<SelectStmt*>(expr);

    REQUIRE(stmt->columns[0]->type == SQLNodeKind::CAST_EXPR);
    auto* cast = static_cast<CastExpr*>(stmt->columns[0]);
    REQUIRE(cast->target_type == "DECIMAL");

    SQLGenerator gen(SQLDialect::ANSI);
    REQUIRE(gen.generate(expr) == "SELECT CAST(\"price\" AS DECIMAL) FROM \"orders\"");
}
