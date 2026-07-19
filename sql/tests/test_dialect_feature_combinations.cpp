// Cross-dialect generation of the same parsed query, asserting the real
// per-dialect differences with exact output strings:
//
//   - identifier quoting:   "col" (ANSI/PostgreSQL)  `col` (MySQL)  [col] (SQL Server)
//   - row limiting:         LIMIT n  vs  TOP n (SQL Server)  vs  FIRST n [SKIP m]
//                           (Firebird / Informix)
//   - boolean literals:     TRUE/FALSE  vs  1/0 (MySQL, SQLite, SQL Server, ClickHouse)
//   - ILIKE:                native (PostgreSQL, Snowflake, DuckDB)
//                           vs LOWER() LIKE LOWER() polyfill (MySQL)
//
// KNOWN BUG (not asserted here): LIMIT n OFFSET m for SQL Server generates
// "SELECT TOP n ... OFFSET m", which is invalid T-SQL (OFFSET requires
// ORDER BY ... OFFSET/FETCH and cannot combine with TOP). Reported instead.

#include <catch2/catch_test_macros.hpp>
#include <libglot/sql/dialect_traits.h>
#include <libglot/sql/generator.h>
#include <libglot/sql/parser.h>
#include <libglot/util/arena.h>

#include <string>

using namespace libglot::sql;

namespace {

// Parse once with the default (PostgreSQL) tokenizer config, generate for the
// requested target dialect - the transpilation direction users actually run.
std::string transpile(const std::string& sql, SQLDialect target) {
    libglot::Arena arena;
    SQLParser parser(arena, sql);
    auto ast = parser.parse_top_level();
    SQLGenerator gen(target);
    return gen.generate(ast);
}

} // namespace

// ============================================================================
// Identifier quoting styles
// ============================================================================

TEST_CASE("Dialect combo - identifier quote per dialect", "[dialect-combo][quoting]") {
    const std::string sql = "SELECT name FROM users";

    REQUIRE(transpile(sql, SQLDialect::ANSI) == "SELECT \"name\" FROM \"users\"");
    REQUIRE(transpile(sql, SQLDialect::PostgreSQL) == "SELECT \"name\" FROM \"users\"");
    REQUIRE(transpile(sql, SQLDialect::MySQL) == "SELECT `name` FROM `users`");
    REQUIRE(transpile(sql, SQLDialect::SQLServer) == "SELECT [name] FROM [users]");
}

TEST_CASE("Dialect combo - qualified column keeps per-part quoting", "[dialect-combo][quoting]") {
    const std::string sql = "SELECT u.id FROM users u";

    REQUIRE(transpile(sql, SQLDialect::PostgreSQL) ==
            "SELECT \"u\".\"id\" FROM \"users\" AS \"u\"");
    REQUIRE(transpile(sql, SQLDialect::MySQL) == "SELECT `u`.`id` FROM `users` AS `u`");
}

// ============================================================================
// LIMIT vs TOP vs FIRST/SKIP
// ============================================================================

TEST_CASE("Dialect combo - LIMIT stays LIMIT where supported", "[dialect-combo][limit]") {
    const std::string sql = "SELECT * FROM users LIMIT 10";

    REQUIRE(transpile(sql, SQLDialect::ANSI) == "SELECT * FROM \"users\" LIMIT 10");
    REQUIRE(transpile(sql, SQLDialect::PostgreSQL) == "SELECT * FROM \"users\" LIMIT 10");
    REQUIRE(transpile(sql, SQLDialect::MySQL) == "SELECT * FROM `users` LIMIT 10");
    REQUIRE(transpile(sql, SQLDialect::DuckDB) == "SELECT * FROM \"users\" LIMIT 10");
}

TEST_CASE("Dialect combo - LIMIT becomes TOP for SQL Server", "[dialect-combo][limit]") {
    REQUIRE(transpile("SELECT * FROM users LIMIT 10", SQLDialect::SQLServer) ==
            "SELECT TOP 10 * FROM [users]");
}

TEST_CASE("Dialect combo - LIMIT becomes FIRST for Firebird and Informix",
          "[dialect-combo][limit]") {
    const std::string sql = "SELECT * FROM users LIMIT 10";

    REQUIRE(transpile(sql, SQLDialect::Firebird) == "SELECT FIRST 10 * FROM \"users\"");
    REQUIRE(transpile(sql, SQLDialect::Informix) == "SELECT FIRST 10 * FROM \"users\"");
}

TEST_CASE("Dialect combo - LIMIT/OFFSET becomes FIRST/SKIP for Firebird and Informix",
          "[dialect-combo][limit]") {
    const std::string sql = "SELECT * FROM users LIMIT 10 OFFSET 5";

    REQUIRE(transpile(sql, SQLDialect::Firebird) == "SELECT FIRST 10 SKIP 5 * FROM \"users\"");
    REQUIRE(transpile(sql, SQLDialect::Informix) == "SELECT FIRST 10 SKIP 5 * FROM \"users\"");
    // Dialects with native LIMIT/OFFSET keep the clause verbatim
    REQUIRE(transpile(sql, SQLDialect::PostgreSQL) == "SELECT * FROM \"users\" LIMIT 10 OFFSET 5");
}

// ============================================================================
// Boolean literal spelling (dialect_traits.h true_literal / false_literal)
// ============================================================================

