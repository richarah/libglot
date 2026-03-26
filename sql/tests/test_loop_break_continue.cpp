#include <catch2/catch_test_macros.hpp>
#include <libglot/sql/generator.h>
#include <libglot/sql/parser.h>
#include <libglot/util/arena.h>

using namespace libglot::sql;

TEST_CASE("LOOP statement parsing", "[loop][procedural]") {
    SECTION("Simple LOOP...END LOOP") {
        std::string sql = "LOOP RETURN 1 END LOOP";
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto expr = parser.parse_top_level();

        REQUIRE(expr != nullptr);
        REQUIRE(expr->type == SQLNodeKind::LOOP_STMT);

        auto* loop = static_cast<LoopStmt*>(expr);
        REQUIRE(loop->body.size() == 1);
    }

    SECTION("LOOP with multiple statements") {
        std::string sql = R"(
            LOOP
                x := x + 1;
                IF x > 10 THEN BREAK END IF
            END LOOP
        )";
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto expr = parser.parse_top_level();

        REQUIRE(expr != nullptr);
        REQUIRE(expr->type == SQLNodeKind::LOOP_STMT);

        auto* loop = static_cast<LoopStmt*>(expr);
        REQUIRE(loop->body.size() == 2);
    }

    SECTION("ENDLOOP single token syntax") {
        std::string sql = "LOOP RETURN 1 ENDLOOP";
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto expr = parser.parse_top_level();

        REQUIRE(expr != nullptr);
        REQUIRE(expr->type == SQLNodeKind::LOOP_STMT);
    }

    SECTION("Nested LOOP statements") {
        std::string sql = R"(
            LOOP
                LOOP
                    BREAK
                END LOOP
            END LOOP
        )";
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto expr = parser.parse_top_level();

        REQUIRE(expr != nullptr);
        REQUIRE(expr->type == SQLNodeKind::LOOP_STMT);

        auto* outer_loop = static_cast<LoopStmt*>(expr);
        REQUIRE(outer_loop->body.size() == 1);
        REQUIRE(outer_loop->body[0]->type == SQLNodeKind::LOOP_STMT);
    }
}

TEST_CASE("BREAK statement parsing", "[break][procedural]") {
    SECTION("Simple BREAK") {
        std::string sql = "BREAK";
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto expr = parser.parse_top_level();

        REQUIRE(expr != nullptr);
        REQUIRE(expr->type == SQLNodeKind::BREAK_STMT);
    }

    SECTION("BREAK in LOOP") {
        std::string sql = R"(
            LOOP
                IF x > 10 THEN BREAK END IF
            END LOOP
        )";
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto expr = parser.parse_top_level();

        REQUIRE(expr != nullptr);
        auto* loop = static_cast<LoopStmt*>(expr);
        REQUIRE(loop->body.size() == 1);
    }

    SECTION("BREAK in WHILE") {
        std::string sql = R"(
            WHILE x < 100 DO
                IF x = 50 THEN BREAK END IF
            END WHILE
        )";
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto expr = parser.parse_top_level();

        REQUIRE(expr != nullptr);
        REQUIRE(expr->type == SQLNodeKind::WHILE_LOOP);
    }

    SECTION("BREAK in FOR") {
        std::string sql = R"(
            FOR i IN 1..100 LOOP
                IF i = 50 THEN BREAK END IF
            END LOOP
        )";
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto expr = parser.parse_top_level();

        REQUIRE(expr != nullptr);
        REQUIRE(expr->type == SQLNodeKind::FOR_LOOP);
    }
}

TEST_CASE("EXIT statement parsing (alias for BREAK)", "[exit][procedural]") {
    SECTION("Simple EXIT") {
        std::string sql = "EXIT";
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto expr = parser.parse_top_level();

        REQUIRE(expr != nullptr);
        REQUIRE(expr->type == SQLNodeKind::BREAK_STMT);
    }

    SECTION("EXIT in LOOP") {
        std::string sql = R"(
            LOOP
                EXIT
            END LOOP
        )";
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto expr = parser.parse_top_level();

        REQUIRE(expr != nullptr);
        auto* loop = static_cast<LoopStmt*>(expr);
        REQUIRE(loop->body.size() == 1);
        REQUIRE(loop->body[0]->type == SQLNodeKind::BREAK_STMT);
    }
}

