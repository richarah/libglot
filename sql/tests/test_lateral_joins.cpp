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

TEST_CASE("LATERAL joins - PostgreSQL", "[lateral][postgresql]") {
    SECTION("Basic LATERAL subquery") {
        std::string sql = R"(
            SELECT u.*, l.order_total
            FROM users u,
            LATERAL (SELECT SUM(amount) AS order_total FROM orders WHERE user_id = u.id) l
        )";

        std::string result = test_round_trip(sql);
        REQUIRE(result.find("LATERAL") != std::string::npos);
    }

    SECTION("LATERAL with LEFT JOIN") {
        std::string sql = R"(
            SELECT u.name, recent_orders.order_count
            FROM users u
            LEFT JOIN LATERAL (
                SELECT COUNT(*) AS order_count
                FROM orders
                WHERE user_id = u.id AND created_at > NOW() - INTERVAL '30 days'
            ) recent_orders ON true
        )";

        std::string result = test_round_trip(sql);
        REQUIRE(result.find("LATERAL") != std::string::npos);
        REQUIRE(result.find("LEFT JOIN") != std::string::npos);
    }

    SECTION("LATERAL with function call") {
        std::string sql = R"(
            SELECT t.*, g.val
            FROM generate_series(1, 10) t,
            LATERAL generate_series(1, t) g(val)
        )";

        std::string result = test_round_trip(sql);
        REQUIRE(result.find("LATERAL") != std::string::npos);
    }

    SECTION("LATERAL for top N per group") {
        std::string sql = R"(
            SELECT c.category_name, top_products.*
            FROM categories c,
            LATERAL (
                SELECT product_name, price
                FROM products p
                WHERE p.category_id = c.id
                ORDER BY price DESC
                LIMIT 5
            ) top_products
        )";

        std::string result = test_round_trip(sql);
        REQUIRE(result.find("LATERAL") != std::string::npos);
        REQUIRE(result.find("LIMIT") != std::string::npos);
    }
}

TEST_CASE("LATERAL joins - Advanced patterns", "[lateral][advanced]") {
    SECTION("Multiple LATERAL subqueries") {
        std::string sql = R"(
            SELECT u.name, order_stats.total, product_stats.count
            FROM users u,
            LATERAL (SELECT SUM(amount) AS total FROM orders WHERE user_id = u.id) order_stats,
            LATERAL (SELECT COUNT(*) AS count FROM order_items oi JOIN orders o ON oi.order_id = o.id WHERE o.user_id = u.id) product_stats
        )";

        std::string result = test_round_trip(sql);
        REQUIRE(result.find("LATERAL") != std::string::npos);
    }

    SECTION("LATERAL with UNNEST") {
        std::string sql = R"(
            SELECT t.id, elem
            FROM tables t,
            LATERAL UNNEST(t.array_column) AS elem
        )";

        std::string result = test_round_trip(sql);
        REQUIRE(result.find("LATERAL") != std::string::npos);
        REQUIRE(result.find("UNNEST") != std::string::npos);
    }

    SECTION("LATERAL for window function alternatives") {
        std::string sql = R"(
            SELECT u.*, running_total.total
            FROM users u
            LEFT JOIN LATERAL (
                SELECT SUM(amount) AS total
                FROM orders o
                WHERE o.user_id = u.id AND o.created_at <= u.created_at
            ) running_total ON true
        )";

        std::string result = test_round_trip(sql);
        REQUIRE(result.find("LATERAL") != std::string::npos);
    }
}

TEST_CASE("LATERAL joins - SQL Server CROSS APPLY", "[lateral][sqlserver]") {
    SECTION("CROSS APPLY equivalent to LATERAL") {
        std::string sql = R"(
            SELECT u.name, recent.order_count
            FROM users u
            CROSS APPLY (
                SELECT COUNT(*) AS order_count
                FROM orders
                WHERE user_id = u.id
            ) recent
        )";

        std::string result = test_round_trip(sql, SQLDialect::SQLServer);
        REQUIRE(result.find("CROSS APPLY") != std::string::npos);
    }

    SECTION("OUTER APPLY equivalent to LEFT JOIN LATERAL") {
        std::string sql = R"(
            SELECT u.name, latest.order_date
            FROM users u
            OUTER APPLY (
                SELECT TOP 1 order_date
                FROM orders
                WHERE user_id = u.id
                ORDER BY order_date DESC
            ) latest
        )";

        std::string result = test_round_trip(sql, SQLDialect::SQLServer);
        REQUIRE(result.find("OUTER APPLY") != std::string::npos);
    }
}

