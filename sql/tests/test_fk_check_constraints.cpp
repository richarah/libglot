// ============================================================================
// CREATE TABLE schema parsing: column definitions, column constraints
// (NOT NULL, DEFAULT, PRIMARY KEY, UNIQUE, REFERENCES, CHECK) and
// table-level constraints (PRIMARY KEY, FOREIGN KEY ... REFERENCES,
// UNIQUE, CHECK), plus faithful regeneration.
// ============================================================================

#include <catch2/catch_test_macros.hpp>
#include <libglot/sql/parser.h>
#include <libglot/sql/generator.h>
#include <libglot/util/arena.h>

using namespace libglot::sql;

TEST_CASE("CREATE TABLE - column definitions are parsed into the AST", "[ddl][create_table]") {
    libglot::Arena arena;
    SQLParser parser(arena,
        "CREATE TABLE users ("
        "id INT PRIMARY KEY, "
        "name VARCHAR(255) NOT NULL, "
        "email VARCHAR(100) UNIQUE, "
        "age INT DEFAULT 18, "
        "dept_id INT REFERENCES departments (id))");

    auto* node = parser.parse_top_level();
    REQUIRE(node->type == SQLNodeKind::CREATE_TABLE_STMT);
    auto* stmt = static_cast<CreateTableStmt*>(node);

    REQUIRE(stmt->columns.size() == 5);

    REQUIRE(stmt->columns[0]->name == "id");
    REQUIRE(stmt->columns[0]->type == "INT");
    REQUIRE(stmt->columns[0]->primary_key == true);

    REQUIRE(stmt->columns[1]->name == "name");
    REQUIRE(stmt->columns[1]->type == "VARCHAR(255)");
    REQUIRE(stmt->columns[1]->not_null == true);

    REQUIRE(stmt->columns[2]->name == "email");
    REQUIRE(stmt->columns[2]->type == "VARCHAR(100)");
    REQUIRE(stmt->columns[2]->unique == true);

    REQUIRE(stmt->columns[3]->name == "age");
    REQUIRE(stmt->columns[3]->default_value != nullptr);
    REQUIRE(stmt->columns[3]->default_value->type == SQLNodeKind::LITERAL);
    REQUIRE(static_cast<Literal*>(stmt->columns[3]->default_value)->value == "18");

    REQUIRE(stmt->columns[4]->name == "dept_id");
    REQUIRE(stmt->columns[4]->references_table == "departments");
    REQUIRE(stmt->columns[4]->references_columns.size() == 1);
    REQUIRE(stmt->columns[4]->references_columns[0] == "id");
}

TEST_CASE("CREATE TABLE - parameterized types keep their parameters", "[ddl][create_table]") {
    libglot::Arena arena;
    SQLParser parser(arena, "CREATE TABLE prices (amount DECIMAL(10,2) NOT NULL)");

    auto* stmt = static_cast<CreateTableStmt*>(parser.parse_top_level());
    REQUIRE(stmt->columns.size() == 1);
    REQUIRE(stmt->columns[0]->type == "DECIMAL(10,2)");
    REQUIRE(stmt->columns[0]->not_null == true);
}

