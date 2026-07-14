// Procedural SQL across dialects: CREATE PROCEDURE/FUNCTION, IF, WHILE, FOR,
// DECLARE (variables and cursors), cursor operations, RAISE vs SIGNAL
// transpilation, and the FOR -> WHILE lowering for SQL Server.
//
// KNOWN BUGS (reported, not asserted):
//   - RAISE EXCEPTION 'fmt %', arg drops the format arguments on output.
//   - RAISE targeted at SQL Server stays "RAISE EXCEPTION ..." instead of
//     RAISERROR/THROW.
//   - WHILE loops are always generated in MySQL form (WHILE..DO..END WHILE),
//     even for PostgreSQL (which wants LOOP..END LOOP) and SQL Server
//     (which wants BEGIN..END); only the MySQL output is exact-asserted.

#include <catch2/catch_test_macros.hpp>
#include <libglot/sql/parser.h>
#include <libglot/sql/generator.h>
#include <libglot/util/arena.h>

#include <string>

using namespace libglot::sql;

namespace {

std::string transpile(const std::string& sql, SQLDialect target) {
    libglot::Arena arena;
    SQLParser parser(arena, sql);
    auto ast = parser.parse_top_level();
    SQLGenerator gen(target);
    return gen.generate(ast);
}

SQLNode* parse(libglot::Arena& arena, const std::string& sql) {
    SQLParser parser(arena, sql);
    return parser.parse_top_level();
}

} // namespace

// ============================================================================
// CREATE PROCEDURE / FUNCTION
// ============================================================================

TEST_CASE("Procedure dialects - basic CREATE PROCEDURE is stable across dialects", "[procedure][create]") {
    const std::string sql = "CREATE PROCEDURE myproc() BEGIN SELECT 1; END";

    REQUIRE(transpile(sql, SQLDialect::PostgreSQL) == "CREATE PROCEDURE myproc() BEGIN SELECT 1 END");
    REQUIRE(transpile(sql, SQLDialect::MySQL) == "CREATE PROCEDURE myproc() BEGIN SELECT 1 END");
    REQUIRE(transpile(sql, SQLDialect::SQLServer) == "CREATE PROCEDURE myproc() BEGIN SELECT 1 END");
    REQUIRE(transpile(sql, SQLDialect::Oracle) == "CREATE PROCEDURE myproc() BEGIN SELECT 1 END");
}

TEST_CASE("Procedure dialects - CREATE PROCEDURE with typed parameters", "[procedure][create]") {
    libglot::Arena arena;
    auto* ast = parse(arena,
        "CREATE PROCEDURE add_user(name VARCHAR(50), age INT) BEGIN SELECT 1; END");

    REQUIRE(ast->type == SQLNodeKind::CREATE_PROCEDURE_STMT);
    auto* stmt = static_cast<CreateProcedureStmt*>(ast);
    REQUIRE(stmt->is_function == false);
    REQUIRE(stmt->name == "add_user");
    REQUIRE(stmt->parameters.size() == 2);
    REQUIRE(stmt->body.size() == 1);

    SQLGenerator gen(SQLDialect::MySQL);
    REQUIRE(gen.generate(ast)
            == "CREATE PROCEDURE add_user(name VARCHAR(50), age INT) BEGIN SELECT 1 END");
}

TEST_CASE("Procedure dialects - CREATE FUNCTION with RETURNS", "[procedure][create]") {
    const std::string sql = "CREATE FUNCTION get_count() RETURNS INT BEGIN RETURN 42; END";

    REQUIRE(transpile(sql, SQLDialect::PostgreSQL)
            == "CREATE FUNCTION get_count() RETURNS INT BEGIN RETURN 42 END");
    REQUIRE(transpile(sql, SQLDialect::SQLServer)
            == "CREATE FUNCTION get_count() RETURNS INT BEGIN RETURN 42 END");

    libglot::Arena arena;
    auto* ast = parse(arena, sql);
    auto* stmt = static_cast<CreateProcedureStmt*>(ast);
    REQUIRE(stmt->is_function == true);
    REQUIRE(stmt->return_type == "INT");
}

// ============================================================================
// IF / ELSE
// ============================================================================

