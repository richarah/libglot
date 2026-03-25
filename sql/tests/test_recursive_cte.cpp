#include <catch2/catch_test_macros.hpp>
#include <libglot/sql/parser.h>
#include <libglot/sql/generator.h>

using namespace libglot::sql;

static std::string test_round_trip(const std::string& sql, SQLDialect dialect = SQLDialect::PostgreSQL) {
    libglot::Arena arena;
    SQLParser parser(arena, sql);
    auto ast = parser.parse_top_level();
    SQLGenerator gen(dialect);
    return gen.generate(ast);
}

TEST_CASE("Recursive CTE - Basic hierarchy", "[cte][recursive][postgresql]") {
    SECTION("Simple recursive CTE") {
        std::string sql = R"(
            WITH RECURSIVE t(n) AS (
                SELECT 1
                UNION ALL
                SELECT n + 1 FROM t WHERE n < 10
            )
            SELECT * FROM t
        )";

        std::string result = test_round_trip(sql);
        REQUIRE(result.find("WITH RECURSIVE") != std::string::npos);
        REQUIRE(result.find("UNION ALL") != std::string::npos);
    }

    SECTION("Employee hierarchy recursive CTE") {
        std::string sql = R"(
            WITH RECURSIVE employee_hierarchy AS (
                SELECT id, name, manager_id, 1 AS level
                FROM employees
                WHERE manager_id IS NULL
                UNION ALL
                SELECT e.id, e.name, e.manager_id, eh.level + 1
                FROM employees e
                JOIN employee_hierarchy eh ON e.manager_id = eh.id
            )
            SELECT * FROM employee_hierarchy ORDER BY level
        )";

        std::string result = test_round_trip(sql);
        REQUIRE(result.find("WITH RECURSIVE") != std::string::npos);
        REQUIRE(result.find("JOIN") != std::string::npos);
    }
}

TEST_CASE("Recursive CTE - MySQL 8.0+", "[cte][recursive][mysql]") {
    SECTION("Basic recursive CTE in MySQL") {
        std::string sql = R"(
            WITH RECURSIVE cte AS (
                SELECT 1 AS n
                UNION ALL
                SELECT n + 1 FROM cte WHERE n < 5
            )
            SELECT * FROM cte
        )";

        std::string result = test_round_trip(sql, SQLDialect::MySQL);
        REQUIRE(result.find("WITH RECURSIVE") != std::string::npos);
    }
}

TEST_CASE("Recursive CTE - SQL Server", "[cte][recursive][sqlserver]") {
    SECTION("SQL Server recursive CTE syntax") {
        std::string sql = R"(
            WITH employee_cte AS (
                SELECT id, name, manager_id, 0 AS level
                FROM employees
                WHERE manager_id IS NULL
                UNION ALL
                SELECT e.id, e.name, e.manager_id, ec.level + 1
                FROM employees e
                INNER JOIN employee_cte ec ON e.manager_id = ec.id
            )
            SELECT * FROM employee_cte
        )";

        std::string result = test_round_trip(sql, SQLDialect::SQLServer);
        // SQL Server doesn't use RECURSIVE keyword, just WITH
        REQUIRE(result.find("WITH") != std::string::npos);
        REQUIRE(result.find("UNION ALL") != std::string::npos);
    }
}

TEST_CASE("Recursive CTE - Oracle", "[cte][recursive][oracle]") {
    SECTION("Oracle CONNECT BY alternative using CTE") {
        std::string sql = R"(
            WITH org_chart AS (
                SELECT employee_id, name, manager_id, 1 AS lvl
                FROM employees
                WHERE manager_id IS NULL
                UNION ALL
                SELECT e.employee_id, e.name, e.manager_id, oc.lvl + 1
                FROM employees e
                JOIN org_chart oc ON e.manager_id = oc.employee_id
            )
            SELECT * FROM org_chart
        )";

        std::string result = test_round_trip(sql, SQLDialect::Oracle);
        REQUIRE(result.find("WITH") != std::string::npos);
    }
}

