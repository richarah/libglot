// CREATE TABLE type and constraint fidelity.
//
// Verifies that column data types (including parameterized types like
// VARCHAR(255) and DECIMAL(10,2)) and column constraints (NOT NULL, DEFAULT,
// PRIMARY KEY, UNIQUE, REFERENCES, CHECK) survive a parse -> generate
// round-trip exactly, and that identifier quoting follows the target dialect.
//
// KNOWN BUG (not asserted here): DEFAULT CURRENT_TIMESTAMP is generated as
// the string literal DEFAULT 'CURRENT_TIMESTAMP', which is wrong SQL.
// Reported instead of enshrined.

#include <catch2/catch_test_macros.hpp>
#include <libglot/sql/parser.h>
#include <libglot/sql/generator.h>
#include <libglot/util/arena.h>

#include <string>

using namespace libglot::sql;

namespace {

std::string roundtrip(const std::string& sql, SQLDialect dialect = SQLDialect::ANSI) {
    libglot::Arena arena;
    SQLParser parser(arena, sql, dialect);
    auto ast = parser.parse_top_level();
    SQLGenerator gen(dialect);
    return gen.generate(ast);
}

CreateTableStmt* parse_create(libglot::Arena& arena, const std::string& sql) {
    SQLParser parser(arena, sql);
    auto ast = parser.parse_top_level();
    REQUIRE(ast->type == SQLNodeKind::CREATE_TABLE_STMT);
    return static_cast<CreateTableStmt*>(ast);
}

} // namespace

// ============================================================================
// Data type fidelity
// ============================================================================

TEST_CASE("Schema type - integer family", "[schema][types]") {
    REQUIRE(roundtrip("CREATE TABLE t (a INT, b BIGINT, c SMALLINT, d TINYINT)")
            == "CREATE TABLE \"t\" (\"a\" INT, \"b\" BIGINT, \"c\" SMALLINT, \"d\" TINYINT)");
}

TEST_CASE("Schema type - parameterized character types", "[schema][types]") {
    REQUIRE(roundtrip("CREATE TABLE t (name VARCHAR(255), code CHAR(1), body TEXT)")
            == "CREATE TABLE \"t\" (\"name\" VARCHAR(255), \"code\" CHAR(1), \"body\" TEXT)");
}

TEST_CASE("Schema type - numeric precision and scale", "[schema][types]") {
    REQUIRE(roundtrip("CREATE TABLE t (price DECIMAL(10,2), qty NUMERIC(5), r FLOAT, s REAL, d DOUBLE)")
            == "CREATE TABLE \"t\" (\"price\" DECIMAL(10,2), \"qty\" NUMERIC(5), \"r\" FLOAT, \"s\" REAL, \"d\" DOUBLE)");
}

TEST_CASE("Schema type - temporal and boolean types", "[schema][types]") {
    REQUIRE(roundtrip("CREATE TABLE t (ts TIMESTAMP, d DATE, flag BOOLEAN)")
            == "CREATE TABLE \"t\" (\"ts\" TIMESTAMP, \"d\" DATE, \"flag\" BOOLEAN)");
}

TEST_CASE("Schema type - AST records exact type text", "[schema][types][ast]") {
    libglot::Arena arena;
    auto* stmt = parse_create(arena,
        "CREATE TABLE t (id INT, name VARCHAR(255), price DECIMAL(10,2))");

    REQUIRE(stmt->columns.size() == 3);
    REQUIRE(stmt->columns[0]->name == "id");
    REQUIRE(stmt->columns[0]->type == "INT");
    REQUIRE(stmt->columns[1]->name == "name");
    REQUIRE(stmt->columns[1]->type == "VARCHAR(255)");
    REQUIRE(stmt->columns[2]->name == "price");
    REQUIRE(stmt->columns[2]->type == "DECIMAL(10,2)");
}

// ============================================================================
// Column constraints
// ============================================================================

