#include <catch2/catch_test_macros.hpp>
#include <libglot/sql/parser.h>
#include <libglot/sql/generator.h>

using namespace libglot::sql;

static std::string test_round_trip(const std::string& sql, SQLDialect dialect = SQLDialect::PostgreSQL) {
    libglot::Arena arena;
    SQLParser parser(arena, sql, dialect);  // Pass dialect to parser
    auto ast = parser.parse_top_level();
    SQLGenerator gen(dialect);
    return gen.generate(ast);
}

TEST_CASE("JSON operations - PostgreSQL", "[json][postgresql]") {
    SECTION("JSON arrow operator (->)") {
        std::string sql = "SELECT data -> 'name' FROM users";
        std::string result = test_round_trip(sql);
        REQUIRE(result.find("->") != std::string::npos);
        REQUIRE(result.find("name") != std::string::npos);
    }

    SECTION("JSON double arrow operator (->>)") {
        std::string sql = "SELECT data ->> 'name' FROM users";
        std::string result = test_round_trip(sql);
        REQUIRE(result.find("->>") != std::string::npos);
    }

    SECTION("JSON path operator (#>)") {
        std::string sql = "SELECT data #> '{address,city}' FROM users";
        std::string result = test_round_trip(sql);
        INFO("Generated: " << result);
        REQUIRE(result.find("#>") != std::string::npos);
    }

    SECTION("JSON path text operator (#>>)") {
        std::string sql = "SELECT data #>> '{address,city}' FROM users";
        std::string result = test_round_trip(sql);
        REQUIRE(result.find("#>>") != std::string::npos);
    }

    SECTION("JSON containment (@>)") {
        std::string sql = "SELECT * FROM users WHERE data @> '{\"name\":\"Alice\"}'";
        std::string result = test_round_trip(sql);
        REQUIRE(result.find("@>") != std::string::npos);
    }

    SECTION("JSON contained by (<@)") {
        std::string sql = "SELECT * FROM users WHERE data <@ '{\"name\":\"Alice\"}'";
        std::string result = test_round_trip(sql);
        REQUIRE(result.find("<@") != std::string::npos);
    }

    SECTION("JSON key exists (?)") {
        std::string sql = "SELECT * FROM users WHERE data ? 'name'";
        std::string result = test_round_trip(sql);
        REQUIRE(result.find("?") != std::string::npos);
    }
}

TEST_CASE("JSON functions - PostgreSQL", "[json][functions][postgresql]") {
    SECTION("JSON_EXTRACT_PATH") {
        std::string sql = "SELECT JSON_EXTRACT_PATH(data, 'name') FROM users";
        std::string result = test_round_trip(sql);
        REQUIRE(result.find("JSON_EXTRACT_PATH") != std::string::npos);
    }

    SECTION("JSON_OBJECT") {
        std::string sql = "SELECT JSON_OBJECT('name', 'Alice', 'age', 30)";
        std::string result = test_round_trip(sql);
        REQUIRE(result.find("JSON_OBJECT") != std::string::npos);
    }

    SECTION("JSON_ARRAY") {
        std::string sql = "SELECT JSON_ARRAY(1, 2, 3, 'hello')";
        std::string result = test_round_trip(sql);
        REQUIRE(result.find("JSON_ARRAY") != std::string::npos);
    }

    SECTION("JSON_AGG aggregate function") {
        std::string sql = "SELECT JSON_AGG(name) FROM users";
        std::string result = test_round_trip(sql);
        REQUIRE(result.find("JSON_AGG") != std::string::npos);
    }

    SECTION("JSON_OBJECTAGG aggregate function") {
        std::string sql = "SELECT JSON_OBJECTAGG(name, value) FROM settings";
        INFO("Input SQL: " << sql);
        std::string result = test_round_trip(sql);
        INFO("Generated SQL: " << result);
        REQUIRE(result.find("JSON_OBJECTAGG") != std::string::npos);
    }

    SECTION("JSONB_PATH_QUERY") {
        std::string sql = "SELECT JSONB_PATH_QUERY(data, '$.items[*].price') FROM orders";
        std::string result = test_round_trip(sql);
        REQUIRE(result.find("JSONB_PATH_QUERY") != std::string::npos);
    }
}

