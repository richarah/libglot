#include <catch2/catch_test_macros.hpp>
#include <libglot/sql/parser.h>
#include <libglot/sql/generator.h>
#include <libglot/util/arena.h>
#include <libglot/sql/parser.h>

using namespace libglot::sql;

// Skip: KeywordLookup tests internal tokenizer implementation
// TEST_CASE("FOR keyword lookup", "[keywords][for]") {
//     REQUIRE(KeywordLookup::lookup("FOR") == TokenType::FOR);
//     REQUIRE(KeywordLookup::lookup("IN") == TokenType::IN);
//     REQUIRE(KeywordLookup::lookup("LOOP") == TokenType::LOOP);
//     REQUIRE(KeywordLookup::lookup("ENDLOOP") == TokenType::ENDLOOP);
// }

TEST_CASE("Simple FOR loop", "[parser][for]") {
    libglot::Arena arena;
    SQLParser parser(arena, "FOR i IN 1..10 LOOP RETURN i END LOOP");

    auto expr = parser.parse_top_level();
    REQUIRE(expr != nullptr);
    // TODO: Check node type is FOR_LOOP

    auto* for_loop = static_cast<ForLoop*>(expr);
    REQUIRE(for_loop->variable == "i");
    REQUIRE(for_loop->start_value != nullptr);
    REQUIRE(for_loop->end_value != nullptr);
    REQUIRE(for_loop->body.size() == 1);

    // Test generation
    SQLGenerator gen(SQLDialect::ANSI);
    std::string sql = gen.generate(expr);
    REQUIRE(sql == "FOR i IN 1..10 LOOP RETURN \"i\" END LOOP");
}

TEST_CASE("FOR loop with ENDLOOP (single token)", "[parser][for]") {
    libglot::Arena arena;
    SQLParser parser(arena, "FOR counter IN 0..99 LOOP RETURN counter ENDLOOP");

    auto expr = parser.parse_top_level();
    REQUIRE(expr != nullptr);
    // TODO: Check node type is FOR_LOOP

    auto* for_loop = static_cast<ForLoop*>(expr);
    REQUIRE(for_loop->variable == "counter");
    REQUIRE(for_loop->start_value != nullptr);
    REQUIRE(for_loop->end_value != nullptr);
    REQUIRE(for_loop->body.size() == 1);

    // Test generation (always outputs END LOOP)
    SQLGenerator gen(SQLDialect::ANSI);
    std::string sql = gen.generate(expr);
    REQUIRE(sql == "FOR counter IN 0..99 LOOP RETURN \"counter\" END LOOP");
}

TEST_CASE("FOR loop with multiple statements", "[parser][for]") {
    libglot::Arena arena;
    SQLParser parser(arena, "FOR idx IN 1..100 LOOP DECLARE temp INTEGER; RETURN temp END LOOP");

    auto expr = parser.parse_top_level();
    REQUIRE(expr != nullptr);
    // TODO: Check node type is FOR_LOOP

    auto* for_loop = static_cast<ForLoop*>(expr);
    REQUIRE(for_loop->variable == "idx");
    REQUIRE(for_loop->start_value != nullptr);
    REQUIRE(for_loop->end_value != nullptr);
    REQUIRE(for_loop->body.size() == 2);

    // Test generation
    SQLGenerator gen(SQLDialect::ANSI);
    std::string sql = gen.generate(expr);
    REQUIRE(sql == "FOR idx IN 1..100 LOOP DECLARE temp INTEGER RETURN \"temp\" END LOOP");
}

TEST_CASE("FOR loop with expressions", "[parser][for]") {
    libglot::Arena arena;
    SQLParser parser(arena, "FOR x IN start_val..end_val LOOP RETURN x * 2 END LOOP");

    auto expr = parser.parse_top_level();
    REQUIRE(expr != nullptr);
    // TODO: Check node type is FOR_LOOP

    auto* for_loop = static_cast<ForLoop*>(expr);
    REQUIRE(for_loop->variable == "x");
    REQUIRE(for_loop->start_value != nullptr);
    REQUIRE(for_loop->end_value != nullptr);
    REQUIRE(for_loop->body.size() == 1);

    // Test generation
    SQLGenerator gen(SQLDialect::ANSI);
    std::string sql = gen.generate(expr);
    REQUIRE(sql == "FOR x IN \"start_val\"..\"end_val\" LOOP RETURN \"x\" * 2 END LOOP");
}

TEST_CASE("Nested FOR and WHILE loops", "[parser][for][while]") {
    libglot::Arena arena;
    SQLParser parser(arena, "FOR i IN 1..10 LOOP WHILE i > 0 DO RETURN i END WHILE END LOOP");

    auto expr = parser.parse_top_level();
    REQUIRE(expr != nullptr);
    // TODO: Check node type is FOR_LOOP

    auto* for_loop = static_cast<ForLoop*>(expr);
    REQUIRE(for_loop->variable == "i");
    REQUIRE(for_loop->body.size() == 1);
    // TODO: Check nested node type is WHILE_LOOP

    // Test generation
    SQLGenerator gen(SQLDialect::ANSI);
    std::string sql = gen.generate(expr);
    REQUIRE(sql == "FOR i IN 1..10 LOOP WHILE \"i\" > 0 DO RETURN \"i\" END WHILE END LOOP");
}

TEST_CASE("FOR loop with IF statement", "[parser][for]") {
    libglot::Arena arena;
    SQLParser parser(arena, "FOR n IN 1..20 LOOP IF n > 10 THEN RETURN n END IF END LOOP");

    auto expr = parser.parse_top_level();
    REQUIRE(expr != nullptr);
    // TODO: Check node type is FOR_LOOP

    auto* for_loop = static_cast<ForLoop*>(expr);
    REQUIRE(for_loop->variable == "n");
    REQUIRE(for_loop->body.size() == 1);
    // TODO: Check nested node type is IF_STMT

    // Test generation
    SQLGenerator gen(SQLDialect::ANSI);
    std::string sql = gen.generate(expr);
    REQUIRE(sql == "FOR n IN 1..20 LOOP IF \"n\" > 10 THEN RETURN \"n\" END IF END LOOP");
}