TEST_CASE("Schema type - NOT NULL", "[schema][constraints]") {
    REQUIRE(roundtrip("CREATE TABLE t (id INT NOT NULL)")
            == "CREATE TABLE \"t\" (\"id\" INT NOT NULL)");

    libglot::Arena arena;
    auto* stmt = parse_create(arena, "CREATE TABLE t (id INT NOT NULL)");
    REQUIRE(stmt->columns[0]->not_null == true);
    REQUIRE(stmt->columns[0]->primary_key == false);
}

TEST_CASE("Schema type - PRIMARY KEY", "[schema][constraints]") {
    REQUIRE(roundtrip("CREATE TABLE t (id INT PRIMARY KEY)")
            == "CREATE TABLE \"t\" (\"id\" INT PRIMARY KEY)");

    libglot::Arena arena;
    auto* stmt = parse_create(arena, "CREATE TABLE t (id INT PRIMARY KEY)");
    REQUIRE(stmt->columns[0]->primary_key == true);
}

TEST_CASE("Schema type - UNIQUE", "[schema][constraints]") {
    REQUIRE(roundtrip("CREATE TABLE t (email VARCHAR(100) UNIQUE)")
            == "CREATE TABLE \"t\" (\"email\" VARCHAR(100) UNIQUE)");

    libglot::Arena arena;
    auto* stmt = parse_create(arena, "CREATE TABLE t (email VARCHAR(100) UNIQUE)");
    REQUIRE(stmt->columns[0]->unique == true);
}

TEST_CASE("Schema type - DEFAULT with numeric and string literals", "[schema][constraints]") {
    REQUIRE(roundtrip("CREATE TABLE t (n INT DEFAULT 0)")
            == "CREATE TABLE \"t\" (\"n\" INT DEFAULT 0)");
    REQUIRE(roundtrip("CREATE TABLE t (s VARCHAR(10) DEFAULT 'x')")
            == "CREATE TABLE \"t\" (\"s\" VARCHAR(10) DEFAULT 'x')");

    libglot::Arena arena;
    auto* stmt = parse_create(arena, "CREATE TABLE t (n INT DEFAULT 0)");
    REQUIRE(stmt->columns[0]->default_value != nullptr);
    REQUIRE(stmt->columns[0]->default_value->type == SQLNodeKind::LITERAL);
}

TEST_CASE("Schema type - REFERENCES with target column", "[schema][constraints]") {
    REQUIRE(roundtrip("CREATE TABLE t (uid INT REFERENCES users(id))")
            == "CREATE TABLE \"t\" (\"uid\" INT REFERENCES \"users\" (\"id\"))");

    libglot::Arena arena;
    auto* stmt = parse_create(arena, "CREATE TABLE t (uid INT REFERENCES users(id))");
    REQUIRE(stmt->columns[0]->references_table == "users");
    REQUIRE(stmt->columns[0]->references_columns.size() == 1);
    REQUIRE(stmt->columns[0]->references_columns[0] == "id");
}

TEST_CASE("Schema type - column CHECK constraint", "[schema][constraints]") {
    REQUIRE(roundtrip("CREATE TABLE t (age INT CHECK (age > 0))")
            == "CREATE TABLE \"t\" (\"age\" INT CHECK (\"age\" > 0))");

    libglot::Arena arena;
    auto* stmt = parse_create(arena, "CREATE TABLE t (age INT CHECK (age > 0))");
    REQUIRE(stmt->columns[0]->check_expr != nullptr);
    REQUIRE(stmt->columns[0]->check_expr->type == SQLNodeKind::BINARY_OP);
}

