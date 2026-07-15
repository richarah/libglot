// The FOR keyword in its distinct grammatical roles:
//
//   1. procedural range FOR loops:      FOR i IN 1..10 LOOP ... END LOOP
//   2. cursor declarations:             DECLARE cur CURSOR FOR SELECT ...
//   3. unsupported FOR forms:           FOR i IN REVERSE ..., FOR rec IN SELECT ...
//      must fail with a clean ParseError, never crash.
//
// KNOWN BUG (reported, not asserted as correct): the FOR UPDATE locking
// clause is neither parsed nor rejected - "SELECT * FROM t FOR UPDATE"
// parses as a plain SELECT and the parser silently ignores the trailing
// FOR UPDATE tokens, so the clause is dropped from generated output.
// The test below only pins down that this input does not crash.

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

} // namespace

// ============================================================================
// FOR as a procedural range loop
// ============================================================================

TEST_CASE("FOR keyword - range loop AST shape", "[for][loop]") {
    libglot::Arena arena;
    SQLParser parser(arena, "FOR i IN 1..10 LOOP SELECT 1; END LOOP");
    auto* ast = parser.parse_top_level();

    REQUIRE(ast->type == SQLNodeKind::FOR_LOOP);
    auto* loop = static_cast<ForLoop*>(ast);
    REQUIRE(loop->variable == "i");
    REQUIRE(loop->start_value != nullptr);
    REQUIRE(loop->start_value->type == SQLNodeKind::LITERAL);
    REQUIRE(static_cast<Literal*>(loop->start_value)->value == "1");
    REQUIRE(loop->end_value != nullptr);
    REQUIRE(loop->end_value->type == SQLNodeKind::LITERAL);
    REQUIRE(static_cast<Literal*>(loop->end_value)->value == "10");
    REQUIRE(loop->body.size() == 1);
    REQUIRE(loop->body[0]->type == SQLNodeKind::SELECT_STMT);
}

TEST_CASE("FOR keyword - range loop round-trips for FOR-native dialects", "[for][loop]") {
    const std::string sql = "FOR i IN 1..10 LOOP SELECT 1; END LOOP";

    REQUIRE(transpile(sql, SQLDialect::PostgreSQL) == "FOR i IN 1..10 LOOP SELECT 1; END LOOP");
    REQUIRE(transpile(sql, SQLDialect::Oracle) == "FOR i IN 1..10 LOOP SELECT 1; END LOOP");
}

TEST_CASE("FOR keyword - loop body may hold multiple statements", "[for][loop]") {
    libglot::Arena arena;
    SQLParser parser(arena, "FOR i IN 1..3 LOOP SELECT 1; SELECT 2; END LOOP");
    auto* ast = parser.parse_top_level();

    REQUIRE(ast->type == SQLNodeKind::FOR_LOOP);
    auto* loop = static_cast<ForLoop*>(ast);
    REQUIRE(loop->body.size() == 2);
}

TEST_CASE("FOR keyword - nested FOR loops", "[for][loop]") {
    libglot::Arena arena;
    SQLParser parser(arena, "FOR i IN 1..3 LOOP FOR j IN 1..3 LOOP SELECT 1; END LOOP; END LOOP");
    auto* ast = parser.parse_top_level();

    REQUIRE(ast->type == SQLNodeKind::FOR_LOOP);
    auto* outer = static_cast<ForLoop*>(ast);
    REQUIRE(outer->variable == "i");
    REQUIRE(outer->body.size() == 1);
    REQUIRE(outer->body[0]->type == SQLNodeKind::FOR_LOOP);
    auto* inner = static_cast<ForLoop*>(outer->body[0]);
    REQUIRE(inner->variable == "j");
}

TEST_CASE("FOR keyword - BREAK and CONTINUE inside a FOR body", "[for][loop]") {
    libglot::Arena arena;
    SQLParser parser(arena,
        "FOR i IN 1..10 LOOP IF i > 5 THEN BREAK; END IF; CONTINUE; END LOOP");
    auto* ast = parser.parse_top_level();

    REQUIRE(ast->type == SQLNodeKind::FOR_LOOP);
    auto* loop = static_cast<ForLoop*>(ast);
    REQUIRE(loop->body.size() == 2);
    REQUIRE(loop->body[0]->type == SQLNodeKind::IF_STMT);
    REQUIRE(loop->body[1]->type == SQLNodeKind::CONTINUE_STMT);
}

TEST_CASE("FOR keyword - loop lowered to WHILE for SQL Server", "[for][loop][transpile]") {
    REQUIRE(transpile("FOR i IN 1..10 LOOP SELECT 1; END LOOP", SQLDialect::SQLServer)
            == "BEGIN DECLARE @i INT = 1; WHILE @i <= 10 BEGIN SELECT 1; SET @i = @i + 1; END; END");
}

// ============================================================================
// FOR in cursor declarations
// ============================================================================

TEST_CASE("FOR keyword - DECLARE CURSOR FOR binds the query", "[for][cursor]") {
    libglot::Arena arena;
    SQLParser parser(arena, "DECLARE cur CURSOR FOR SELECT id FROM users");
    auto* ast = parser.parse_top_level();

    REQUIRE(ast->type == SQLNodeKind::DECLARE_CURSOR_STMT);
    auto* stmt = static_cast<DeclareCursorStmt*>(ast);
    REQUIRE(stmt->cursor_name == "cur");
    REQUIRE(stmt->query != nullptr);
    REQUIRE(stmt->query->type == SQLNodeKind::SELECT_STMT);

    SQLGenerator gen(SQLDialect::PostgreSQL);
    REQUIRE(gen.generate(ast) == "DECLARE cur CURSOR FOR SELECT \"id\" FROM \"users\"");
}

