# libglot

Header-only C++20 framework for building parsers and transpilers, with two
domains built on it: a SQL parser/cross-dialect generator and a MIME/email
parser designed for hostile input.

Every claim below names the thing that proves it. CI runs GCC + Clang ×
Debug/Release, ASan/UBSan, warnings-as-errors, clang-tidy, an install +
`find_package` consumer check, libFuzzer smoke runs, a coverage report, and
two corpus gates.

## Verified

| Claim | Evidence |
|---|---|
| **1,287 tests**, including generate→parse fixed-point property tests | `ctest`; CI on GCC and Clang, Debug and Release |
| **Differentially tested against Python's `email`** — the parsed structure of a message is compared field by field, not just "did it crash" | `scripts/mime_diff.py` + `tools/mime_dump`; CI job `mime-differential` gates the committed corpus at **100% agreement**. On a 500-message raw SpamAssassin sample: **79% agreement**, residual classified in [`docs/ROADMAP.md`](docs/ROADMAP.md#stage-5---corpus-breadth---done-partial-see-remaining-work) |
| **98.6% parse / 99.0% text-decode** over 3,303 real messages (raw SpamAssassin, mbox-split, no preprocessing) | `tools/mime_corpus --mbox`; CI job `mime-corpus` |
| **SQL: 33–58× faster parse, 47–93× faster transpile** than Python sqlglot 30.12 | [`bench/RESULTS_2026-07.md`](bench/RESULTS_2026-07.md) — methodology and caveats included |
| **MIME: 8–227× faster** than Python's `email` — 139–227× vs `policy.default`, 8–15× vs the lazier `compat32`. Both are stated because the honest number depends on how much work you ask Python to do | [`bench/RESULTS_2026-07.md`](bench/RESULTS_2026-07.md) |
| Parsers fuzzed under ASan/UBSan; the transpiler round-trip contract is fuzzed too | `fuzz/`; CI job `fuzzers` |

## Honest limits

- **22 of 45 dialects are first-class** (test-backed behavior). The other 23
  differ only in quoting/traits and are labelled as such — not counted as
  "45 dialects supported". See [`docs/FEATURE_MATRIX.md`](docs/FEATURE_MATRIX.md).
- **Charset support is deliberately small** (UTF-8, US-ASCII, ISO-8859-1,
  ISO-8859-15, Windows-1252, UTF-16). No ICU dependency. Unknown charsets
  are reported as unknown, never silently mislabelled.
- **No signature verification.** `multipart/signed` parts are preserved
  byte-exactly so a caller can verify them; libglot ships no crypto.
- **mbox is a storage format**, not a message: the parser takes one RFC 5322
  message. Splitting is a tooling concern (`--mbox`).
- Out of scope, failing cleanly rather than emitting something wrong: XML
  functions, PL/SQL packages, cost-based optimization, `message/partial`
  reassembly.

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

**Dialects, honestly:** 22 first-class, test-backed dialects — ANSI,
PostgreSQL, MySQL, SQLite, SQL Server, Snowflake, Oracle, DB2, BigQuery,
DuckDB, plus the PostgreSQL family (Redshift, Greenplum, TimescaleDB,
CockroachDB, YugabyteDB, Citus, RisingWave, Materialize), the MySQL family
(MariaDB, TiDB, SingleStore) and Azure Synapse. Dialects are expressed as a
family base profile plus an explicit delta, so a family member states only
what genuinely differs. The remaining 23 entries in the enum differ only in
quoting/traits and are labelled as such rather than counted as support.
`sql/tests/test_dialect_*.cpp` and `test_roundtrip_property.cpp` are the
source of truth.

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

Actively developed. See [`docs/FEATURE_MATRIX.md`](docs/FEATURE_MATRIX.md)
for the row-by-row source of truth (every row is DONE with a named test, or
explicitly out of scope with its rejection behavior) and
[`docs/ROADMAP.md`](docs/ROADMAP.md) for what is measured and what remains.

## License

MIT — see [LICENSE](LICENSE).
