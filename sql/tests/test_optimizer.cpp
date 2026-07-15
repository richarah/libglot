// SQLOptimizer: constant folding, boolean simplification, and WHERE-clause
// pruning - each independently toggleable, idempotent, arena-backed, and
// safe across every statement kind.
//
// Design choices (documented in optimizer.h):
//  - WHERE TRUE is removed; WHERE FALSE is preserved as-is (the literal
//    clause is the marker; the statement is never deleted).
//  - Folding skips on any doubt: division/modulo by zero, overflow, floats,
//    hex/binary literals, and non-genuine boolean literals are left alone.

#include <catch2/catch_test_macros.hpp>
#include <libglot/sql/parser.h>
#include <libglot/sql/generator.h>
#include <libglot/sql/optimizer.h>
#include <libglot/util/arena.h>

#include <string>
#include <vector>

using namespace libglot::sql;

namespace {

std::string optimize_sql(const std::string& sql,
                         SQLOptimizer::Options opts = SQLOptimizer::Options{},
                         SQLDialect d = SQLDialect::PostgreSQL) {
    libglot::Arena arena;
    SQLParser parser(arena, sql, d);
    auto ast = parser.parse_top_level();
    SQLOptimizer optimizer(arena, opts);
    ast = optimizer.optimize(ast);
    SQLGenerator gen(d);
    return gen.generate(ast);
}

std::string plain_sql(const std::string& sql, SQLDialect d = SQLDialect::PostgreSQL) {
    libglot::Arena arena;
    SQLParser parser(arena, sql, d);
    auto ast = parser.parse_top_level();
    SQLGenerator gen(d);
    return gen.generate(ast);
}

} // namespace

// ============================================================================
// Pass 1: constant folding - integer arithmetic
// ============================================================================

TEST_CASE("Optimizer - integer constant folding", "[optimizer][fold]") {
    REQUIRE(optimize_sql("SELECT 1 + 2") == "SELECT 3");
    REQUIRE(optimize_sql("SELECT 1 + 2 * 3") == "SELECT 7");
    REQUIRE(optimize_sql("SELECT (1 + 2) * 3") == "SELECT 9");
    REQUIRE(optimize_sql("SELECT 10 - 4") == "SELECT 6");
    REQUIRE(optimize_sql("SELECT 10 / 2") == "SELECT 5");
    REQUIRE(optimize_sql("SELECT 7 % 3") == "SELECT 1");
    // Negative results and unary-minus operands fold too
    REQUIRE(optimize_sql("SELECT 1 - 2") == "SELECT -1");
    REQUIRE(optimize_sql("SELECT -2 + 3") == "SELECT 1");
    // Folding inside larger statements
    REQUIRE(optimize_sql("SELECT a FROM t WHERE x > 2 + 3")
            == "SELECT \"a\" FROM \"t\" WHERE \"x\" > 5");
    REQUIRE(optimize_sql("SELECT a FROM t LIMIT 5 * 2")
            == "SELECT \"a\" FROM \"t\" LIMIT 10");
}

TEST_CASE("Optimizer - folding guards: division by zero", "[optimizer][fold][guard]") {
    REQUIRE(optimize_sql("SELECT 1 / 0") == "SELECT 1 / 0");
    REQUIRE(optimize_sql("SELECT 7 % 0") == "SELECT 7 % 0");
}

TEST_CASE("Optimizer - folding guards: overflow", "[optimizer][fold][guard]") {
    // LLONG_MAX + 1 must not fold
    REQUIRE(optimize_sql("SELECT 9223372036854775807 + 1")
            == "SELECT 9223372036854775807 + 1");
    REQUIRE(optimize_sql("SELECT 9223372036854775807 * 2")
            == "SELECT 9223372036854775807 * 2");
}

TEST_CASE("Optimizer - folding guards: non-integer literals", "[optimizer][fold][guard]") {
    // Floats, exponents, hex, and mixed operands are skipped (any doubt)
    REQUIRE(optimize_sql("SELECT 1.5 + 2") == "SELECT 1.5 + 2");
    REQUIRE(optimize_sql("SELECT 1.5e10 + 1") == "SELECT 1.5e10 + 1");
    REQUIRE(optimize_sql("SELECT 0x1F + 1") == "SELECT 0x1F + 1");
    REQUIRE(optimize_sql("SELECT a + 1 FROM t") == "SELECT \"a\" + 1 FROM \"t\"");
}