// ============================================================================
// Wave 2: FOR i IN REVERSE a..b LOOP (Oracle/PostgreSQL PL/SQL)
// ============================================================================

TEST_CASE("FOR keyword - REVERSE range loop AST shape", "[for][loop][reverse]") {
    libglot::Arena arena;
    SQLParser parser(arena, "FOR i IN REVERSE 10..1 LOOP SELECT 1; END LOOP");
    auto* ast = parser.parse_top_level();

    REQUIRE(ast->type == SQLNodeKind::FOR_LOOP);
    auto* loop = static_cast<ForLoop*>(ast);
    REQUIRE(loop->reverse == true);
    REQUIRE(loop->variable == "i");
    REQUIRE(loop->query == nullptr);
}

TEST_CASE("FOR keyword - REVERSE range loop round-trips for FOR-native dialects",
          "[for][loop][reverse]") {
    const std::string sql = "FOR i IN REVERSE 10..1 LOOP SELECT 1; END LOOP";
    REQUIRE(transpile(sql, SQLDialect::PostgreSQL) == sql);
    REQUIRE(transpile(sql, SQLDialect::Oracle) == sql);
}

TEST_CASE("FOR keyword - REVERSE loop lowered to a descending WHILE for SQL Server",
          "[for][loop][reverse][transpile]") {
    REQUIRE(transpile("FOR i IN REVERSE 10..1 LOOP SELECT 1; END LOOP", SQLDialect::SQLServer)
            == "BEGIN DECLARE @i INT = 10; WHILE @i >= 1 BEGIN SELECT 1; SET @i = @i - 1; END; END");
}

// ============================================================================
// Wave 2: FOR rec IN SELECT ... LOOP (PL/pgSQL / Oracle record iteration)
// ============================================================================

TEST_CASE("FOR keyword - record iteration (FOR rec IN SELECT) AST shape",
          "[for][loop][record]") {
    libglot::Arena arena;
    SQLParser parser(arena, "FOR rec IN SELECT id FROM users LOOP SELECT 1; END LOOP");
    auto* ast = parser.parse_top_level();

    REQUIRE(ast->type == SQLNodeKind::FOR_LOOP);
    auto* loop = static_cast<ForLoop*>(ast);
    REQUIRE(loop->variable == "rec");
    REQUIRE(loop->query != nullptr);
    REQUIRE(loop->query->type == SQLNodeKind::SELECT_STMT);
    REQUIRE(loop->start_value == nullptr);
}

TEST_CASE("FOR keyword - record iteration round-trips for PostgreSQL (no parens)",
          "[for][loop][record]") {
    REQUIRE(transpile("FOR rec IN SELECT id FROM users LOOP SELECT 1; END LOOP", SQLDialect::PostgreSQL)
            == "FOR rec IN SELECT \"id\" FROM \"users\" LOOP SELECT 1; END LOOP");
}

TEST_CASE("FOR keyword - record iteration generates Oracle's parenthesized form",
          "[for][loop][record]") {
    REQUIRE(transpile("FOR rec IN SELECT id FROM users LOOP SELECT 1; END LOOP", SQLDialect::Oracle)
            == "FOR rec IN (SELECT \"id\" FROM \"users\") LOOP SELECT 1; END LOOP");
    // Oracle's own parenthesized spelling parses too, and is a fixed point.
    REQUIRE(transpile("FOR rec IN (SELECT id FROM users) LOOP SELECT 1; END LOOP", SQLDialect::Oracle)
            == "FOR rec IN (SELECT \"id\" FROM \"users\") LOOP SELECT 1; END LOOP");
}

TEST_CASE("FOR keyword - record iteration has no T-SQL lowering (clean std::logic_error)",
          "[for][loop][record][error]") {
    REQUIRE_THROWS_AS(
        transpile("FOR rec IN SELECT id FROM users LOOP SELECT 1; END LOOP", SQLDialect::SQLServer),
        std::logic_error);
}

// ============================================================================
// Unsupported FOR forms fail cleanly (ParseError, not a crash)
// ============================================================================

TEST_CASE("FOR keyword - missing END LOOP raises a clean ParseError", "[for][error]") {
    libglot::Arena arena;
    SQLParser parser(arena, "FOR i IN 1..10 LOOP SELECT 1;");
    REQUIRE_THROWS_AS(parser.parse_top_level(), libglot::ParseError);
}

// ============================================================================
// FOR UPDATE locking clause (currently a silent no-op - see header comment)
// ============================================================================

TEST_CASE("FOR keyword - SELECT ... FOR UPDATE does not crash", "[for][locking]") {
    // KNOWN BUG: the clause is silently discarded instead of being parsed or
    // rejected. This test only guarantees the input cannot crash the parser;
    // it deliberately does not bless the dropped-clause output.
    libglot::Arena arena;
    SQLParser parser(arena, "SELECT * FROM t WHERE id = 1 FOR UPDATE");

    SQLNode* ast = nullptr;
    REQUIRE_NOTHROW(ast = parser.parse_top_level());
    REQUIRE(ast != nullptr);
    REQUIRE(ast->type == SQLNodeKind::SELECT_STMT);
}
