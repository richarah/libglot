#include <catch2/catch_test_macros.hpp>
#include <libglot/sql/parser.h>
#include <libglot/sql/generator.h>
#include <libglot/util/arena.h>

using namespace libglot::sql;

// ============================================================================
// CALL STATEMENT TESTS
// ============================================================================

TEST_CASE("CALL - Simple procedure call with no arguments", "[call][basic]") {
    libglot::Arena arena;
    SQLParser parser(arena, "CALL update_statistics()");

    auto expr = parser.parse();
    REQUIRE(expr != nullptr);
    REQUIRE(expr->type == SQLNodeKind::CALL_PROCEDURE_STMT);

    auto* stmt = static_cast<CallProcedureStmt*>(expr);
    REQUIRE(stmt->name == "update_statistics");
    REQUIRE(stmt->arguments.empty());

    // Test generator
    SQLGenerator gen(SQLDialect::PostgreSQL);
    std::string sql = gen.generate(expr);
    REQUIRE(sql == "CALL update_statistics()");

    // Round-trip test
    SQLParser parser2(arena, sql);
    auto expr2 = parser2.parse();
    REQUIRE(expr2 != nullptr);
    REQUIRE(expr2->type == SQLNodeKind::CALL_PROCEDURE_STMT);
}

TEST_CASE("CALL - Procedure call with simple arguments", "[call][args]") {
    libglot::Arena arena;
    SQLParser parser(arena, "CALL insert_user('john_doe', 'john@example.com', 25)");

    auto expr = parser.parse();
    REQUIRE(expr != nullptr);
    REQUIRE(expr->type == SQLNodeKind::CALL_PROCEDURE_STMT);

    auto* stmt = static_cast<CallProcedureStmt*>(expr);
    REQUIRE(stmt->name == "insert_user");
    REQUIRE(stmt->arguments.size() == 3);

    // Verify arguments are parsed correctly
    REQUIRE(stmt->arguments[0]->type == SQLNodeKind::LITERAL);
    REQUIRE(stmt->arguments[1]->type == SQLNodeKind::LITERAL);
    REQUIRE(stmt->arguments[2]->type == SQLNodeKind::LITERAL);

    // Test generator
    SQLGenerator gen(SQLDialect::PostgreSQL);
    std::string sql = gen.generate(expr);
    REQUIRE(sql.find("CALL insert_user") != std::string::npos);
    REQUIRE(sql.find("john_doe") != std::string::npos);
    REQUIRE(sql.find("john@example.com") != std::string::npos);
    REQUIRE(sql.find("25") != std::string::npos);

    // Round-trip test
    SQLParser parser2(arena, sql);
    auto expr2 = parser2.parse();
    REQUIRE(expr2 != nullptr);
    REQUIRE(expr2->type == SQLNodeKind::CALL_PROCEDURE_STMT);
    auto* stmt2 = static_cast<CallProcedureStmt*>(expr2);
    REQUIRE(stmt2->arguments.size() == 3);
}

TEST_CASE("CALL - Procedure call with expression arguments", "[call][expressions]") {
    libglot::Arena arena;
    SQLParser parser(arena, "CALL calculate_total(price * quantity, discount + tax, NOW())");

    auto expr = parser.parse();
    REQUIRE(expr != nullptr);
    REQUIRE(expr->type == SQLNodeKind::CALL_PROCEDURE_STMT);

    auto* stmt = static_cast<CallProcedureStmt*>(expr);
    REQUIRE(stmt->name == "calculate_total");
    REQUIRE(stmt->arguments.size() == 3);

    // First argument: price * quantity (binary operation)
    REQUIRE(stmt->arguments[0]->type == SQLNodeKind::BINARY_OP);

    // Second argument: discount + tax (binary operation)
    REQUIRE(stmt->arguments[1]->type == SQLNodeKind::BINARY_OP);

    // Third argument: NOW() (function call)
    REQUIRE(stmt->arguments[2]->type == SQLNodeKind::FUNCTION_CALL);

    // Test generator
    SQLGenerator gen(SQLDialect::PostgreSQL);
    std::string sql = gen.generate(expr);
    REQUIRE(sql.find("CALL calculate_total") != std::string::npos);
    REQUIRE(sql.find("*") != std::string::npos);
    REQUIRE(sql.find("+") != std::string::npos);

    // Round-trip test
    SQLParser parser2(arena, sql);
    auto expr2 = parser2.parse();
    REQUIRE(expr2 != nullptr);
    REQUIRE(expr2->type == SQLNodeKind::CALL_PROCEDURE_STMT);
}

TEST_CASE("CALL - Procedure call with NULL argument", "[call][null]") {
    libglot::Arena arena;
    SQLParser parser(arena, "CALL process_order(12345, NULL)");

    auto expr = parser.parse();
    REQUIRE(expr != nullptr);
    REQUIRE(expr->type == SQLNodeKind::CALL_PROCEDURE_STMT);

    auto* stmt = static_cast<CallProcedureStmt*>(expr);
    REQUIRE(stmt->name == "process_order");
    REQUIRE(stmt->arguments.size() == 2);

    // Second argument should be NULL
    REQUIRE(stmt->arguments[1]->type == SQLNodeKind::LITERAL);
    auto* null_arg = static_cast<Literal*>(stmt->arguments[1]);
    REQUIRE(null_arg->value == "NULL");

    // Test generator
    SQLGenerator gen(SQLDialect::PostgreSQL);
    std::string sql = gen.generate(expr);
    REQUIRE(sql.find("CALL process_order") != std::string::npos);
    REQUIRE(sql.find("NULL") != std::string::npos);
}

