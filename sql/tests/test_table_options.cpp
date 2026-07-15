// Wave 2: CREATE TABLE trailing table options (ENGINE=, AUTO_INCREMENT=,
// DEFAULT CHARSET=, COMMENT=, DISTSTYLE/DISTKEY/SORTKEY, PARTITION BY, ...).
//
// Previously these were consumed and silently discarded (see the old
// "skips unmodeled trailing table options" comment in parse_create_table).
// They are now modeled as an ordered list of (name, value) pairs on
// CreateTableStmt and regenerated verbatim - never dropped, never gated by
// dialect (every dialect's own trailing options round-trip through it).

#include <catch2/catch_test_macros.hpp>
#include <libglot/sql/parser.h>
#include <libglot/sql/generator.h>
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

TEST_CASE("Table options - single ENGINE=", "[table-options]") {
    REQUIRE(transpile("CREATE TABLE t (id INT) ENGINE=InnoDB", SQLDialect::MySQL)
            == "CREATE TABLE `t` (`id` INT) ENGINE=InnoDB");
}

TEST_CASE("Table options - MySQL combination", "[table-options]") {
    REQUIRE(transpile(
                "CREATE TABLE t (id INT) ENGINE=InnoDB AUTO_INCREMENT=10 "
                "DEFAULT CHARSET=utf8mb4 COMMENT='hi'",
                SQLDialect::MySQL)
            == "CREATE TABLE `t` (`id` INT) ENGINE=InnoDB AUTO_INCREMENT=10 "
               "DEFAULT CHARSET=utf8mb4 COMMENT='hi'");
}

TEST_CASE("Table options - COLLATE=", "[table-options]") {
    REQUIRE(transpile("CREATE TABLE t (id INT) COLLATE=utf8mb4_general_ci", SQLDialect::MySQL)
            == "CREATE TABLE `t` (`id` INT) COLLATE=utf8mb4_general_ci");
}

TEST_CASE("Table options - bare DISTSTYLE KEY (Redshift)", "[table-options]") {
    REQUIRE(transpile("CREATE TABLE t (id INT) DISTSTYLE KEY", SQLDialect::Redshift)
            == "CREATE TABLE \"t\" (\"id\" INT) DISTSTYLE KEY");
}

TEST_CASE("Table options - DISTSTYLE + DISTKEY + SORTKEY (Redshift)", "[table-options]") {
    REQUIRE(transpile("CREATE TABLE t (id INT) DISTSTYLE KEY DISTKEY(id) SORTKEY(ts)",
                       SQLDialect::Redshift)
            == "CREATE TABLE \"t\" (\"id\" INT) DISTSTYLE KEY DISTKEY(id) SORTKEY(ts)");
}

TEST_CASE("Table options - PARTITION BY with a parenthesized partition list", "[table-options]") {
    const std::string sql =
        "CREATE TABLE t (id INT) PARTITION BY RANGE (id) "
        "(PARTITION p0 VALUES LESS THAN (10), PARTITION p1 VALUES LESS THAN (20))";
    REQUIRE(transpile(sql, SQLDialect::MySQL)
            == "CREATE TABLE `t` (`id` INT) PARTITION BY RANGE (id) "
               "(PARTITION p0 VALUES LESS THAN (10), PARTITION p1 VALUES LESS THAN (20))");
}

TEST_CASE("Table options - AST shape", "[table-options]") {
    libglot::Arena arena;
    SQLParser parser(arena, "CREATE TABLE t (id INT) ENGINE=InnoDB DISTSTYLE KEY", SQLDialect::MySQL);
    auto* ast = static_cast<CreateTableStmt*>(parser.parse_top_level());
    REQUIRE(ast->table_options.size() == 2);
    REQUIRE(ast->table_options[0].name == "ENGINE");
    REQUIRE(ast->table_options[0].value == "InnoDB");
    REQUIRE(ast->table_options[0].has_equals == true);
    REQUIRE(ast->table_options[1].name == "DISTSTYLE");
    REQUIRE(ast->table_options[1].value == "KEY");
    REQUIRE(ast->table_options[1].has_equals == false);
}

TEST_CASE("Table options - no trailing options is unaffected", "[table-options]") {
    REQUIRE(transpile("CREATE TABLE t (id INT)", SQLDialect::MySQL) == "CREATE TABLE `t` (`id` INT)");
}

TEST_CASE("Table options - fixed point across dialects", "[table-options][roundtrip]") {
    const std::string queries[] = {
        "CREATE TABLE t (id INT) ENGINE=InnoDB",
        "CREATE TABLE t (id INT) ENGINE=InnoDB AUTO_INCREMENT=10 DEFAULT CHARSET=utf8mb4 COMMENT='hi'",
        "CREATE TABLE t (id INT) DISTSTYLE KEY DISTKEY(id) SORTKEY(ts)",
    };
    for (const auto& q : queries) {
        const std::string g1 = transpile(q, SQLDialect::MySQL);
        REQUIRE(transpile(g1, SQLDialect::MySQL) == g1);
    }
}

TEST_CASE("Table options - missing '(' after table name is a clean ParseError",
          "[table-options][error]") {
    libglot::Arena arena;
    SQLParser parser(arena, "CREATE TABLE t id INT) ENGINE=InnoDB", SQLDialect::MySQL);
    REQUIRE_THROWS_AS(parser.parse_top_level(), libglot::ParseError);
}
