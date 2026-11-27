// The SQL that libglot writes must mean what the SQL that went in meant.
#include <catch2/catch_test_macros.hpp>
#include <libglot/sql/parser.h>
#include <libglot/sql/generator.h>
#include <libglot/util/arena.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>
#include <utility>

using namespace libglot::sql;

namespace {

std::string gen(const std::string& sql, SQLDialect dialect = SQLDialect::PostgreSQL) {
    libglot::Arena arena;
    SQLParser parser(arena, sql);
    auto* ast = parser.parse_top_level();
    SQLGenerator generator(dialect);
    return generator.generate(ast);
}

// Whitespace and identifier quote characters removed, so one expectation fits every dialect.
std::string bare(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == ' ' || c == '\n' || c == '\t' || c == '\r' || c == '"' || c == '`' || c == '[' || c == ']') continue;
        out += c;
    }
    return out;
}

std::string squash(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == ' ' || c == '\n' || c == '\t' || c == '\r') continue;
        out += c;
    }
    return out;
}

// Writes over the stack that parse() used, so a string_view left pointing into it shows junk
// every time and not only on some runs.
__attribute__((noinline)) void scramble_stack() {
    volatile char junk[32768];
    for (size_t i = 0; i < sizeof(junk); ++i) junk[i] = 'Z';
    (void)junk[100];
}

std::string gen_after_scramble(const std::string& sql, SQLDialect dialect = SQLDialect::PostgreSQL) {
    libglot::Arena arena;
    SQLParser parser(arena, sql);
    auto* ast = parser.parse_top_level();
    scramble_stack();
    SQLGenerator generator(dialect);
    return generator.generate(ast);
}

const std::vector<SQLDialect> kDialects = {
    SQLDialect::PostgreSQL, SQLDialect::MySQL, SQLDialect::SQLite, SQLDialect::SQLServer,
    SQLDialect::Oracle, SQLDialect::BigQuery, SQLDialect::Snowflake, SQLDialect::DuckDB,
    SQLDialect::Redshift, SQLDialect::ClickHouse, SQLDialect::MariaDB, SQLDialect::DB2,
    SQLDialect::Teradata, SQLDialect::SparkSQL, SQLDialect::Trino, SQLDialect::Hive,
};

}  // namespace

TEST_CASE("parentheses that the meaning needs are written in every dialect") {
    const std::vector<std::pair<std::string, std::string>> cases = {
        {"SELECT (a + b) * 2 FROM t", "SELECT(a+b)*2FROMt"},
        {"SELECT a - (b - c) FROM t", "SELECTa-(b-c)FROMt"},
        {"SELECT a / (b * c) FROM t", "SELECTa/(b*c)FROMt"},
        {"SELECT (a + b) * (c - d) FROM t", "SELECT(a+b)*(c-d)FROMt"},
        {"SELECT a FROM t WHERE (a = 1 OR b = 2) AND c = 3", "SELECTaFROMtWHERE(a=1ORb=2)ANDc=3"},
        {"SELECT a FROM t WHERE NOT (a = 1 AND b = 2)", "SELECTaFROMtWHERENOT(a=1ANDb=2)"},
        {"SELECT -(a + b) FROM t", "SELECT-(a+b)FROMt"},
    };
    for (auto dialect : kDialects) {
        for (const auto& [input, expected] : cases) {
            INFO("dialect " << static_cast<int>(dialect) << " input: " << input);
            CHECK(bare(gen(input, dialect)) == expected);
        }
    }
}

TEST_CASE("parentheses are written inside every part of a query") {
    CHECK(bare(gen("SELECT CASE WHEN (a + b) * 2 > 5 THEN 1 ELSE 0 END FROM t")) ==
          "SELECTCASEWHEN(a+b)*2>5THEN1ELSE0ENDFROMt");
    CHECK(bare(gen("SELECT x FROM t GROUP BY x HAVING (SUM(a) + 1) * 2 > 10")) ==
          "SELECTxFROMtGROUPBYxHAVING(SUM(a)+1)*2>10");
    CHECK(bare(gen("SELECT a FROM t ORDER BY (a - b) * (a - b) DESC")) ==
          "SELECTaFROMtORDERBY(a-b)*(a-b)DESC");
    CHECK(bare(gen("UPDATE t SET a = (b + c) * 2 WHERE (x = 1 OR y = 2) AND z = 3")) ==
          "UPDATEtSETa=(b+c)*2WHERE(x=1ORy=2)ANDz=3");
    CHECK(bare(gen("SELECT a FROM t WHERE x BETWEEN (a + 1) * 2 AND 10")) ==
          "SELECTaFROMtWHERExBETWEEN(a+1)*2AND10");
}

