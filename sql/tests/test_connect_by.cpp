#include <catch2/catch_test_macros.hpp>
#include "libglot/sql/complete_features.h"
#include "libglot/core/arena.h"

using namespace libglot::sql;
using namespace libglot;
using TK = libsqlglot::TokenType;

TEST_CASE("CONNECT BY - Basic hierarchical query", "[sql][connect_by][oracle]") {
    const char* sql = "CONNECT BY PRIOR employee_id = manager_id";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    parser.advance(); parser.advance();
    auto* stmt = parser.parse_connect_by();

    REQUIRE(stmt != nullptr);
    REQUIRE(stmt->prior_left == true);
    REQUIRE(stmt->nocycle == false);
    REQUIRE(stmt->condition != nullptr);
}

TEST_CASE("CONNECT BY - With NOCYCLE", "[sql][connect_by][oracle]") {
    const char* sql = "CONNECT BY NOCYCLE PRIOR emp_id = mgr_id";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    parser.advance(); parser.advance();
    auto* stmt = parser.parse_connect_by();

    REQUIRE(stmt != nullptr);
    REQUIRE(stmt->nocycle == true);
    REQUIRE(stmt->prior_left == true);
}

TEST_CASE("CONNECT BY - Without PRIOR on left", "[sql][connect_by][oracle]") {
    const char* sql = "CONNECT BY parent_id = PRIOR child_id";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    parser.advance(); parser.advance();
    auto* stmt = parser.parse_connect_by();

    REQUIRE(stmt != nullptr);
    REQUIRE(stmt->prior_left == false);
}

TEST_CASE("CONNECT BY - START WITH clause", "[sql][start_with][oracle]") {
    const char* sql = "START WITH manager_id IS NULL";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    parser.advance(); parser.advance();
    auto* stmt = parser.parse_start_with();

    REQUIRE(stmt != nullptr);
    REQUIRE(stmt->condition != nullptr);
}

TEST_CASE("CONNECT BY - Complete hierarchical query", "[sql][connect_by][complete]") {
    const char* sql = R"(
        SELECT employee_id, manager_id, level
        FROM employees
        START WITH manager_id IS NULL
        CONNECT BY PRIOR employee_id = manager_id
    )";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    auto* stmt = parser.parse();

    REQUIRE(stmt != nullptr);
}

TEST_CASE("CONNECT BY - With NOCYCLE and START WITH", "[sql][connect_by][complete]") {
    const char* sql = R"(
        SELECT id, parent_id, name
        FROM categories
        START WITH parent_id IS NULL
        CONNECT BY NOCYCLE PRIOR id = parent_id
    )";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    auto* stmt = parser.parse();

    REQUIRE(stmt != nullptr);
}

TEST_CASE("CONNECT BY - Generator output", "[sql][connect_by][generator]") {
    const char* sql = "CONNECT BY NOCYCLE PRIOR emp_id = mgr_id";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    parser.advance(); parser.advance();
    auto* stmt = parser.parse_connect_by();

    REQUIRE(stmt != nullptr);

    class TestGenerator : public CompleteSQLGenerator<TestGenerator> {
    public:
        using CompleteSQLGenerator::CompleteSQLGenerator;
        std::string generate(ConnectByClause* cb) {
            visit_connect_by(cb);
            return get_output();
        }
    };

    TestGenerator gen(arena, SQLDialect::Oracle);
    std::string result = gen.generate(stmt);

    REQUIRE(result.find("CONNECT BY") != std::string::npos);
    REQUIRE(result.find("NOCYCLE") != std::string::npos);
    REQUIRE(result.find("PRIOR") != std::string::npos);
}

TEST_CASE("CONNECT BY - START WITH generator", "[sql][start_with][generator]") {
    const char* sql = "START WITH department_id = 10";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    parser.advance(); parser.advance();
    auto* stmt = parser.parse_start_with();

    REQUIRE(stmt != nullptr);

    class TestGenerator : public CompleteSQLGenerator<TestGenerator> {
    public:
        using CompleteSQLGenerator::CompleteSQLGenerator;
        std::string generate(StartWithClause* sw) {
            visit_start_with(sw);
            return get_output();
        }
    };

    TestGenerator gen(arena, SQLDialect::Oracle);
    std::string result = gen.generate(stmt);

    REQUIRE(result.find("START WITH") != std::string::npos);
}

TEST_CASE("CONNECT BY - Multiple conditions", "[sql][connect_by][oracle]") {
    const char* sql = R"(
        SELECT *
        FROM employees
        START WITH job_id = 'CEO'
        CONNECT BY PRIOR employee_id = manager_id AND department_id = 10
    )";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    auto* stmt = parser.parse();

    REQUIRE(stmt != nullptr);
}

TEST_CASE("CONNECT BY - Using LEVEL pseudocolumn", "[sql][connect_by][oracle]") {
    const char* sql = R"(
        SELECT LPAD(' ', 2 * (LEVEL - 1)) || name AS hierarchy
        FROM categories
        START WITH parent_id IS NULL
        CONNECT BY PRIOR id = parent_id
        ORDER SIBLINGS BY name
    )";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    auto* stmt = parser.parse();

    REQUIRE(stmt != nullptr);
}

TEST_CASE("CONNECT BY - Self-join alternative pattern", "[sql][connect_by][oracle]") {
    const char* sql = R"(
        SELECT node_id, parent_node_id
        FROM tree_table
        START WITH parent_node_id IS NULL
        CONNECT BY PRIOR node_id = parent_node_id
    )";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    auto* stmt = parser.parse();

    REQUIRE(stmt != nullptr);
}

TEST_CASE("CONNECT BY - With WHERE clause", "[sql][connect_by][oracle]") {
    const char* sql = R"(
        SELECT employee_id, manager_id, salary
        FROM employees
        WHERE salary > 50000
        START WITH manager_id IS NULL
        CONNECT BY PRIOR employee_id = manager_id
    )";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    auto* stmt = parser.parse();

    REQUIRE(stmt != nullptr);
}

TEST_CASE("CONNECT BY - Complex START WITH condition", "[sql][start_with][oracle]") {
    const char* sql = "START WITH (status = 'active' AND created_date > '2020-01-01')";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    parser.advance(); parser.advance();
    auto* stmt = parser.parse_start_with();

    REQUIRE(stmt != nullptr);
}

TEST_CASE("CONNECT BY - Reverse hierarchy", "[sql][connect_by][oracle]") {
    const char* sql = R"(
        SELECT employee_id, manager_id
        FROM employees
        START WITH employee_id = 100
        CONNECT BY employee_id = PRIOR manager_id
    )";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    auto* stmt = parser.parse();

    REQUIRE(stmt != nullptr);
}

TEST_CASE("CONNECT BY - File system hierarchy example", "[sql][connect_by][oracle]") {
    const char* sql = R"(
        SELECT file_id, parent_file_id, file_name
        FROM file_system
        START WITH parent_file_id IS NULL
        CONNECT BY NOCYCLE PRIOR file_id = parent_file_id
        ORDER BY file_name
    )";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    auto* stmt = parser.parse();

    REQUIRE(stmt != nullptr);
}