TEST_CASE("CREATE TABLE - table-level constraints", "[ddl][create_table][constraints]") {
    libglot::Arena arena;
    SQLParser parser(arena,
        "CREATE TABLE order_items ("
        "order_id INT, "
        "product_id INT, "
        "qty INT NOT NULL, "
        "PRIMARY KEY (order_id, product_id), "
        "FOREIGN KEY (order_id) REFERENCES orders (id) ON DELETE CASCADE, "
        "UNIQUE (product_id), "
        "CHECK (qty > 0))");

    auto* stmt = static_cast<CreateTableStmt*>(parser.parse_top_level());

    REQUIRE(stmt->columns.size() == 3);
    REQUIRE(stmt->constraints.size() == 4);

    auto* pk = stmt->constraints[0];
    REQUIRE(pk->constraint_type == TableConstraint::Type::PRIMARY_KEY);
    REQUIRE(pk->columns.size() == 2);
    REQUIRE(pk->columns[0] == "order_id");
    REQUIRE(pk->columns[1] == "product_id");

    auto* fk = stmt->constraints[1];
    REQUIRE(fk->constraint_type == TableConstraint::Type::FOREIGN_KEY);
    REQUIRE(fk->columns.size() == 1);
    REQUIRE(fk->columns[0] == "order_id");
    REQUIRE(fk->ref_table != nullptr);
    REQUIRE(fk->ref_table->table == "orders");
    REQUIRE(fk->ref_columns.size() == 1);
    REQUIRE(fk->ref_columns[0] == "id");
    REQUIRE(fk->on_delete_action == "CASCADE");

    auto* uq = stmt->constraints[2];
    REQUIRE(uq->constraint_type == TableConstraint::Type::UNIQUE);
    REQUIRE(uq->columns.size() == 1);

    auto* ck = stmt->constraints[3];
    REQUIRE(ck->constraint_type == TableConstraint::Type::CHECK);
    REQUIRE(ck->check_expr != nullptr);
    REQUIRE(ck->check_expr->type == SQLNodeKind::BINARY_OP);
}

TEST_CASE("CREATE TABLE - roundtrip of a realistic multi-column table", "[ddl][create_table][roundtrip]") {
    libglot::Arena arena;
    SQLParser parser(arena,
        "CREATE TABLE users ("
        "id INT PRIMARY KEY, "
        "name VARCHAR(255) NOT NULL, "
        "age INT DEFAULT 18 CHECK (age > 0), "
        "dept_id INT REFERENCES departments (id), "
        "UNIQUE (name))");

    auto* stmt = parser.parse_top_level();
    SQLGenerator gen(SQLDialect::ANSI);
    std::string output = gen.generate(stmt);

    REQUIRE(output ==
        "CREATE TABLE \"users\" ("
        "\"id\" INT PRIMARY KEY, "
        "\"name\" VARCHAR(255) NOT NULL, "
        "\"age\" INT DEFAULT 18 CHECK (\"age\" > 0), "
        "\"dept_id\" INT REFERENCES \"departments\" (\"id\"), "
        "UNIQUE (\"name\"))");

    // The regenerated DDL must parse back to the same schema shape
    libglot::Arena arena2;
    SQLParser parser2(arena2, output);
    auto* stmt2 = static_cast<CreateTableStmt*>(parser2.parse_top_level());
    REQUIRE(stmt2->columns.size() == 4);
    REQUIRE(stmt2->constraints.size() == 1);
    REQUIRE(stmt2->columns[0]->primary_key == true);
    REQUIRE(stmt2->columns[1]->not_null == true);
    REQUIRE(stmt2->columns[2]->check_expr != nullptr);
    REQUIRE_FALSE(stmt2->columns[3]->references_table.empty());
    REQUIRE(stmt2->constraints[0]->constraint_type == TableConstraint::Type::UNIQUE);
}

TEST_CASE("CREATE TABLE - named constraint", "[ddl][create_table][constraints]") {
    libglot::Arena arena;
    SQLParser parser(arena,
        "CREATE TABLE t (a INT, CONSTRAINT pk_t PRIMARY KEY (a))");

    auto* stmt = static_cast<CreateTableStmt*>(parser.parse_top_level());
    REQUIRE(stmt->constraints.size() == 1);
    REQUIRE(stmt->constraints[0]->name == "pk_t");
    REQUIRE(stmt->constraints[0]->constraint_type == TableConstraint::Type::PRIMARY_KEY);

    SQLGenerator gen(SQLDialect::ANSI);
    REQUIRE(gen.generate(stmt) ==
            "CREATE TABLE \"t\" (\"a\" INT, CONSTRAINT \"pk_t\" PRIMARY KEY (\"a\"))");
}