TEST_CASE("Recursive CTE - Advanced patterns", "[cte][recursive][advanced]") {
    SECTION("Bill of materials (BOM) explosion") {
        std::string sql = R"(
            WITH RECURSIVE bom AS (
                SELECT part_id, component_id, quantity, 1 AS level
                FROM parts
                WHERE part_id = 100
                UNION ALL
                SELECT p.part_id, p.component_id, p.quantity * b.quantity, b.level + 1
                FROM parts p
                JOIN bom b ON p.part_id = b.component_id
            )
            SELECT * FROM bom
        )";

        std::string result = test_round_trip(sql);
        REQUIRE(result.find("WITH RECURSIVE") != std::string::npos);
        REQUIRE(result.find("level + 1") != std::string::npos);
    }

    SECTION("Graph traversal - finding all paths") {
        std::string sql = R"(
            WITH RECURSIVE paths AS (
                SELECT node_id, ARRAY[node_id] AS path
                FROM nodes
                WHERE node_id = 1
                UNION ALL
                SELECT e.target_id, p.path || e.target_id
                FROM edges e
                JOIN paths p ON e.source_id = p.node_id
                WHERE NOT e.target_id = ANY(p.path)
            )
            SELECT * FROM paths
        )";

        std::string result = test_round_trip(sql);
        REQUIRE(result.find("WITH RECURSIVE") != std::string::npos);
    }

    SECTION("Calendar generation with recursive CTE") {
        std::string sql = R"(
            WITH RECURSIVE dates AS (
                SELECT DATE '2024-01-01' AS d
                UNION ALL
                SELECT d + INTERVAL '1 day'
                FROM dates
                WHERE d < DATE '2024-12-31'
            )
            SELECT * FROM dates
        )";

        std::string result = test_round_trip(sql);
        REQUIRE(result.find("WITH RECURSIVE") != std::string::npos);
        REQUIRE(result.find("INTERVAL") != std::string::npos);
    }
}

TEST_CASE("Recursive CTE - Multiple CTEs", "[cte][recursive][multiple]") {
    SECTION("Mix of recursive and non-recursive CTEs") {
        std::string sql = R"(
            WITH
                active_users AS (
                    SELECT id, name FROM users WHERE status = 'active'
                ),
                RECURSIVE user_hierarchy AS (
                    SELECT id, name, parent_id, 1 AS level
                    FROM active_users
                    WHERE parent_id IS NULL
                    UNION ALL
                    SELECT u.id, u.name, u.parent_id, uh.level + 1
                    FROM active_users u
                    JOIN user_hierarchy uh ON u.parent_id = uh.id
                )
            SELECT * FROM user_hierarchy
        )";

        std::string result = test_round_trip(sql);
        REQUIRE(result.find("WITH") != std::string::npos);
        REQUIRE(result.find("RECURSIVE") != std::string::npos);
    }
}

TEST_CASE("Recursive CTE - Depth limiting", "[cte][recursive][limits]") {
    SECTION("Limit recursion depth") {
        std::string sql = R"(
            WITH RECURSIVE hierarchy AS (
                SELECT id, name, parent_id, 1 AS depth
                FROM categories
                WHERE parent_id IS NULL
                UNION ALL
                SELECT c.id, c.name, c.parent_id, h.depth + 1
                FROM categories c
                JOIN hierarchy h ON c.parent_id = h.id
                WHERE h.depth < 10
            )
            SELECT * FROM hierarchy
        )";

        std::string result = test_round_trip(sql);
        REQUIRE(result.find("WITH RECURSIVE") != std::string::npos);
        REQUIRE(result.find("depth < 10") != std::string::npos);
    }
}

TEST_CASE("Recursive CTE - Fibonacci sequence", "[cte][recursive][fibonacci]") {
    SECTION("Generate Fibonacci numbers") {
        std::string sql = R"(
            WITH RECURSIVE fibonacci AS (
                SELECT 1 AS n, 0 AS fib, 1 AS next_fib
                UNION ALL
                SELECT n + 1, next_fib, fib + next_fib
                FROM fibonacci
                WHERE n < 20
            )
            SELECT n, fib FROM fibonacci
        )";

        std::string result = test_round_trip(sql);
        REQUIRE(result.find("WITH RECURSIVE") != std::string::npos);
    }
}

TEST_CASE("Recursive CTE - BigQuery", "[cte][recursive][bigquery]") {
    SECTION("BigQuery recursive CTE") {
        std::string sql = R"(
            WITH RECURSIVE T1 AS (
                SELECT 1 AS n
                UNION ALL
                SELECT n + 1 FROM T1 WHERE n < 3
            )
            SELECT * FROM T1
        )";

        std::string result = test_round_trip(sql, SQLDialect::BigQuery);
        REQUIRE(result.find("WITH RECURSIVE") != std::string::npos);
    }
}

TEST_CASE("Recursive CTE - Snowflake", "[cte][recursive][snowflake]") {
    SECTION("Snowflake recursive CTE") {
        std::string sql = R"(
            WITH RECURSIVE manager_hierarchy AS (
                SELECT employee_id, name, manager_id
                FROM employees
                WHERE manager_id IS NULL
                UNION ALL
                SELECT e.employee_id, e.name, e.manager_id
                FROM employees e
                INNER JOIN manager_hierarchy mh ON e.manager_id = mh.employee_id
            )
            SELECT * FROM manager_hierarchy
        )";

        std::string result = test_round_trip(sql, SQLDialect::Snowflake);
        REQUIRE(result.find("WITH RECURSIVE") != std::string::npos);
    }
}
