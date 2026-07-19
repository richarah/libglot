// MySQL family conformance (docs/ROADMAP.md stage 2, issue #3 follow-on):
// promoting MariaDB, TiDB, and SingleStore to first-class now that dialect
// family inheritance exists (stage 1, commit 4c5f58e).
//
// All three inherit `mysql_base()` in dialect_traits.h with no delta
// (backtick identifiers, 1/0 boolean literals, LIMIT/OFFSET) - this is a
// correction from before this pass: TiDB and SingleStore previously
// overrode true_literal/false_literal to "TRUE"/"FALSE" (issue #5's
// illustrative example of a delta overriding a base value). That override
// could not be confirmed as a real dialect difference while promoting them
// here - both are MySQL wire-compatible forks with no documented boolean-
// literal display difference from MySQL - so per the rules of honesty it
// was removed; see the comment in dialect_traits.h.
//
// Real, testable deltas encoded here (MariaDB only):
//   - MariaDB supports CREATE/DROP/ALTER SEQUENCE and NEXTVAL(seq)/
//     LASTVAL(seq) (its CURRVAL equivalent) - unlike MySQL, which has none
//     of this. This also required a real fix: MariaDB's NEXTVAL/LASTVAL
//     take a bare *identifier* argument (`NEXTVAL(seq)`), not a quoted
//     string like PostgreSQL's `nextval('seq')` - the generic function-
//     style fallback every other sequence-supporting dialect used was
//     wrong for MariaDB specifically.
//   - MariaDB supports RETURNING on INSERT (10.5+) and DELETE (10.0+), but
//     never added it for UPDATE. Encoding this honestly required fixing a
//     pre-existing gap first: the generic RETURNING generation path had no
//     dialect gate *at all* before this pass, so MySQL itself would have
//     silently produced invalid `... RETURNING ...` SQL. Fixed alongside
//     the MariaDB delta - without it there would be nothing to actually
//     distinguish MariaDB's RETURNING from MySQL's lack of it.
//
// Deliberately NOT encoded for TiDB/SingleStore (uncertain, so left on the
// generic inherited/default path rather than guessed at either direction):
//   - Sequence support: TiDB is believed to support CREATE SEQUENCE (added
//     in TiDB 4.0), but whether its NEXTVAL/LASTVAL take a bare identifier
//     (MariaDB-style) or a quoted string (the generic fallback used by
//     everything else) was not verified with confidence, so neither a
//     positive nor a negative claim is tested here.
//   - RETURNING: not known to be supported by either engine, but not
//     confirmed absent either, so left un-restricted (same generic
//     fallback as most of the other 45 dialects) and un-tested.
//   - ON DUPLICATE KEY UPDATE and MATCH ... AGAINST (fulltext) were
//     already gated to exactly {MySQL, MariaDB} before this stage
//     (generator.h), deliberately not widened to is_family(MySQL) because
//     TiDB/SingleStore support was never verified - left untouched here,
//     and tested below to confirm they still throw.

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

void require_fixpoint(const std::string& sql, SQLDialect dialect) {
    const std::string g1 = transpile(sql, dialect);
    const std::string g2 = transpile(g1, dialect);
    REQUIRE(g1 == g2);
}

constexpr SQLDialect kMySqlFamilyMembers[] = {SQLDialect::MariaDB, SQLDialect::TiDB,
                                              SQLDialect::SingleStore};

} // namespace

// ============================================================================
// Traits
// ============================================================================

TEST_CASE("MySQL family - every promoted member's family() is MySQL",
          "[dialect][mysqlfamily][traits]") {
    for (auto d : kMySqlFamilyMembers) {
        INFO("dialect = " << SQLDialectTraits::name(d));
        CHECK(SQLDialectTraits::is_family(d, SQLDialectFamily::MySQL));
    }
}

TEST_CASE("MySQL family - every promoted member inherits mysql_base() verbatim (no delta)",
          "[dialect][mysqlfamily][traits]") {
    for (auto d : kMySqlFamilyMembers) {
        INFO("dialect = " << SQLDialectTraits::name(d));
        const auto& f = SQLDialectTraits::get_features(d);
        CHECK(f.identifier_quote == '`');
        CHECK(f.string_quote == '\'');
        CHECK(f.supports_limit_offset);
        CHECK_FALSE(f.supports_ilike);
        CHECK(std::string(f.true_literal) == "1");
        CHECK(std::string(f.false_literal) == "0");
    }
}