TEST_CASE("LATERAL joins - Oracle", "[lateral][oracle]") {
    SECTION("LATERAL keyword in Oracle 12c+") {
        std::string sql = R"(
            SELECT d.department_name, emp_count.cnt
            FROM departments d,
            LATERAL (SELECT COUNT(*) AS cnt FROM employees WHERE department_id = d.department_id) emp_count
        )";

        std::string result = test_round_trip(sql, SQLDialect::Oracle);
        REQUIRE(result.find("LATERAL") != std::string::npos);
    }
}

TEST_CASE("LATERAL joins - Use cases", "[lateral][usecases]") {
    SECTION("Get latest record per group") {
        std::string sql = R"(
            SELECT customers.*, latest_order.order_date
            FROM customers
            LEFT JOIN LATERAL (
                SELECT order_date, amount
                FROM orders
                WHERE customer_id = customers.id
                ORDER BY order_date DESC
                LIMIT 1
            ) latest_order ON true
        )";

        std::string result = test_round_trip(sql);
        REQUIRE(result.find("LATERAL") != std::string::npos);
    }

    SECTION("Array/JSON processing") {
        std::string sql = R"(
            SELECT doc.id, element
            FROM documents doc,
            LATERAL jsonb_array_elements(doc.tags) AS element
        )";

        std::string result = test_round_trip(sql);
        REQUIRE(result.find("LATERAL") != std::string::npos);
    }

    SECTION("Correlated subquery replacement") {
        std::string sql = R"(
            SELECT p.product_name, avg_price.avg
            FROM products p,
            LATERAL (
                SELECT AVG(price) AS avg
                FROM products
                WHERE category = p.category AND id != p.id
            ) avg_price
        )";

        std::string result = test_round_trip(sql);
        REQUIRE(result.find("LATERAL") != std::string::npos);
    }

    SECTION("Statistical functions per row") {
        std::string sql = R"(
            SELECT students.name, percentile.rank
            FROM students,
            LATERAL (
                SELECT PERCENT_RANK() OVER (ORDER BY score) AS rank
                FROM test_scores
                WHERE student_id = students.id
            ) percentile
        )";

        std::string result = test_round_trip(sql);
        REQUIRE(result.find("LATERAL") != std::string::npos);
        REQUIRE(result.find("PERCENT_RANK") != std::string::npos);
    }
}

TEST_CASE("LATERAL joins - Complex scenarios", "[lateral][complex]") {
    SECTION("Nested LATERAL subqueries") {
        std::string sql = R"(
            SELECT c.name, order_details.*
            FROM customers c,
            LATERAL (
                SELECT o.id, item_details.*
                FROM orders o,
                LATERAL (
                    SELECT product_name, quantity
                    FROM order_items
                    WHERE order_id = o.id
                ) item_details
                WHERE o.customer_id = c.id
            ) order_details
        )";

        std::string result = test_round_trip(sql);
        REQUIRE(result.find("LATERAL") != std::string::npos);
    }

    SECTION("LATERAL with CTEs") {
        std::string sql = R"(
            WITH active_users AS (
                SELECT id, name FROM users WHERE status = 'active'
            )
            SELECT au.name, recent.*
            FROM active_users au,
            LATERAL (
                SELECT order_date, amount
                FROM orders
                WHERE user_id = au.id
                ORDER BY order_date DESC
                LIMIT 3
            ) recent
        )";

        std::string result = test_round_trip(sql);
        REQUIRE(result.find("WITH") != std::string::npos);
        REQUIRE(result.find("LATERAL") != std::string::npos);
    }
}
