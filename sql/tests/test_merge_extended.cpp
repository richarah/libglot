// Wave 2: MERGE ... WHEN NOT MATCHED BY SOURCE (T-SQL/Azure Synapse), plus
// the generalizations that came with it: WHEN MATCHED THEN DELETE and an
// optional AND <cond> on any WHEN clause. MergeStmt now holds an ordered
// list of MergeWhenClause entries instead of one fixed UPDATE/INSERT slot
// each, so a MERGE with several WHEN arms of the same kind is no longer
// silently collapsed to just the last one.

#include <catch2/catch_test_macros.hpp>
#include <libglot/sql/generator.h>
#include <libglot/sql/parser.h>
#include <libglot/util/arena.h>

#include <string>

using namespace libglot::sql;

namespace {

std::string transpile(const std::string& sql, SQLDialect dialect) {
    libglot::Arena arena;
    SQLParser parser(arena, sql, dialect);
    auto* ast = parser.parse_top_level();
    SQLGenerator gen(dialect);
    return gen.generate(ast);
}

} // namespace

TEST_CASE("MERGE - WHEN NOT MATCHED BY SOURCE THEN DELETE (T-SQL)", "[merge][by-source]") {
    const std::string sql = "MERGE INTO t USING u ON t.id = u.id "
                            "WHEN MATCHED THEN UPDATE SET a = 1 "
                            "WHEN NOT MATCHED THEN INSERT (a) VALUES (1) "
                            "WHEN NOT MATCHED BY SOURCE THEN DELETE";
    REQUIRE(transpile(sql, SQLDialect::SQLServer) ==
            "MERGE INTO [t] USING [u] ON [t].[id] = [u].[id] "
            "WHEN MATCHED THEN UPDATE SET [a] = 1 "
            "WHEN NOT MATCHED THEN INSERT ([a]) VALUES (1) "
            "WHEN NOT MATCHED BY SOURCE THEN DELETE");
}

TEST_CASE("MERGE - WHEN NOT MATCHED BY SOURCE THEN UPDATE (T-SQL)", "[merge][by-source]") {
    const std::string sql = "MERGE INTO t USING u ON t.id = u.id "
                            "WHEN NOT MATCHED BY SOURCE THEN UPDATE SET a = 0";
    REQUIRE(transpile(sql, SQLDialect::AzureSynapse) ==
            "MERGE INTO \"t\" USING \"u\" ON \"t\".\"id\" = \"u\".\"id\" "
            "WHEN NOT MATCHED BY SOURCE THEN UPDATE SET \"a\" = 0");
}

TEST_CASE("MERGE - WHEN NOT MATCHED BY SOURCE AND <cond> THEN DELETE", "[merge][by-source]") {
    const std::string sql = "MERGE INTO t USING u ON t.id = u.id "
                            "WHEN NOT MATCHED BY SOURCE AND t.stale = 1 THEN DELETE";
    REQUIRE(transpile(sql, SQLDialect::SQLServer) ==
            "MERGE INTO [t] USING [u] ON [t].[id] = [u].[id] "
            "WHEN NOT MATCHED BY SOURCE AND [t].[stale] = 1 THEN DELETE");
}

TEST_CASE("MERGE - WHEN MATCHED THEN DELETE", "[merge][delete]") {
    REQUIRE(transpile("MERGE INTO t USING u ON t.id = u.id WHEN MATCHED THEN DELETE",
                      SQLDialect::PostgreSQL) ==
            "MERGE INTO \"t\" USING \"u\" ON \"t\".\"id\" = \"u\".\"id\" "
            "WHEN MATCHED THEN DELETE");
}

TEST_CASE("MERGE - WHEN MATCHED AND <cond> THEN UPDATE (portable, not T-SQL-only)",
          "[merge][and-cond]") {
    const std::string sql = "MERGE INTO t USING u ON t.id = u.id "
                            "WHEN MATCHED AND u.active = 1 THEN UPDATE SET a = u.a";
    REQUIRE(transpile(sql, SQLDialect::PostgreSQL) ==
            "MERGE INTO \"t\" USING \"u\" ON \"t\".\"id\" = \"u\".\"id\" "
            "WHEN MATCHED AND \"u\".\"active\" = 1 THEN UPDATE SET \"a\" = \"u\".\"a\"");
}

TEST_CASE("MERGE - AST shape for WHEN NOT MATCHED BY SOURCE", "[merge][by-source]") {
    libglot::Arena arena;
    SQLParser parser(arena,
                     "MERGE INTO t USING u ON t.id = u.id "
                     "WHEN NOT MATCHED BY SOURCE AND t.x = 1 THEN DELETE",
                     SQLDialect::SQLServer);
    auto* stmt = static_cast<MergeStmt*>(parser.parse_top_level());
    REQUIRE(stmt->when_clauses.size() == 1);
    const auto& clause = stmt->when_clauses[0];
    REQUIRE(clause.match_kind == MergeMatchKind::NOT_MATCHED_BY_SOURCE);
    REQUIRE(clause.extra_condition != nullptr);
    REQUIRE(clause.action == MergeActionKind::DELETE_ACTION);
}

TEST_CASE("MERGE - fixed point for WHEN NOT MATCHED BY SOURCE (T-SQL)", "[merge][roundtrip]") {
    const std::string queries[] = {
        "MERGE INTO t USING u ON t.id = u.id "
        "WHEN MATCHED THEN UPDATE SET a = 1 "
        "WHEN NOT MATCHED THEN INSERT (a) VALUES (1) "
        "WHEN NOT MATCHED BY SOURCE THEN DELETE",
        "MERGE INTO t USING u ON t.id = u.id "
        "WHEN NOT MATCHED BY SOURCE AND t.stale = 1 THEN UPDATE SET a = 0",
    };
    for (auto d : {SQLDialect::SQLServer, SQLDialect::AzureSynapse}) {
        for (const auto& q : queries) {
            const std::string g1 = transpile(q, d);
            REQUIRE(transpile(g1, d) == g1);
        }
    }
}

TEST_CASE("MERGE - WHEN NOT MATCHED BY SOURCE throws outside T-SQL", "[merge][error]") {
    const std::string sql =
        "MERGE INTO t USING u ON t.id = u.id WHEN NOT MATCHED BY SOURCE THEN DELETE";
    for (auto d :
         {SQLDialect::PostgreSQL, SQLDialect::Oracle, SQLDialect::MySQL, SQLDialect::ANSI}) {
        REQUIRE_THROWS_AS(transpile(sql, d), std::logic_error);
    }
}

TEST_CASE("MERGE - bad WHEN clause is a clean ParseError", "[merge][error]") {
    libglot::Arena arena;
    SQLParser parser(arena,
                     "MERGE INTO t USING u ON t.id = u.id WHEN NOT MATCHED BY WHATEVER THEN DELETE",
                     SQLDialect::SQLServer);
    REQUIRE_THROWS_AS(parser.parse_top_level(), libglot::ParseError);
}