TEST_CASE("CALL - Procedure call with column reference", "[call][column]") {
    libglot::Arena arena;
    SQLParser parser(arena, "CALL update_price(product_id, new_amount)");

    auto expr = parser.parse();
    REQUIRE(expr != nullptr);
    REQUIRE(expr->type == SQLNodeKind::CALL_PROCEDURE_STMT);

    auto* stmt = static_cast<CallProcedureStmt*>(expr);
    REQUIRE(stmt->name == "update_price");
    REQUIRE(stmt->arguments.size() == 2);

    // Arguments should be columns
    REQUIRE(stmt->arguments[0]->type == SQLNodeKind::COLUMN);
    REQUIRE(stmt->arguments[1]->type == SQLNodeKind::COLUMN);

    auto* col1 = static_cast<Column*>(stmt->arguments[0]);
    auto* col2 = static_cast<Column*>(stmt->arguments[1]);
    REQUIRE(col1->column == "product_id");
    REQUIRE(col2->column == "new_amount");

    // Test generator
    SQLGenerator gen(SQLDialect::PostgreSQL);
    std::string sql = gen.generate(expr);
    REQUIRE(sql == "CALL update_price(\"product_id\", \"new_amount\")");
}

TEST_CASE("CALL - Procedure call with subquery argument", "[call][subquery]") {
    libglot::Arena arena;
    SQLParser parser(arena, "CALL process_batch((SELECT id FROM pending_orders WHERE status = 'new'))");

    auto expr = parser.parse();
    REQUIRE(expr != nullptr);
    REQUIRE(expr->type == SQLNodeKind::CALL_PROCEDURE_STMT);

    auto* stmt = static_cast<CallProcedureStmt*>(expr);
    REQUIRE(stmt->name == "process_batch");
    REQUIRE(stmt->arguments.size() == 1);

    // Argument should be a subquery
    REQUIRE(stmt->arguments[0]->type == SQLNodeKind::SUBQUERY_EXPR);

    // Test generator
    SQLGenerator gen(SQLDialect::PostgreSQL);
    std::string sql = gen.generate(expr);
    REQUIRE(sql.find("CALL process_batch") != std::string::npos);
    REQUIRE(sql.find("SELECT") != std::string::npos);
    REQUIRE(sql.find("pending_orders") != std::string::npos);
}

TEST_CASE("CALL - Procedure call with function call argument", "[call][function]") {
    libglot::Arena arena;
    SQLParser parser(arena, "CALL log_event(NOW(), CONCAT('User ', user_name, ' logged in'))");

    auto expr = parser.parse();
    REQUIRE(expr != nullptr);
    REQUIRE(expr->type == SQLNodeKind::CALL_PROCEDURE_STMT);

    auto* stmt = static_cast<CallProcedureStmt*>(expr);
    REQUIRE(stmt->name == "log_event");
    REQUIRE(stmt->arguments.size() == 2);

    // First argument: NOW() (function)
    REQUIRE(stmt->arguments[0]->type == SQLNodeKind::FUNCTION_CALL);

    // Second argument: CONCAT function
    REQUIRE(stmt->arguments[1]->type == SQLNodeKind::FUNCTION_CALL);
    auto* concat_func = static_cast<FunctionCall*>(stmt->arguments[1]);
    REQUIRE(concat_func->name == "CONCAT");
    REQUIRE(concat_func->args.size() == 3);

    // Test generator
    SQLGenerator gen(SQLDialect::PostgreSQL);
    std::string sql = gen.generate(expr);
    REQUIRE(sql.find("CALL log_event") != std::string::npos);
    REQUIRE(sql.find("NOW") != std::string::npos);
    REQUIRE(sql.find("CONCAT") != std::string::npos);
}

// ============================================================================
// RETURN STATEMENT TESTS
// ============================================================================

TEST_CASE("RETURN - Simple return with value", "[return][basic]") {
    libglot::Arena arena;
    SQLParser parser(arena, "RETURN 42");

    auto expr = parser.parse();
    REQUIRE(expr != nullptr);
    REQUIRE(expr->type == SQLNodeKind::RETURN_STMT);

    auto* stmt = static_cast<ReturnStmt*>(expr);
    REQUIRE(stmt->return_value != nullptr);
    REQUIRE(stmt->return_value->type == SQLNodeKind::LITERAL);

    // Test generator
    SQLGenerator gen(SQLDialect::PostgreSQL);
    std::string sql = gen.generate(expr);
    REQUIRE(sql == "RETURN 42");

    // Round-trip test
    SQLParser parser2(arena, sql);
    auto expr2 = parser2.parse();
    REQUIRE(expr2 != nullptr);
    REQUIRE(expr2->type == SQLNodeKind::RETURN_STMT);
}

TEST_CASE("RETURN - Return NULL", "[return][null]") {
    libglot::Arena arena;
    SQLParser parser(arena, "RETURN NULL");

    auto expr = parser.parse();
    REQUIRE(expr != nullptr);
    REQUIRE(expr->type == SQLNodeKind::RETURN_STMT);

    auto* stmt = static_cast<ReturnStmt*>(expr);
    REQUIRE(stmt->return_value != nullptr);
    REQUIRE(stmt->return_value->type == SQLNodeKind::LITERAL);

    // Test generator
    SQLGenerator gen(SQLDialect::PostgreSQL);
    std::string sql = gen.generate(expr);
    REQUIRE(sql == "RETURN NULL");
}
