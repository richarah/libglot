#include <catch2/catch_test_macros.hpp>
#include <libglot/sql/parser.h>
#include <libglot/sql/generator.h>
#include <libglot/sql/dialect_traits.h>
#include <libglot/util/arena.h>

using namespace libglot::sql;

// ============================================================================
// SET OPERATIONS - Tests for UNION/INTERSECT/EXCEPT
// ============================================================================

TEST_CASE("Set operations - UNION", "[advanced][set_operations]") {
    libglot::Arena arena;
    SQLParser parser(arena, "SELECT id FROM users UNION SELECT id FROM admins");

    auto stmt = parser.parse_top_level();

    REQUIRE(stmt != nullptr);
    REQUIRE(stmt->type == SQLNodeKind::UNION_STMT);

    SQLGenerator gen(SQLDialect::ANSI);
    std::string sql = gen.generate(stmt);
    REQUIRE(sql.find("UNION") != std::string::npos);
}

TEST_CASE("Set operations - UNION ALL", "[advanced][set_operations]") {
    libglot::Arena arena;
    SQLParser parser(arena, "SELECT id FROM users UNION ALL SELECT id FROM admins");

    auto stmt = parser.parse_top_level();

    REQUIRE(stmt != nullptr);
    REQUIRE(stmt->type == SQLNodeKind::UNION_STMT);
    auto* set_op = static_cast<UnionStmt*>(stmt);
    REQUIRE(set_op->all == true);

    SQLGenerator gen(SQLDialect::ANSI);
    std::string sql = gen.generate(stmt);
    REQUIRE(sql.find("UNION ALL") != std::string::npos);
}

TEST_CASE("Set operations - INTERSECT", "[advanced][set_operations]") {
    libglot::Arena arena;
    SQLParser parser(arena, "SELECT id FROM users INTERSECT SELECT id FROM admins");

    auto stmt = parser.parse_top_level();

    REQUIRE(stmt != nullptr);
    REQUIRE(stmt->type == SQLNodeKind::INTERSECT_STMT);
    auto* set_op = static_cast<IntersectStmt*>(stmt);
    REQUIRE(set_op->all == false);

    SQLGenerator gen(SQLDialect::ANSI);
    std::string sql = gen.generate(stmt);
    REQUIRE(sql.find("INTERSECT") != std::string::npos);
}

TEST_CASE("Set operations - INTERSECT ALL", "[advanced][set_operations]") {
    libglot::Arena arena;
    SQLParser parser(arena, "SELECT id FROM users INTERSECT ALL SELECT id FROM admins");

    auto stmt = parser.parse_top_level();

    REQUIRE(stmt != nullptr);
    REQUIRE(stmt->type == SQLNodeKind::INTERSECT_STMT);
    auto* set_op = static_cast<IntersectStmt*>(stmt);
    REQUIRE(set_op->all == true);

    SQLGenerator gen(SQLDialect::ANSI);
    std::string sql = gen.generate(stmt);
    REQUIRE(sql.find("INTERSECT ALL") != std::string::npos);
}

TEST_CASE("Set operations - EXCEPT", "[advanced][set_operations]") {
    libglot::Arena arena;
    SQLParser parser(arena, "SELECT id FROM users EXCEPT SELECT id FROM admins");

    auto stmt = parser.parse_top_level();

    REQUIRE(stmt != nullptr);
    REQUIRE(stmt->type == SQLNodeKind::EXCEPT_STMT);
    auto* set_op = static_cast<ExceptStmt*>(stmt);
    REQUIRE(set_op->all == false);

    SQLGenerator gen(SQLDialect::ANSI);
    std::string sql = gen.generate(stmt);
    REQUIRE(sql.find("EXCEPT") != std::string::npos);
}

TEST_CASE("Set operations - EXCEPT ALL", "[advanced][set_operations]") {
    libglot::Arena arena;
    SQLParser parser(arena, "SELECT id FROM users EXCEPT ALL SELECT id FROM admins");

    auto stmt = parser.parse_top_level();

    REQUIRE(stmt != nullptr);
    REQUIRE(stmt->type == SQLNodeKind::EXCEPT_STMT);
    auto* set_op = static_cast<ExceptStmt*>(stmt);
    REQUIRE(set_op->all == true);

    SQLGenerator gen(SQLDialect::ANSI);
    std::string sql = gen.generate(stmt);
    REQUIRE(sql.find("EXCEPT ALL") != std::string::npos);
}

// ============================================================================
// CASE EXPRESSIONS
// ============================================================================

TEST_CASE("CASE - Simple CASE expression", "[advanced][case]") {
    libglot::Arena arena;
    SQLParser parser(arena,
        "SELECT CASE status WHEN 1 THEN 'active' WHEN 2 THEN 'inactive' ELSE 'unknown' END FROM users");

    auto stmt = static_cast<SelectStmt*>(parser.parse_top_level());

    REQUIRE(stmt != nullptr);
    REQUIRE(stmt->columns.size() == 1);
    REQUIRE(stmt->columns[0]->type == SQLNodeKind::CASE_EXPR);

    SQLGenerator gen(SQLDialect::ANSI);
    std::string sql = gen.generate(stmt);
    REQUIRE(sql.find("CASE") != std::string::npos);
    REQUIRE(sql.find("WHEN") != std::string::npos);
    REQUIRE(sql.find("THEN") != std::string::npos);
    REQUIRE(sql.find("ELSE") != std::string::npos);
    REQUIRE(sql.find("END") != std::string::npos);
}