TEST_CASE("Dialect combo - TRUE literal spelling", "[dialect-combo][boolean]") {
    const std::string sql = "SELECT * FROM t WHERE active = TRUE";

    REQUIRE(transpile(sql, SQLDialect::ANSI) == "SELECT * FROM \"t\" WHERE \"active\" = TRUE");
    REQUIRE(transpile(sql, SQLDialect::PostgreSQL) ==
            "SELECT * FROM \"t\" WHERE \"active\" = TRUE");
    REQUIRE(transpile(sql, SQLDialect::MySQL) == "SELECT * FROM `t` WHERE `active` = 1");
    REQUIRE(transpile(sql, SQLDialect::SQLServer) == "SELECT * FROM [t] WHERE [active] = 1");
    REQUIRE(transpile(sql, SQLDialect::SQLite) == "SELECT * FROM \"t\" WHERE \"active\" = 1");
}

TEST_CASE("Dialect combo - FALSE literal spelling", "[dialect-combo][boolean]") {
    const std::string sql = "SELECT * FROM t WHERE b = FALSE";

    REQUIRE(transpile(sql, SQLDialect::PostgreSQL) == "SELECT * FROM \"t\" WHERE \"b\" = FALSE");
    REQUIRE(transpile(sql, SQLDialect::ClickHouse) == "SELECT * FROM `t` WHERE `b` = 0");
}

TEST_CASE("Dialect combo - boolean traits match generated output", "[dialect-combo][boolean]") {
    REQUIRE(std::string(SQLDialectTraits::get_features(SQLDialect::MySQL).true_literal) == "1");
    REQUIRE(std::string(SQLDialectTraits::get_features(SQLDialect::MySQL).false_literal) == "0");
    REQUIRE(std::string(SQLDialectTraits::get_features(SQLDialect::PostgreSQL).true_literal) ==
            "TRUE");
    REQUIRE(std::string(SQLDialectTraits::get_features(SQLDialect::SQLServer).true_literal) == "1");
}

// ============================================================================
// ILIKE: native vs LOWER() polyfill
// ============================================================================

TEST_CASE("Dialect combo - ILIKE native for PostgreSQL, Snowflake, DuckDB",
          "[dialect-combo][ilike]") {
    const std::string sql = "SELECT * FROM t WHERE name ILIKE 'a%'";

    REQUIRE(transpile(sql, SQLDialect::PostgreSQL) ==
            "SELECT * FROM \"t\" WHERE \"name\" ILIKE 'a%'");
    REQUIRE(transpile(sql, SQLDialect::Snowflake) ==
            "SELECT * FROM \"t\" WHERE \"name\" ILIKE 'a%'");
    REQUIRE(transpile(sql, SQLDialect::DuckDB) == "SELECT * FROM \"t\" WHERE \"name\" ILIKE 'a%'");
}

TEST_CASE("Dialect combo - ILIKE polyfilled with LOWER() for MySQL", "[dialect-combo][ilike]") {
    REQUIRE(transpile("SELECT * FROM t WHERE name ILIKE 'a%'", SQLDialect::MySQL) ==
            "SELECT * FROM `t` WHERE LOWER(`name`) LIKE LOWER('a%')");
}

// ============================================================================
// Combined query - several features at once
// ============================================================================

TEST_CASE("Dialect combo - one query, four dialects, all features", "[dialect-combo][combined]") {
    const std::string sql = "SELECT id, name FROM users WHERE active = TRUE AND age >= 18 LIMIT 25";

    REQUIRE(transpile(sql, SQLDialect::ANSI) ==
            "SELECT \"id\", \"name\" FROM \"users\" "
            "WHERE \"active\" = TRUE AND \"age\" >= 18 LIMIT 25");
    REQUIRE(transpile(sql, SQLDialect::PostgreSQL) ==
            "SELECT \"id\", \"name\" FROM \"users\" "
            "WHERE \"active\" = TRUE AND \"age\" >= 18 LIMIT 25");
    REQUIRE(transpile(sql, SQLDialect::MySQL) == "SELECT `id`, `name` FROM `users` "
                                                 "WHERE `active` = 1 AND `age` >= 18 LIMIT 25");
    REQUIRE(transpile(sql, SQLDialect::SQLServer) == "SELECT TOP 25 [id], [name] FROM [users] "
                                                     "WHERE [active] = 1 AND [age] >= 18");
    REQUIRE(transpile(sql, SQLDialect::Firebird) ==
            "SELECT FIRST 25 \"id\", \"name\" FROM \"users\" "
            "WHERE \"active\" = TRUE AND \"age\" >= 18");
}

TEST_CASE("Dialect combo - identifier quote trait matches generated quoting",
          "[dialect-combo][traits]") {
    REQUIRE(SQLDialectTraits::get_features(SQLDialect::ANSI).identifier_quote == '"');
    REQUIRE(SQLDialectTraits::get_features(SQLDialect::MySQL).identifier_quote == '`');
    REQUIRE(SQLDialectTraits::get_features(SQLDialect::SQLServer).identifier_quote == '[');
    REQUIRE(SQLDialectTraits::get_features(SQLDialect::BigQuery).identifier_quote == '`');
    REQUIRE(SQLDialectTraits::get_features(SQLDialect::Snowflake).identifier_quote == '"');
}