TEST_CASE("Schema type - stacked constraints on one column", "[schema][constraints]") {
    REQUIRE(roundtrip(
        "CREATE TABLE t (id INT NOT NULL PRIMARY KEY, name VARCHAR(50) NOT NULL UNIQUE)")
        == "CREATE TABLE \"t\" (\"id\" INT NOT NULL PRIMARY KEY, \"name\" VARCHAR(50) NOT NULL UNIQUE)");

    libglot::Arena arena;
    auto* stmt = parse_create(arena,
        "CREATE TABLE t (id INT NOT NULL PRIMARY KEY, name VARCHAR(50) NOT NULL UNIQUE)");
    REQUIRE(stmt->columns[0]->not_null == true);
    REQUIRE(stmt->columns[0]->primary_key == true);
    REQUIRE(stmt->columns[1]->not_null == true);
    REQUIRE(stmt->columns[1]->unique == true);
}

// ============================================================================
// Table forms
// ============================================================================

TEST_CASE("Schema type - CREATE TEMPORARY TABLE", "[schema][table]") {
    REQUIRE(roundtrip("CREATE TEMPORARY TABLE t (id INT)")
            == "CREATE TEMPORARY TABLE \"t\" (\"id\" INT)");

    libglot::Arena arena;
    auto* stmt = parse_create(arena, "CREATE TEMPORARY TABLE t (id INT)");
    REQUIRE(stmt->temporary == true);
}

TEST_CASE("Schema type - full mixed-type table", "[schema][table]") {
    REQUIRE(roundtrip(
        "CREATE TABLE orders (id BIGINT PRIMARY KEY, customer VARCHAR(100) NOT NULL, "
        "total DECIMAL(12,2) DEFAULT 0, placed TIMESTAMP, open BOOLEAN)")
        == "CREATE TABLE \"orders\" (\"id\" BIGINT PRIMARY KEY, \"customer\" VARCHAR(100) NOT NULL, "
           "\"total\" DECIMAL(12,2) DEFAULT 0, \"placed\" TIMESTAMP, \"open\" BOOLEAN)");
}

// ============================================================================
// Per-dialect identifier quoting
// ============================================================================

TEST_CASE("Schema type - MySQL uses backtick quoting", "[schema][dialect]") {
    REQUIRE(roundtrip("CREATE TABLE t (id INT PRIMARY KEY, name VARCHAR(255) NOT NULL)",
                      SQLDialect::MySQL)
            == "CREATE TABLE `t` (`id` INT PRIMARY KEY, `name` VARCHAR(255) NOT NULL)");
}

TEST_CASE("Schema type - SQL Server uses bracket quoting", "[schema][dialect]") {
    REQUIRE(roundtrip("CREATE TABLE t (id INT PRIMARY KEY, name VARCHAR(255) NOT NULL)",
                      SQLDialect::SQLServer)
            == "CREATE TABLE [t] ([id] INT PRIMARY KEY, [name] VARCHAR(255) NOT NULL)");
}

TEST_CASE("Schema type - PostgreSQL uses double-quote quoting", "[schema][dialect]") {
    REQUIRE(roundtrip("CREATE TABLE t (id INT PRIMARY KEY, name VARCHAR(255) NOT NULL)",
                      SQLDialect::PostgreSQL)
            == "CREATE TABLE \"t\" (\"id\" INT PRIMARY KEY, \"name\" VARCHAR(255) NOT NULL)");
}

TEST_CASE("Schema type - types are dialect-invariant while quoting changes", "[schema][dialect]") {
    const std::string sql = "CREATE TABLE t (price DECIMAL(10,2), ts TIMESTAMP)";

    REQUIRE(roundtrip(sql, SQLDialect::MySQL)
            == "CREATE TABLE `t` (`price` DECIMAL(10,2), `ts` TIMESTAMP)");
    REQUIRE(roundtrip(sql, SQLDialect::SQLServer)
            == "CREATE TABLE [t] ([price] DECIMAL(10,2), [ts] TIMESTAMP)");
    REQUIRE(roundtrip(sql, SQLDialect::ANSI)
            == "CREATE TABLE \"t\" (\"price\" DECIMAL(10,2), \"ts\" TIMESTAMP)");
}