TEST_CASE("JSON operations - MySQL", "[json][mysql]") {
    SECTION("JSON_EXTRACT with path") {
        std::string sql = "SELECT JSON_EXTRACT(data, '$.name') FROM users";
        std::string result = test_round_trip(sql, SQLDialect::MySQL);
        REQUIRE(result.find("JSON_EXTRACT") != std::string::npos);
    }

    SECTION("JSON_EXTRACT with -> operator") {
        std::string sql = "SELECT data -> '$.name' FROM users";
        std::string result = test_round_trip(sql, SQLDialect::MySQL);
        REQUIRE(result.find("->") != std::string::npos);
    }

    SECTION("JSON_UNQUOTE with ->> operator") {
        std::string sql = "SELECT data ->> '$.name' FROM users";
        std::string result = test_round_trip(sql, SQLDialect::MySQL);
        REQUIRE(result.find("->>") != std::string::npos);
    }

    SECTION("JSON_SET") {
        std::string sql = "UPDATE users SET data = JSON_SET(data, '$.age', 31) WHERE id = 1";
        std::string result = test_round_trip(sql, SQLDialect::MySQL);
        REQUIRE(result.find("JSON_SET") != std::string::npos);
    }

    SECTION("JSON_ARRAY") {
        std::string sql = "SELECT JSON_ARRAY(1, 2, 3)";
        std::string result = test_round_trip(sql, SQLDialect::MySQL);
        REQUIRE(result.find("JSON_ARRAY") != std::string::npos);
    }

    SECTION("JSON_OBJECT") {
        std::string sql = "SELECT JSON_OBJECT('name', 'Alice', 'age', 30)";
        std::string result = test_round_trip(sql, SQLDialect::MySQL);
        REQUIRE(result.find("JSON_OBJECT") != std::string::npos);
    }

    SECTION("JSON_CONTAINS") {
        std::string sql = "SELECT * FROM users WHERE JSON_CONTAINS(tags, '\"admin\"')";
        std::string result = test_round_trip(sql, SQLDialect::MySQL);
        REQUIRE(result.find("JSON_CONTAINS") != std::string::npos);
    }
}

TEST_CASE("JSON operations - BigQuery", "[json][bigquery]") {
    SECTION("JSON_EXTRACT") {
        std::string sql = "SELECT JSON_EXTRACT(data, '$.name') FROM users";
        std::string result = test_round_trip(sql, SQLDialect::BigQuery);
        REQUIRE(result.find("JSON_EXTRACT") != std::string::npos);
    }

    SECTION("JSON_EXTRACT_SCALAR") {
        std::string sql = "SELECT JSON_EXTRACT_SCALAR(data, '$.name') FROM users";
        std::string result = test_round_trip(sql, SQLDialect::BigQuery);
        REQUIRE(result.find("JSON_EXTRACT_SCALAR") != std::string::npos);
    }

    SECTION("JSON_VALUE") {
        std::string sql = "SELECT JSON_VALUE(data, '$.name') FROM users";
        std::string result = test_round_trip(sql, SQLDialect::BigQuery);
        REQUIRE(result.find("JSON_VALUE") != std::string::npos);
    }

    SECTION("JSON_QUERY") {
        std::string sql = "SELECT JSON_QUERY(data, '$.address') FROM users";
        std::string result = test_round_trip(sql, SQLDialect::BigQuery);
        REQUIRE(result.find("JSON_QUERY") != std::string::npos);
    }

    SECTION("TO_JSON_STRING") {
        std::string sql = "SELECT TO_JSON_STRING(STRUCT(1 AS id, 'Alice' AS name))";
        std::string result = test_round_trip(sql, SQLDialect::BigQuery);
        REQUIRE(result.find("TO_JSON_STRING") != std::string::npos);
    }
}

