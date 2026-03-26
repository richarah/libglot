/// ============================================================================
/// Phase C1 Gate Condition Test: SQL Roundtrip
/// ============================================================================
///
/// This test validates that the libglot-core abstractions work correctly for
/// SQL by:
/// 1. Parsing a representative query using SQLParser
/// 2. Emitting it back using SQLGenerator (multiple dialects)
/// 3. Comparing the roundtrip result
///
/// Representative query:
///   SELECT col AS alias FROM table WHERE col = 1 ORDER BY col LIMIT 10
///
/// Success criteria:
/// - Parse completes without errors
/// - Emit completes without errors
/// - Roundtrip preserves semantics (allows for whitespace differences)
/// - Dialect transpilation works (parse in ANSI, emit in MySQL with backticks)
/// ============================================================================

#include <catch2/catch_test_macros.hpp>
#include "../include/libglot/sql/parser.h"
#include "../include/libglot/sql/generator.h"
#include "../include/libglot/sql/ast_nodes.h"
#include <string_view>
#include <string>

using namespace libglot::sql;

TEST_CASE("SQL Roundtrip: Parse and emit representative query", "[sql][roundtrip]") {
    constexpr std::string_view query = "SELECT col AS alias FROM table WHERE col = 1 ORDER BY col LIMIT 10";

    SECTION("Parse query successfully") {
        libglot::Arena arena;
        SQLParser parser(arena, query);

        // Parse should not throw
        REQUIRE_NOTHROW([&]() {
            auto* stmt = parser.parse_top_level();
            REQUIRE(stmt != nullptr);
            REQUIRE(stmt->type == SQLNodeKind::SELECT_STMT);

            auto* select = static_cast<SelectStmt*>(stmt);

            // Verify SELECT list (should have 1 item: col AS alias)
            REQUIRE(select->columns.size() == 1);
            REQUIRE(select->columns[0]->type == SQLNodeKind::ALIAS);

            auto* alias_node = static_cast<Alias*>(select->columns[0]);
            REQUIRE(alias_node->alias == "alias");
            REQUIRE(alias_node->expr->type == SQLNodeKind::COLUMN);

            auto* col = static_cast<Column*>(alias_node->expr);
            REQUIRE(col->column == "col");

            // Verify FROM clause
            REQUIRE(select->from != nullptr);
            REQUIRE(select->from->type == SQLNodeKind::TABLE_REF);
            auto* tbl = static_cast<TableRef*>(select->from);
            REQUIRE(tbl->table == "table");

            // Verify WHERE clause
            REQUIRE(select->where != nullptr);
            REQUIRE(select->where->type == SQLNodeKind::BINARY_OP);

            auto* where_op = static_cast<BinaryOp*>(select->where);
            REQUIRE(where_op->op == libsqlglot::TokenType::EQ);

            // Verify ORDER BY
            REQUIRE(select->order_by.size() == 1);
            REQUIRE(select->order_by[0]->expr != nullptr);
            REQUIRE(select->order_by[0]->ascending == true);

            // Verify LIMIT
            REQUIRE(select->limit != nullptr);
            REQUIRE(select->limit->type == SQLNodeKind::LITERAL);

            auto* limit_lit = static_cast<Literal*>(select->limit);
            REQUIRE(limit_lit->value == "10");
        }());
    }

    SECTION("Emit query in ANSI SQL dialect") {
        libglot::Arena arena;
        SQLParser parser(arena, query);
        auto* stmt = parser.parse_top_level();

        SQLGenerator gen(SQLDialect::ANSI);
        std::string output = gen.generate(stmt);

        // Verify output contains key elements (whitespace may differ)
        REQUIRE(output.find("SELECT") != std::string::npos);
        REQUIRE(output.find("\"col\"") != std::string::npos);  // ANSI uses double quotes
        REQUIRE(output.find("AS") != std::string::npos);
        REQUIRE(output.find("\"alias\"") != std::string::npos);
        REQUIRE(output.find("FROM") != std::string::npos);
        REQUIRE(output.find("\"table\"") != std::string::npos);
        REQUIRE(output.find("WHERE") != std::string::npos);
        REQUIRE(output.find("=") != std::string::npos);
        REQUIRE(output.find("1") != std::string::npos);
        REQUIRE(output.find("ORDER BY") != std::string::npos);
        REQUIRE(output.find("LIMIT") != std::string::npos);
        REQUIRE(output.find("10") != std::string::npos);
    }

    SECTION("Emit query in PostgreSQL dialect") {
        libglot::Arena arena;
        SQLParser parser(arena, query);
        auto* stmt = parser.parse_top_level();

        SQLGenerator gen(SQLDialect::PostgreSQL);
        std::string output = gen.generate(stmt);

        // PostgreSQL also uses double quotes
        REQUIRE(output.find("SELECT") != std::string::npos);
        REQUIRE(output.find("\"col\"") != std::string::npos);
        REQUIRE(output.find("\"table\"") != std::string::npos);
    }

    SECTION("Emit query in MySQL dialect (demonstrates transpilation)") {
        libglot::Arena arena;
        SQLParser parser(arena, query);
        auto* stmt = parser.parse_top_level();

        SQLGenerator gen(SQLDialect::MySQL);
        std::string output = gen.generate(stmt);

        // MySQL uses backticks for identifiers
        REQUIRE(output.find("SELECT") != std::string::npos);
        REQUIRE(output.find("`col`") != std::string::npos);
        REQUIRE(output.find("AS") != std::string::npos);
        REQUIRE(output.find("`alias`") != std::string::npos);
        REQUIRE(output.find("FROM") != std::string::npos);
        REQUIRE(output.find("`table`") != std::string::npos);
        REQUIRE(output.find("WHERE") != std::string::npos);
        REQUIRE(output.find("=") != std::string::npos);
        REQUIRE(output.find("1") != std::string::npos);
        REQUIRE(output.find("ORDER BY") != std::string::npos);
        REQUIRE(output.find("LIMIT") != std::string::npos);
        REQUIRE(output.find("10") != std::string::npos);
    }

    SECTION("Roundtrip: parse → emit → parse → compare") {
        libglot::Arena arena1;
        SQLParser parser1(arena1, query);
        auto* stmt1 = parser1.parse_top_level();

        // Emit
        SQLGenerator gen(SQLDialect::ANSI);
        std::string emitted = gen.generate(stmt1);

        // Parse emitted SQL
        libglot::Arena arena2;
        SQLParser parser2(arena2, emitted);
        auto* stmt2 = parser2.parse_top_level();

        // Both should be SELECT statements
        REQUIRE(stmt1->type == SQLNodeKind::SELECT_STMT);
        REQUIRE(stmt2->type == SQLNodeKind::SELECT_STMT);

        auto* select1 = static_cast<SelectStmt*>(stmt1);
        auto* select2 = static_cast<SelectStmt*>(stmt2);

        // Verify structural equality (simplified for Phase C1)
        REQUIRE(select1->columns.size() == select2->columns.size());
        REQUIRE(select1->order_by.size() == select2->order_by.size());
        REQUIRE((select1->from != nullptr) == (select2->from != nullptr));
        REQUIRE((select1->where != nullptr) == (select2->where != nullptr));
        REQUIRE((select1->limit != nullptr) == (select2->limit != nullptr));
    }
}

