#include <catch2/catch_test_macros.hpp>
#include "libglot/sql/complete_features.h"
#include "libglot/core/arena.h"

using namespace libglot::sql;
using namespace libglot;
using TK = libsqlglot::TokenType;

TEST_CASE("OUTPUT - Basic INSERTED columns", "[sql][output][tsql]") {
    const char* sql = "OUTPUT INSERTED.id, INSERTED.name";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    parser.advance();
    auto* stmt = parser.parse_output_clause();

    REQUIRE(stmt != nullptr);
    REQUIRE(stmt->columns.size() == 2);
    REQUIRE(stmt->columns[0].first == OutputClause::Target::INSERTED);
    REQUIRE(stmt->columns[1].first == OutputClause::Target::INSERTED);
}

TEST_CASE("OUTPUT - DELETED columns", "[sql][output][tsql]") {
    const char* sql = "OUTPUT DELETED.id, DELETED.old_value";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    parser.advance();
    auto* stmt = parser.parse_output_clause();

    REQUIRE(stmt != nullptr);
    REQUIRE(stmt->columns.size() == 2);
    REQUIRE(stmt->columns[0].first == OutputClause::Target::DELETED);
    REQUIRE(stmt->columns[1].first == OutputClause::Target::DELETED);
}

TEST_CASE("OUTPUT - Mixed INSERTED and DELETED", "[sql][output][tsql]") {
    const char* sql = "OUTPUT INSERTED.new_val, DELETED.old_val";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    parser.advance();
    auto* stmt = parser.parse_output_clause();

    REQUIRE(stmt != nullptr);
    REQUIRE(stmt->columns.size() == 2);
    REQUIRE(stmt->columns[0].first == OutputClause::Target::INSERTED);
    REQUIRE(stmt->columns[1].first == OutputClause::Target::DELETED);
}

TEST_CASE("OUTPUT - With INTO table", "[sql][output][tsql]") {
    const char* sql = "OUTPUT INSERTED.id, INSERTED.name INTO @AuditTable";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    parser.advance();
    auto* stmt = parser.parse_output_clause();

    REQUIRE(stmt != nullptr);
    REQUIRE(stmt->columns.size() == 2);
    REQUIRE(stmt->into_table != nullptr);
}

TEST_CASE("OUTPUT - In INSERT statement", "[sql][output][tsql][complete]") {
    const char* sql = R"(
        INSERT INTO employees (name, salary)
        OUTPUT INSERTED.id, INSERTED.name, INSERTED.salary
        VALUES ('John Doe', 50000)
    )";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    auto* stmt = parser.parse();

    REQUIRE(stmt != nullptr);
}

TEST_CASE("OUTPUT - In UPDATE statement", "[sql][output][tsql][complete]") {
    const char* sql = R"(
        UPDATE employees
        SET salary = salary * 1.1
        OUTPUT INSERTED.id, INSERTED.salary, DELETED.salary
        WHERE department = 'Sales'
    )";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    auto* stmt = parser.parse();

    REQUIRE(stmt != nullptr);
}

TEST_CASE("OUTPUT - In DELETE statement", "[sql][output][tsql][complete]") {
    const char* sql = R"(
        DELETE FROM employees
        OUTPUT DELETED.id, DELETED.name, DELETED.salary
        WHERE termination_date < '2020-01-01'
    )";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    auto* stmt = parser.parse();

    REQUIRE(stmt != nullptr);
}

TEST_CASE("OUTPUT - Generator output", "[sql][output][generator]") {
    const char* sql = "OUTPUT INSERTED.id, DELETED.old_value INTO @log";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    parser.advance();
    auto* stmt = parser.parse_output_clause();

    REQUIRE(stmt != nullptr);

    class TestGenerator : public CompleteSQLGenerator<TestGenerator> {
    public:
        using CompleteSQLGenerator::CompleteSQLGenerator;
        std::string generate(OutputClause* output) {
            visit_output_clause(output);
            return get_output();
        }
    };

    TestGenerator gen(arena, SQLDialect::TSQL);
    std::string result = gen.generate(stmt);

    REQUIRE(result.find("OUTPUT") != std::string::npos);
    REQUIRE(result.find("INSERTED") != std::string::npos);
    REQUIRE(result.find("DELETED") != std::string::npos);
    REQUIRE(result.find("INTO") != std::string::npos);
}

TEST_CASE("OUTPUT - Single column", "[sql][output][tsql]") {
    const char* sql = "OUTPUT INSERTED.created_at";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    parser.advance();
    auto* stmt = parser.parse_output_clause();

    REQUIRE(stmt != nullptr);
    REQUIRE(stmt->columns.size() == 1);
}

TEST_CASE("OUTPUT - Many columns", "[sql][output][tsql]") {
    const char* sql = "OUTPUT INSERTED.id, INSERTED.name, INSERTED.email, INSERTED.phone, INSERTED.address";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    parser.advance();
    auto* stmt = parser.parse_output_clause();

    REQUIRE(stmt != nullptr);
    REQUIRE(stmt->columns.size() == 5);
}

TEST_CASE("OUTPUT - With table variable", "[sql][output][tsql]") {
    const char* sql = R"(
        DECLARE @MyTableVar TABLE (id INT, name VARCHAR(50));

        INSERT INTO employees (name, department)
        OUTPUT INSERTED.id, INSERTED.name INTO @MyTableVar
        VALUES ('Jane Smith', 'IT');
    )";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    auto* stmt = parser.parse();

    REQUIRE(stmt != nullptr);
}

TEST_CASE("OUTPUT - In MERGE statement", "[sql][output][tsql][complete]") {
    const char* sql = R"(
        MERGE INTO target AS t
        USING source AS s ON t.id = s.id
        WHEN MATCHED THEN UPDATE SET t.value = s.value
        WHEN NOT MATCHED THEN INSERT (id, value) VALUES (s.id, s.value)
        OUTPUT INSERTED.id, INSERTED.value, DELETED.value;
    )";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    auto* stmt = parser.parse();

    REQUIRE(stmt != nullptr);
}

TEST_CASE("OUTPUT - Audit trail pattern", "[sql][output][tsql]") {
    const char* sql = R"(
        UPDATE inventory
        SET quantity = quantity - 10
        OUTPUT
            INSERTED.product_id,
            DELETED.quantity AS old_qty,
            INSERTED.quantity AS new_qty,
            GETDATE() AS change_date
        INTO inventory_audit
        WHERE product_id = 123
    )";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    auto* stmt = parser.parse();

    REQUIRE(stmt != nullptr);
}

TEST_CASE("OUTPUT - Without INTO clause", "[sql][output][tsql]") {
    const char* sql = R"(
        DELETE FROM old_records
        OUTPUT DELETED.*
        WHERE created_date < '2015-01-01'
    )";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    auto* stmt = parser.parse();

    REQUIRE(stmt != nullptr);
}

TEST_CASE("OUTPUT - Temp table destination", "[sql][output][tsql]") {
    const char* sql = R"(
        INSERT INTO employees (name)
        OUTPUT INSERTED.id, INSERTED.name INTO #temp_employees
        SELECT name FROM staging_employees
    )";

    Arena arena;
    CompleteSQLParser parser(arena, sql);
    auto* stmt = parser.parse();

    REQUIRE(stmt != nullptr);
}