// ============================================================================
// Common inherited behavior: backtick quoting, 1/0 booleans, LIMIT/OFFSET
// ============================================================================

TEST_CASE("MySQL family - identifiers backtick-quoted for every member",
          "[dialect][mysqlfamily][quoting]") {
    for (auto d : kMySqlFamilyMembers) {
        INFO("dialect = " << SQLDialectTraits::name(d));
        CHECK(transpile("SELECT id, name FROM users", d) ==
              "SELECT `id`, `name` FROM `users`");
    }
}

TEST_CASE("MySQL family - 1/0 boolean literals for every member",
          "[dialect][mysqlfamily][boolean]") {
    for (auto d : kMySqlFamilyMembers) {
        INFO("dialect = " << SQLDialectTraits::name(d));
        CHECK(transpile("SELECT true, false", d) == "SELECT 1, 0");
    }
}

TEST_CASE("MySQL family - LIMIT/OFFSET round-trips for every member",
          "[dialect][mysqlfamily][limit]") {
    for (auto d : kMySqlFamilyMembers) {
        INFO("dialect = " << SQLDialectTraits::name(d));
        CHECK(transpile("SELECT * FROM t LIMIT 10 OFFSET 5", d) ==
              "SELECT * FROM `t` LIMIT 10 OFFSET 5");
    }
}

TEST_CASE("MySQL family - common inherited behavior is a fixed point",
          "[dialect][mysqlfamily][roundtrip]") {
    for (auto d : kMySqlFamilyMembers) {
        require_fixpoint("SELECT id, name FROM users WHERE id > 10 LIMIT 5", d);
    }
}

// ============================================================================
// MariaDB: sequences (NEXTVAL/LASTVAL, bare identifier - not MySQL's lack
// of any sequence object, and not PostgreSQL's quoted-string convention)
// ============================================================================

TEST_CASE("MariaDB - CREATE SEQUENCE round-trips (unlike MySQL, which has none)",
          "[dialect][mariadb][sequence]") {
    REQUIRE(transpile("CREATE SEQUENCE seq_a START WITH 1 INCREMENT BY 1", SQLDialect::MariaDB) ==
            "CREATE SEQUENCE `seq_a` START WITH 1 INCREMENT BY 1");
}

TEST_CASE("MariaDB - NEXTVAL(seq) round-trips with a bare (backtick-quotable) identifier "
          "argument, not a quoted string",
          "[dialect][mariadb][sequence]") {
    REQUIRE(transpile("SELECT NEXTVAL(seq_a)", SQLDialect::MariaDB) ==
            "SELECT NEXTVAL(`seq_a`)");
}

TEST_CASE("MariaDB - LASTVAL(seq) (its CURRVAL equivalent) round-trips",
          "[dialect][mariadb][sequence]") {
    REQUIRE(transpile("SELECT LASTVAL(seq_a)", SQLDialect::MariaDB) ==
            "SELECT LASTVAL(`seq_a`)");
}

TEST_CASE("MariaDB - CURRVAL(seq) also parses and canonicalizes to LASTVAL",
          "[dialect][mariadb][sequence]") {
    // CURRVAL is not MariaDB's real spelling, but the parser accepts it as
    // an alternate surface form of the same "current value" concept (like
    // Oracle's CURRVAL / PostgreSQL's currval) and MariaDB always
    // regenerates its own real spelling, LASTVAL.
    REQUIRE(transpile("SELECT CURRVAL(seq_a)", SQLDialect::MariaDB) ==
            "SELECT LASTVAL(`seq_a`)");
}

TEST_CASE("MariaDB - sequences fixpoint", "[dialect][mariadb][sequence][roundtrip]") {
    require_fixpoint("CREATE SEQUENCE `seq_a` START WITH 1 INCREMENT BY 1", SQLDialect::MariaDB);
    require_fixpoint("SELECT NEXTVAL(seq_a)", SQLDialect::MariaDB);
    require_fixpoint("SELECT LASTVAL(seq_a)", SQLDialect::MariaDB);
}

TEST_CASE("MariaDB - sequences still throw for plain MySQL (the actual delta being tested)",
          "[dialect][mariadb][sequence][error]") {
    libglot::Arena arena;
    SQLParser parser(arena, "SELECT NEXTVAL(seq_a)", SQLDialect::MariaDB);
    auto* ast = parser.parse_top_level();
    SQLGenerator gen(SQLDialect::MySQL);
    REQUIRE_THROWS_AS(gen.generate(ast), std::logic_error);
}