TEST_CASE("SQL Dialect Features: TRUE/FALSE literals", "[sql][dialect]") {
    constexpr std::string_view query = "SELECT TRUE, FALSE";

    SECTION("ANSI dialect uses TRUE/FALSE keywords") {
        libglot::Arena arena;
        SQLParser parser(arena, query);
        auto* stmt = parser.parse_top_level();

        SQLGenerator gen(SQLDialect::ANSI);
        std::string output = gen.generate(stmt);

        REQUIRE(output.find("TRUE") != std::string::npos);
        REQUIRE(output.find("FALSE") != std::string::npos);
    }

    SECTION("MySQL dialect uses 1/0 for boolean literals") {
        libglot::Arena arena;
        SQLParser parser(arena, query);
        auto* stmt = parser.parse_top_level();

        SQLGenerator gen(SQLDialect::MySQL);
        std::string output = gen.generate(stmt);

        REQUIRE(output.find("1") != std::string::npos);  // TRUE → 1
        REQUIRE(output.find("0") != std::string::npos);  // FALSE → 0
    }
}

TEST_CASE("SQL SQLParser: Error handling", "[sql][error]") {
    SECTION("Parse error on invalid syntax") {
        libglot::Arena arena;
        std::string_view bad_query = "SELECT FROM WHERE";  // Missing column list

        SQLParser parser(arena, bad_query);

        // Should throw ParseError
        REQUIRE_THROWS_AS(parser.parse_top_level(), libglot::ParseError);
    }

    SECTION("Parse error on unexpected token") {
        libglot::Arena arena;
        std::string_view bad_query = "SELECT col FROM";  // Missing table name

        SQLParser parser(arena, bad_query);
        REQUIRE_THROWS_AS(parser.parse_top_level(), libglot::ParseError);
    }
}

