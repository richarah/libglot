/**
 * Test suite for improved error messages
 *
 * Verifies that ParseError includes:
 * - Line number
 * - Column number
 * - Context (the token that caused the error)
 * - Clear, human-readable messages
 */

#include <catch2/catch_test_macros.hpp>
#include <libglot/sql/parser.h>
#include <libglot/util/arena.h>
#include <string>
#include <iostream>
#include <algorithm>

using namespace libglot::sql;

TEST_CASE("Error messages include line and column numbers", "[error][messages]") {
    libglot::Arena arena;

    SECTION("Missing column list in SELECT") {
        SQLParser parser(arena, "SELECT FROM users");
        try {
            parser.parse_top_level();
            FAIL("Should have thrown ParseError");
        } catch (const libglot::ParseError& e) {
            REQUIRE(e.line == 1);
            REQUIRE(e.column == 8);
            std::string msg(e.what());
            REQUIRE(msg.find("Line 1") != std::string::npos);
            REQUIRE(msg.find("column 8") != std::string::npos);
            REQUIRE(msg.find("FROM") != std::string::npos);
        }
    }

    SECTION("Missing table name after CREATE TABLE") {
        SQLParser parser(arena, "CREATE TABLE (id INT)");
        try {
            parser.parse_top_level();
            FAIL("Should have thrown ParseError");
        } catch (const libglot::ParseError& e) {
            REQUIRE(e.line == 1);
            REQUIRE(e.column == 14);
            std::string msg(e.what());
            REQUIRE(msg.find("table name") != std::string::npos);
            REQUIRE(msg.find("CREATE TABLE") != std::string::npos);
        }
    }

    SECTION("Invalid token in WHERE clause") {
        SQLParser parser(arena, "SELECT * FROM users WHERE");
        try {
            parser.parse_top_level();
            FAIL("Should have thrown ParseError");
        } catch (const libglot::ParseError& e) {
            REQUIRE(e.line == 1);
            // Error occurs at end of WHERE
            std::string msg(e.what());
            REQUIRE(msg.find("Line 1") != std::string::npos);
            REQUIRE(msg.find("column") != std::string::npos);
        }
    }

    SECTION("Invalid syntax in expression") {
        SQLParser parser(arena, "SELECT * FROM users WHERE");
        try {
            parser.parse_top_level();
            FAIL("Should have thrown ParseError");
        } catch (const libglot::ParseError& e) {
            REQUIRE(e.line == 1);
            std::string msg(e.what());
            REQUIRE(msg.find("Line 1") != std::string::npos);
            REQUIRE(msg.find("column") != std::string::npos);
        }
    }
}

TEST_CASE("Error messages include context token", "[error][messages]") {
    libglot::Arena arena;

    SECTION("Context shows unexpected token") {
        SQLParser parser(arena, "SELECT FROM users");
        try {
            parser.parse_top_level();
            FAIL("Should have thrown ParseError");
        } catch (const libglot::ParseError& e) {
            REQUIRE(!e.context.empty());
            REQUIRE(e.context == "FROM");
        }
    }

    SECTION("Context captured for invalid syntax") {
        SQLParser parser(arena, "SELECT * FROM UNION");
        try {
            parser.parse_top_level();
            FAIL("Should have thrown ParseError");
        } catch (const libglot::ParseError& e) {
            std::string msg(e.what());
            REQUIRE(!e.context.empty());
            REQUIRE(e.context == "UNION");
        }
    }
}

TEST_CASE("Error messages are human-readable", "[error][messages]") {
    libglot::Arena arena;

    SECTION("Clear message for missing table name") {
        SQLParser parser(arena, "CREATE TABLE");
        try {
            parser.parse_top_level();
            FAIL("Should have thrown ParseError");
        } catch (const libglot::ParseError& e) {
            std::string msg(e.what());
            // Should mention what was expected
            REQUIRE(msg.find("table name") != std::string::npos);
            // Should mention the context (what statement we're in)
            REQUIRE(msg.find("CREATE TABLE") != std::string::npos);
        }
    }

    SECTION("Clear message for unexpected token") {
        SQLParser parser(arena, "SELECT * FROM UNION");
        try {
            parser.parse_top_level();
            FAIL("Should have thrown ParseError");
        } catch (const libglot::ParseError& e) {
            std::string msg(e.what());
            REQUIRE(msg.find("Expected") != std::string::npos);
            REQUIRE(msg.find("UNION") != std::string::npos);
        }
    }

    SECTION("Missing keyword error is clear") {
        SQLParser parser(arena, "INSERT INTO users VALUES");
        try {
            parser.parse_top_level();
            FAIL("Should have thrown ParseError");
        } catch (const libglot::ParseError& e) {
            std::string msg(e.what());
            // Should mention what was expected
            REQUIRE(msg.find("Expected") != std::string::npos);
            // Should have line/column info
            REQUIRE(e.line == 1);
        }
    }
}