TEST_CASE("CONTINUE statement parsing", "[continue][procedural]") {
    SECTION("Simple CONTINUE") {
        std::string sql = "CONTINUE";
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto expr = parser.parse_top_level();

        REQUIRE(expr != nullptr);
        REQUIRE(expr->type == SQLNodeKind::CONTINUE_STMT);
    }

    SECTION("CONTINUE in LOOP") {
        std::string sql = R"(
            LOOP
                IF x % 2 = 0 THEN CONTINUE END IF
                RETURN x
            END LOOP
        )";
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto expr = parser.parse_top_level();

        REQUIRE(expr != nullptr);
        auto* loop = static_cast<LoopStmt*>(expr);
        REQUIRE(loop->body.size() == 2);
    }

    SECTION("CONTINUE in WHILE") {
        std::string sql = R"(
            WHILE x < 100 DO
                CONTINUE
            END WHILE
        )";
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto expr = parser.parse_top_level();

        REQUIRE(expr != nullptr);
        REQUIRE(expr->type == SQLNodeKind::WHILE_LOOP);
    }
}

TEST_CASE("Complex loop control scenarios", "[loop][break][continue][integration]") {
    SECTION("Infinite loop with conditional exit") {
        std::string sql = R"(
            LOOP
                x := x + 1;
                IF x >= 100 THEN
                    EXIT
                END IF
            END LOOP
        )";
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto expr = parser.parse_top_level();

        REQUIRE(expr != nullptr);
        REQUIRE(expr->type == SQLNodeKind::LOOP_STMT);
    }

    SECTION("Skip even numbers pattern") {
        std::string sql = R"(
            FOR i IN 1..100 LOOP
                IF i % 2 = 0 THEN
                    CONTINUE
                END IF;
                RETURN i
            END LOOP
        )";
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto expr = parser.parse_top_level();

        REQUIRE(expr != nullptr);
        REQUIRE(expr->type == SQLNodeKind::FOR_LOOP);
    }

    SECTION("Search pattern with early exit") {
        std::string sql = R"(
            LOOP
                DECLARE val INTEGER;
                IF val = target THEN
                    EXIT
                END IF;
                IF val IS NULL THEN
                    BREAK
                END IF
            END LOOP
        )";
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto expr = parser.parse_top_level();

        REQUIRE(expr != nullptr);
    }

    SECTION("Nested loops with different control flow") {
        std::string sql = R"(
            FOR i IN 1..10 LOOP
                FOR j IN 1..10 LOOP
                    IF i * j > 50 THEN
                        BREAK
                    END IF;
                    IF i * j % 3 = 0 THEN
                        CONTINUE
                    END IF
                END LOOP
            END LOOP
        )";
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto expr = parser.parse_top_level();

        REQUIRE(expr != nullptr);
        REQUIRE(expr->type == SQLNodeKind::FOR_LOOP);

        auto* outer = static_cast<ForLoop*>(expr);
        REQUIRE(outer->body.size() == 1);
        REQUIRE(outer->body[0]->type == SQLNodeKind::FOR_LOOP);
    }
}

TEST_CASE("Real-world loop patterns", "[loop][patterns][integration]") {
    SECTION("Cursor processing loop") {
        std::string sql = R"(
            LOOP
                FETCH cur INTO rec;
                IF rec IS NULL THEN
                    EXIT
                END IF;
                RETURN rec
            END LOOP
        )";
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto expr = parser.parse_top_level();

        REQUIRE(expr != nullptr);
    }

    SECTION("Retry loop with max attempts") {
        std::string sql = R"(
            DECLARE attempts INTEGER := 0;
            LOOP
                attempts := attempts + 1;
                IF attempts > 5 THEN
                    BREAK
                END IF
            END LOOP
        )";
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto expr = parser.parse_top_level();

        REQUIRE(expr != nullptr);
    }

    SECTION("Poll until ready pattern") {
        std::string sql = R"(
            LOOP
                IF is_ready() THEN
                    EXIT
                END IF
            END LOOP
        )";
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto expr = parser.parse_top_level();

        REQUIRE(expr != nullptr);
        REQUIRE(expr->type == SQLNodeKind::LOOP_STMT);
    }
}
