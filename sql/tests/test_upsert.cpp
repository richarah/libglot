// INSERT upsert forms:
//   PostgreSQL: ON CONFLICT [(col, ...)] DO NOTHING / DO UPDATE SET c = ... [WHERE ...]
//   MySQL:      ON DUPLICATE KEY UPDATE c = VALUES(c)
//
// Design choice (documented in generator.h and the feature matrix): both
// forms parse into their own dedicated AST node regardless of dialect, but
// generation is gated - ON CONFLICT only generates for PostgreSQL and
// ON DUPLICATE KEY UPDATE only for MySQL/MariaDB. Cross-dialect transpile
// between the two (PG <-> MySQL) is NOT attempted: the conflict-target
// column list and EXCLUDED/VALUES() semantics do not map over cleanly, so
// generating a PG-parsed ON CONFLICT for MySQL (or vice versa) throws
// std::logic_error with an explanatory message instead of guessing.

#include <catch2/catch_test_macros.hpp>
#include <libglot/sql/generator.h>
#include <libglot/sql/parser.h>
#include <libglot/util/arena.h>

#include <stdexcept>
#include <string>

using namespace libglot::sql;

namespace {

std::string gen(const std::string& sql, SQLDialect parse_d, SQLDialect gen_d) {
    libglot::Arena arena;
    SQLParser parser(arena, sql, parse_d);
    auto ast = parser.parse_top_level();
    SQLGenerator generator(gen_d);
    return generator.generate(ast);
}

std::string pg(const std::string& sql) {
    return gen(sql, SQLDialect::PostgreSQL, SQLDialect::PostgreSQL);
}

std::string mysql(const std::string& sql) {
    return gen(sql, SQLDialect::MySQL, SQLDialect::MySQL);
}

} // namespace

// ============================================================================
// PostgreSQL ON CONFLICT
// ============================================================================

TEST_CASE("ON CONFLICT DO NOTHING - exact string", "[upsert][postgresql]") {
    REQUIRE(pg("INSERT INTO t (id, name) VALUES (1, 'a') ON CONFLICT (id) DO NOTHING") ==
            "INSERT INTO \"t\" (\"id\", \"name\") VALUES (1, 'a') ON CONFLICT (\"id\") DO NOTHING");
    REQUIRE(pg("INSERT INTO t (id) VALUES (1) ON CONFLICT DO NOTHING") ==
            "INSERT INTO \"t\" (\"id\") VALUES (1) ON CONFLICT DO NOTHING");
}

TEST_CASE("ON CONFLICT DO UPDATE SET ... EXCLUDED - exact string", "[upsert][postgresql]") {
    REQUIRE(pg("INSERT INTO t (id, qty) VALUES (1, 1) "
               "ON CONFLICT (id) DO UPDATE SET qty = EXCLUDED.qty") ==
            "INSERT INTO \"t\" (\"id\", \"qty\") VALUES (1, 1) "
            "ON CONFLICT (\"id\") DO UPDATE SET \"qty\" = EXCLUDED.\"qty\"");
}

TEST_CASE("ON CONFLICT DO UPDATE SET ... WHERE - exact string", "[upsert][postgresql]") {
    REQUIRE(pg("INSERT INTO t (id, qty) VALUES (1, 1) "
               "ON CONFLICT (id) DO UPDATE SET qty = EXCLUDED.qty WHERE t.active") ==
            "INSERT INTO \"t\" (\"id\", \"qty\") VALUES (1, 1) "
            "ON CONFLICT (\"id\") DO UPDATE SET \"qty\" = EXCLUDED.\"qty\" WHERE \"t\".\"active\"");
}

TEST_CASE("ON CONFLICT with multiple conflict columns and RETURNING", "[upsert][postgresql]") {
    REQUIRE(pg("INSERT INTO t (a, b) VALUES (1, 2) "
               "ON CONFLICT (a, b) DO UPDATE SET a = EXCLUDED.a RETURNING id") ==
            "INSERT INTO \"t\" (\"a\", \"b\") VALUES (1, 2) "
            "ON CONFLICT (\"a\", \"b\") DO UPDATE SET \"a\" = EXCLUDED.\"a\" RETURNING \"id\"");
}