// ============================================================================
// Pass 1: constant folding - string concatenation
// ============================================================================

TEST_CASE("Optimizer - string literal concatenation", "[optimizer][fold][concat]") {
    REQUIRE(optimize_sql("SELECT 'foo' || 'bar'") == "SELECT 'foobar'");
    // Chained concatenation folds left-to-right
    REQUIRE(optimize_sql("SELECT 'a' || 'b' || 'c'") == "SELECT 'abc'");
    // Embedded escaped quotes survive the splice
    REQUIRE(optimize_sql("SELECT 'it''s' || ' ok'") == "SELECT 'it''s ok'");
    // Mixed operands are not folded
    REQUIRE(optimize_sql("SELECT a || 'b' FROM t") == "SELECT \"a\" || 'b' FROM \"t\"");
    REQUIRE(optimize_sql("SELECT 'a' || 1") == "SELECT 'a' || 1");
}

// ============================================================================
// Pass 2: boolean simplification
// ============================================================================

TEST_CASE("Optimizer - boolean simplification", "[optimizer][bool]") {
    REQUIRE(optimize_sql("SELECT a FROM t WHERE a = 1 AND TRUE")
            == "SELECT \"a\" FROM \"t\" WHERE \"a\" = 1");
    REQUIRE(optimize_sql("SELECT a FROM t WHERE TRUE AND a = 1")
            == "SELECT \"a\" FROM \"t\" WHERE \"a\" = 1");
    REQUIRE(optimize_sql("SELECT a FROM t WHERE a = 1 AND FALSE")
            == "SELECT \"a\" FROM \"t\" WHERE FALSE");
    REQUIRE(optimize_sql("SELECT a FROM t WHERE a = 1 OR FALSE")
            == "SELECT \"a\" FROM \"t\" WHERE \"a\" = 1");
    // x OR TRUE -> TRUE, then WHERE TRUE is pruned by pass 3
    REQUIRE(optimize_sql("SELECT a FROM t WHERE a = 1 OR TRUE")
            == "SELECT \"a\" FROM \"t\"");
    REQUIRE(optimize_sql("SELECT NOT TRUE") == "SELECT FALSE");
    REQUIRE(optimize_sql("SELECT NOT FALSE") == "SELECT TRUE");
    REQUIRE(optimize_sql("SELECT NOT NOT a = 1 FROM t")
            == "SELECT \"a\" = 1 FROM \"t\"");
    // Cascade: NOT (TRUE AND FALSE) -> NOT FALSE -> TRUE
    REQUIRE(optimize_sql("SELECT NOT (TRUE AND FALSE)") == "SELECT TRUE");
}

TEST_CASE("Optimizer - only genuine boolean literals simplify", "[optimizer][bool][guard]") {
    // String 'TRUE' is not a boolean literal
    REQUIRE(optimize_sql("SELECT a FROM t WHERE a = 1 AND 'TRUE'")
            == "SELECT \"a\" FROM \"t\" WHERE \"a\" = 1 AND 'TRUE'");
    // A column happens to survive: no simplification without a literal
    REQUIRE(optimize_sql("SELECT a AND b FROM t")
            == "SELECT \"a\" AND \"b\" FROM \"t\"");
}

// ============================================================================
// Pass 3: WHERE-clause pruning
// ============================================================================

TEST_CASE("Optimizer - WHERE TRUE is removed", "[optimizer][where]") {
    REQUIRE(optimize_sql("SELECT a FROM t WHERE TRUE") == "SELECT \"a\" FROM \"t\"");
    REQUIRE(optimize_sql("UPDATE t SET a = 1 WHERE TRUE") == "UPDATE \"t\" SET \"a\" = 1");
    REQUIRE(optimize_sql("DELETE FROM t WHERE TRUE") == "DELETE FROM \"t\"");
    // Simplification feeding pruning: WHERE TRUE AND TRUE -> gone
    REQUIRE(optimize_sql("SELECT a FROM t WHERE TRUE AND TRUE")
            == "SELECT \"a\" FROM \"t\"");
}