TEST_CASE("queries without parentheses come out as they did before") {
    const std::vector<std::pair<std::string, std::string>> cases = {
        {"SELECT a + b * 2 FROM t", R"(SELECT "a" + "b" * 2 FROM "t")"},
        {"SELECT a * b + c * d FROM t", R"(SELECT "a" * "b" + "c" * "d" FROM "t")"},
        {"SELECT a - b - c FROM t", R"(SELECT "a" - "b" - "c" FROM "t")"},
        {"SELECT -a + b FROM t", R"(SELECT - "a" + "b" FROM "t")"},
        {"SELECT a FROM t WHERE a = 1 OR b = 2 AND c = 3", R"(SELECT "a" FROM "t" WHERE "a" = 1 OR "b" = 2 AND "c" = 3)"},
        {"SELECT a FROM t WHERE a = 1 AND b = 2 AND c = 3", R"(SELECT "a" FROM "t" WHERE "a" = 1 AND "b" = 2 AND "c" = 3)"},
        {"SELECT a FROM t WHERE NOT x = 1 AND y = 2", R"(SELECT "a" FROM "t" WHERE NOT "x" = 1 AND "y" = 2)"},
        {"SELECT a FROM t WHERE x IS NOT NULL AND y IS NULL", R"(SELECT "a" FROM "t" WHERE "x" IS NOT NULL AND "y" IS NULL)"},
        {"SELECT a FROM t WHERE x BETWEEN 1 AND 10 AND y = 2", R"(SELECT "a" FROM "t" WHERE "x" BETWEEN 1 AND 10 AND "y" = 2)"},
        {"SELECT a FROM t WHERE x IN (1, 2)", R"(SELECT "a" FROM "t" WHERE "x" IN (1, 2))"},
        {"SELECT a FROM t WHERE a IN (SELECT a FROM u)", R"(SELECT "a" FROM "t" WHERE "a" IN (SELECT "a" FROM "u"))"},
        {"SELECT (SELECT 1) FROM t", R"(SELECT (SELECT 1) FROM "t")"},
        // A minus sign in front of something is written as it always was, with a space after it
        {"SELECT -b FROM t", R"(SELECT - "b" FROM "t")"},
        {"SELECT -4 * b + c FROM t", R"(SELECT - 4 * "b" + "c" FROM "t")"},
        {"SELECT a FROM t WHERE a % -d > 1 AND -c < 2", R"(SELECT "a" FROM "t" WHERE "a" % - "d" > 1 AND - "c" < 2)"},
        {"SELECT a - -b FROM t", R"(SELECT "a" - - "b" FROM "t")"},
        {"SELECT - -a FROM t", R"(SELECT - - "a" FROM "t")"},
    };
    for (const auto& [input, expected] : cases) {
        INFO("input: " << input);
        CHECK(gen(input) == expected);
    }
}

TEST_CASE("a subquery keeps its alias") {
    CHECK(squash(gen("SELECT x.a FROM (SELECT a FROM t) AS x")) ==
          squash(R"(SELECT "x"."a" FROM (SELECT "a" FROM "t") AS "x")"));
    CHECK(squash(gen("SELECT x.a FROM (SELECT a FROM t) x")) ==
          squash(R"(SELECT "x"."a" FROM (SELECT "a" FROM "t") AS "x")"));
    CHECK(squash(gen("SELECT y.a FROM t JOIN (SELECT a FROM u) AS y ON t.a = y.a")) ==
          squash(R"(SELECT "y"."a" FROM "t" INNER JOIN (SELECT "a" FROM "u") AS "y" ON "t"."a" = "y"."a")"));
    CHECK(squash(gen("SELECT x.a, y.b FROM (SELECT a FROM t) x, (SELECT b FROM u) y")) ==
          squash(R"(SELECT "x"."a", "y"."b" FROM (SELECT "a" FROM "t") AS "x" CROSS JOIN (SELECT "b" FROM "u") AS "y")"));
    for (auto dialect : kDialects) {
        const std::string out = bare(gen("SELECT x.a FROM (SELECT a FROM t) AS x", dialect));
        INFO("dialect " << static_cast<int>(dialect) << " output: " << out);
        // The alias must follow the subquery, with or without AS (Oracle does not allow AS here)
        CHECK((out.find(")ASx") != std::string::npos || out.find(")x") != std::string::npos));
    }
}

TEST_CASE("nothing after an IN list is lost") {
    for (auto dialect : kDialects) {
        INFO("dialect " << static_cast<int>(dialect));
        CHECK(bare(gen("SELECT a FROM t WHERE x IN (1, 2) AND y = 1", dialect)) ==
              "SELECTaFROMtWHERExIN(1,2)ANDy=1");
        CHECK(bare(gen("SELECT a FROM t WHERE x IN (1, 2) AND (y = 1 OR z = 2)", dialect)) ==
              "SELECTaFROMtWHERExIN(1,2)AND(y=1ORz=2)");
    }
}