// ============================================================================
// DML Statement Tests (Ported from libsqlglot/tests/test_dml_statements.cpp)
// ============================================================================

TEST_CASE("INSERT - Simple VALUES", "[sql][dml][insert]") {
    libglot::Arena arena;
    SQLParser parser(arena, "INSERT INTO users (name, email) VALUES ('Alice', 'alice@example.com')");

    auto* stmt = parser.parse_top_level();

    REQUIRE(stmt != nullptr);
    REQUIRE(stmt->type == SQLNodeKind::INSERT_STMT);

    auto* insert = static_cast<InsertStmt*>(stmt);
    REQUIRE(insert->table != nullptr);
    REQUIRE(insert->table->table == "users");
    REQUIRE(insert->columns.size() == 2);
    REQUIRE(insert->columns[0] == "name");
    REQUIRE(insert->columns[1] == "email");
    REQUIRE(insert->values.size() == 1);
    REQUIRE(insert->values[0].size() == 2);

    SQLGenerator gen(SQLDialect::ANSI);
    std::string sql = gen.generate(stmt);
    REQUIRE(sql.find("INSERT INTO") != std::string::npos);
    REQUIRE(sql.find("\"users\"") != std::string::npos);
}

TEST_CASE("INSERT - Multiple rows", "[sql][dml][insert]") {
    libglot::Arena arena;
    SQLParser parser(arena,
        "INSERT INTO users (name, age) VALUES ('Alice', 25), ('Bob', 30), ('Charlie', 35)");

    auto* stmt = static_cast<InsertStmt*>(parser.parse_top_level());

    REQUIRE(stmt->values.size() == 3);
    REQUIRE(stmt->values[0].size() == 2);
    REQUIRE(stmt->values[1].size() == 2);
    REQUIRE(stmt->values[2].size() == 2);
}

TEST_CASE("INSERT - SELECT subquery", "[sql][dml][insert]") {
    libglot::Arena arena;
    SQLParser parser(arena, "INSERT INTO users_backup SELECT name, email FROM users WHERE active = 1");

    auto* stmt = static_cast<InsertStmt*>(parser.parse_top_level());

    REQUIRE(stmt->select_query != nullptr);
    REQUIRE(stmt->values.empty());
}

TEST_CASE("UPDATE - Simple SET", "[sql][dml][update]") {
    libglot::Arena arena;
    SQLParser parser(arena, "UPDATE users SET name = 'Bob', age = 30 WHERE id = 1");

    auto* stmt = static_cast<UpdateStmt*>(parser.parse_top_level());

    REQUIRE(stmt != nullptr);
    REQUIRE(stmt->table->table == "users");
    REQUIRE(stmt->assignments.size() == 2);
    REQUIRE(stmt->assignments[0].first == "name");
    REQUIRE(stmt->assignments[1].first == "age");
    REQUIRE(stmt->where != nullptr);
}

TEST_CASE("UPDATE - Without WHERE", "[sql][dml][update]") {
    libglot::Arena arena;
    SQLParser parser(arena, "UPDATE products SET price = 19.99");

    auto* stmt = static_cast<UpdateStmt*>(parser.parse_top_level());

    REQUIRE(stmt->assignments.size() == 1);
    REQUIRE(stmt->where == nullptr);
}

TEST_CASE("DELETE - Simple WHERE", "[sql][dml][delete]") {
    libglot::Arena arena;
    SQLParser parser(arena, "DELETE FROM users WHERE age < 18");

    auto* stmt = static_cast<DeleteStmt*>(parser.parse_top_level());

    REQUIRE(stmt != nullptr);
    REQUIRE(stmt->table->table == "users");
    REQUIRE(stmt->where != nullptr);
}

TEST_CASE("DELETE - Without WHERE", "[sql][dml][delete]") {
    libglot::Arena arena;
    SQLParser parser(arena, "DELETE FROM temp_table");

    auto* stmt = static_cast<DeleteStmt*>(parser.parse_top_level());

    REQUIRE(stmt->where == nullptr);
}

TEST_CASE("TRUNCATE - Simple table", "[sql][dml][truncate]") {
    libglot::Arena arena;
    SQLParser parser(arena, "TRUNCATE TABLE logs");

    auto* stmt = static_cast<TruncateStmt*>(parser.parse_top_level());

    REQUIRE(stmt != nullptr);
    REQUIRE(stmt->table->table == "logs");
}
