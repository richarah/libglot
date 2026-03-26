#include <catch2/catch_test_macros.hpp>
#include <libglot/sql/parser.h>

using namespace libglot::sql;

TEST_CASE("RETURNS clause - Comprehensive testing", "[returns][functions][comprehensive]") {
    SECTION("Simple RETURNS with scalar type") {
        std::string sql = "CREATE FUNCTION get_count() RETURNS INT AS BEGIN RETURN 42; END";
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto expr = parser.parse();

        REQUIRE(expr != nullptr);
        REQUIRE(expr->type == SQLNodeKind::CREATE_PROCEDURE_STMT);

        auto* func = static_cast<CreateProcedureStmt*>(expr);
        REQUIRE(func->is_function == true);
        REQUIRE(func->name == "get_count");
        REQUIRE(func->return_type == "INT");
    }

    SECTION("RETURNS with VARCHAR type") {
        std::string sql = "CREATE FUNCTION get_name() RETURNS VARCHAR(100) AS BEGIN RETURN 'test'; END";
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto expr = parser.parse();

        REQUIRE(expr != nullptr);
        REQUIRE(expr->type == SQLNodeKind::CREATE_PROCEDURE_STMT);

        auto* func = static_cast<CreateProcedureStmt*>(expr);
        REQUIRE(func->return_type == "VARCHAR(100)");  // Fixed: includes type parameters
    }

    SECTION("RETURNS TABLE - complex type") {
        std::string sql = "CREATE FUNCTION get_users() RETURNS TABLE(id INT, name VARCHAR) AS BEGIN RETURN NULL; END";
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto expr = parser.parse();

        REQUIRE(expr != nullptr);
        REQUIRE(expr->type == SQLNodeKind::CREATE_PROCEDURE_STMT);

        auto* func = static_cast<CreateProcedureStmt*>(expr);
        // Verify the complex return type is captured (exact format may have spaces/commas)
        REQUIRE(func->return_type.find("TABLE") != std::string::npos);
        REQUIRE(func->return_type.find("id") != std::string::npos);
    }

    SECTION("RETURNS SETOF - table-valued function") {
        std::string sql = "CREATE FUNCTION get_all_ids() RETURNS SETOF INT AS BEGIN RETURN NULL; END";
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto expr = parser.parse();

        REQUIRE(expr != nullptr);
        REQUIRE(expr->type == SQLNodeKind::CREATE_PROCEDURE_STMT);

        auto* func = static_cast<CreateProcedureStmt*>(expr);
        REQUIRE(func->return_type == "SETOF INT");
    }

    SECTION("RETURNS void - procedure-like function") {
        std::string sql = "CREATE FUNCTION log_event(msg TEXT) RETURNS VOID AS BEGIN INSERT INTO logs VALUES (msg); END";
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto expr = parser.parse();

        REQUIRE(expr != nullptr);
        REQUIRE(expr->type == SQLNodeKind::CREATE_PROCEDURE_STMT);

        auto* func = static_cast<CreateProcedureStmt*>(expr);
        REQUIRE(func->return_type == "VOID");
    }

    SECTION("DECLARE with := assignment") {
        std::string sql = R"(
            CREATE FUNCTION test_declare() RETURNS INT AS
            BEGIN
                DECLARE i INT := 0;
                RETURN i;
            END
        )";
        libglot::Arena arena;
        SQLParser parser(arena, sql);

        // This should parse without throwing
        REQUIRE_NOTHROW(parser.parse());
    }

    SECTION("Multiple RETURN statements in function") {
        std::string sql = R"(
            CREATE FUNCTION check_status(val INT) RETURNS TEXT AS
            BEGIN
                IF val > 100 THEN
                    RETURN 'HIGH';
                ELSEIF val > 50 THEN
                    RETURN 'MEDIUM';
                ELSE
                    RETURN 'LOW';
                END IF;
            END
        )";
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto expr = parser.parse();

        REQUIRE(expr != nullptr);
        REQUIRE(expr->type == SQLNodeKind::CREATE_PROCEDURE_STMT);
    }

    SECTION("RETURN with expression") {
        std::string sql = "CREATE FUNCTION double_value(x INT) RETURNS INT AS BEGIN RETURN x * 2; END";
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto expr = parser.parse();

        REQUIRE(expr != nullptr);
        REQUIRE(expr->type == SQLNodeKind::CREATE_PROCEDURE_STMT);
    }

    SECTION("RETURN with function call") {
        std::string sql = "CREATE FUNCTION get_upper(s TEXT) RETURNS TEXT AS BEGIN RETURN UPPER(s); END";
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto expr = parser.parse();

        REQUIRE(expr != nullptr);
        REQUIRE(expr->type == SQLNodeKind::CREATE_PROCEDURE_STMT);
    }

    SECTION("RETURN with subquery") {
        std::string sql = "CREATE FUNCTION get_max_id() RETURNS INT AS BEGIN RETURN (SELECT MAX(id) FROM users); END";
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto expr = parser.parse();

        REQUIRE(expr != nullptr);
        REQUIRE(expr->type == SQLNodeKind::CREATE_PROCEDURE_STMT);
    }

    SECTION("RETURN NULL explicitly") {
        std::string sql = "CREATE FUNCTION maybe_value(flag BOOLEAN) RETURNS INT AS BEGIN IF flag THEN RETURN 1; ELSE RETURN NULL; END IF; END";
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto expr = parser.parse();

        REQUIRE(expr != nullptr);
        REQUIRE(expr->type == SQLNodeKind::CREATE_PROCEDURE_STMT);
    }

    SECTION("RETURNS with DECIMAL type and precision") {
        std::string sql = "CREATE FUNCTION calculate_rate() RETURNS DECIMAL(10, 2) AS BEGIN RETURN 3.14; END";
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto expr = parser.parse();

        REQUIRE(expr != nullptr);
        REQUIRE(expr->type == SQLNodeKind::CREATE_PROCEDURE_STMT);
    }

    SECTION("Function with no parameters but RETURNS") {
        std::string sql = "CREATE FUNCTION get_timestamp() RETURNS TIMESTAMP AS BEGIN RETURN CURRENT_TIMESTAMP; END";
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto expr = parser.parse();

        REQUIRE(expr != nullptr);
        REQUIRE(expr->type == SQLNodeKind::CREATE_PROCEDURE_STMT);

        auto* func = static_cast<CreateProcedureStmt*>(expr);
        REQUIRE(func->parameters.size() == 0);
        REQUIRE(func->return_type == "TIMESTAMP");
    }

    SECTION("RETURN in nested block") {
        std::string sql = R"(
            CREATE FUNCTION process(x INT) RETURNS INT AS
            BEGIN
                BEGIN
                    IF x > 0 THEN
                        RETURN x;
                    END IF;
                END;
                RETURN 0;
            END
        )";
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto expr = parser.parse();

        REQUIRE(expr != nullptr);
        REQUIRE(expr->type == SQLNodeKind::CREATE_PROCEDURE_STMT);
    }

    SECTION("RETURN in LOOP with EXIT") {
        std::string sql = R"(
            CREATE FUNCTION find_value(target INT) RETURNS INT AS
            BEGIN
                DECLARE i INT := 0;
                LOOP
                    IF i = target THEN
                        RETURN i;
                    END IF;
                    i := i + 1;
                    IF i > 100 THEN
                        EXIT;
                    END IF;
                END LOOP;
                RETURN -1;
            END
        )";
        libglot::Arena arena;
        SQLParser parser(arena, sql);

        // This should parse without throwing
        REQUIRE_NOTHROW(parser.parse());
    }

    SECTION("RETURNS with BOOLEAN type") {
        std::string sql = "CREATE FUNCTION is_valid(x INT) RETURNS BOOLEAN AS BEGIN RETURN x > 0 AND x < 100; END";
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto expr = parser.parse();

        REQUIRE(expr != nullptr);
        REQUIRE(expr->type == SQLNodeKind::CREATE_PROCEDURE_STMT);

        auto* func = static_cast<CreateProcedureStmt*>(expr);
        REQUIRE(func->return_type == "BOOLEAN");
    }
}

