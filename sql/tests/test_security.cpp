// ============================================================================
// Security tests: identifier and string-literal escaping in the generator.
//
// An identifier or literal containing its own quoting character must never
// be able to terminate the quoting and smuggle raw SQL into the output.
// ============================================================================

#include <catch2/catch_test_macros.hpp>
#include <libglot/sql/generator.h>
#include <libglot/sql/parser.h>
#include <libglot/util/arena.h>

using namespace libglot::sql;

TEST_CASE("Identifier escaping - embedded quote characters are doubled", "[security][identifier]") {
    libglot::Arena arena;

    SECTION("ANSI double-quote identifiers: foo\"bar") {
        auto* col = arena.create<Column>(std::string_view("foo\"bar"));
        SQLGenerator gen(SQLDialect::ANSI);
        REQUIRE(gen.generate(col) == "\"foo\"\"bar\"");
    }

    SECTION("SQL Server bracket identifiers: foo]bar") {
        auto* col = arena.create<Column>(std::string_view("foo]bar"));
        SQLGenerator gen(SQLDialect::SQLServer);
        REQUIRE(gen.generate(col) == "[foo]]bar]");
    }

    SECTION("MySQL backtick identifiers: foo`bar") {
        auto* col = arena.create<Column>(std::string_view("foo`bar"));
        SQLGenerator gen(SQLDialect::MySQL);
        REQUIRE(gen.generate(col) == "`foo``bar`");
    }

    SECTION("Injection-shaped identifier cannot close its own quoting") {
        auto* col = arena.create<Column>(std::string_view("x\"; DROP TABLE users; --"));
        SQLGenerator gen(SQLDialect::ANSI);
        // The embedded double quote is doubled, so the identifier stays one
        // quoted token: "x""; DROP TABLE users; --"
        REQUIRE(gen.generate(col) == "\"x\"\"; DROP TABLE users; --\"");
    }
}

TEST_CASE("String literal escaping - embedded single quotes are doubled", "[security][literal]") {
    libglot::Arena arena;

    SECTION("Programmatically constructed literal: O'Brien") {
        auto* lit = arena.create<Literal>(std::string_view("O'Brien"));
        SQLGenerator gen(SQLDialect::ANSI);
        REQUIRE(gen.generate(lit) == "'O''Brien'");
    }

    SECTION("Injection-shaped literal stays inside its quotes") {
        auto* lit = arena.create<Literal>(std::string_view("'; DROP TABLE users; --"));
        SQLGenerator gen(SQLDialect::ANSI);
        // Every embedded quote is doubled: '''; DROP TABLE users; --'
        REQUIRE(gen.generate(lit) == "'''; DROP TABLE users; --'");
    }
}

TEST_CASE("String literal roundtrip - source-level quote escaping preserved",
          "[security][literal]") {
    libglot::Arena arena;
    SQLParser parser(arena, "SELECT * FROM users WHERE name = 'O''Brien'");

    auto expr = parser.parse_top_level();
    SQLGenerator gen(SQLDialect::ANSI);
    std::string output = gen.generate(expr);
    REQUIRE(output == "SELECT * FROM \"users\" WHERE \"name\" = 'O''Brien'");

    // Re-parse the generated SQL: it must stay a single string literal
    // (no injection-shaped output that terminates the quoting early).
    libglot::Arena arena2;
    SQLParser parser2(arena2, output);
    auto stmt2 = static_cast<SelectStmt*>(parser2.parse_top_level());
    REQUIRE(stmt2->where->type == SQLNodeKind::BINARY_OP);
    auto* eq = static_cast<BinaryOp*>(stmt2->where);
    REQUIRE(eq->right->type == SQLNodeKind::LITERAL);
    REQUIRE(static_cast<Literal*>(eq->right)->value == "'O''Brien'");
}
