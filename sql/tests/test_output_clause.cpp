// SQL Server OUTPUT clause and PostgreSQL RETURNING clause, mapped onto the
// shared OutputClause AST.
//
// Design choices (documented in generator.h):
//  - T-SQL dialects (SQLServer, AzureSynapse) emit OUTPUT; unqualified items
//    get the statement's default row image (INSERTED for INSERT/UPDATE,
//    DELETED for DELETE).
//  - Every other dialect emits RETURNING with the qualifier stripped - valid
//    only when items reference the statement's own result rows. References
//    to the other row image (e.g. DELETED.x in an UPDATE, or any mix of
//    INSERTED and DELETED) throw std::logic_error.

#include <catch2/catch_test_macros.hpp>
#include <libglot/sql/parser.h>
#include <libglot/sql/generator.h>
#include <libglot/util/arena.h>

#include <stdexcept>
#include <string>

using namespace libglot::sql;

namespace {

std::string transpile(const std::string& sql,
                      SQLDialect parse_dialect,
                      SQLDialect gen_dialect) {
    libglot::Arena arena;
    SQLParser parser(arena, sql, parse_dialect);
    auto ast = parser.parse_top_level();
    SQLGenerator gen(gen_dialect);
    return gen.generate(ast);
}

std::string sqlserver(const std::string& sql) {
    return transpile(sql, SQLDialect::SQLServer, SQLDialect::SQLServer);
}

std::string postgres(const std::string& sql) {
    return transpile(sql, SQLDialect::PostgreSQL, SQLDialect::PostgreSQL);
}

} // namespace

// ============================================================================
// T-SQL OUTPUT round trips (SQL Server)
// ============================================================================

TEST_CASE("OUTPUT - INSERT with INSERTED columns", "[output][insert][sqlserver]") {
    REQUIRE(sqlserver("INSERT INTO t (a, b) OUTPUT INSERTED.a, INSERTED.b VALUES (1, 2)")
            == "INSERT INTO [t] ([a], [b]) OUTPUT INSERTED.[a], INSERTED.[b] VALUES (1, 2)");
}

TEST_CASE("OUTPUT - INSERT ... SELECT with OUTPUT", "[output][insert][sqlserver]") {
    REQUIRE(sqlserver("INSERT INTO t (a) OUTPUT INSERTED.a SELECT a FROM u")
            == "INSERT INTO [t] ([a]) OUTPUT INSERTED.[a] SELECT [a] FROM [u]");
}

TEST_CASE("OUTPUT - UPDATE with INSERTED and DELETED", "[output][update][sqlserver]") {
    REQUIRE(sqlserver("UPDATE t SET a = 1 OUTPUT INSERTED.a, DELETED.a WHERE b = 2")
            == "UPDATE [t] SET [a] = 1 OUTPUT INSERTED.[a], DELETED.[a] WHERE [b] = 2");
}

TEST_CASE("OUTPUT - DELETE with DELETED star", "[output][delete][sqlserver]") {
    REQUIRE(sqlserver("DELETE FROM t OUTPUT DELETED.* WHERE a = 1")
            == "DELETE FROM [t] OUTPUT DELETED.* WHERE [a] = 1");
    // Without a WHERE clause
    REQUIRE(sqlserver("DELETE FROM t OUTPUT DELETED.id")
            == "DELETE FROM [t] OUTPUT DELETED.[id]");
}

TEST_CASE("OUTPUT - aliased items", "[output][alias][sqlserver]") {
    REQUIRE(sqlserver("UPDATE t SET a = 1 OUTPUT INSERTED.a AS new_a, DELETED.a AS old_a")
            == "UPDATE [t] SET [a] = 1 OUTPUT INSERTED.[a] AS [new_a], DELETED.[a] AS [old_a]");
}

TEST_CASE("OUTPUT - generated T-SQL is a fixed point", "[output][fixpoint][sqlserver]") {
    const std::string queries[] = {
        "INSERT INTO t (a) OUTPUT INSERTED.a VALUES (1)",
        "UPDATE t SET a = 1 OUTPUT INSERTED.a, DELETED.a WHERE b = 2",
        "DELETE FROM t OUTPUT DELETED.* WHERE a = 1",
    };
    for (const auto& q : queries) {
        const std::string g1 = sqlserver(q);
        REQUIRE(sqlserver(g1) == g1);
    }
}

// ============================================================================
// PostgreSQL RETURNING parses natively onto the same AST
// ============================================================================

TEST_CASE("RETURNING - INSERT/UPDATE/DELETE native round trips", "[returning][postgresql]") {
    REQUIRE(postgres("INSERT INTO t (a) VALUES (1) RETURNING id")
            == "INSERT INTO \"t\" (\"a\") VALUES (1) RETURNING \"id\"");
    REQUIRE(postgres("INSERT INTO t (a) VALUES (1) RETURNING id, a + 1 AS next_a")
            == "INSERT INTO \"t\" (\"a\") VALUES (1) RETURNING \"id\", \"a\" + 1 AS \"next_a\"");
    REQUIRE(postgres("UPDATE t SET a = 1 WHERE b = 2 RETURNING a")
            == "UPDATE \"t\" SET \"a\" = 1 WHERE \"b\" = 2 RETURNING \"a\"");
    REQUIRE(postgres("DELETE FROM t WHERE a = 1 RETURNING *")
            == "DELETE FROM \"t\" WHERE \"a\" = 1 RETURNING *");
}

