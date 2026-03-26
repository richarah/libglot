#include <catch2/catch_test_macros.hpp>
#include <libglot/sql/parser.h>
#include <libglot/sql/generator.h>
#include <libglot/util/arena.h>
#include <libglot/sql/parser.h>

using namespace libglot::sql;

// Skip: KeywordLookup tests internal tokenizer implementation
// TEST_CASE("IF keyword lookup", "[keywords][if]") {
//     REQUIRE(KeywordLookup::lookup("IF") == TokenType::IF_KW);
//     REQUIRE(KeywordLookup::lookup("THEN") == TokenType::THEN);
//     REQUIRE(KeywordLookup::lookup("ELSE") == TokenType::ELSE);
//     REQUIRE(KeywordLookup::lookup("ELSEIF") == TokenType::ELSEIF);
//     REQUIRE(KeywordLookup::lookup("ENDIF") == TokenType::ENDIF);
// }

TEST_CASE("Simple IF THEN END IF", "[parser][if]") {
    libglot::Arena arena;
    SQLParser parser(arena, "IF x > 0 THEN RETURN 1 END IF");

    auto expr = parser.parse_top_level();
    REQUIRE(expr != nullptr);
    // TODO: Check node type is IF_STMT

    auto* if_stmt = static_cast<IfStmt*>(expr);
    REQUIRE(if_stmt->condition != nullptr);
    REQUIRE(if_stmt->then_stmts.size() == 1);
    // TODO: ELSEIF not implemented - IfStmt lacks elseif_branches field
    // REQUIRE(if_stmt->elseif_branches.size() == 0);
    REQUIRE(if_stmt->else_stmts.size() == 0);

    // Test generation
    SQLGenerator gen(SQLDialect::ANSI);
    std::string sql = gen.generate(expr);
    REQUIRE(sql == "IF \"x\" > 0 THEN RETURN 1 END IF");
}

TEST_CASE("IF THEN ELSE END IF", "[parser][if]") {
    libglot::Arena arena;
    SQLParser parser(arena, "IF x > 0 THEN RETURN 1 ELSE RETURN 0 END IF");

    auto expr = parser.parse_top_level();
    REQUIRE(expr != nullptr);
    // TODO: Check node type is IF_STMT

    auto* if_stmt = static_cast<IfStmt*>(expr);
    REQUIRE(if_stmt->condition != nullptr);
    REQUIRE(if_stmt->then_stmts.size() == 1);
    // TODO: ELSEIF not implemented
    // REQUIRE(if_stmt->elseif_branches.size() == 0);
    REQUIRE(if_stmt->else_stmts.size() == 1);

    // Test generation
    SQLGenerator gen(SQLDialect::ANSI);
    std::string sql = gen.generate(expr);
    REQUIRE(sql == "IF \"x\" > 0 THEN RETURN 1 ELSE RETURN 0 END IF");
}

TEST_CASE("IF THEN ELSEIF END IF", "[parser][if]") {
    libglot::Arena arena;
    SQLParser parser(arena, "IF x > 10 THEN RETURN 1 ELSEIF x > 5 THEN RETURN 2 END IF");

    auto expr = parser.parse_top_level();
    REQUIRE(expr != nullptr);
    // TODO: Check node type is IF_STMT

    auto* if_stmt = static_cast<IfStmt*>(expr);
    REQUIRE(if_stmt->condition != nullptr);
    REQUIRE(if_stmt->then_stmts.size() == 1);
    // ELSEIF is now properly implemented in elseif_branches
    REQUIRE(if_stmt->elseif_branches.size() == 1);
    REQUIRE(if_stmt->else_stmts.size() == 0);

    const auto& elseif = if_stmt->elseif_branches[0];
    REQUIRE(elseif.first != nullptr);  // condition
    REQUIRE(elseif.second.size() == 1);  // statements

    // Test generation
    SQLGenerator gen(SQLDialect::ANSI);
    std::string sql = gen.generate(expr);
    REQUIRE(sql == "IF \"x\" > 10 THEN RETURN 1 ELSEIF \"x\" > 5 THEN RETURN 2 END IF");
}

TEST_CASE("IF with multiple ELSEIF and ELSE", "[parser][if]") {
    libglot::Arena arena;
    SQLParser parser(arena, "IF x > 10 THEN RETURN 1 ELSEIF x > 5 THEN RETURN 2 ELSEIF x > 0 THEN RETURN 3 ELSE RETURN 0 END IF");

    auto expr = parser.parse_top_level();
    REQUIRE(expr != nullptr);
    // TODO: Check node type is IF_STMT

    auto* if_stmt = static_cast<IfStmt*>(expr);
    REQUIRE(if_stmt->condition != nullptr);
    REQUIRE(if_stmt->then_stmts.size() == 1);
    // 2 ELSEIFs in elseif_branches
    REQUIRE(if_stmt->elseif_branches.size() == 2);
    // 1 ELSE statement
    REQUIRE(if_stmt->else_stmts.size() == 1);

    // Test generation
    SQLGenerator gen(SQLDialect::ANSI);
    std::string sql = gen.generate(expr);
    REQUIRE(sql == "IF \"x\" > 10 THEN RETURN 1 ELSEIF \"x\" > 5 THEN RETURN 2 ELSEIF \"x\" > 0 THEN RETURN 3 ELSE RETURN 0 END IF");
}

TEST_CASE("IF with ENDIF (single token)", "[parser][if]") {
    libglot::Arena arena;
    SQLParser parser(arena, "IF x > 0 THEN RETURN 1 ENDIF");

    auto expr = parser.parse_top_level();
    REQUIRE(expr != nullptr);
    // TODO: Check node type is IF_STMT

    auto* if_stmt = static_cast<IfStmt*>(expr);
    REQUIRE(if_stmt->condition != nullptr);
    REQUIRE(if_stmt->then_stmts.size() == 1);

    // Test generation (always outputs END IF)
    SQLGenerator gen(SQLDialect::ANSI);
    std::string sql = gen.generate(expr);
    REQUIRE(sql == "IF \"x\" > 0 THEN RETURN 1 END IF");
}

TEST_CASE("IF with multiple statements in THEN", "[parser][if]") {
    libglot::Arena arena;
    SQLParser parser(arena, "IF x > 0 THEN DECLARE y INTEGER; RETURN y END IF");

    auto expr = parser.parse_top_level();
    REQUIRE(expr != nullptr);
    // TODO: Check node type is IF_STMT

    auto* if_stmt = static_cast<IfStmt*>(expr);
    REQUIRE(if_stmt->condition != nullptr);
    REQUIRE(if_stmt->then_stmts.size() == 2);

    // Test generation
    SQLGenerator gen(SQLDialect::ANSI);
    std::string sql = gen.generate(expr);
    REQUIRE(sql == "IF \"x\" > 0 THEN DECLARE y INTEGER RETURN \"y\" END IF");
}
