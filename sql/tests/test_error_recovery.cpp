#include <catch2/catch_test_macros.hpp>
#include <libglot/parse/error_recovery.h>
#include <libglot/sql/parser.h>
#include <libglot/util/arena.h>
#include <chrono>

using namespace libglot;
using namespace libglot::sql;

TEST_CASE("Error Recovery - ErrorCollector basics", "[error_recovery]") {
    SECTION("Default mode is THROW_ON_ERROR") {
        ErrorCollector collector;
        REQUIRE(collector.get_mode() == ErrorRecoveryMode::THROW_ON_ERROR);
        REQUIRE_FALSE(collector.has_errors());
        REQUIRE(collector.error_count() == 0);
    }

    SECTION("Add single error") {
        ErrorCollector collector(ErrorRecoveryMode::COLLECT_ERRORS);
        collector.add_error("Test error", 10, 1, 5, "SELECT * FROM");

        REQUIRE(collector.has_errors());
        REQUIRE(collector.error_count() == 1);

        const auto& errors = collector.get_errors();
        REQUIRE(errors.size() == 1);
        REQUIRE(errors[0].message == "Test error");
        REQUIRE(errors[0].position == 10);
        REQUIRE(errors[0].line == 1);
        REQUIRE(errors[0].column == 5);
    }

    SECTION("Add multiple errors") {
        ErrorCollector collector(ErrorRecoveryMode::COLLECT_ERRORS);
        collector.add_error("Error 1", 5);
        collector.add_error("Error 2", 15);
        collector.add_error("Error 3", 25);

        REQUIRE(collector.error_count() == 3);

        std::string report = collector.format_errors();
        REQUIRE(report.find("Found 3 error(s)") != std::string::npos);
        REQUIRE(report.find("Error 1") != std::string::npos);
        REQUIRE(report.find("Error 2") != std::string::npos);
        REQUIRE(report.find("Error 3") != std::string::npos);
    }

    SECTION("Clear errors") {
        ErrorCollector collector(ErrorRecoveryMode::COLLECT_ERRORS);
        collector.add_error("Error 1", 5);
        collector.add_error("Error 2", 15);

        REQUIRE(collector.error_count() == 2);

        collector.clear();
        REQUIRE_FALSE(collector.has_errors());
        REQUIRE(collector.error_count() == 0);
    }

    SECTION("Max errors limit") {
        ErrorCollector collector(ErrorRecoveryMode::COLLECT_ERRORS);
        collector.set_max_errors(5);

        for (int i = 0; i < 10; ++i) {
            collector.add_error("Error " + std::to_string(i), i * 10);
        }

        // Should stop at max + 1 (the overflow message)
        REQUIRE(collector.error_count() <= 6);
    }
}

TEST_CASE("Error Recovery - ParseErrorDetail formatting", "[error_recovery]") {
    SECTION("Format error with position only") {
        ParseErrorDetail error("Unexpected token", 42);
        std::string formatted = error.format();

        REQUIRE(formatted.find("position 42") != std::string::npos);
        REQUIRE(formatted.find("Unexpected token") != std::string::npos);
    }

    SECTION("Format error with line and column") {
        ParseErrorDetail error("Missing semicolon", 100, 5, 12);
        std::string formatted = error.format();

        REQUIRE(formatted.find("line 5") != std::string::npos);
        REQUIRE(formatted.find("column 12") != std::string::npos);
        REQUIRE(formatted.find("Missing semicolon") != std::string::npos);
    }

    SECTION("Format error with context") {
        ParseErrorDetail error("Syntax error", 50, 3, 8, "SELECT * FORM users");
        std::string formatted = error.format();

        REQUIRE(formatted.find("SELECT * FORM users") != std::string::npos);
    }
}