TEST_CASE("Error messages for multiline SQL", "[error][messages]") {
    libglot::Arena arena;

    SECTION("Error on second line") {
        std::string sql = "SELECT *\nFROM\nWHERE age > 18";
        SQLParser parser(arena, sql);
        try {
            parser.parse_top_level();
            FAIL("Should have thrown ParseError");
        } catch (const libglot::ParseError& e) {
            REQUIRE(e.line == 3);
            std::string msg(e.what());
            REQUIRE(msg.find("Line 3") != std::string::npos);
        }
    }

    SECTION("Error on third line") {
        std::string sql = "SELECT id, name\nFROM users\nWHERE";
        SQLParser parser(arena, sql);
        try {
            parser.parse_top_level();
            FAIL("Should have thrown ParseError");
        } catch (const libglot::ParseError& e) {
            REQUIRE(e.line == 3);
            std::string msg(e.what());
            REQUIRE(msg.find("Line 3") != std::string::npos);
        }
    }
}

TEST_CASE("Error messages demonstrate fail-fast behavior", "[error][messages]") {
    libglot::Arena arena;

    SECTION("First error stops parsing") {
        // Multiple errors in SQL - should only report first one
        SQLParser parser(arena, "SELECT FROM WHERE");
        try {
            parser.parse_top_level();
            FAIL("Should have thrown ParseError");
        } catch (const libglot::ParseError& e) {
            // Should get first error only (fail-fast)
            REQUIRE(e.line == 1);
            REQUIRE(e.column == 8);
            std::string msg(e.what());
            // First error: missing column list after SELECT
            REQUIRE(msg.find("Line 1") != std::string::npos);
        }
    }

    SECTION("No error recovery - precise single error") {
        SQLParser parser(arena, "SELECT * FROM");  // Missing table name
        try {
            parser.parse_top_level();
            FAIL("Should have thrown ParseError");
        } catch (const libglot::ParseError& e) {
            std::string msg(e.what());
            // Should report the precise location of the error
            REQUIRE(e.line == 1);
            REQUIRE(msg.find("table") != std::string::npos);
        }
    }
}