TEST_CASE("CAST, SAFE_CAST and EXTRACT never come out with junk") {
    for (int round = 0; round < 5; ++round) {
        CHECK(squash(gen_after_scramble("SELECT CAST(a AS INT) FROM t")) ==
              squash(R"(SELECT CAST("a" AS INT) FROM "t")"));
        {
            // What a database without SAFE_CAST gets is left open: CAST (as before), SAFE_CAST or TRY_CAST
            // kept, or an error that says so. Whatever is written must not contain junk.
            bool wrote = true;
            std::string out;
            try {
                out = squash(gen_after_scramble("SELECT SAFE_CAST(a AS INT) FROM t"));
            } catch (const std::exception&) {
                wrote = false;
            }
            if (wrote) {
                INFO("output: " << out);
                CHECK((out == squash(R"(SELECT CAST("a" AS INT) FROM "t")") ||
                       out == squash(R"(SELECT SAFE_CAST("a" AS INT) FROM "t")") ||
                       out == squash(R"(SELECT TRY_CAST("a" AS INT) FROM "t")")));
            }
        }
        CHECK(squash(gen_after_scramble("SELECT EXTRACT(DAY FROM d) FROM t")) ==
              squash(R"(SELECT EXTRACT(DAY FROM "d") FROM "t")"));
    }
    for (auto dialect : kDialects) {
        const std::string out = bare(gen_after_scramble("SELECT CAST(a AS INT), EXTRACT(DAY FROM d) FROM t", dialect));
        INFO("dialect " << static_cast<int>(dialect) << " output: " << out);
        CHECK(out.find('Z') == std::string::npos);
        CHECK(out.find("CAST(aASINT)") != std::string::npos);
        CHECK(out.find("EXTRACT(DAYFROMd)") != std::string::npos);
    }
}

TEST_CASE("the query that comes out returns the same rows") {
    const std::vector<std::pair<char, std::string>> cases = {
        {'q', "SELECT (a + b) * 2 FROM t"},
        {'q', "SELECT a - (b - c) FROM t"},
        {'q', "SELECT a / (b + 1) FROM t"},
        {'q', "SELECT a / ((b + 1) * (c + 1)) FROM t"},
        {'q', "SELECT (a + b) * (c - d) FROM t"},
        {'q', "SELECT -(a + b) FROM t"},
        {'q', "SELECT -(a - b) * 2 FROM t"},
        {'q', "SELECT (a = 1) = (b = 2) FROM t"},
        {'q', "SELECT a FROM t WHERE (a = 1 OR b = 2) AND c = 3"},
        {'q', "SELECT a FROM t WHERE (a = 1 OR b = 2) AND (c = 3 OR d = 4)"},
        {'q', "SELECT a, b FROM t WHERE NOT (a = 1 AND b = 2)"},
        {'q', "SELECT a FROM t WHERE a - (b + c) > 0 OR (a + b) * c = 4 AND d = 1"},
        {'q', "SELECT CASE WHEN (a + b) * 2 > 5 THEN 1 ELSE 0 END FROM t"},
        {'q', "SELECT a FROM t WHERE x BETWEEN (a + 1) * 2 AND 10 AND y = 1"},
        {'q', "SELECT x, SUM(a) FROM t GROUP BY x HAVING (SUM(a) + 1) * 2 > 10"},
        {'o', "SELECT a, b, c FROM t ORDER BY (a - b) * (a - b) DESC, a, b, c"},
        {'q', "SELECT (a + b) * 2 AS s FROM t WHERE (a + b) * 2 > 4 AND (c - d) * (c - d) < 6"},
        {'q', "SELECT x.a FROM (SELECT a, b FROM t WHERE b > 1) AS x WHERE x.a > 0"},
        {'q', "SELECT x.a, u.b FROM (SELECT a FROM t) AS x JOIN u ON x.a = u.a"},
        {'q', "SELECT x.a, y.b FROM (SELECT a FROM t) x, (SELECT b FROM u) y WHERE x.a = y.b"},
        {'q', "SELECT a FROM t WHERE x IN (1, 2) AND y = 1"},
        {'q', "SELECT a FROM t WHERE x IN (1, 2) AND (y = 1 OR z = 2)"},
        {'q', "SELECT a FROM t WHERE (y = 1) AND x IN (1, 2)"},
        {'q', "SELECT a FROM t WHERE a + b IN (3, 5) OR c = 0"},
        {'q', "SELECT a FROM t WHERE NOT x IN (1, 2) AND y = 1"},
        {'q', "SELECT a FROM t WHERE a IN (SELECT a FROM u WHERE (b + 1) * 2 > 3)"},
        {'u', "UPDATE t SET a = (b + c) * 2 WHERE (x = 1 OR y = 2) AND z = 3"},
    };
    const char* path = "/tmp/same_meaning_cases.tsv";
    {
        std::ofstream out(path);
        for (const auto& [mode, sql] : cases) {
            const std::string generated = gen(sql);
            out << mode << '\t' << sql << '\t' << generated << '\n';
        }
    }
    const char* script = std::getenv("SAME_MEANING_CHECK");
    std::string command = std::string("python3 ") + (script ? script : SAME_MEANING_CHECK_DEFAULT) + " " + path + " 2>&1";
    FILE* pipe = popen(command.c_str(), "r");
    REQUIRE(pipe != nullptr);
    std::string output;
    char buffer[512];
    while (fgets(buffer, sizeof(buffer), pipe)) output += buffer;
    const int status = pclose(pipe);
    INFO(output);
    CHECK(status == 0);
}