TEST_CASE("CASE - Searched CASE expression", "[advanced][case]") {
    libglot::Arena arena;
    SQLParser parser(arena,
        "SELECT CASE WHEN age < 18 THEN 'minor' WHEN age < 65 THEN 'adult' ELSE 'senior' END FROM users");

    auto stmt = static_cast<SelectStmt*>(parser.parse_top_level());

    REQUIRE(stmt != nullptr);
    REQUIRE(stmt->columns.size() == 1);
    REQUIRE(stmt->columns[0]->type == SQLNodeKind::CASE_EXPR);

    SQLGenerator gen(SQLDialect::ANSI);
    std::string sql = gen.generate(stmt);
    REQUIRE(sql.find("CASE") != std::string::npos);
    REQUIRE(sql.find("WHEN") != std::string::npos);
}

TEST_CASE("CASE - Without ELSE clause", "[advanced][case]") {
    libglot::Arena arena;
    SQLParser parser(arena,
        "SELECT CASE WHEN premium = 1 THEN 'Premium' END FROM users");

    auto stmt = static_cast<SelectStmt*>(parser.parse_top_level());

    REQUIRE(stmt != nullptr);
    REQUIRE(stmt->columns[0]->type == SQLNodeKind::CASE_EXPR);

    SQLGenerator gen(SQLDialect::ANSI);
    std::string sql = gen.generate(stmt);
    REQUIRE(!sql.empty());
}

// ============================================================================
// PREDICATES - EXISTS
// ============================================================================

TEST_CASE("Predicates - EXISTS", "[advanced][predicates]") {
    libglot::Arena arena;
    SQLParser parser(arena, "SELECT * FROM users WHERE EXISTS (SELECT 1 FROM orders WHERE user_id = users.id)");

    auto stmt = static_cast<SelectStmt*>(parser.parse_top_level());

    REQUIRE(stmt->where != nullptr);
    REQUIRE(stmt->where->type == SQLNodeKind::EXISTS_EXPR);

    SQLGenerator gen(SQLDialect::ANSI);
    std::string sql = gen.generate(stmt);
    REQUIRE(sql.find("EXISTS") != std::string::npos);
}

// ============================================================================
// TRANSACTION STATEMENTS
// ============================================================================

TEST_CASE("Transaction - BEGIN", "[advanced][transaction]") {
    libglot::Arena arena;
    SQLParser parser(arena, "BEGIN");

    auto stmt = parser.parse_top_level();

    REQUIRE(stmt != nullptr);
    REQUIRE(stmt->type == SQLNodeKind::BEGIN_STMT);

    SQLGenerator gen(SQLDialect::ANSI);
    std::string sql = gen.generate(stmt);
    REQUIRE(sql.find("BEGIN") != std::string::npos);
}

TEST_CASE("Transaction - COMMIT", "[advanced][transaction]") {
    libglot::Arena arena;
    SQLParser parser(arena, "COMMIT");

    auto stmt = parser.parse_top_level();

    REQUIRE(stmt != nullptr);
    REQUIRE(stmt->type == SQLNodeKind::COMMIT_STMT);

    SQLGenerator gen(SQLDialect::ANSI);
    std::string sql = gen.generate(stmt);
    REQUIRE(sql.find("COMMIT") != std::string::npos);
}

TEST_CASE("Transaction - ROLLBACK", "[advanced][transaction]") {
    libglot::Arena arena;
    SQLParser parser(arena, "ROLLBACK");

    auto stmt = parser.parse_top_level();

    REQUIRE(stmt != nullptr);
    REQUIRE(stmt->type == SQLNodeKind::ROLLBACK_STMT);

    SQLGenerator gen(SQLDialect::ANSI);
    std::string sql = gen.generate(stmt);
    REQUIRE(sql.find("ROLLBACK") != std::string::npos);
}

// ============================================================================
// UTILITY STATEMENTS
// ============================================================================

TEST_CASE("Utility - SET variable", "[advanced][utility]") {
    libglot::Arena arena;
    SQLParser parser(arena, "SET autocommit = 1");

    auto stmt = parser.parse_top_level();

    REQUIRE(stmt != nullptr);
    REQUIRE(stmt->type == SQLNodeKind::SET_STMT);

    SQLGenerator gen(SQLDialect::ANSI);
    std::string sql = gen.generate(stmt);
    REQUIRE(sql.find("SET") != std::string::npos);
}

TEST_CASE("Utility - DESCRIBE table", "[advanced][utility]") {
    libglot::Arena arena;
    SQLParser parser(arena, "DESCRIBE users");

    auto stmt = parser.parse_top_level();

    REQUIRE(stmt != nullptr);
    REQUIRE(stmt->type == SQLNodeKind::DESCRIBE_STMT);

    SQLGenerator gen(SQLDialect::ANSI);
    std::string sql = gen.generate(stmt);
    REQUIRE(sql.find("DESCRIBE") != std::string::npos);
}