TEST_CASE("Optimizer - WHERE FALSE is preserved, statement kept", "[optimizer][where]") {
    REQUIRE(optimize_sql("SELECT a FROM t WHERE FALSE")
            == "SELECT \"a\" FROM \"t\" WHERE FALSE");
    REQUIRE(optimize_sql("DELETE FROM t WHERE FALSE")
            == "DELETE FROM \"t\" WHERE FALSE");
}

// ============================================================================
// Pass toggles
// ============================================================================

TEST_CASE("Optimizer - pass toggles are independent", "[optimizer][options]") {
    SQLOptimizer::Options no_fold;
    no_fold.fold_constants = false;
    REQUIRE(optimize_sql("SELECT 1 + 2", no_fold) == "SELECT 1 + 2");
    // The other passes still run
    REQUIRE(optimize_sql("SELECT a FROM t WHERE TRUE", no_fold)
            == "SELECT \"a\" FROM \"t\"");

    SQLOptimizer::Options no_bool;
    no_bool.simplify_booleans = false;
    REQUIRE(optimize_sql("SELECT a FROM t WHERE a = 1 AND TRUE", no_bool)
            == "SELECT \"a\" FROM \"t\" WHERE \"a\" = 1 AND TRUE");
    REQUIRE(optimize_sql("SELECT 1 + 2", no_bool) == "SELECT 3");

    SQLOptimizer::Options no_prune;
    no_prune.prune_where = false;
    REQUIRE(optimize_sql("SELECT a FROM t WHERE TRUE", no_prune)
            == "SELECT \"a\" FROM \"t\" WHERE TRUE");
    REQUIRE(optimize_sql("SELECT 1 + 2", no_prune) == "SELECT 3");

    SQLOptimizer::Options all_off;
    all_off.fold_constants = false;
    all_off.simplify_booleans = false;
    all_off.prune_where = false;
    const std::string q = "SELECT 1 + 2 FROM t WHERE TRUE AND a = 1";
    REQUIRE(optimize_sql(q, all_off) == plain_sql(q));
}

// ============================================================================
// Idempotence: optimize(optimize(x)) == optimize(x) via generated SQL
// ============================================================================

TEST_CASE("Optimizer - idempotent over the same tree", "[optimizer][idempotence]") {
    const std::vector<std::string> queries = {
        "SELECT 1 + 2 * 3",
        "SELECT 'a' || 'b' || 'c'",
        "SELECT a FROM t WHERE TRUE AND a = 1 OR FALSE",
        "SELECT a FROM t WHERE FALSE",
        "SELECT NOT NOT a = 1 FROM t",
        "SELECT 1 - 2",
        "SELECT a, SUM(b) FROM t GROUP BY ROLLUP(a) HAVING SUM(b) > 1 + 1",
        "UPDATE t SET a = 1 + 1 WHERE TRUE",
    };
    for (const auto& q : queries) {
        libglot::Arena arena;
        SQLParser parser(arena, q, SQLDialect::PostgreSQL);
        auto ast = parser.parse_top_level();
        SQLOptimizer optimizer(arena);
        auto* once = optimizer.optimize(ast);
        SQLGenerator gen1(SQLDialect::PostgreSQL);
        const std::string g1 = gen1.generate(once);
        auto* twice = optimizer.optimize(once);
        SQLGenerator gen2(SQLDialect::PostgreSQL);
        const std::string g2 = gen2.generate(twice);
        INFO("query: " << q);
        REQUIRE(g1 == g2);
    }
}

// ============================================================================
// No-change guarantee for non-foldable queries
// ============================================================================

