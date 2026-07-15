# libglot

Header-only C++20 framework for building parsers and transpilers, with two
domains built on it: a SQL parser/cross-dialect generator and a MIME/email
parser designed for hostile input.

Every claim in this README is enforced by CI: GCC + Clang, Debug/Release,
ASan/UBSan, warnings-as-errors, an install + `find_package` consumer check,
libFuzzer smoke runs, and a coverage report.

## SQL

Parses SQL into a common AST and regenerates it for a target dialect.

```cpp
#include <libglot/sql/parser.h>
#include <libglot/sql/generator.h>
#include <libglot/util/arena.h>

libglot::Arena arena;
libglot::sql::SQLParser parser(arena, "SELECT * FROM users LIMIT 10");
auto* ast = parser.parse_top_level();

libglot::sql::SQLGenerator gen(libglot::sql::SQLDialect::SQLServer);
std::string out = gen.generate(ast);   // SELECT TOP 10 * FROM [users]
```

Covered and test-backed: SELECT (joins, CTEs, window functions with real
frame clauses, set operations, GROUPING SETS/ROLLUP/CUBE), DML
(INSERT/UPDATE/DELETE/MERGE, OUTPUT/RETURNING), DDL (CREATE TABLE with full
column definitions and constraints, views, indexes, triggers), procedural
SQL (procedures, functions, IF/WHILE/FOR, cursors, exceptions), GRANT/
REVOKE, transactions, JSON operators, and an optional optimizer (constant
folding, boolean simplification).

**Dialects, honestly:** first-class, test-backed behavior for ANSI,
PostgreSQL, MySQL, SQLite, SQL Server, and Snowflake (dialect-aware lexing,
LIMIT/TOP/OFFSET-FETCH mapping, boolean spelling, ILIKE polyfill, RAISE/
SIGNAL/RAISERROR, FOR→WHILE lowering), partial support for Oracle, DB2,
Firebird, Informix, and BigQuery, and quoting-only defaults for the rest of
the 45-entry dialect enum. `sql/tests/test_dialect_feature_combinations.cpp`
and `test_roundtrip_property.cpp` are the source of truth.

**Transpiler contract:** `generate(parse(q))` is a fixed point — generated
SQL re-parses to identical output. This is enforced by a property test over
a corpus across four dialects, and by a fuzzer.

## MIME

One entry point, built for untrusted email:

```cpp
#include <libglot/mime/mime.h>

libglot::Arena arena;
auto result = libglot::mime::parse_message(arena, raw_bytes);
// result.message   — header/part tree
// result.report    — recorded anomalies (severity + applied policy)
// result.rejected  — true when a Reject-policy Security/DoS anomaly fired
```

RFC 5322 header parsing with unfolding and comment stripping, RFC 2045/2046
multipart with line-anchored boundary matching, RFC 2047 encoded words and
RFC 2231 parameter continuations decoded to UTF-8, strict base64/quoted-
printable, charset conversion (ISO-8859-1, Windows-1252), resource limits
(nesting depth, part count), and a 75-kind anomaly taxonomy with
per-severity policies (Ignore/Repair/Reject).

## Building

```
git clone https://github.com/richarah/libglot
cd libglot
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
```

Requires C++20 (GCC 12+/Clang 16+ are exercised in CI). Header-only; consume
via `add_subdirectory` or `find_package(libglot)` after `cmake --install`
(targets `libglot::core`, `libglot::sql`, `libglot::mime`).

Options: `LIBGLOT_BUILD_EXAMPLES` (ON), `LIBGLOT_BUILD_BENCHMARKS` (OFF),
`LIBGLOT_BUILD_FUZZERS` (OFF, Clang), `LIBGLOT_WERROR`, sanitizer toggles;
see `CMakePresets.json` for ready-made configurations.

## Project structure

```
core/      Framework: concepts, CRTP ParserBase/GeneratorBase, arena, interning
sql/       SQL tokenizer (sql/lex/), parser, generator, optimizer, dialects
mime/      MIME pipeline: headers, multipart, encodings, anomalies, limits
fuzz/      libFuzzer harnesses (SQL parser, roundtrip contract, MIME parser)
examples/  sql_transpile, mime_inspect (built by default)
docs/      ARCHITECTURE.md (verified design doc), plans, migration history
```

## Status

Actively developed. The test suite (Catch2 + CTest) currently runs 800+
tests including property-based roundtrip tests; parsers are fuzzed under
ASan/UBSan. Known limitations are listed at the end of
`docs/ARCHITECTURE.md` — notably: dialect depth beyond the first-class set
is quoting/traits only, and MIME charset conversion is deliberately small
(no ICU dependency).

## License

MIT — see [LICENSE](LICENSE).