TEST_CASE("NOT IN, NOT BETWEEN and NOT LIKE parse and keep what follows") {
    CHECK(gen("SELECT a FROM t WHERE x NOT IN (1, 2) AND y = 1") ==
          R"(SELECT "a" FROM "t" WHERE "x" NOT IN (1, 2) AND "y" = 1)");
    CHECK(gen("SELECT a FROM t WHERE x NOT IN (SELECT y FROM u) AND z = 1") ==
          R"(SELECT "a" FROM "t" WHERE "x" NOT IN (SELECT "y" FROM "u") AND "z" = 1)");
    CHECK(gen("SELECT a FROM t WHERE x NOT BETWEEN 1 AND 2 AND y = 1") ==
          R"(SELECT "a" FROM "t" WHERE "x" NOT BETWEEN 1 AND 2 AND "y" = 1)");
    CHECK(gen("SELECT a FROM t WHERE x NOT LIKE 'a%' AND y = 1") ==
          R"(SELECT "a" FROM "t" WHERE NOT "x" LIKE 'a%' AND "y" = 1)");
}

TEST_CASE("INTERVAL does not take the rest of the condition") {
    CHECK(gen("SELECT a FROM t WHERE d > NOW() - INTERVAL '1 day' AND x = 1") ==
          R"(SELECT "a" FROM "t" WHERE "d" > NOW() - INTERVAL('1 day') AND "x" = 1)");
}

TEST_CASE("a CAST type with parentheses keeps the rest of the query") {
    CHECK(gen_after_scramble("SELECT CAST(a AS DECIMAL(10, 2)), b FROM t WHERE c = 1") ==
          R"(SELECT CAST("a" AS DECIMAL(10, 2)), "b" FROM "t" WHERE "c" = 1)");
    CHECK(gen_after_scramble("SELECT CAST(a AS VARCHAR(10)) FROM t") ==
          R"(SELECT CAST("a" AS VARCHAR(10)) FROM "t")");
}

TEST_CASE("SAFE_CAST keeps its meaning in each dialect") {
    const std::string q = "SELECT SAFE_CAST(a AS INT) FROM t";
    CHECK(gen(q, SQLDialect::BigQuery) == "SELECT SAFE_CAST(`a` AS INT) FROM `t`");
    CHECK(gen(q, SQLDialect::SQLServer) == "SELECT TRY_CAST([a] AS INT) FROM [t]");
    CHECK(gen(q, SQLDialect::DuckDB) == R"(SELECT TRY_CAST("a" AS INT) FROM "t")");
    CHECK(gen(q, SQLDialect::PostgreSQL) == R"(SELECT SAFE_CAST("a" AS INT) FROM "t")");
    CHECK(gen("SELECT CAST(a AS INT) FROM t", SQLDialect::SQLServer) == "SELECT CAST([a] AS INT) FROM [t]");
}

TEST_CASE("a LATERAL subquery keeps its parentheses and alias") {
    CHECK(gen("SELECT * FROM t, LATERAL (SELECT 2) AS y") ==
          R"(SELECT * FROM "t" CROSS JOIN LATERAL (SELECT 2) AS "y")");
    CHECK(gen("SELECT * FROM t CROSS APPLY (SELECT 2) AS y", SQLDialect::SQLServer) ==
          "SELECT * FROM [t] CROSS APPLY (SELECT 2) AS [y]");
}

TEST_CASE("Oracle table and subquery aliases have no AS") {
    CHECK(gen("SELECT x.a FROM (SELECT a FROM t) x JOIN u y ON x.a = y.a", SQLDialect::Oracle) ==
          R"(SELECT "x"."a" FROM (SELECT "a" FROM "t") "x" INNER JOIN "u" "y" ON "x"."a" = "y"."a")");
}
