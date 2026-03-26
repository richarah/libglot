#include <catch2/catch_test_macros.hpp>
#include <libglot/sql/parser.h>
#include <libglot/sql/generator.h>
#include <libglot/sql/dialect_traits.h>
#include <libglot/util/arena.h>

using namespace libglot::sql;

TEST_CASE("Transpiler - Simple parse and generate", "[transpiler]") {
    libglot::Arena arena;
    std::string sql = "SELECT * FROM users";

    SQLParser parser(arena, sql);
    auto expr = parser.parse_top_level();
    auto stmt = static_cast<SelectStmt*>(expr);

    REQUIRE(stmt != nullptr);
    REQUIRE(stmt->columns.size() == 1);

    SQLGenerator gen(SQLDialect::ANSI);
    std::string output = gen.generate(expr);
    REQUIRE(output.find("SELECT") != std::string::npos);
    REQUIRE(output.find("users") != std::string::npos);
}

// TEST_CASE("Transpiler - Parse, optimize, generate", "[transpiler]") {
//     // TODO: Optimizer not implemented in libglot-sql
// }

TEST_CASE("Transpiler - Full transpile API", "[transpiler]") {
    std::string sql = "SELECT id, name FROM users WHERE active = 1";

    libglot::Arena arena;
    SQLParser parser(arena, sql);
    auto ast = parser.parse_top_level();
    SQLGenerator gen(SQLDialect::ANSI);
    std::string output = gen.generate(ast);

    REQUIRE(!output.empty());
    REQUIRE(output.find("SELECT") != std::string::npos);
    REQUIRE(output.find("FROM") != std::string::npos);
}

TEST_CASE("Transpiler - Complex query transpilation", "[transpiler]") {
    std::string sql = "SELECT u.id, u.name FROM users u WHERE u.age > 18";

    libglot::Arena arena;
    SQLParser parser(arena, sql);
    auto ast = parser.parse_top_level();
    SQLGenerator gen(SQLDialect::ANSI);
    std::string output = gen.generate(ast);

    REQUIRE(!output.empty());
    REQUIRE(output.find("SELECT") != std::string::npos);
}

// TEST_CASE("Optimizer - Qualify columns", "[optimizer]") {
//     // TODO: Optimizer not implemented in libglot-sql
// }

TEST_CASE("Dialect - Identifier quotes", "[dialect]") {
    auto& mysql = SQLDialectTraits::get_features(SQLDialect::MySQL);
    auto& postgres = SQLDialectTraits::get_features(SQLDialect::PostgreSQL);

    REQUIRE(mysql.identifier_quote == '`');
    REQUIRE(postgres.identifier_quote == '"');
}

TEST_CASE("Dialect - Feature support", "[dialect]") {
    auto& ansi = SQLDialectTraits::get_features(SQLDialect::ANSI);
    auto& postgres = SQLDialectTraits::get_features(SQLDialect::PostgreSQL);
    auto& tsql = SQLDialectTraits::get_features(SQLDialect::SQLServer);

    // ANSI and PostgreSQL support LIMIT/OFFSET
    REQUIRE(ansi.supports_limit_offset == true);
    REQUIRE(postgres.supports_limit_offset == true);
    REQUIRE(postgres.supports_ilike == true);  // PostgreSQL supports ILIKE
}

TEST_CASE("Dialect - Names", "[dialect]") {
    REQUIRE(SQLDialectTraits::name(SQLDialect::DuckDB) == "DuckDB");
    REQUIRE(SQLDialectTraits::name(SQLDialect::BigQuery) == "BigQuery");
}
