#include <catch2/catch_test_macros.hpp>
#include <libglot/sql/parser.h>
#include <libglot/sql/generator.h>
#include <libglot/util/arena.h>
#include <libglot/sql/parser.h>

using namespace libglot::sql;

TEST_CASE("DECLARE simple variable", "[parser][declare]") {
    libglot::Arena arena;
    SQLParser parser(arena, "DECLARE x INTEGER");

    auto expr = parser.parse_top_level();
    REQUIRE(expr != nullptr);
    REQUIRE(expr->type == SQLNodeKind::DECLARE_VAR_STMT);

    auto* decl = static_cast<DeclareVarStmt*>(expr);
    REQUIRE(decl->variable_name == "x");
    REQUIRE(decl->type == "INTEGER");
    REQUIRE(decl->default_value == nullptr);

    // Test generation
    SQLGenerator gen(SQLDialect::PostgreSQL);
    std::string sql = gen.generate(expr);
    REQUIRE(sql == "DECLARE x INTEGER");
}

TEST_CASE("DECLARE with parameterized type", "[parser][declare]") {
    libglot::Arena arena;
    SQLParser parser(arena, "DECLARE name VARCHAR(255)");

    auto expr = parser.parse_top_level();
    REQUIRE(expr != nullptr);
    REQUIRE(expr->type == SQLNodeKind::DECLARE_VAR_STMT);

    auto* decl = static_cast<DeclareVarStmt*>(expr);
    REQUIRE(decl->variable_name == "name");
    REQUIRE(decl->type == "VARCHAR(255)");
    REQUIRE(decl->default_value == nullptr);

    // Test generation
    SQLGenerator gen(SQLDialect::PostgreSQL);
    std::string sql = gen.generate(expr);
    REQUIRE(sql == "DECLARE name VARCHAR(255)");
}

TEST_CASE("DECLARE with DEFAULT value", "[parser][declare]") {
    libglot::Arena arena;
    SQLParser parser(arena, "DECLARE count INTEGER DEFAULT 0");

    auto expr = parser.parse_top_level();
    REQUIRE(expr != nullptr);
    REQUIRE(expr->type == SQLNodeKind::DECLARE_VAR_STMT);

    auto* decl = static_cast<DeclareVarStmt*>(expr);
    REQUIRE(decl->variable_name == "count");
    REQUIRE(decl->type == "INTEGER");
    REQUIRE(decl->default_value != nullptr);

    // Test generation
    SQLGenerator gen(SQLDialect::PostgreSQL);
    std::string sql = gen.generate(expr);
    REQUIRE(sql == "DECLARE count INTEGER DEFAULT 0");
}

TEST_CASE("DECLARE with complex DEFAULT expression", "[parser][declare]") {
    libglot::Arena arena;
    SQLParser parser(arena, "DECLARE total DECIMAL(10,2) DEFAULT 100.50");

    auto expr = parser.parse_top_level();
    REQUIRE(expr != nullptr);
    REQUIRE(expr->type == SQLNodeKind::DECLARE_VAR_STMT);

    auto* decl = static_cast<DeclareVarStmt*>(expr);
    REQUIRE(decl->variable_name == "total");
    REQUIRE(decl->type == "DECIMAL(10,2)");
    REQUIRE(decl->default_value != nullptr);

    // Test generation
    SQLGenerator gen(SQLDialect::PostgreSQL);
    std::string sql = gen.generate(expr);
    REQUIRE(sql == "DECLARE total DECIMAL(10,2) DEFAULT 100.50");
}