TEST_CASE("Error Recovery - MultipleParseErrors exception", "[error_recovery]") {
    SECTION("Create exception from collector") {
        ErrorCollector collector(ErrorRecoveryMode::COLLECT_ERRORS);
        collector.add_error("Error 1", 10);
        collector.add_error("Error 2", 20);

        MultipleParseErrors ex(collector);
        REQUIRE(ex.get_errors().size() == 2);

        std::string msg = ex.what();
        REQUIRE(msg.find("Found 2 error(s)") != std::string::npos);
    }
}

TEST_CASE("Error Recovery - ErrorRecoveryGuard", "[error_recovery]") {
    SECTION("Guard tracks error additions") {
        ErrorCollector collector(ErrorRecoveryMode::COLLECT_ERRORS);

        {
            ErrorRecoveryGuard guard(collector, "test_context");
            REQUIRE_FALSE(guard.added_errors());

            collector.add_error("Error in context", 15);
            REQUIRE(guard.added_errors());
        }

        REQUIRE(collector.error_count() == 1);
    }

    SECTION("Guard with no errors") {
        ErrorCollector collector(ErrorRecoveryMode::COLLECT_ERRORS);

        {
            ErrorRecoveryGuard guard(collector, "clean_context");
            // No errors added
            REQUIRE_FALSE(guard.added_errors());
        }

        REQUIRE(collector.error_count() == 0);
    }
}

TEST_CASE("Error Recovery - Integration with SQLParser", "[error_recovery][parser]") {
    SECTION("SQLParser throws on missing closing paren") {
        libglot::Arena arena;
        const std::string invalid_sql = "SELECT (id + 1 FROM users";  // Missing )

        REQUIRE_THROWS_AS([&]() {
            SQLParser parser(arena, invalid_sql);
            parser.parse_select();
        }(), ParseError);
    }

    SECTION("Valid query succeeds") {
        libglot::Arena arena;
        const std::string valid_sql = "SELECT * FROM users";

        REQUIRE_NOTHROW([&]() {
            SQLParser parser(arena, valid_sql);
            auto expr = parser.parse_select();
            REQUIRE(expr != nullptr);
        }());
    }

    SECTION("Multiple syntax errors in sequence") {
        libglot::Arena arena;

        // Missing closing paren in expression
        REQUIRE_THROWS_AS([&]() {
            SQLParser parser(arena, "SELECT (id FROM users");
            parser.parse_select();
        }(), ParseError);

        // Invalid CASE without END
        REQUIRE_THROWS_AS([&]() {
            SQLParser parser(arena, "SELECT CASE WHEN age > 18 THEN 'adult' FROM users");
            parser.parse_select();
        }(), ParseError);

        // Invalid table name (number not allowed)
        REQUIRE_THROWS_AS([&]() {
            SQLParser parser(arena, "CREATE TABLE 123invalid (id INTEGER)");
            parser.parse_create_table();
        }(), ParseError);
    }
}

TEST_CASE("Error Recovery - Stress test", "[error_recovery][stress]") {
    SECTION("Handle many errors") {
        ErrorCollector collector(ErrorRecoveryMode::COLLECT_ERRORS);
        collector.set_max_errors(1000);

        for (int i = 0; i < 500; ++i) {
            collector.add_error(
                "Error number " + std::to_string(i),
                i * 10,
                i / 80 + 1,  // Line number
                i % 80       // Column number
            );
        }

        REQUIRE(collector.error_count() == 500);
        REQUIRE(collector.has_errors());

        std::string report = collector.format_errors();
        REQUIRE(report.find("Found 500 error(s)") != std::string::npos);
    }
}