TEST_CASE("RETURN statement edge cases", "[return][edge_cases]") {
    SECTION("Early RETURN before end") {
        std::string sql = R"(
            CREATE FUNCTION early_exit(flag BOOLEAN) RETURNS INT AS
            BEGIN
                IF flag THEN
                    RETURN 1;
                END IF;
                DECLARE x INT := 10;
                RETURN x;
            END
        )";
        libglot::Arena arena;
        SQLParser parser(arena, sql);

        // This should parse without throwing
        REQUIRE_NOTHROW(parser.parse());
    }

    SECTION("RETURN in EXCEPTION block") {
        std::string sql = R"(
            CREATE FUNCTION safe_calc(x INT) RETURNS INT AS
            BEGIN
                RETURN 100 / x;
            EXCEPTION
                WHEN division_by_zero THEN
                    RETURN 0;
            END
        )";
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto expr = parser.parse();

        REQUIRE(expr != nullptr);
    }

    SECTION("RETURN with CASE expression") {
        std::string sql = R"(
            CREATE FUNCTION categorize(val INT) RETURNS TEXT AS
            BEGIN
                RETURN CASE
                    WHEN val < 10 THEN 'small'
                    WHEN val < 100 THEN 'medium'
                    ELSE 'large'
                END;
            END
        )";
        libglot::Arena arena;
        SQLParser parser(arena, sql);
        auto expr = parser.parse();

        REQUIRE(expr != nullptr);
    }
}