TEST_CASE("RETURNING - INSERT ... SELECT ... RETURNING", "[returning][postgresql]") {
    REQUIRE(postgres("INSERT INTO t (a) SELECT a FROM u RETURNING id")
            == "INSERT INTO \"t\" (\"a\") SELECT \"a\" FROM \"u\" RETURNING \"id\"");
}

// ============================================================================
// Cross-dialect transpilation: OUTPUT <-> RETURNING
// ============================================================================

TEST_CASE("OUTPUT INSERTED.x transpiles to RETURNING x for PostgreSQL", "[output][transpile]") {
    REQUIRE(transpile("INSERT INTO t (a) OUTPUT INSERTED.a VALUES (1)",
                      SQLDialect::SQLServer, SQLDialect::PostgreSQL)
            == "INSERT INTO \"t\" (\"a\") VALUES (1) RETURNING \"a\"");
    REQUIRE(transpile("UPDATE t SET a = 1 OUTPUT INSERTED.a WHERE b = 2",
                      SQLDialect::SQLServer, SQLDialect::PostgreSQL)
            == "UPDATE \"t\" SET \"a\" = 1 WHERE \"b\" = 2 RETURNING \"a\"");
    // DELETE returns the deleted rows: DELETED.x maps to RETURNING x
    REQUIRE(transpile("DELETE FROM t OUTPUT DELETED.* WHERE a = 1",
                      SQLDialect::SQLServer, SQLDialect::PostgreSQL)
            == "DELETE FROM \"t\" WHERE \"a\" = 1 RETURNING *");
}

TEST_CASE("RETURNING transpiles to OUTPUT for SQL Server", "[returning][transpile]") {
    REQUIRE(transpile("INSERT INTO t (a) VALUES (1) RETURNING id",
                      SQLDialect::PostgreSQL, SQLDialect::SQLServer)
            == "INSERT INTO [t] ([a]) OUTPUT INSERTED.[id] VALUES (1)");
    REQUIRE(transpile("UPDATE t SET a = 1 WHERE b = 2 RETURNING a",
                      SQLDialect::PostgreSQL, SQLDialect::SQLServer)
            == "UPDATE [t] SET [a] = 1 OUTPUT INSERTED.[a] WHERE [b] = 2");
    REQUIRE(transpile("DELETE FROM t WHERE a = 1 RETURNING *",
                      SQLDialect::PostgreSQL, SQLDialect::SQLServer)
            == "DELETE FROM [t] OUTPUT DELETED.* WHERE [a] = 1");
}

// ============================================================================
// Untranslatable combinations throw std::logic_error for non-T-SQL targets
// ============================================================================

TEST_CASE("Mixed INSERTED + DELETED throws for non-T-SQL dialects", "[output][error]") {
    const std::string sql = "UPDATE t SET a = 1 OUTPUT INSERTED.a, DELETED.a WHERE b = 2";
    for (auto d : {SQLDialect::PostgreSQL, SQLDialect::MySQL, SQLDialect::ANSI}) {
        libglot::Arena arena;
        SQLParser parser(arena, sql, SQLDialect::SQLServer);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(d);
        REQUIRE_THROWS_AS(gen.generate(ast), std::logic_error);
    }
}

TEST_CASE("DELETED in UPDATE / INSERTED in DELETE throw for non-T-SQL", "[output][error]") {
    // Old-row values from an UPDATE cannot be expressed with RETURNING
    {
        libglot::Arena arena;
        SQLParser parser(arena, "UPDATE t SET a = 1 OUTPUT DELETED.a", SQLDialect::SQLServer);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        REQUIRE_THROWS_AS(gen.generate(ast), std::logic_error);
    }
    // INSERTED rows make no sense for a DELETE outside T-SQL
    {
        libglot::Arena arena;
        SQLParser parser(arena, "DELETE FROM t OUTPUT INSERTED.a", SQLDialect::SQLServer);
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::PostgreSQL);
        REQUIRE_THROWS_AS(gen.generate(ast), std::logic_error);
    }
    // ... but both are fine when targeting SQL Server itself
    REQUIRE(sqlserver("UPDATE t SET a = 1 OUTPUT DELETED.a")
            == "UPDATE [t] SET [a] = 1 OUTPUT DELETED.[a]");
}

// ============================================================================
// Strictness: trailing input after OUTPUT/RETURNING is still an error
// ============================================================================

TEST_CASE("OUTPUT/RETURNING do not relax trailing-input checking", "[output][strict]") {
    {
        libglot::Arena arena;
        SQLParser parser(arena, "INSERT INTO t (a) VALUES (1) RETURNING id id2 id3",
                         SQLDialect::PostgreSQL);
        REQUIRE_THROWS_AS(parser.parse_top_level(), libglot::ParseError);
    }
    {
        libglot::Arena arena;
        SQLParser parser(arena, "DELETE FROM t OUTPUT DELETED. WHERE a = 1",
                         SQLDialect::SQLServer);
        REQUIRE_THROWS_AS(parser.parse_top_level(), libglot::ParseError);
    }
}