// ============================================================================
// MySQL ON DUPLICATE KEY UPDATE
// ============================================================================

TEST_CASE("ON DUPLICATE KEY UPDATE ... VALUES(c) - exact string", "[upsert][mysql]") {
    REQUIRE(mysql("INSERT INTO t (id, qty) VALUES (1, 1) "
                  "ON DUPLICATE KEY UPDATE qty = VALUES(qty)") ==
            "INSERT INTO `t` (`id`, `qty`) VALUES (1, 1) "
            "ON DUPLICATE KEY UPDATE `qty` = VALUES(`qty`)");
}

TEST_CASE("ON DUPLICATE KEY UPDATE with multiple assignments", "[upsert][mysql]") {
    REQUIRE(mysql("INSERT INTO t (id, a, b) VALUES (1, 2, 3) "
                  "ON DUPLICATE KEY UPDATE a = VALUES(a), b = b + 1") ==
            "INSERT INTO `t` (`id`, `a`, `b`) VALUES (1, 2, 3) "
            "ON DUPLICATE KEY UPDATE `a` = VALUES(`a`), `b` = `b` + 1");
}

// ============================================================================
// Same-dialect fixed point
// ============================================================================

TEST_CASE("Upsert forms are a fixed point in their own dialect", "[upsert][fixpoint]") {
    const std::string pg_queries[] = {
        "INSERT INTO t (id) VALUES (1) ON CONFLICT (id) DO NOTHING",
        "INSERT INTO t (id, c) VALUES (1, 1) ON CONFLICT (id) DO UPDATE SET c = EXCLUDED.c",
        "INSERT INTO t (id, c) VALUES (1, 1) ON CONFLICT (id) DO UPDATE SET c = EXCLUDED.c WHERE "
        "t.active",
    };
    for (const auto& q : pg_queries) {
        const std::string g1 = pg(q);
        REQUIRE(pg(g1) == g1);
    }

    const std::string mysql_queries[] = {
        "INSERT INTO t (id, c) VALUES (1, 1) ON DUPLICATE KEY UPDATE c = VALUES(c)",
    };
    for (const auto& q : mysql_queries) {
        const std::string g1 = mysql(q);
        REQUIRE(mysql(g1) == g1);
    }
}

// ============================================================================
// Cross-dialect: not attempted, throws a clear error
// ============================================================================

TEST_CASE("ON CONFLICT throws for non-PostgreSQL targets", "[upsert][error]") {
    for (auto d : {SQLDialect::MySQL, SQLDialect::ANSI, SQLDialect::SQLServer}) {
        REQUIRE_THROWS_AS(gen("INSERT INTO t (id) VALUES (1) ON CONFLICT (id) DO NOTHING",
                              SQLDialect::PostgreSQL, d),
                          std::logic_error);
    }
}

TEST_CASE("ON DUPLICATE KEY UPDATE throws for non-MySQL targets", "[upsert][error]") {
    for (auto d : {SQLDialect::PostgreSQL, SQLDialect::ANSI, SQLDialect::SQLServer}) {
        REQUIRE_THROWS_AS(
            gen("INSERT INTO t (id, c) VALUES (1, 1) ON DUPLICATE KEY UPDATE c = VALUES(c)",
                SQLDialect::MySQL, d),
            std::logic_error);
    }
}

// ============================================================================
// Clean parse errors for malformed clauses
// ============================================================================

TEST_CASE("Upsert - malformed clauses are clean ParseErrors", "[upsert][error]") {
    {
        libglot::Arena arena;
        SQLParser parser(arena, "INSERT INTO t (id) VALUES (1) ON CONFLICT (id) DO",
                         SQLDialect::PostgreSQL);
        REQUIRE_THROWS_AS(parser.parse_top_level(), libglot::ParseError);
    }
    {
        libglot::Arena arena;
        SQLParser parser(arena, "INSERT INTO t (id) VALUES (1) ON DUPLICATE KEY",
                         SQLDialect::MySQL);
        REQUIRE_THROWS_AS(parser.parse_top_level(), libglot::ParseError);
    }
}
