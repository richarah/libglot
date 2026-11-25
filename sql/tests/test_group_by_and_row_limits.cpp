// Tests for the GROUP BY extensions (ROLLUP, CUBE, GROUPING SETS, WITH ROLLUP and GROUPING()) and for
// LIMIT / OFFSET / FETCH / TOP (with PERCENT and WITH TIES) read as one row limit and written per dialect.
//
// Outputs are compared after removing whitespace and upper-casing, with FETCH NEXT read as FETCH FIRST
// and ROW read as ROWS (the standard treats each pair as the same).
// "Fails with an error" means generate() throws; any exception type is accepted.
#include <catch2/catch_test_macros.hpp>
#include <libglot/sql/parser.h>
#include <libglot/sql/generator.h>
#include <libglot/util/arena.h>
#include <string>
#include <vector>
#include <tuple>
#include <regex>

using namespace libglot::sql;

namespace {

std::string gen(const std::string& sql, SQLDialect dialect) {
    libglot::Arena arena;
    SQLParser parser(arena, sql);
    auto* ast = parser.parse_top_level();
    SQLGenerator generator(dialect);
    return generator.generate(ast);
}

void replace_all(std::string& s, const std::string& from, const std::string& to) {
    for (size_t pos = s.find(from); pos != std::string::npos; pos = s.find(from, pos + to.size())) {
        s.replace(pos, from.size(), to);
    }
}

std::string norm(std::string s) {
    std::string out;
    for (char c : s) {
        if (c == ' ' || c == '\n' || c == '\t' || c == '\r') continue;
        out += (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c;
    }
    replace_all(out, "FETCHNEXT", "FETCHFIRST");
    replace_all(out, "ROWONLY", "ROWSONLY");
    replace_all(out, "ROWFETCH", "ROWSFETCH");
    replace_all(out, "ROWWITHTIES", "ROWSWITHTIES");
    if (out.size() >= 3 && out.compare(out.size() - 3, 3, "ROW") == 0) out += "S";
    return out;
}

void same(const std::string& sql, SQLDialect dialect, const std::string& expected) {
    INFO("input: " << sql);
    CHECK(norm(gen(sql, dialect)) == norm(expected));
}

void one_of(const std::string& sql, SQLDialect dialect, const std::vector<std::string>& expected) {
    INFO("input: " << sql);
    const std::string got = norm(gen(sql, dialect));
    INFO("got: " << got);
    bool ok = false;
    for (const auto& e : expected) ok = ok || got == norm(e);
    CHECK(ok);
}

// The normalised output must match the pattern (written for the normalised form).
void matches(const std::string& sql, SQLDialect dialect, const std::string& pattern) {
    INFO("input: " << sql);
    const std::string got = norm(gen(sql, dialect));
    INFO("got: " << got);
    CHECK(std::regex_match(got, std::regex(pattern)));
}

void fails(const std::string& sql, SQLDialect dialect) {
    INFO("input: " << sql);
    CHECK_THROWS(gen(sql, dialect));
}

const SQLDialect STANDARD[] = {SQLDialect::PostgreSQL, SQLDialect::Oracle, SQLDialect::DB2, SQLDialect::SQLServer};

std::string q(SQLDialect d, const std::string& name) {
    if (d == SQLDialect::SQLServer) return "[" + name + "]";
    if (d == SQLDialect::MySQL || d == SQLDialect::MariaDB) return "`" + name + "`";
    return "\"" + name + "\"";
}

}  // namespace

// ---------------------------------------------------------------- GROUP BY extensions

TEST_CASE("ROLLUP is written as ROLLUP for the standard dialects", "[rollup]") {
    for (auto d : STANDARD) {
        same("SELECT a, b, SUM(c) FROM t GROUP BY ROLLUP (a, b)", d,
             "SELECT " + q(d, "a") + ", " + q(d, "b") + ", SUM(" + q(d, "c") + ") FROM " + q(d, "t") +
                 " GROUP BY ROLLUP(" + q(d, "a") + ", " + q(d, "b") + ")");
    }
}

TEST_CASE("MySQL's WITH ROLLUP is read as ROLLUP over the whole GROUP BY", "[rollup]") {
    for (auto d : STANDARD) {
        same("SELECT a, b, SUM(c) FROM t GROUP BY a, b WITH ROLLUP", d,
             "SELECT " + q(d, "a") + ", " + q(d, "b") + ", SUM(" + q(d, "c") + ") FROM " + q(d, "t") +
                 " GROUP BY ROLLUP(" + q(d, "a") + ", " + q(d, "b") + ")");
    }
}

TEST_CASE("ROLLUP is written as WITH ROLLUP for MySQL and MariaDB", "[rollup]") {
    for (auto d : {SQLDialect::MySQL, SQLDialect::MariaDB}) {
        same("SELECT a, b, SUM(c) FROM t GROUP BY ROLLUP (a, b)", d,
             "SELECT `a`, `b`, SUM(`c`) FROM `t` GROUP BY `a`, `b` WITH ROLLUP");
        same("SELECT a, b, SUM(c) FROM t GROUP BY a, b WITH ROLLUP", d,
             "SELECT `a`, `b`, SUM(`c`) FROM `t` GROUP BY `a`, `b` WITH ROLLUP");
        same("SELECT a, SUM(c) FROM t GROUP BY ROLLUP (a) HAVING SUM(c) > 1 ORDER BY a", d,
             "SELECT `a`, SUM(`c`) FROM `t` GROUP BY `a` WITH ROLLUP HAVING SUM(`c`) > 1 ORDER BY `a`");
    }
}

TEST_CASE("A ROLLUP that is not the whole GROUP BY cannot be written for MySQL", "[rollup]") {
    fails("SELECT x, a, SUM(c) FROM t GROUP BY x, ROLLUP (a)", SQLDialect::MySQL);
    fails("SELECT x, a, SUM(c) FROM t GROUP BY x, ROLLUP (a)", SQLDialect::MariaDB);
    same("SELECT x, a, SUM(c) FROM t GROUP BY x, ROLLUP (a)", SQLDialect::PostgreSQL,
         R"(SELECT "x", "a", SUM("c") FROM "t" GROUP BY "x", ROLLUP("a"))");
}

TEST_CASE("CUBE is written as CUBE where it exists and fails elsewhere", "[cube]") {
    for (auto d : STANDARD) {
        same("SELECT a, b, SUM(c) FROM t GROUP BY CUBE (a, b)", d,
             "SELECT " + q(d, "a") + ", " + q(d, "b") + ", SUM(" + q(d, "c") + ") FROM " + q(d, "t") +
                 " GROUP BY CUBE(" + q(d, "a") + ", " + q(d, "b") + ")");
    }
    fails("SELECT a, b, SUM(c) FROM t GROUP BY CUBE (a, b)", SQLDialect::MySQL);
    fails("SELECT a, b, SUM(c) FROM t GROUP BY CUBE (a, b)", SQLDialect::MariaDB);
    fails("SELECT a, b, SUM(c) FROM t GROUP BY CUBE (a, b)", SQLDialect::SQLite);
}

TEST_CASE("GROUPING SETS keeps every set, including the empty one", "[grouping-sets]") {
    for (auto d : STANDARD) {
        const std::string a = q(d, "a"), b = q(d, "b"), c = q(d, "c"), t = q(d, "t");
        one_of("SELECT a, b, SUM(c) FROM t GROUP BY GROUPING SETS ((a, b), (a), ())", d,
               {"SELECT " + a + ", " + b + ", SUM(" + c + ") FROM " + t + " GROUP BY GROUPING SETS ((" + a + ", " + b + "), (" + a + "), ())",
                "SELECT " + a + ", " + b + ", SUM(" + c + ") FROM " + t + " GROUP BY GROUPING SETS ((" + a + ", " + b + "), " + a + ", ())"});
    }
}

TEST_CASE("GROUPING SETS from the prompt is not turned into a column", "[grouping-sets]") {
    one_of("SELECT a FROM t GROUP BY GROUPING SETS ((a), ())", SQLDialect::PostgreSQL,
           {R"(SELECT "a" FROM "t" GROUP BY GROUPING SETS (("a"), ()))",
            R"(SELECT "a" FROM "t" GROUP BY GROUPING SETS ("a", ()))"});
}

TEST_CASE("GROUPING SETS can hold a ROLLUP", "[grouping-sets]") {
    same("SELECT a, b, c FROM t GROUP BY GROUPING SETS (ROLLUP (a), (b, c))", SQLDialect::PostgreSQL,
         R"(SELECT "a", "b", "c" FROM "t" GROUP BY GROUPING SETS (ROLLUP("a"), ("b", "c")))");
}

TEST_CASE("GROUPING SETS fails for MySQL, MariaDB and SQLite", "[grouping-sets]") {
    // (a), (b) is not a ROLLUP, so MySQL and MariaDB have no way to write it.
    for (auto d : {SQLDialect::MySQL, SQLDialect::MariaDB, SQLDialect::SQLite}) {
        fails("SELECT a, b, SUM(c) FROM t GROUP BY GROUPING SETS ((a), (b))", d);
    }
}

TEST_CASE("SQLite has no ROLLUP", "[rollup]") {
    fails("SELECT a, SUM(c) FROM t GROUP BY ROLLUP (a)", SQLDialect::SQLite);
    fails("SELECT a, SUM(c) FROM t GROUP BY a WITH ROLLUP", SQLDialect::SQLite);
}

TEST_CASE("Lower-case keywords are read the same", "[rollup]") {
    same("select a, sum(c) from t group by rollup (a)", SQLDialect::PostgreSQL,
         R"(SELECT "a", SUM("c") FROM "t" GROUP BY ROLLUP("a"))");
    one_of("select a from t group by grouping sets ((a), ())", SQLDialect::PostgreSQL,
           {R"(SELECT "a" FROM "t" GROUP BY GROUPING SETS (("a"), ()))",
            R"(SELECT "a" FROM "t" GROUP BY GROUPING SETS ("a", ()))"});
    same("select a, sum(c) from t group by a with rollup", SQLDialect::PostgreSQL,
         R"(SELECT "a", SUM("c") FROM "t" GROUP BY ROLLUP("a"))");
}

TEST_CASE("GROUPING() is kept where it exists and fails for SQLite", "[grouping-function]") {
    same("SELECT a, GROUPING(a), SUM(c) FROM t GROUP BY ROLLUP (a)", SQLDialect::PostgreSQL,
         R"(SELECT "a", GROUPING("a"), SUM("c") FROM "t" GROUP BY ROLLUP("a"))");
    same("SELECT a, GROUPING(a), SUM(c) FROM t GROUP BY a WITH ROLLUP", SQLDialect::MySQL,
         "SELECT `a`, GROUPING(`a`), SUM(`c`) FROM `t` GROUP BY `a` WITH ROLLUP");
    same("SELECT a, GROUPING(a), SUM(c) FROM t GROUP BY ROLLUP (a)", SQLDialect::SQLServer,
         "SELECT [a], GROUPING([a]), SUM([c]) FROM [t] GROUP BY ROLLUP([a])");
    fails("SELECT a, GROUPING(a) FROM t GROUP BY a", SQLDialect::SQLite);
}

TEST_CASE("A column called rollup is still a column", "[rollup]") {
    same("SELECT rollup FROM t GROUP BY rollup", SQLDialect::PostgreSQL,
         R"(SELECT "rollup" FROM "t" GROUP BY "rollup")");
}

// ---------------------------------------------------------------- Row limits

TEST_CASE("LIMIT, OFFSET ... ROWS and FETCH are the same row limit in every checked dialect", "[limits]") {
    for (auto d : {SQLDialect::ANSI, SQLDialect::PostgreSQL, SQLDialect::MySQL, SQLDialect::MariaDB, SQLDialect::SQLite,
                   SQLDialect::SQLServer, SQLDialect::Oracle, SQLDialect::DB2}) {
        INFO("dialect " << static_cast<int>(d));
        const std::string with_limit = norm(gen("SELECT a FROM t ORDER BY a LIMIT 10 OFFSET 5", d));
        CHECK(norm(gen("SELECT a FROM t ORDER BY a OFFSET 5 ROWS FETCH NEXT 10 ROWS ONLY", d)) == with_limit);
        CHECK(norm(gen("SELECT a FROM t ORDER BY a OFFSET 5 ROW FETCH FIRST 10 ROWS ONLY", d)) == with_limit);
        CHECK(norm(gen("SELECT a FROM t ORDER BY a FETCH FIRST 3 ROWS ONLY", d)) ==
              norm(gen("SELECT a FROM t ORDER BY a LIMIT 3", d)));
    }
}

TEST_CASE("FETCH FIRST is not dropped", "[limits]") {
    same("SELECT a FROM t FETCH FIRST 3 ROWS ONLY", SQLDialect::PostgreSQL, R"(SELECT "a" FROM "t" LIMIT 3)");
    same("SELECT a FROM t FETCH FIRST 3 ROWS ONLY", SQLDialect::MySQL, "SELECT `a` FROM `t` LIMIT 3");
    same("SELECT a FROM t FETCH FIRST 3 ROWS ONLY", SQLDialect::SQLite, R"(SELECT "a" FROM "t" LIMIT 3)");
    same("SELECT a FROM t FETCH FIRST 3 ROWS ONLY", SQLDialect::SQLServer, "SELECT TOP 3 [a] FROM [t]");
    same("SELECT a FROM t FETCH FIRST 3 ROWS ONLY", SQLDialect::Oracle, R"(SELECT "a" FROM "t" FETCH FIRST 3 ROWS ONLY)");
    same("SELECT a FROM t ORDER BY a OFFSET 5 ROWS FETCH NEXT 10 ROWS ONLY", SQLDialect::PostgreSQL,
         R"(SELECT "a" FROM "t" ORDER BY "a" LIMIT 10 OFFSET 5)");
}

TEST_CASE("Oracle and DB2 get OFFSET ... ROWS and FETCH instead of LIMIT", "[limits]") {
    for (auto d : {SQLDialect::Oracle, SQLDialect::DB2}) {
        same("SELECT a FROM t ORDER BY a LIMIT 10", d, R"(SELECT "a" FROM "t" ORDER BY "a" FETCH FIRST 10 ROWS ONLY)");
        same("SELECT a FROM t ORDER BY a LIMIT 10 OFFSET 5", d,
             R"(SELECT "a" FROM "t" ORDER BY "a" OFFSET 5 ROWS FETCH NEXT 10 ROWS ONLY)");
        same("SELECT a FROM t ORDER BY a OFFSET 5", d, R"(SELECT "a" FROM "t" ORDER BY "a" OFFSET 5 ROWS)");
    }
}

TEST_CASE("SQL Server gets OFFSET ... FETCH and not TOP when there is an offset", "[limits]") {
    same("SELECT a FROM t ORDER BY a LIMIT 10 OFFSET 5", SQLDialect::SQLServer,
         "SELECT [a] FROM [t] ORDER BY [a] OFFSET 5 ROWS FETCH NEXT 10 ROWS ONLY");
    same("SELECT a FROM t ORDER BY a OFFSET 5 ROWS", SQLDialect::SQLServer,
         "SELECT [a] FROM [t] ORDER BY [a] OFFSET 5 ROWS");
    same("SELECT a FROM t ORDER BY a LIMIT 10", SQLDialect::SQLServer, "SELECT TOP 10 [a] FROM [t] ORDER BY [a]");
}

TEST_CASE("SQL Server never gets TOP together with OFFSET", "[limits]") {
    // SQL Server only allows OFFSET after ORDER BY. Either generating fails, or the output has an ORDER BY,
    // OFFSET 5 ROWS and no TOP.
    std::string out;
    bool threw = false;
    try {
        out = norm(gen("SELECT a FROM t LIMIT 10 OFFSET 5", SQLDialect::SQLServer));
    } catch (...) {
        threw = true;
    }
    INFO("got: " << out);
    CHECK((threw || (out.find("TOP") == std::string::npos && out.find("ORDERBY") != std::string::npos &&
                     out.find("OFFSET5ROWS") != std::string::npos)));
}

TEST_CASE("A row limit inside a subquery is written for the dialect too", "[limits]") {
    same("SELECT * FROM (SELECT a FROM t ORDER BY a FETCH FIRST 2 ROWS ONLY)", SQLDialect::PostgreSQL,
         R"(SELECT * FROM (SELECT "a" FROM "t" ORDER BY "a" LIMIT 2))");
    same("SELECT * FROM (SELECT a FROM t ORDER BY a LIMIT 2)", SQLDialect::Oracle,
         R"(SELECT * FROM (SELECT "a" FROM "t" ORDER BY "a" FETCH FIRST 2 ROWS ONLY))");
}

// ---------------------------------------------------------------- more row-limit forms

TEST_CASE("MySQL's LIMIT offset, count is read as an offset and a count", "[limits]") {
    const std::string sql = "SELECT a FROM t ORDER BY a LIMIT 5, 10";
    same(sql, SQLDialect::PostgreSQL, "SELECT \"a\" FROM \"t\" ORDER BY \"a\" LIMIT 10 OFFSET 5");
    same(sql, SQLDialect::SQLite, "SELECT \"a\" FROM \"t\" ORDER BY \"a\" LIMIT 10 OFFSET 5");
    one_of(sql, SQLDialect::MySQL, {"SELECT `a` FROM `t` ORDER BY `a` LIMIT 10 OFFSET 5",
                                    "SELECT `a` FROM `t` ORDER BY `a` LIMIT 5, 10"});
    same(sql, SQLDialect::Oracle, "SELECT \"a\" FROM \"t\" ORDER BY \"a\" OFFSET 5 ROWS FETCH NEXT 10 ROWS ONLY");
    same(sql, SQLDialect::SQLServer, "SELECT [a] FROM [t] ORDER BY [a] OFFSET 5 ROWS FETCH NEXT 10 ROWS ONLY");
}

TEST_CASE("An OFFSET with no limit is written for every checked dialect", "[limits]") {
    for (const std::string sql : {"SELECT a FROM t ORDER BY a OFFSET 5", "SELECT a FROM t ORDER BY a OFFSET 5 ROWS"}) {
        one_of(sql, SQLDialect::PostgreSQL, {"SELECT \"a\" FROM \"t\" ORDER BY \"a\" OFFSET 5",
                                             "SELECT \"a\" FROM \"t\" ORDER BY \"a\" OFFSET 5 ROWS"});
        // MySQL and MariaDB need a LIMIT; the manual uses the largest one. SQLite takes a negative LIMIT.
        for (auto d : {SQLDialect::MySQL, SQLDialect::MariaDB}) {
            matches(sql, d, "SELECT`A`FROM`T`ORDERBY`A`(LIMIT[0-9]{18,}OFFSET5|LIMIT5,[0-9]{18,})");
        }
        matches(sql, SQLDialect::SQLite,
                "SELECT\"A\"FROM\"T\"ORDERBY\"A\"(LIMIT(-[0-9]+|[0-9]{18,})OFFSET5|LIMIT5,(-[0-9]+|[0-9]{18,}))");
        for (auto d : {SQLDialect::Oracle, SQLDialect::DB2, SQLDialect::SQLServer}) {
            same(sql, d, "SELECT " + q(d, "a") + " FROM " + q(d, "t") + " ORDER BY " + q(d, "a") + " OFFSET 5 ROWS");
        }
    }
}

TEST_CASE("An OFFSET with no limit inside a subquery is written for MySQL", "[limits]") {
    matches("SELECT * FROM (SELECT a FROM t ORDER BY a OFFSET 5) WHERE a > 1", SQLDialect::MySQL,
            "SELECT\\*FROM\\(SELECT`A`FROM`T`ORDERBY`A`(LIMIT[0-9]{18,}OFFSET5|LIMIT5,[0-9]{18,})\\)WHERE`A`>1");
}

TEST_CASE("LIMIT ALL means no limit", "[limits]") {
    const std::string sql = "SELECT a FROM t ORDER BY a LIMIT ALL";
    one_of(sql, SQLDialect::PostgreSQL, {"SELECT \"a\" FROM \"t\" ORDER BY \"a\"",
                                         "SELECT \"a\" FROM \"t\" ORDER BY \"a\" LIMIT ALL"});
    for (auto d : {SQLDialect::MySQL, SQLDialect::Oracle, SQLDialect::DB2, SQLDialect::SQLServer}) {
        same(sql, d, "SELECT " + q(d, "a") + " FROM " + q(d, "t") + " ORDER BY " + q(d, "a"));
    }
    one_of(sql, SQLDialect::SQLite, {"SELECT \"a\" FROM \"t\" ORDER BY \"a\"",
                                     "SELECT \"a\" FROM \"t\" ORDER BY \"a\" LIMIT -1"});

    const std::string with_offset = "SELECT a FROM t ORDER BY a LIMIT ALL OFFSET 3";
    one_of(with_offset, SQLDialect::PostgreSQL, {"SELECT \"a\" FROM \"t\" ORDER BY \"a\" OFFSET 3",
                                                 "SELECT \"a\" FROM \"t\" ORDER BY \"a\" OFFSET 3 ROWS",
                                                 "SELECT \"a\" FROM \"t\" ORDER BY \"a\" LIMIT ALL OFFSET 3"});
    matches(with_offset, SQLDialect::MySQL, "SELECT`A`FROM`T`ORDERBY`A`(LIMIT[0-9]{18,}OFFSET3|LIMIT3,[0-9]{18,})");
    for (auto d : {SQLDialect::Oracle, SQLDialect::SQLServer}) {
        same(with_offset, d, "SELECT " + q(d, "a") + " FROM " + q(d, "t") + " ORDER BY " + q(d, "a") + " OFFSET 3 ROWS");
    }
}

TEST_CASE("SQL Server's TOP is read as the row limit", "[limits]") {
    const std::string sql = "SELECT TOP 5 a FROM t ORDER BY a";
    same(sql, SQLDialect::PostgreSQL, "SELECT \"a\" FROM \"t\" ORDER BY \"a\" LIMIT 5");
    same(sql, SQLDialect::MySQL, "SELECT `a` FROM `t` ORDER BY `a` LIMIT 5");
    same(sql, SQLDialect::Oracle, "SELECT \"a\" FROM \"t\" ORDER BY \"a\" FETCH FIRST 5 ROWS ONLY");
    same(sql, SQLDialect::SQLServer, "SELECT TOP 5 [a] FROM [t] ORDER BY [a]");
}

TEST_CASE("A TOP count that is not a number is put in parentheses", "[limits]") {
    for (const std::string sql : {"SELECT a FROM t ORDER BY a LIMIT 10 + 5", "SELECT TOP (10 + 5) a FROM t ORDER BY a"}) {
        same(sql, SQLDialect::SQLServer, "SELECT TOP (10 + 5) [a] FROM [t] ORDER BY [a]");
    }
}

TEST_CASE("PERCENT is kept where it can be written and fails elsewhere", "[limits][percent]") {
    for (const std::string sql : {"SELECT TOP 10 PERCENT a FROM t ORDER BY a",
                                  "SELECT a FROM t ORDER BY a FETCH FIRST 10 PERCENT ROWS ONLY"}) {
        same(sql, SQLDialect::SQLServer, "SELECT TOP 10 PERCENT [a] FROM [t] ORDER BY [a]");
        same(sql, SQLDialect::Oracle, "SELECT \"a\" FROM \"t\" ORDER BY \"a\" FETCH FIRST 10 PERCENT ROWS ONLY");
        for (auto d : {SQLDialect::PostgreSQL, SQLDialect::MySQL, SQLDialect::SQLite}) fails(sql, d);
    }
}

TEST_CASE("PERCENT with an offset is kept for Oracle and fails for SQL Server", "[limits][percent]") {
    const std::string sql = "SELECT a FROM t ORDER BY a OFFSET 5 ROWS FETCH NEXT 10 PERCENT ROWS ONLY";
    same(sql, SQLDialect::Oracle,
         "SELECT \"a\" FROM \"t\" ORDER BY \"a\" OFFSET 5 ROWS FETCH NEXT 10 PERCENT ROWS ONLY");
    fails(sql, SQLDialect::SQLServer);
}

TEST_CASE("WITH TIES is kept where it can be written and fails elsewhere", "[limits][ties]") {
    for (const std::string sql : {"SELECT TOP 5 WITH TIES a FROM t ORDER BY a",
                                  "SELECT a FROM t ORDER BY a FETCH FIRST 5 ROWS WITH TIES"}) {
        same(sql, SQLDialect::SQLServer, "SELECT TOP 5 WITH TIES [a] FROM [t] ORDER BY [a]");
        same(sql, SQLDialect::PostgreSQL, "SELECT \"a\" FROM \"t\" ORDER BY \"a\" FETCH FIRST 5 ROWS WITH TIES");
        same(sql, SQLDialect::Oracle, "SELECT \"a\" FROM \"t\" ORDER BY \"a\" FETCH FIRST 5 ROWS WITH TIES");
        for (auto d : {SQLDialect::MySQL, SQLDialect::SQLite}) fails(sql, d);
    }
}

TEST_CASE("WITH TIES with an offset is kept for PostgreSQL and Oracle and fails for SQL Server", "[limits][ties]") {
    const std::string sql = "SELECT a FROM t ORDER BY a OFFSET 1 ROWS FETCH NEXT 3 ROWS WITH TIES";
    one_of(sql, SQLDialect::PostgreSQL, {"SELECT \"a\" FROM \"t\" ORDER BY \"a\" OFFSET 1 ROWS FETCH NEXT 3 ROWS WITH TIES",
                                         "SELECT \"a\" FROM \"t\" ORDER BY \"a\" OFFSET 1 FETCH NEXT 3 ROWS WITH TIES"});
    same(sql, SQLDialect::Oracle, "SELECT \"a\" FROM \"t\" ORDER BY \"a\" OFFSET 1 ROWS FETCH NEXT 3 ROWS WITH TIES");
    fails(sql, SQLDialect::SQLServer);
    fails(sql, SQLDialect::MySQL);
}

TEST_CASE("WITH TIES without ORDER BY fails for PostgreSQL and SQL Server", "[limits][ties]") {
    // Both databases reject WITH TIES when there is no ORDER BY.
    for (const std::string sql : {"SELECT TOP 5 WITH TIES a FROM t", "SELECT a FROM t FETCH FIRST 5 ROWS WITH TIES"}) {
        fails(sql, SQLDialect::PostgreSQL);
        fails(sql, SQLDialect::SQLServer);
    }
}

TEST_CASE("LIMIT and OFFSET stay as they were where they are right", "[limits][unchanged]") {
    for (auto d : {SQLDialect::PostgreSQL, SQLDialect::SQLite, SQLDialect::DuckDB, SQLDialect::Snowflake}) {
        same("SELECT a FROM t ORDER BY a LIMIT 10 OFFSET 5", d, R"(SELECT "a" FROM "t" ORDER BY "a" LIMIT 10 OFFSET 5)");
        same("SELECT a FROM t ORDER BY a LIMIT 10", d, R"(SELECT "a" FROM "t" ORDER BY "a" LIMIT 10)");
    }
    for (auto d : {SQLDialect::MySQL, SQLDialect::MariaDB}) {
        same("SELECT a FROM t ORDER BY a LIMIT 10 OFFSET 5", d, "SELECT `a` FROM `t` ORDER BY `a` LIMIT 10 OFFSET 5");
    }
}

// ---------------------------------------------------------------- Everything else is unchanged

TEST_CASE("Plain queries come out exactly as before in every dialect", "[unchanged]") {
    const char* queries[] = {
        "SELECT a, COUNT(*) FROM t WHERE b = 1 GROUP BY a HAVING COUNT(*) > 1 ORDER BY a",
        "SELECT a, b, SUM(c) FROM t GROUP BY a, b",
        "SELECT DISTINCT a FROM t ORDER BY a DESC",
    };
    // Output of the starting code (exact strings, no normalisation).
    const std::vector<std::tuple<int, SQLDialect, std::string>> expected = {
    {0, SQLDialect::ANSI, R"SQL(SELECT "a", COUNT(*) FROM "t" WHERE "b" = 1 GROUP BY "a" HAVING COUNT(*) > 1 ORDER BY "a")SQL"},
    {0, SQLDialect::PostgreSQL, R"SQL(SELECT "a", COUNT(*) FROM "t" WHERE "b" = 1 GROUP BY "a" HAVING COUNT(*) > 1 ORDER BY "a")SQL"},
    {0, SQLDialect::MySQL, R"SQL(SELECT `a`, COUNT(*) FROM `t` WHERE `b` = 1 GROUP BY `a` HAVING COUNT(*) > 1 ORDER BY `a`)SQL"},
    {0, SQLDialect::SQLite, R"SQL(SELECT "a", COUNT(*) FROM "t" WHERE "b" = 1 GROUP BY "a" HAVING COUNT(*) > 1 ORDER BY "a")SQL"},
    {0, SQLDialect::SQLServer, R"SQL(SELECT [a], COUNT(*) FROM [t] WHERE [b] = 1 GROUP BY [a] HAVING COUNT(*) > 1 ORDER BY [a])SQL"},
    {0, SQLDialect::Oracle, R"SQL(SELECT "a", COUNT(*) FROM "t" WHERE "b" = 1 GROUP BY "a" HAVING COUNT(*) > 1 ORDER BY "a")SQL"},
    {0, SQLDialect::DB2, R"SQL(SELECT "a", COUNT(*) FROM "t" WHERE "b" = 1 GROUP BY "a" HAVING COUNT(*) > 1 ORDER BY "a")SQL"},
    {0, SQLDialect::Teradata, R"SQL(SELECT "a", COUNT(*) FROM "t" WHERE "b" = 1 GROUP BY "a" HAVING COUNT(*) > 1 ORDER BY "a")SQL"},
    {0, SQLDialect::MariaDB, R"SQL(SELECT `a`, COUNT(*) FROM `t` WHERE `b` = 1 GROUP BY `a` HAVING COUNT(*) > 1 ORDER BY `a`)SQL"},
    {0, SQLDialect::Informix, R"SQL(SELECT "a", COUNT(*) FROM "t" WHERE "b" = 1 GROUP BY "a" HAVING COUNT(*) > 1 ORDER BY "a")SQL"},
    {0, SQLDialect::Firebird, R"SQL(SELECT "a", COUNT(*) FROM "t" WHERE "b" = 1 GROUP BY "a" HAVING COUNT(*) > 1 ORDER BY "a")SQL"},
    {0, SQLDialect::SAPHANA, R"SQL(SELECT "a", COUNT(*) FROM "t" WHERE "b" = 1 GROUP BY "a" HAVING COUNT(*) > 1 ORDER BY "a")SQL"},
    {0, SQLDialect::Snowflake, R"SQL(SELECT "a", COUNT(*) FROM "t" WHERE "b" = 1 GROUP BY "a" HAVING COUNT(*) > 1 ORDER BY "a")SQL"},
    {0, SQLDialect::Redshift, R"SQL(SELECT "a", COUNT(*) FROM "t" WHERE "b" = 1 GROUP BY "a" HAVING COUNT(*) > 1 ORDER BY "a")SQL"},
    {0, SQLDialect::BigQuery, R"SQL(SELECT `a`, COUNT(*) FROM `t` WHERE `b` = 1 GROUP BY `a` HAVING COUNT(*) > 1 ORDER BY `a`)SQL"},
    {0, SQLDialect::AzureSynapse, R"SQL(SELECT "a", COUNT(*) FROM "t" WHERE "b" = 1 GROUP BY "a" HAVING COUNT(*) > 1 ORDER BY "a")SQL"},
    {0, SQLDialect::Athena, R"SQL(SELECT "a", COUNT(*) FROM "t" WHERE "b" = 1 GROUP BY "a" HAVING COUNT(*) > 1 ORDER BY "a")SQL"},
    {0, SQLDialect::DuckDB, R"SQL(SELECT "a", COUNT(*) FROM "t" WHERE "b" = 1 GROUP BY "a" HAVING COUNT(*) > 1 ORDER BY "a")SQL"},
    {0, SQLDialect::ClickHouse, R"SQL(SELECT `a`, COUNT(*) FROM `t` WHERE `b` = 1 GROUP BY `a` HAVING COUNT(*) > 1 ORDER BY `a`)SQL"},
    {0, SQLDialect::Presto, R"SQL(SELECT "a", COUNT(*) FROM "t" WHERE "b" = 1 GROUP BY "a" HAVING COUNT(*) > 1 ORDER BY "a")SQL"},
    {0, SQLDialect::Trino, R"SQL(SELECT "a", COUNT(*) FROM "t" WHERE "b" = 1 GROUP BY "a" HAVING COUNT(*) > 1 ORDER BY "a")SQL"},
    {0, SQLDialect::Hive, R"SQL(SELECT `a`, COUNT(*) FROM `t` WHERE `b` = 1 GROUP BY `a` HAVING COUNT(*) > 1 ORDER BY `a`)SQL"},
    {0, SQLDialect::Impala, R"SQL(SELECT `a`, COUNT(*) FROM `t` WHERE `b` = 1 GROUP BY `a` HAVING COUNT(*) > 1 ORDER BY `a`)SQL"},
    {0, SQLDialect::Drill, R"SQL(SELECT `a`, COUNT(*) FROM `t` WHERE `b` = 1 GROUP BY `a` HAVING COUNT(*) > 1 ORDER BY `a`)SQL"},
    {0, SQLDialect::SparkSQL, R"SQL(SELECT `a`, COUNT(*) FROM `t` WHERE `b` = 1 GROUP BY `a` HAVING COUNT(*) > 1 ORDER BY `a`)SQL"},
    {0, SQLDialect::Databricks, R"SQL(SELECT `a`, COUNT(*) FROM `t` WHERE `b` = 1 GROUP BY `a` HAVING COUNT(*) > 1 ORDER BY `a`)SQL"},
    {0, SQLDialect::Dremio, R"SQL(SELECT "a", COUNT(*) FROM "t" WHERE "b" = 1 GROUP BY "a" HAVING COUNT(*) > 1 ORDER BY "a")SQL"},
    {0, SQLDialect::Vertica, R"SQL(SELECT "a", COUNT(*) FROM "t" WHERE "b" = 1 GROUP BY "a" HAVING COUNT(*) > 1 ORDER BY "a")SQL"},
    {0, SQLDialect::Greenplum, R"SQL(SELECT "a", COUNT(*) FROM "t" WHERE "b" = 1 GROUP BY "a" HAVING COUNT(*) > 1 ORDER BY "a")SQL"},
    {0, SQLDialect::Netezza, R"SQL(SELECT "a", COUNT(*) FROM "t" WHERE "b" = 1 GROUP BY "a" HAVING COUNT(*) > 1 ORDER BY "a")SQL"},
    {0, SQLDialect::Exasol, R"SQL(SELECT "a", COUNT(*) FROM "t" WHERE "b" = 1 GROUP BY "a" HAVING COUNT(*) > 1 ORDER BY "a")SQL"},
    {0, SQLDialect::MonetDB, R"SQL(SELECT "a", COUNT(*) FROM "t" WHERE "b" = 1 GROUP BY "a" HAVING COUNT(*) > 1 ORDER BY "a")SQL"},
    {0, SQLDialect::CockroachDB, R"SQL(SELECT "a", COUNT(*) FROM "t" WHERE "b" = 1 GROUP BY "a" HAVING COUNT(*) > 1 ORDER BY "a")SQL"},
    {0, SQLDialect::YugabyteDB, R"SQL(SELECT "a", COUNT(*) FROM "t" WHERE "b" = 1 GROUP BY "a" HAVING COUNT(*) > 1 ORDER BY "a")SQL"},
    {0, SQLDialect::TiDB, R"SQL(SELECT `a`, COUNT(*) FROM `t` WHERE `b` = 1 GROUP BY `a` HAVING COUNT(*) > 1 ORDER BY `a`)SQL"},
    {0, SQLDialect::Spanner, R"SQL(SELECT `a`, COUNT(*) FROM `t` WHERE `b` = 1 GROUP BY `a` HAVING COUNT(*) > 1 ORDER BY `a`)SQL"},
    {0, SQLDialect::Citus, R"SQL(SELECT "a", COUNT(*) FROM "t" WHERE "b" = 1 GROUP BY "a" HAVING COUNT(*) > 1 ORDER BY "a")SQL"},
    {1, SQLDialect::ANSI, R"SQL(SELECT "a", "b", SUM("c") FROM "t" GROUP BY "a", "b")SQL"},
    {1, SQLDialect::PostgreSQL, R"SQL(SELECT "a", "b", SUM("c") FROM "t" GROUP BY "a", "b")SQL"},
    {1, SQLDialect::MySQL, R"SQL(SELECT `a`, `b`, SUM(`c`) FROM `t` GROUP BY `a`, `b`)SQL"},
    {1, SQLDialect::SQLite, R"SQL(SELECT "a", "b", SUM("c") FROM "t" GROUP BY "a", "b")SQL"},
    {1, SQLDialect::SQLServer, R"SQL(SELECT [a], [b], SUM([c]) FROM [t] GROUP BY [a], [b])SQL"},
    {1, SQLDialect::Oracle, R"SQL(SELECT "a", "b", SUM("c") FROM "t" GROUP BY "a", "b")SQL"},
    {1, SQLDialect::DB2, R"SQL(SELECT "a", "b", SUM("c") FROM "t" GROUP BY "a", "b")SQL"},
    {1, SQLDialect::Teradata, R"SQL(SELECT "a", "b", SUM("c") FROM "t" GROUP BY "a", "b")SQL"},
    {1, SQLDialect::MariaDB, R"SQL(SELECT `a`, `b`, SUM(`c`) FROM `t` GROUP BY `a`, `b`)SQL"},
    {1, SQLDialect::Informix, R"SQL(SELECT "a", "b", SUM("c") FROM "t" GROUP BY "a", "b")SQL"},
    {1, SQLDialect::Firebird, R"SQL(SELECT "a", "b", SUM("c") FROM "t" GROUP BY "a", "b")SQL"},
    {1, SQLDialect::SAPHANA, R"SQL(SELECT "a", "b", SUM("c") FROM "t" GROUP BY "a", "b")SQL"},
    {1, SQLDialect::Snowflake, R"SQL(SELECT "a", "b", SUM("c") FROM "t" GROUP BY "a", "b")SQL"},
    {1, SQLDialect::Redshift, R"SQL(SELECT "a", "b", SUM("c") FROM "t" GROUP BY "a", "b")SQL"},
    {1, SQLDialect::BigQuery, R"SQL(SELECT `a`, `b`, SUM(`c`) FROM `t` GROUP BY `a`, `b`)SQL"},
    {1, SQLDialect::AzureSynapse, R"SQL(SELECT "a", "b", SUM("c") FROM "t" GROUP BY "a", "b")SQL"},
    {1, SQLDialect::Athena, R"SQL(SELECT "a", "b", SUM("c") FROM "t" GROUP BY "a", "b")SQL"},
    {1, SQLDialect::DuckDB, R"SQL(SELECT "a", "b", SUM("c") FROM "t" GROUP BY "a", "b")SQL"},
    {1, SQLDialect::ClickHouse, R"SQL(SELECT `a`, `b`, SUM(`c`) FROM `t` GROUP BY `a`, `b`)SQL"},
    {1, SQLDialect::Presto, R"SQL(SELECT "a", "b", SUM("c") FROM "t" GROUP BY "a", "b")SQL"},
    {1, SQLDialect::Trino, R"SQL(SELECT "a", "b", SUM("c") FROM "t" GROUP BY "a", "b")SQL"},
    {1, SQLDialect::Hive, R"SQL(SELECT `a`, `b`, SUM(`c`) FROM `t` GROUP BY `a`, `b`)SQL"},
    {1, SQLDialect::Impala, R"SQL(SELECT `a`, `b`, SUM(`c`) FROM `t` GROUP BY `a`, `b`)SQL"},
    {1, SQLDialect::Drill, R"SQL(SELECT `a`, `b`, SUM(`c`) FROM `t` GROUP BY `a`, `b`)SQL"},
    {1, SQLDialect::SparkSQL, R"SQL(SELECT `a`, `b`, SUM(`c`) FROM `t` GROUP BY `a`, `b`)SQL"},
    {1, SQLDialect::Databricks, R"SQL(SELECT `a`, `b`, SUM(`c`) FROM `t` GROUP BY `a`, `b`)SQL"},
    {1, SQLDialect::Dremio, R"SQL(SELECT "a", "b", SUM("c") FROM "t" GROUP BY "a", "b")SQL"},
    {1, SQLDialect::Vertica, R"SQL(SELECT "a", "b", SUM("c") FROM "t" GROUP BY "a", "b")SQL"},
    {1, SQLDialect::Greenplum, R"SQL(SELECT "a", "b", SUM("c") FROM "t" GROUP BY "a", "b")SQL"},
    {1, SQLDialect::Netezza, R"SQL(SELECT "a", "b", SUM("c") FROM "t" GROUP BY "a", "b")SQL"},
    {1, SQLDialect::Exasol, R"SQL(SELECT "a", "b", SUM("c") FROM "t" GROUP BY "a", "b")SQL"},
    {1, SQLDialect::MonetDB, R"SQL(SELECT "a", "b", SUM("c") FROM "t" GROUP BY "a", "b")SQL"},
    {1, SQLDialect::CockroachDB, R"SQL(SELECT "a", "b", SUM("c") FROM "t" GROUP BY "a", "b")SQL"},
    {1, SQLDialect::YugabyteDB, R"SQL(SELECT "a", "b", SUM("c") FROM "t" GROUP BY "a", "b")SQL"},
    {1, SQLDialect::TiDB, R"SQL(SELECT `a`, `b`, SUM(`c`) FROM `t` GROUP BY `a`, `b`)SQL"},
    {1, SQLDialect::Spanner, R"SQL(SELECT `a`, `b`, SUM(`c`) FROM `t` GROUP BY `a`, `b`)SQL"},
    {1, SQLDialect::Citus, R"SQL(SELECT "a", "b", SUM("c") FROM "t" GROUP BY "a", "b")SQL"},
    {2, SQLDialect::ANSI, R"SQL(SELECT DISTINCT "a" FROM "t" ORDER BY "a" DESC)SQL"},
    {2, SQLDialect::PostgreSQL, R"SQL(SELECT DISTINCT "a" FROM "t" ORDER BY "a" DESC)SQL"},
    {2, SQLDialect::MySQL, R"SQL(SELECT DISTINCT `a` FROM `t` ORDER BY `a` DESC)SQL"},
    {2, SQLDialect::SQLite, R"SQL(SELECT DISTINCT "a" FROM "t" ORDER BY "a" DESC)SQL"},
    {2, SQLDialect::SQLServer, R"SQL(SELECT DISTINCT [a] FROM [t] ORDER BY [a] DESC)SQL"},
    {2, SQLDialect::Oracle, R"SQL(SELECT DISTINCT "a" FROM "t" ORDER BY "a" DESC)SQL"},
    {2, SQLDialect::DB2, R"SQL(SELECT DISTINCT "a" FROM "t" ORDER BY "a" DESC)SQL"},
    {2, SQLDialect::Teradata, R"SQL(SELECT DISTINCT "a" FROM "t" ORDER BY "a" DESC)SQL"},
    {2, SQLDialect::MariaDB, R"SQL(SELECT DISTINCT `a` FROM `t` ORDER BY `a` DESC)SQL"},
    {2, SQLDialect::Informix, R"SQL(SELECT DISTINCT "a" FROM "t" ORDER BY "a" DESC)SQL"},
    {2, SQLDialect::Firebird, R"SQL(SELECT DISTINCT "a" FROM "t" ORDER BY "a" DESC)SQL"},
    {2, SQLDialect::SAPHANA, R"SQL(SELECT DISTINCT "a" FROM "t" ORDER BY "a" DESC)SQL"},
    {2, SQLDialect::Snowflake, R"SQL(SELECT DISTINCT "a" FROM "t" ORDER BY "a" DESC)SQL"},
    {2, SQLDialect::Redshift, R"SQL(SELECT DISTINCT "a" FROM "t" ORDER BY "a" DESC)SQL"},
    {2, SQLDialect::BigQuery, R"SQL(SELECT DISTINCT `a` FROM `t` ORDER BY `a` DESC)SQL"},
    {2, SQLDialect::AzureSynapse, R"SQL(SELECT DISTINCT "a" FROM "t" ORDER BY "a" DESC)SQL"},
    {2, SQLDialect::Athena, R"SQL(SELECT DISTINCT "a" FROM "t" ORDER BY "a" DESC)SQL"},
    {2, SQLDialect::DuckDB, R"SQL(SELECT DISTINCT "a" FROM "t" ORDER BY "a" DESC)SQL"},
    {2, SQLDialect::ClickHouse, R"SQL(SELECT DISTINCT `a` FROM `t` ORDER BY `a` DESC)SQL"},
    {2, SQLDialect::Presto, R"SQL(SELECT DISTINCT "a" FROM "t" ORDER BY "a" DESC)SQL"},
    {2, SQLDialect::Trino, R"SQL(SELECT DISTINCT "a" FROM "t" ORDER BY "a" DESC)SQL"},
    {2, SQLDialect::Hive, R"SQL(SELECT DISTINCT `a` FROM `t` ORDER BY `a` DESC)SQL"},
    {2, SQLDialect::Impala, R"SQL(SELECT DISTINCT `a` FROM `t` ORDER BY `a` DESC)SQL"},
    {2, SQLDialect::Drill, R"SQL(SELECT DISTINCT `a` FROM `t` ORDER BY `a` DESC)SQL"},
    {2, SQLDialect::SparkSQL, R"SQL(SELECT DISTINCT `a` FROM `t` ORDER BY `a` DESC)SQL"},
    {2, SQLDialect::Databricks, R"SQL(SELECT DISTINCT `a` FROM `t` ORDER BY `a` DESC)SQL"},
    {2, SQLDialect::Dremio, R"SQL(SELECT DISTINCT "a" FROM "t" ORDER BY "a" DESC)SQL"},
    {2, SQLDialect::Vertica, R"SQL(SELECT DISTINCT "a" FROM "t" ORDER BY "a" DESC)SQL"},
    {2, SQLDialect::Greenplum, R"SQL(SELECT DISTINCT "a" FROM "t" ORDER BY "a" DESC)SQL"},
    {2, SQLDialect::Netezza, R"SQL(SELECT DISTINCT "a" FROM "t" ORDER BY "a" DESC)SQL"},
    {2, SQLDialect::Exasol, R"SQL(SELECT DISTINCT "a" FROM "t" ORDER BY "a" DESC)SQL"},
    {2, SQLDialect::MonetDB, R"SQL(SELECT DISTINCT "a" FROM "t" ORDER BY "a" DESC)SQL"},
    {2, SQLDialect::CockroachDB, R"SQL(SELECT DISTINCT "a" FROM "t" ORDER BY "a" DESC)SQL"},
    {2, SQLDialect::YugabyteDB, R"SQL(SELECT DISTINCT "a" FROM "t" ORDER BY "a" DESC)SQL"},
    {2, SQLDialect::TiDB, R"SQL(SELECT DISTINCT `a` FROM `t` ORDER BY `a` DESC)SQL"},
    {2, SQLDialect::Spanner, R"SQL(SELECT DISTINCT `a` FROM `t` ORDER BY `a` DESC)SQL"},
    {2, SQLDialect::Citus, R"SQL(SELECT DISTINCT "a" FROM "t" ORDER BY "a" DESC)SQL"},
    };
    for (const auto& [qi, d, want] : expected) {
        INFO("query " << qi << " dialect " << static_cast<int>(d));
        CHECK(gen(queries[qi], d) == want);
    }
}

// ---------------------------------------------------------------- extra cases

TEST_CASE("A second row limit in one query is an error, not silently dropped", "[row-limit]") {
    const char* sql = "SELECT a FROM t ORDER BY a OFFSET 2 ROWS FETCH NEXT 3 ROWS ONLY LIMIT 4";
    for (auto d : {SQLDialect::PostgreSQL, SQLDialect::MySQL, SQLDialect::Oracle, SQLDialect::SQLServer}) {
        fails(sql, d);
    }
    fails("SELECT TOP 3 a FROM t LIMIT 4", SQLDialect::SQLServer);
}

TEST_CASE("Firebird and Informix keep an OFFSET that has no limit", "[row-limit]") {
    for (auto d : {SQLDialect::Firebird, SQLDialect::Informix}) {
        same("SELECT a FROM t OFFSET 5", d, R"(SELECT SKIP 5 "a" FROM "t")");
    }
}

TEST_CASE("Empty ROLLUP and CUBE lists are errors", "[rollup][cube]") {
    fails("SELECT a FROM t GROUP BY ROLLUP()", SQLDialect::PostgreSQL);
    fails("SELECT a FROM t GROUP BY CUBE()", SQLDialect::PostgreSQL);
    same("SELECT a FROM t GROUP BY GROUPING SETS ((a), ())", SQLDialect::PostgreSQL,
         R"(SELECT "a" FROM "t" GROUP BY GROUPING SETS (("a"), ()))");
}

TEST_CASE("A composite column inside ROLLUP keeps its parentheses", "[rollup]") {
    same("SELECT a, b, c, SUM(d) FROM t GROUP BY ROLLUP((a, b), c)", SQLDialect::PostgreSQL,
         R"(SELECT "a", "b", "c", SUM("d") FROM "t" GROUP BY ROLLUP(("a", "b"), "c"))");
    same("SELECT a, b, c, SUM(d) FROM t GROUP BY ROLLUP((a, b), c)", SQLDialect::SQLServer,
         "SELECT [a], [b], [c], SUM([d]) FROM [t] GROUP BY ROLLUP(([a], [b]), [c])");
}