TEST_CASE("Error message edge cases", "[error][messages][edge_cases]") {
    libglot::Arena arena;

    SECTION("Empty SQL input") {
        SQLParser parser(arena, "");
        try {
            parser.parse_top_level();
            FAIL("Should have thrown ParseError for empty input");
        } catch (const libglot::ParseError& e) {
            std::string msg(e.what());
            REQUIRE(e.line == 1);
            // Should get a clear error
            REQUIRE(!msg.empty());
        }
    }

    SECTION("Only whitespace") {
        SQLParser parser(arena, "   \n\n  \t  ");
        try {
            parser.parse_top_level();
            FAIL("Should have thrown ParseError for whitespace-only input");
        } catch (const libglot::ParseError& e) {
            std::string msg(e.what());
            // Should report appropriate line
            REQUIRE(e.line >= 1);
        }
    }

    SECTION("Error at end of file") {
        SQLParser parser(arena, "SELECT * FROM users WHERE age >");
        try {
            parser.parse_top_level();
            FAIL("Should have thrown ParseError");
        } catch (const libglot::ParseError& e) {
            std::string msg(e.what());
            REQUIRE(e.line == 1);
            // Should indicate unexpected EOF or missing value
            REQUIRE(msg.find("column") != std::string::npos);
        }
    }

    SECTION("Very long line with error at end") {
        std::string sql = "SELECT ";
        for (int i = 0; i < 100; ++i) {
            sql += "col" + std::to_string(i) + ", ";
        }
        sql += "FROM users";  // Error: trailing comma before FROM

        SQLParser parser(arena, sql);
        try {
            parser.parse_top_level();
            FAIL("Should have thrown ParseError");
        } catch (const libglot::ParseError& e) {
            std::string msg(e.what());
            REQUIRE(e.line == 1);
            // Column number should be accurate even for long lines
            REQUIRE(e.column > 100);
            REQUIRE(msg.find("FROM") != std::string::npos);
        }
    }

    SECTION("Multiple errors - fail fast on first") {
        SQLParser parser(arena, "SELECT FROM WHERE GROUP BY");
        try {
            parser.parse_top_level();
            FAIL("Should have thrown ParseError");
        } catch (const libglot::ParseError& e) {
            // Should only report first error (missing column list)
            REQUIRE(e.line == 1);
            REQUIRE(e.column == 8);  // Position of FROM token
            std::string msg(e.what());
            REQUIRE(msg.find("FROM") != std::string::npos);
        }
    }

    SECTION("Nested subquery error") {
        SQLParser parser(arena, "SELECT * FROM (SELECT FROM inner_table) t");
        try {
            parser.parse_top_level();
            FAIL("Should have thrown ParseError");
        } catch (const libglot::ParseError& e) {
            std::string msg(e.what());
            REQUIRE(e.line == 1);
            // Error should be in the subquery
            REQUIRE(e.column > 15);  // After opening parenthesis
            REQUIRE(msg.find("FROM") != std::string::npos);
        }
    }

    SECTION("Error in CTE") {
        SQLParser parser(arena, "WITH cte AS (SELECT FROM users) SELECT * FROM cte");
        try {
            parser.parse_top_level();
            FAIL("Should have thrown ParseError");
        } catch (const libglot::ParseError& e) {
            std::string msg(e.what());
            REQUIRE(e.line == 1);
            // Error should be inside CTE definition
            REQUIRE(e.column > 13);  // After CTE opening
        }
    }

    SECTION("Multiline error with tabs") {
        std::string sql = "SELECT\n\t\tid,\n\t\tname\n\t\tFROM";
        SQLParser parser(arena, sql);
        try {
            parser.parse_top_level();
            FAIL("Should have thrown ParseError");
        } catch (const libglot::ParseError& e) {
            std::string msg(e.what());
            // Error on line 4 (the FROM line)
            REQUIRE(e.line == 4);
        }
    }

    SECTION("Missing closing parenthesis") {
        SQLParser parser(arena, "SELECT * FROM (SELECT * FROM users");
        try {
            parser.parse_top_level();
            FAIL("Should have thrown ParseError");
        } catch (const libglot::ParseError& e) {
            std::string msg(e.what());
            REQUIRE(e.line == 1);
            // Should mention missing closing paren or EOF
            REQUIRE(msg.find("Expected") != std::string::npos);
        }
    }

    SECTION("Invalid operator sequence") {
        SQLParser parser(arena, "SELECT * FROM users WHERE age > > 18");
        try {
            parser.parse_top_level();
            FAIL("Should have thrown ParseError");
        } catch (const libglot::ParseError& e) {
            std::string msg(e.what());
            REQUIRE(e.line == 1);
            // Error at second '>'
            REQUIRE(e.column > 30);
        }
    }

    SECTION("Reserved keyword as identifier without quotes") {
        SQLParser parser(arena, "SELECT select FROM users");
        try {
            parser.parse_top_level();
            FAIL("Should have thrown ParseError");
        } catch (const libglot::ParseError& e) {
            std::string msg(e.what());
            REQUIRE(e.line == 1);
            // Should error on the duplicate SELECT keyword (keywords are lowercase in context)
            std::string ctx = e.context;
            std::transform(ctx.begin(), ctx.end(), ctx.begin(), ::tolower);
            REQUIRE(ctx == "select");
        }
    }
}

TEST_CASE("Error message format examples", "[error][messages][examples]") {
    libglot::Arena arena;

    SECTION("Example 1: Missing column list") {
        SQLParser parser(arena, "SELECT FROM users");
        try {
            parser.parse_top_level();
            FAIL("Should have thrown");
        } catch (const libglot::ParseError& e) {
            // Example output: Line 1, column 8: Expected column list after SELECT (found: 'FROM')
            std::string msg(e.what());
            std::cout << "Example error 1: " << msg << "\n";
            REQUIRE(msg.find("Line 1, column 8") != std::string::npos);
        }
    }

    SECTION("Example 2: Missing table name in CREATE") {
        SQLParser parser(arena, "CREATE TABLE (id INT)");
        try {
            parser.parse_top_level();
            FAIL("Should have thrown");
        } catch (const libglot::ParseError& e) {
            // Example output: Line 1, column 14: Expected table name after CREATE TABLE (found: '(')
            std::string msg(e.what());
            std::cout << "Example error 2: " << msg << "\n";
            REQUIRE(msg.find("table name after CREATE TABLE") != std::string::npos);
        }
    }

    SECTION("Example 3: Multiline error") {
        std::string sql = "SELECT id,\n       name\nFROM";
        SQLParser parser(arena, sql);
        try {
            parser.parse_top_level();
            FAIL("Should have thrown");
        } catch (const libglot::ParseError& e) {
            // Example output: Line 3, column 1: Expected table name (found: '<EOF>')
            std::string msg(e.what());
            std::cout << "Example error 3: " << msg << "\n";
            REQUIRE(msg.find("Line 3") != std::string::npos);
        }
    }
}