TEST_CASE("Optimizer - non-foldable queries generate identically", "[optimizer][no-change]") {
    const std::vector<std::string> queries = {
        "SELECT a + b FROM t",
        "SELECT * FROM t WHERE a = 1 AND b = 2",
        "SELECT COUNT(*) FROM t GROUP BY a HAVING COUNT(*) > 1",
        "SELECT u.id FROM users u INNER JOIN orders o ON u.id = o.user_id",
        "WITH c AS (SELECT a FROM t) SELECT * FROM c",
        "INSERT INTO t (a, b) VALUES (1, 'x')",
        "UPDATE t SET a = b + 1 WHERE c = 2",
        "DELETE FROM t WHERE a = 1",
        "MERGE INTO t USING u ON t.id = u.id WHEN MATCHED THEN UPDATE SET a = 1",
        "SELECT CASE WHEN a > 1 THEN 'x' ELSE 'y' END FROM t",
        "SELECT ROW_NUMBER() OVER (PARTITION BY a ORDER BY b) FROM t",
        "SELECT 1 / 0",
        "SELECT a FROM t WHERE x BETWEEN 1 AND 10",
    };
    for (const auto& q : queries) {
        INFO("query: " << q);
        REQUIRE(optimize_sql(q) == plain_sql(q));
    }
}

// ============================================================================
// Folded output re-parses (fixpoint)
// ============================================================================

TEST_CASE("Optimizer - folded output re-parses and is a fixed point", "[optimizer][fixpoint]") {
    const std::vector<std::string> queries = {
        "SELECT 1 + 2 * 3",
        "SELECT 1 - 2",
        "SELECT 'it''s' || ' ok'",
        "SELECT a FROM t WHERE TRUE AND a = 1",
        "SELECT a FROM t WHERE FALSE",
        "SELECT NOT TRUE",
    };
    for (const auto& q : queries) {
        INFO("query: " << q);
        const std::string folded = optimize_sql(q);
        // Folded output re-parses cleanly...
        const std::string reparsed = plain_sql(folded);
        // ...and is a parse -> generate fixed point
        REQUIRE(reparsed == folded);
    }
}

// ============================================================================
// Safety: the walker handles every statement type without crashing
// ============================================================================

TEST_CASE("Optimizer - walks all statement kinds safely", "[optimizer][safety]") {
    const std::vector<std::string> statements = {
        "CREATE TABLE t (id INT PRIMARY KEY, x INT DEFAULT 5, CHECK (x > 0))",
        "CREATE VIEW v AS SELECT 1 + 2",
        "DROP TABLE t",
        "TRUNCATE TABLE t",
        "ALTER TABLE t ADD COLUMN c INT",
        "CREATE INDEX i ON t (a, b)",
        "GRANT SELECT ON users TO alice",
        "REVOKE SELECT ON users FROM alice",
        "COMMIT",
        "ROLLBACK",
        "SAVEPOINT sp1",
        "SET x = 1 + 1",
        "SHOW TABLES",
        "EXPLAIN SELECT 1 + 2",
        "CALL myproc(1 + 1, 'a')",
        "DECLARE x INT DEFAULT 1 + 1",
        "BEGIN SELECT 1 + 1; SELECT 2; END",
        "IF 1 > 0 THEN SELECT 1 + 1; ELSE SELECT 2; END IF",
        "WHILE 1 = 1 LOOP BREAK; END LOOP",
        "FOR i IN 1..10 LOOP SELECT 1 + 1; END LOOP",
        "RETURN 1 + 1",
        "RAISE EXCEPTION 'boom'",
        "OPEN cur(1 + 1)",
        "FETCH cur INTO x",
        "CLOSE cur",
        "MERGE INTO t USING u ON t.id = u.id WHEN MATCHED THEN UPDATE SET a = 1 + 1",
    };
    for (const auto& q : statements) {
        INFO("statement: " << q);
        libglot::Arena arena;
        SQLParser parser(arena, q, SQLDialect::PostgreSQL);
        auto ast = parser.parse_top_level();
        SQLOptimizer optimizer(arena);
        SQLNode* optimized = nullptr;
        REQUIRE_NOTHROW(optimized = optimizer.optimize(ast));
        REQUIRE(optimized != nullptr);
        // The optimized tree still generates
        SQLGenerator gen(SQLDialect::PostgreSQL);
        REQUIRE_NOTHROW(gen.generate(optimized));
    }
    // A null root is returned as null, not dereferenced
    libglot::Arena arena;
    SQLOptimizer optimizer(arena);
    REQUIRE(optimizer.optimize(nullptr) == nullptr);
}