TEST_CASE("Error Recovery - Edge cases", "[error_recovery]") {
    SECTION("Empty error message") {
        ErrorCollector collector(ErrorRecoveryMode::COLLECT_ERRORS);
        collector.add_error("", 0);

        REQUIRE(collector.has_errors());
        REQUIRE(collector.error_count() == 1);
    }

    SECTION("Very long error message") {
        ErrorCollector collector(ErrorRecoveryMode::COLLECT_ERRORS);
        std::string long_msg(10000, 'x');
        collector.add_error(long_msg, 100);

        REQUIRE(collector.has_errors());
        const auto& errors = collector.get_errors();
        REQUIRE(errors[0].message.size() == 10000);
    }

    SECTION("Zero position") {
        ParseErrorDetail error("Error at start", 0, 1, 1);
        std::string formatted = error.format();

        REQUIRE(formatted.find("position 0") != std::string::npos);
    }
}

TEST_CASE("Error Recovery - Real-world scenarios", "[error_recovery]") {
    SECTION("Missing semicolon between statements") {
        ErrorCollector collector(ErrorRecoveryMode::COLLECT_ERRORS);
        collector.add_error("Expected semicolon", 30, 2, 0, "SELECT * FROM users SELECT * FROM orders");

        REQUIRE(collector.error_count() == 1);
        REQUIRE(collector.get_errors()[0].message == "Expected semicolon");
    }

    SECTION("Typo in keyword") {
        ErrorCollector collector(ErrorRecoveryMode::COLLECT_ERRORS);
        collector.add_error("Unknown keyword 'FORM', did you mean 'FROM'?", 15, 1, 10, "SELECT * FORM users");

        REQUIRE(collector.error_count() == 1);
        std::string report = collector.format_errors();
        REQUIRE(report.find("FORM") != std::string::npos);
    }

    SECTION("Unmatched parentheses") {
        ErrorCollector collector(ErrorRecoveryMode::COLLECT_ERRORS);
        collector.add_error("Unmatched '(' in expression", 25, 1, 20, "SELECT (id + salary FROM users");

        REQUIRE(collector.error_count() == 1);
    }

    SECTION("Invalid column name") {
        ErrorCollector collector(ErrorRecoveryMode::COLLECT_ERRORS);
        collector.add_error("Expected identifier for column name", 18, 1, 15, "SELECT 123invalid FROM users");

        REQUIRE(collector.error_count() == 1);
    }
}

TEST_CASE("Error Recovery - Performance", "[error_recovery][performance]") {
    SECTION("Fast error collection") {
        ErrorCollector collector(ErrorRecoveryMode::COLLECT_ERRORS);
        collector.set_max_errors(10000);

        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < 10000; ++i) {
            collector.add_error("Error " + std::to_string(i), i);
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        // Should complete in reasonable time (< 1 second for 10k errors)
        REQUIRE(duration.count() < 1000);
        REQUIRE(collector.error_count() == 10000);
    }
}

TEST_CASE("Error Recovery - Correctness verification", "[error_recovery][correctness]") {
    SECTION("Error order preserved") {
        ErrorCollector collector(ErrorRecoveryMode::COLLECT_ERRORS);

        collector.add_error("First", 10);
        collector.add_error("Second", 20);
        collector.add_error("Third", 30);

        const auto& errors = collector.get_errors();
        REQUIRE(errors.size() == 3);
        REQUIRE(errors[0].message == "First");
        REQUIRE(errors[1].message == "Second");
        REQUIRE(errors[2].message == "Third");
    }

    SECTION("Position tracking accuracy") {
        ErrorCollector collector(ErrorRecoveryMode::COLLECT_ERRORS);

        collector.add_error("Err1", 5, 1, 5);
        collector.add_error("Err2", 15, 2, 3);
        collector.add_error("Err3", 42, 3, 10);

        const auto& errors = collector.get_errors();
        REQUIRE(errors[0].position == 5);
        REQUIRE(errors[0].line == 1);
        REQUIRE(errors[0].column == 5);

        REQUIRE(errors[1].position == 15);
        REQUIRE(errors[1].line == 2);
        REQUIRE(errors[1].column == 3);

        REQUIRE(errors[2].position == 42);
        REQUIRE(errors[2].line == 3);
        REQUIRE(errors[2].column == 10);
    }
}