TEST_CASE("Procedure dialects - IF THEN END IF per dialect quoting", "[procedure][if]") {
    const std::string sql = "IF x > 1 THEN SELECT 1; END IF";

    REQUIRE(transpile(sql, SQLDialect::PostgreSQL) == "IF \"x\" > 1 THEN SELECT 1 END IF");
    REQUIRE(transpile(sql, SQLDialect::MySQL) == "IF `x` > 1 THEN SELECT 1 END IF");
    REQUIRE(transpile(sql, SQLDialect::SQLServer) == "IF [x] > 1 THEN SELECT 1 END IF");
}

TEST_CASE("Procedure dialects - IF with ELSE branch", "[procedure][if]") {
    REQUIRE(transpile("IF x > 1 THEN SELECT 1; ELSE SELECT 2; END IF", SQLDialect::MySQL)
            == "IF `x` > 1 THEN SELECT 1 ELSE SELECT 2 END IF");

    libglot::Arena arena;
    auto* ast = parse(arena, "IF x > 1 THEN SELECT 1; ELSE SELECT 2; END IF");
    REQUIRE(ast->type == SQLNodeKind::IF_STMT);
    auto* stmt = static_cast<IfStmt*>(ast);
    REQUIRE(stmt->condition->type == SQLNodeKind::BINARY_OP);
    REQUIRE(stmt->then_stmts.size() == 1);
    REQUIRE(stmt->else_stmts.size() == 1);
}

// ============================================================================
// WHILE
// ============================================================================

TEST_CASE("Procedure dialects - WHILE loop AST and MySQL output", "[procedure][while]") {
    const std::string sql = "WHILE x < 10 LOOP SET x = x + 1; END LOOP";

    libglot::Arena arena;
    auto* ast = parse(arena, sql);
    REQUIRE(ast->type == SQLNodeKind::WHILE_LOOP);
    auto* loop = static_cast<WhileLoop*>(ast);
    REQUIRE(loop->condition->type == SQLNodeKind::BINARY_OP);
    REQUIRE(loop->body.size() == 1);

    // MySQL's WHILE..DO..END WHILE is the one dialect-correct output today.
    REQUIRE(transpile(sql, SQLDialect::MySQL)
            == "WHILE `x` < 10 DO SET `x` = `x` + 1 END WHILE");
}

// ============================================================================
// FOR and the FOR -> WHILE lowering for SQL Server
// ============================================================================

TEST_CASE("Procedure dialects - FOR loop preserved for PostgreSQL and Oracle", "[procedure][for]") {
    const std::string sql = "FOR i IN 1..10 LOOP SELECT 1; END LOOP";

    REQUIRE(transpile(sql, SQLDialect::PostgreSQL) == "FOR i IN 1..10 LOOP SELECT 1 END LOOP");
    REQUIRE(transpile(sql, SQLDialect::Oracle) == "FOR i IN 1..10 LOOP SELECT 1 END LOOP");
    REQUIRE(transpile(sql, SQLDialect::MySQL) == "FOR i IN 1..10 LOOP SELECT 1 END LOOP");
}

TEST_CASE("Procedure dialects - FOR lowered to DECLARE/WHILE for SQL Server", "[procedure][for]") {
    REQUIRE(transpile("FOR i IN 1..10 LOOP SELECT 1; END LOOP", SQLDialect::SQLServer)
            == "DECLARE @i INT = 1 WHILE @i <= 10 BEGIN SELECT 1 SET @i = @i + 1 END");
}

TEST_CASE("Procedure dialects - FOR lowering keeps variable name and bounds", "[procedure][for]") {
    REQUIRE(transpile("FOR counter IN 0..100 LOOP SELECT 5; END LOOP", SQLDialect::SQLServer)
            == "DECLARE @counter INT = 0 WHILE @counter <= 100 BEGIN SELECT 5 SET @counter = @counter + 1 END");
}

// ============================================================================
// DECLARE (variables)
// ============================================================================

