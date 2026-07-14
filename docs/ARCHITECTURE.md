# libglot Architecture

libglot is a header-only C++20 framework for building parsers and
transpilers, with two production domains: SQL (parse + cross-dialect
generation) and MIME (hostile-input email parsing). Everything described
here is implemented and exercised by the test suite; where something is
partial, this document says so.

## Layout

```
core/   Domain-agnostic infrastructure (concepts, CRTP bases, arena, interning)
sql/    SQL tokenizer, parser, generator, dialect handling
mime/   MIME pipeline: headers, multipart, encodings, anomalies, limits
fuzz/   libFuzzer harnesses (SQL parser, SQL roundtrip contract, MIME parser)
examples/  Small programs using the public APIs (built in CI)
```

## Core (`core/include/libglot/`)

- **Concepts** (`lex/spec.h`, `parse/grammar.h`, `ast/node.h`,
  `dialect/traits.h`): `TokenSpec`, `GrammarSpec`, `AstNode`,
  `DialectTraits` define the contract a domain implements. Both domains
  `static_assert` conformance.
- **`ParserBase<Spec, Derived>`** (`parse/parser.h`): CRTP recursive-descent
  base with precedence-climbing expression parsing. Postfix forms are
  interleaved with the binary-operator loop so `f(x) + 1` and
  `x IN (...) AND y` compose correctly. A recursion-depth guard
  (`kMaxRecursionDepth = 256`) bounds hostile nesting. There are no virtual
  functions; `token_name` is a shadowed CRTP customization point.
- **`GeneratorBase<Spec, Derived>`** (`gen/generator.h`): output writer with
  dialect features, indentation, and quote-doubling string emission.
- **`Arena`** (`util/arena.h`): monotonic chunk allocator. `create<T>`
  registers destructors of non-trivially-destructible objects and runs them
  at `reset()`/destruction, so nodes holding vectors/strings do not leak.
  Sources are copied into the arena (`copy_source`) so every token and AST
  `string_view` outlives the parse (see `core/include/libglot/LIFETIME.md`).
- **String interning** (`util/intern.h`): thread-safe `StringPool` and
  per-parse `LocalStringPool`.
- **Error handling** (`parse/error_recovery.h`): `ParseError` with
  line/column (32-bit), plus an `ErrorCollector` for multi-error reporting.

Dispatch is compile-time (CRTP + concepts). That is a real property of the
code, not a benchmark claim; performance numbers belong in `bench/` and are
only cited when measured.

## SQL (`sql/include/libglot/sql/`)

- **Tokenizer** (`lex/`): self-contained (vendored and owned by this repo,
  namespace `libglot::sql::lex`). `TokenizerConfig` captures genuinely
  lexical dialect differences: `#` comments vs `#temp` identifiers vs
  `#>`/`#>>` JSON operators, Snowflake `:` path access, PostgreSQL `?`
  key-exists, bracket identifiers. Dollar-quoted strings, doubled-quote
  escapes, hex/binary literals.
- **Parser** (`parser.h`): `SQLParser : ParserBase`. Statements: SELECT
  (joins, CTEs, windows with real frame clauses, set operations
  left-associative), INSERT/UPDATE/DELETE/MERGE, CREATE TABLE with full
  column definitions and table constraints, CREATE VIEW/INDEX/PROCEDURE/
  FUNCTION/TRIGGER, GRANT/REVOKE, transactions, and procedural SQL
  (DECLARE, IF/WHILE/FOR/LOOP, cursors, exceptions, RAISE).
- **Generator** (`generator.h`): precedence-aware parenthesization (shares
  the parser's precedence table, so they cannot drift), quote-escaped
  identifiers and literals, dialect-specific emission (quoting style,
  LIMIT/TOP/FIRST-SKIP, boolean literal spelling, ILIKE polyfill,
  FOR→WHILE lowering for T-SQL, RAISE/SIGNAL mapping). Unhandled node
  kinds throw `std::logic_error` instead of silently emitting nothing.
- **Dialects** (`dialect_traits.h`): 45 enum values exist, but only a
  subset has first-class, test-backed behavior (ANSI, PostgreSQL, MySQL,
  SQLite, SQL Server, Snowflake, and partially Firebird/Informix/BigQuery).
  The rest currently differ only in quoting/boolean traits. The dialect
  matrix in the test suite (`test_dialect_feature_combinations.cpp`,
  `test_roundtrip_property.cpp`) is the source of truth for what each
  dialect actually does.

**Transpiler contract** (enforced by tests and a fuzzer): anything the
parser accepts, the generator must emit as SQL that re-parses to the same
output (`generate(parse(q))` is a fixed point).

## MIME (`mime/include/libglot/mime/`)

Single entry point (`mime.h`):

```cpp
libglot::Arena arena;
auto result = libglot::mime::parse_message(arena, raw_bytes, options);
// result.message, result.report (anomalies), result.rejected
```

The pipeline: header unfolding (RFC 5322) → header parsing → comment
stripping, RFC 2231 parameter continuation/percent/charset decoding,
Content-Type validation, address groups → body extraction → RFC 2046
multipart splitting (line-anchored boundary matching, close-delimiter
semantics, preamble/epilogue) with recursive parts, external-body refs →
limits enforcement (`ParserLimits`: nesting depth, part count) → anomaly
recording per `AnomalyConfig` policy (Ignore/Repair/Reject; Reject on
Security/DoS marks the result rejected and stops descent).

Transfer decoding (strict base64, quoted-printable) and charset conversion
(ISO-8859-1, Windows-1252, US-ASCII/UTF-8 passthrough) are exposed via
`decoded_body()` / `decoded_body_utf8()`. RFC 2047 encoded words decode to
UTF-8. Unknown charsets are reported, not silently mislabeled.

## Testing and verification

- Catch2 suites per module, registered with CTest via
  `catch_discover_tests`; `ctest` from the build root runs everything.
- Assertions are exact strings or AST-shape checks, not substrings.
- Property tests: generate→parse fixed point across dialects
  (`sql/tests/test_roundtrip_property.cpp`).
- Fuzzing: three libFuzzer harnesses under ASan/UBSan
  (`-DLIBGLOT_BUILD_FUZZERS=ON`, Clang), smoke-run in CI.
- CI (GitHub Actions): GCC + Clang × Debug/Release, ASan/UBSan jobs,
  warnings-as-errors, install + `find_package` consumer smoke test,
  fuzz smoke, coverage report artifact.

## Known limitations

- Dialect depth beyond the first-class set is quoting/traits only.
- The optimizer from earlier plans does not exist; if/when added it will be
  built pass-by-pass with tests (constant folding first).
- MIME charset support is intentionally small (no ICU dependency);
  UTF-16 and Asian charsets are detected but not converted.
- `logglot` (log-format parsing, `docs/LOGGLOT_PLAN.md`) is a plan, not code.