TEST_CASE("JSON operations - Snowflake", "[json][snowflake]") {
    SECTION("Colon notation for JSON access") {
        std::string sql = "SELECT data:name FROM users";
        std::string result = test_round_trip(sql, SQLDialect::Snowflake);
        INFO("Input: " << sql);
        INFO("Output: " << result);
        REQUIRE(result.find(":") != std::string::npos);
    }

    SECTION("Bracket notation for JSON arrays") {
        std::string sql = "SELECT data:items[0] FROM orders";
        std::string result = test_round_trip(sql, SQLDialect::Snowflake);
        INFO("Input: " << sql);
        INFO("Output: " << result);
        REQUIRE(result.find("[0]") != std::string::npos);
    }

    SECTION("PARSE_JSON function") {
        std::string sql = "SELECT PARSE_JSON('{\"name\":\"Alice\"}')";
        std::string result = test_round_trip(sql, SQLDialect::Snowflake);
        REQUIRE(result.find("PARSE_JSON") != std::string::npos);
    }

    SECTION("OBJECT_CONSTRUCT") {
        std::string sql = "SELECT OBJECT_CONSTRUCT('name', 'Alice', 'age', 30)";
        std::string result = test_round_trip(sql, SQLDialect::Snowflake);
        REQUIRE(result.find("OBJECT_CONSTRUCT") != std::string::npos);
    }

    SECTION("ARRAY_CONSTRUCT") {
        std::string sql = "SELECT ARRAY_CONSTRUCT(1, 2, 3)";
        std::string result = test_round_trip(sql, SQLDialect::Snowflake);
        REQUIRE(result.find("ARRAY_CONSTRUCT") != std::string::npos);
    }
}

TEST_CASE("JSON in WHERE clauses", "[json][filtering]") {
    SECTION("PostgreSQL - Filter by JSON field") {
        std::string sql = "SELECT * FROM users WHERE data ->> 'status' = 'active'";
        std::string result = test_round_trip(sql);
        REQUIRE(result.find("WHERE") != std::string::npos);
        REQUIRE(result.find("->>") != std::string::npos);
    }

    SECTION("MySQL - Filter by JSON path") {
        std::string sql = "SELECT * FROM users WHERE JSON_EXTRACT(data, '$.status') = 'active'";
        std::string result = test_round_trip(sql, SQLDialect::MySQL);
        REQUIRE(result.find("JSON_EXTRACT") != std::string::npos);
    }

    SECTION("BigQuery - Filter by JSON value") {
        std::string sql = "SELECT * FROM users WHERE JSON_VALUE(data, '$.status') = 'active'";
        std::string result = test_round_trip(sql, SQLDialect::BigQuery);
        REQUIRE(result.find("JSON_VALUE") != std::string::npos);
    }
}

TEST_CASE("JSON in JOINs", "[json][joins]") {
    SECTION("Join using JSON field") {
        std::string sql = "SELECT u.*, o.* FROM users u JOIN orders o ON u.data ->> 'id' = o.user_id";
        std::string result = test_round_trip(sql);
        REQUIRE(result.find("JOIN") != std::string::npos);
        REQUIRE(result.find("->>") != std::string::npos);
    }
}

TEST_CASE("Complex JSON queries", "[json][complex]") {
    SECTION("Nested JSON access") {
        std::string sql = "SELECT data #>> '{address,city}' AS city FROM users";
        std::string result = test_round_trip(sql);
        REQUIRE(result.find("#>>") != std::string::npos);
    }

    SECTION("JSON array element access") {
        std::string sql = "SELECT data -> 'items' -> 0 -> 'name' FROM orders";
        std::string result = test_round_trip(sql);
        REQUIRE(result.find("->") != std::string::npos);
    }

    SECTION("JSON aggregation") {
        std::string sql = "SELECT category, JSON_AGG(JSON_BUILD_OBJECT('id', id, 'name', name)) FROM products GROUP BY category";
        std::string result = test_round_trip(sql);
        REQUIRE(result.find("JSON_AGG") != std::string::npos);
        REQUIRE(result.find("GROUP BY") != std::string::npos);
    }
}