TEST_CASE("Procedure dialects - DECLARE variable", "[procedure][declare]") {
    REQUIRE(transpile("DECLARE x INT", SQLDialect::PostgreSQL) == "DECLARE x INT");
    REQUIRE(transpile("DECLARE x INT", SQLDialect::SQLServer) == "DECLARE x INT");

    libglot::Arena arena;
    auto* ast = parse(arena, "DECLARE x INT");
    REQUIRE(ast->type == SQLNodeKind::DECLARE_VAR_STMT);
    auto* stmt = static_cast<DeclareVarStmt*>(ast);
    REQUIRE(stmt->variable_name == "x");
    REQUIRE(stmt->type == "INT");
    REQUIRE(stmt->default_value == nullptr);
}

TEST_CASE("Procedure dialects - DECLARE with DEFAULT", "[procedure][declare]") {
    REQUIRE(transpile("DECLARE x INT DEFAULT 5", SQLDialect::MySQL) == "DECLARE x INT DEFAULT 5");

    libglot::Arena arena;
    auto* ast = parse(arena, "DECLARE x INT DEFAULT 5");
    auto* stmt = static_cast<DeclareVarStmt*>(ast);
    REQUIRE(stmt->default_value != nullptr);
    REQUIRE(stmt->default_value->type == SQLNodeKind::LITERAL);
}

// ============================================================================
// Cursors
// ============================================================================

TEST_CASE("Procedure dialects - DECLARE CURSOR FOR SELECT", "[procedure][cursor]") {
    const std::string sql = "DECLARE cur CURSOR FOR SELECT id FROM users";

    REQUIRE(transpile(sql, SQLDialect::PostgreSQL)
            == "DECLARE cur CURSOR FOR SELECT \"id\" FROM \"users\"");
    REQUIRE(transpile(sql, SQLDialect::SQLServer)
            == "DECLARE cur CURSOR FOR SELECT [id] FROM [users]");

    libglot::Arena arena;
    auto* ast = parse(arena, sql);
    REQUIRE(ast->type == SQLNodeKind::DECLARE_CURSOR_STMT);
    auto* stmt = static_cast<DeclareCursorStmt*>(ast);
    REQUIRE(stmt->cursor_name == "cur");
    REQUIRE(stmt->query != nullptr);
    REQUIRE(stmt->query->type == SQLNodeKind::SELECT_STMT);
}

TEST_CASE("Procedure dialects - OPEN, FETCH INTO, CLOSE", "[procedure][cursor]") {
    REQUIRE(transpile("OPEN cur", SQLDialect::PostgreSQL) == "OPEN cur");
    REQUIRE(transpile("FETCH cur INTO x", SQLDialect::PostgreSQL) == "FETCH cur INTO x");
    REQUIRE(transpile("CLOSE cur", SQLDialect::PostgreSQL) == "CLOSE cur");

    libglot::Arena arena;
    auto* fetch = parse(arena, "FETCH cur INTO x");
    REQUIRE(fetch->type == SQLNodeKind::FETCH_CURSOR_STMT);
    auto* stmt = static_cast<FetchCursorStmt*>(fetch);
    REQUIRE(stmt->cursor_name == "cur");
    REQUIRE(stmt->into_variables.size() == 1);
    REQUIRE(stmt->into_variables[0] == "x");
}

// ============================================================================
// RAISE vs SIGNAL
// ============================================================================

TEST_CASE("Procedure dialects - RAISE becomes SIGNAL for MySQL", "[procedure][raise]") {
    REQUIRE(transpile("RAISE EXCEPTION 'bad thing'", SQLDialect::MySQL)
            == "SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'bad thing'");
}

TEST_CASE("Procedure dialects - RAISE stays RAISE for PostgreSQL", "[procedure][raise]") {
    REQUIRE(transpile("RAISE EXCEPTION 'bad thing'", SQLDialect::PostgreSQL)
            == "RAISE EXCEPTION 'bad thing'");
}

TEST_CASE("Procedure dialects - SIGNAL parses and carries SQLSTATE", "[procedure][raise]") {
    libglot::Arena arena;
    auto* ast = parse(arena, "SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'oops'");
    REQUIRE(ast->type == SQLNodeKind::RAISE_STMT);

    SQLGenerator gen(SQLDialect::MySQL);
    REQUIRE(gen.generate(ast) == "SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'oops'");
}