// ============================================================================
// MariaDB: RETURNING on INSERT/DELETE (not UPDATE) - unlike MySQL, which
// has none at all
// ============================================================================

TEST_CASE("MariaDB - RETURNING on INSERT round-trips", "[dialect][mariadb][returning]") {
    REQUIRE(transpile("INSERT INTO t (a) VALUES (1) RETURNING id", SQLDialect::MariaDB) ==
            "INSERT INTO `t` (`a`) VALUES (1) RETURNING `id`");
}

TEST_CASE("MariaDB - RETURNING on DELETE round-trips", "[dialect][mariadb][returning]") {
    REQUIRE(transpile("DELETE FROM t WHERE a = 1 RETURNING id", SQLDialect::MariaDB) ==
            "DELETE FROM `t` WHERE `a` = 1 RETURNING `id`");
}

TEST_CASE("MariaDB - RETURNING on UPDATE throws (MariaDB never added this, confirmed - not a "
          "guess)",
          "[dialect][mariadb][returning][error]") {
    libglot::Arena arena;
    SQLParser parser(arena, "UPDATE t SET a = 1 RETURNING id", SQLDialect::MariaDB);
    auto* ast = parser.parse_top_level();
    SQLGenerator gen(SQLDialect::MariaDB);
    REQUIRE_THROWS_AS(gen.generate(ast), std::logic_error);
}

TEST_CASE("MariaDB - RETURNING fixpoint on the two supported statement kinds",
          "[dialect][mariadb][returning][roundtrip]") {
    require_fixpoint("INSERT INTO t (a) VALUES (1) RETURNING id", SQLDialect::MariaDB);
    require_fixpoint("DELETE FROM t WHERE a = 1 RETURNING id", SQLDialect::MariaDB);
}

TEST_CASE("MySQL - RETURNING throws on every statement kind (this is the delta MariaDB is "
          "measured against; also a pre-existing bug fix - see file header)",
          "[dialect][mysql][returning][error]") {
    const std::string queries[] = {
        "INSERT INTO t (a) VALUES (1) RETURNING id",
        "UPDATE t SET a = 1 RETURNING id",
        "DELETE FROM t WHERE a = 1 RETURNING id",
    };
    for (const auto& q : queries) {
        libglot::Arena arena;
        SQLParser parser(arena, q, SQLDialect::PostgreSQL);
        auto* ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::MySQL);
        REQUIRE_THROWS_AS(gen.generate(ast), std::logic_error);
    }
}

// ============================================================================
// TiDB / SingleStore: deliberately-not-extended restrictions (ON DUPLICATE
// KEY UPDATE, MATCH ... AGAINST) - confirms the existing explicit
// MySQL/MariaDB-only gates were not silently widened by expressing them
// via is_family() during the stage-1 refactor
// ============================================================================

TEST_CASE("TiDB/SingleStore - ON DUPLICATE KEY UPDATE still throws (never verified for these "
          "two, so not widened from the pre-existing MySQL/MariaDB-only gate)",
          "[dialect][mysqlfamily][error]") {
    libglot::Arena arena;
    SQLParser parser(arena, "INSERT INTO t (a) VALUES (1) ON DUPLICATE KEY UPDATE a = 2",
                     SQLDialect::MySQL);
    auto* ast = parser.parse_top_level();
    for (auto d : {SQLDialect::TiDB, SQLDialect::SingleStore}) {
        SQLGenerator gen(d);
        REQUIRE_THROWS_AS(gen.generate(ast), std::logic_error);
    }
}

TEST_CASE("TiDB/SingleStore - MATCH ... AGAINST (fulltext) still throws (same reasoning)",
          "[dialect][mysqlfamily][error]") {
    libglot::Arena arena;
    SQLParser parser(arena, "SELECT * FROM t WHERE MATCH (a) AGAINST ('x')", SQLDialect::MySQL);
    auto* ast = parser.parse_top_level();
    for (auto d : {SQLDialect::TiDB, SQLDialect::SingleStore}) {
        SQLGenerator gen(d);
        REQUIRE_THROWS_AS(gen.generate(ast), std::logic_error);
    }
}
