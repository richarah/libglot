#include <catch2/catch_test_macros.hpp>
#include <libglot/sql/parser.h>
#include <libglot/sql/generator.h>
#include <libglot/sql/dialect_traits.h>
#include <libglot/util/arena.h>

using namespace libglot::sql;

// ============================================================================
// INSERT TESTS
// ============================================================================

TEST_CASE("INSERT - Simple VALUES", "[dml][insert]") {
    libglot::Arena arena;
    SQLParser parser(arena, "INSERT INTO users (name, email) VALUES ('Alice', 'alice@example.com')");

    auto stmt = static_cast<InsertStmt*>(parser.parse_top_level());

    REQUIRE(stmt != nullptr);
    REQUIRE(stmt->table != nullptr);
    REQUIRE(stmt->table->table == "users");
    REQUIRE(stmt->columns.size() == 2);
    REQUIRE(stmt->columns[0] == "name");
    REQUIRE(stmt->columns[1] == "email");
    REQUIRE(stmt->values.size() == 1);
    REQUIRE(stmt->values[0].size() == 2);

    SQLGenerator gen(SQLDialect::ANSI);
    std::string sql = gen.generate(stmt);
    REQUIRE(sql.find("INSERT INTO") != std::string::npos);
    REQUIRE(sql.find("users") != std::string::npos);
}

TEST_CASE("INSERT - Multiple rows", "[dml][insert]") {
    libglot::Arena arena;
    SQLParser parser(arena,
        "INSERT INTO users (name, age) VALUES ('Alice', 25), ('Bob', 30), ('Charlie', 35)");

    auto stmt = static_cast<InsertStmt*>(parser.parse_top_level());

    REQUIRE(stmt->values.size() == 3);
    REQUIRE(stmt->values[0].size() == 2);
    REQUIRE(stmt->values[1].size() == 2);
    REQUIRE(stmt->values[2].size() == 2);

    SQLGenerator gen(SQLDialect::ANSI);
    std::string sql = gen.generate(stmt);
    REQUIRE(sql.find("INSERT INTO") != std::string::npos);
}

TEST_CASE("INSERT - SELECT subquery", "[dml][insert]") {
    libglot::Arena arena;
    SQLParser parser(arena, "INSERT INTO users_backup SELECT * FROM users WHERE active = 1");

    auto stmt = static_cast<InsertStmt*>(parser.parse_top_level());

    REQUIRE(stmt->select_query != nullptr);
    REQUIRE(stmt->values.empty());

    SQLGenerator gen(SQLDialect::ANSI);
    std::string sql = gen.generate(stmt);
    REQUIRE(sql.find("INSERT INTO") != std::string::npos);
    REQUIRE(sql.find("SELECT") != std::string::npos);
}

TEST_CASE("INSERT - Without column list", "[dml][insert]") {
    libglot::Arena arena;
    SQLParser parser(arena, "INSERT INTO users VALUES (1, 'Alice', 'alice@example.com')");

    auto stmt = static_cast<InsertStmt*>(parser.parse_top_level());

    REQUIRE(stmt->columns.empty());
    REQUIRE(stmt->values.size() == 1);

    SQLGenerator gen(SQLDialect::ANSI);
    std::string sql = gen.generate(stmt);
    REQUIRE(sql.find("INSERT INTO") != std::string::npos);
}

// ============================================================================
// UPDATE TESTS
// ============================================================================

TEST_CASE("UPDATE - Simple SET", "[dml][update]") {
    libglot::Arena arena;
    SQLParser parser(arena, "UPDATE users SET name = 'Bob', age = 30 WHERE id = 1");

    auto stmt = static_cast<UpdateStmt*>(parser.parse_top_level());

    REQUIRE(stmt != nullptr);
    REQUIRE(stmt->table->table == "users");
    REQUIRE(stmt->assignments.size() == 2);
    REQUIRE(stmt->assignments[0].first == "name");
    REQUIRE(stmt->assignments[1].first == "age");
    REQUIRE(stmt->where != nullptr);

    SQLGenerator gen(SQLDialect::ANSI);
    std::string sql = gen.generate(stmt);
    REQUIRE(sql.find("UPDATE") != std::string::npos);
    REQUIRE(sql.find("SET") != std::string::npos);
}

TEST_CASE("UPDATE - Without WHERE", "[dml][update]") {
    libglot::Arena arena;
    SQLParser parser(arena, "UPDATE products SET price = 19.99");

    auto stmt = static_cast<UpdateStmt*>(parser.parse_top_level());

    REQUIRE(stmt->assignments.size() == 1);
    REQUIRE(stmt->where == nullptr);

    SQLGenerator gen(SQLDialect::ANSI);
    std::string sql = gen.generate(stmt);
    REQUIRE(sql.find("UPDATE") != std::string::npos);
}

TEST_CASE("UPDATE - With FROM clause (PostgreSQL)", "[dml][update]") {
    libglot::Arena arena;
    SQLParser parser(arena, "UPDATE orders SET status = 'shipped' FROM users WHERE orders.user_id = users.id");

    auto stmt = static_cast<UpdateStmt*>(parser.parse_top_level());

    REQUIRE(stmt->from != nullptr);
    REQUIRE(stmt->where != nullptr);

    SQLGenerator gen(SQLDialect::PostgreSQL);
    std::string sql = gen.generate(stmt);
    REQUIRE(!sql.empty());
}

// ============================================================================
// DELETE TESTS
// ============================================================================

TEST_CASE("DELETE - Simple WHERE", "[dml][delete]") {
    libglot::Arena arena;
    SQLParser parser(arena, "DELETE FROM users WHERE age < 18");

    auto stmt = static_cast<DeleteStmt*>(parser.parse_top_level());

    REQUIRE(stmt != nullptr);
    REQUIRE(stmt->table->table == "users");
    REQUIRE(stmt->where != nullptr);

    SQLGenerator gen(SQLDialect::ANSI);
    std::string sql = gen.generate(stmt);
    REQUIRE(sql.find("DELETE FROM") != std::string::npos);
}

TEST_CASE("DELETE - Without WHERE", "[dml][delete]") {
    libglot::Arena arena;
    SQLParser parser(arena, "DELETE FROM temp_table");

    auto stmt = static_cast<DeleteStmt*>(parser.parse_top_level());

    REQUIRE(stmt->where == nullptr);

    SQLGenerator gen(SQLDialect::ANSI);
    std::string sql = gen.generate(stmt);
    REQUIRE(sql.find("DELETE FROM") != std::string::npos);
}

TEST_CASE("DELETE - With USING clause", "[dml][delete]") {
    libglot::Arena arena;
    SQLParser parser(arena, "DELETE FROM orders USING users WHERE orders.user_id = users.id");

    auto stmt = static_cast<DeleteStmt*>(parser.parse_top_level());

    REQUIRE(stmt->using_clause != nullptr);
    REQUIRE(stmt->where != nullptr);

    SQLGenerator gen(SQLDialect::PostgreSQL);
    std::string sql = gen.generate(stmt);
    REQUIRE(!sql.empty());
}

// ============================================================================
// MERGE TESTS
// ============================================================================

TEST_CASE("MERGE - Basic UPSERT", "[dml][merge]") {
    libglot::Arena arena;
    std::string sql = R"(
        MERGE INTO target_table t
        USING source_table s ON t.id = s.id
        WHEN MATCHED THEN UPDATE SET t.value = s.value
        WHEN NOT MATCHED THEN INSERT (id, value) VALUES (s.id, s.value)
    )";
    SQLParser parser(arena, sql);

    auto stmt = static_cast<MergeStmt*>(parser.parse_top_level());

    REQUIRE(stmt != nullptr);
    REQUIRE(stmt->target != nullptr);
    REQUIRE(stmt->source != nullptr);
    REQUIRE(stmt->on_condition != nullptr);

    SQLGenerator gen(SQLDialect::ANSI);
    std::string output = gen.generate(stmt);
    REQUIRE(output.find("MERGE") != std::string::npos);
}

// ============================================================================
// TRUNCATE TESTS
// ============================================================================

TEST_CASE("TRUNCATE - Simple", "[dml][truncate]") {
    libglot::Arena arena;
    SQLParser parser(arena, "TRUNCATE TABLE users");

    auto stmt = static_cast<TruncateStmt*>(parser.parse_top_level());

    REQUIRE(stmt != nullptr);
    REQUIRE(stmt->table != nullptr);
    REQUIRE(stmt->table->table == "users");

    SQLGenerator gen(SQLDialect::ANSI);
    std::string sql = gen.generate(stmt);
    REQUIRE(sql.find("TRUNCATE") != std::string::npos);
}

TEST_CASE("TRUNCATE - Without TABLE keyword", "[dml][truncate]") {
    libglot::Arena arena;
    SQLParser parser(arena, "TRUNCATE users");

    auto stmt = static_cast<TruncateStmt*>(parser.parse_top_level());

    REQUIRE(stmt != nullptr);
    REQUIRE(stmt->table->table == "users");

    SQLGenerator gen(SQLDialect::ANSI);
    std::string sql = gen.generate(stmt);
    REQUIRE(sql.find("TRUNCATE") != std::string::npos);
}
