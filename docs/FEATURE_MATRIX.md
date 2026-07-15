# Feature matrix: claimed/expected vs implemented

The single source of truth for what libglot has, what it once claimed to
have, and what remains. Every DONE row is enforced by named tests; every
GAP row is either scheduled (issue/wave) or explicitly out of scope with
the rejection behavior documented. Update this file in the same PR as the
feature.

Legend: **DONE** (tested) · **GAP** (planned) · **OOS** (out of scope —
parser must fail cleanly, never silently mis-parse).

## SQL — statements and clauses

| Feature | Status | Evidence / plan |
|---|---|---|
| SELECT core (joins, subqueries, CTEs, set ops) | DONE | test_parser, test_cte_windows_subqueries, test_roundtrip_property |
| Window functions incl. real frames, GROUPS | DONE | test_unbounded_following |
| Named windows (`WINDOW w AS (...)`) | DONE | test_named_windows; `OVER w` references a `WINDOW` clause entry; `WINDOW` is a soft keyword (lexes as IDENTIFIER, no reserved-word cost) |
| GROUPING SETS / ROLLUP / CUBE (nested, empty set) | DONE | test_group_by_extensions |
| ORDER BY ... NULLS FIRST/LAST | DONE | test_order_by_nulls, test_roundtrip_property ("ORDER BY NULLS FIRST/LAST"); MySQL/MariaDB/SQLServer/AzureSynapse throw std::logic_error (no native syntax - chose "throw" over the ISNULL/CASE-prefix workaround) |
| DISTINCT ON (PostgreSQL) | DONE | test_distinct_on, test_roundtrip_property ("DISTINCT ON"); every other dialect throws std::logic_error |
| VALUES as table source (`FROM (VALUES ...) v(c1)`) | DONE | test_values_table_source, test_roundtrip_property corpus; reuses the previously-dormant `ValuesClause` node with an added alias + column list |
| USING / NATURAL joins | DONE | test_join_using_natural, test_roundtrip_property corpus |
| TABLESAMPLE | DONE | test_tablesample, test_roundtrip_property ("TABLESAMPLE"); fixed two bugs - the keyword check tested `TK::IDENTIFIER` but `TABLESAMPLE` lexes as its own reserved token (branch was dead code), and the `Tablesample` node had no field for the sampled table (silently discarded it). Added `REPEATABLE(seed)`. MySQL/MariaDB throw std::logic_error |
| QUALIFY | DONE | test_qualify, test_roundtrip_property ("QUALIFY"); fixed a silent-drop bug - `SelectStmt::qualify` parsed correctly but `visit_select_stmt` never read it back out, so QUALIFY vanished from generated SQL with no error. Now emitted for Snowflake/BigQuery/DuckDB; every other dialect throws std::logic_error |
| INTERVAL literals | DONE | test_interval_literals, test_roundtrip_property corpus; replaced a broken `FunctionCall("INTERVAL", ...)` encoding (regenerated as `INTERVAL(7, DAY)`, invalid SQL and not a fixed point) with a dedicated `IntervalLiteral` node covering both `INTERVAL '1 day'` and `INTERVAL '2' HOUR` / `INTERVAL 7 DAY` |
| INSERT ... ON CONFLICT (PG) / ON DUPLICATE KEY UPDATE (MySQL) | DONE | test_upsert, test_roundtrip_property ("upsert forms"); same-dialect fixpoint only - cross-dialect PG&lt;-&gt;MySQL transpile throws std::logic_error (conflict-target columns and EXCLUDED/VALUES() semantics don't map over cleanly) |
| MERGE (all WHEN arms) | DONE | test_bugfix_regressions |
| MERGE ... WHEN NOT MATCHED BY SOURCE (T-SQL) | GAP (wave 2) | |
| OUTPUT / RETURNING (cross-dialect) | DONE | test_output_clause |
| CREATE TABLE full column/constraint schema | DONE | test_schema_type, test_fk_check_constraints |
| CREATE TABLE trailing table options (ENGINE=, DISTSTYLE, ...) | GAP (wave 2) | currently consumed, not modeled; model + regenerate |
| CREATE/ALTER/DROP SEQUENCE, NEXTVAL/CURRVAL | GAP (wave 2) | historically claimed "partial" |
| Temporal tables (`FOR SYSTEM_TIME AS OF ...`) | GAP (wave 2) | historically claimed "syntax support" |
| CONNECT BY / START WITH (Oracle, Snowflake) | DONE | test_connect_by; non-native dialects throw |
| CONNECT BY → recursive CTE lowering | GAP (issue #2) | |
| Procedural SQL (IF/WHILE/FOR, cursors, RAISE map) | DONE | test_procedure_dialects, test_for_keyword |
| FOR record IN SELECT loops, REVERSE | GAP (wave 2) | currently clean ParseError |
| GRANT/REVOKE, transactions, utility stmts | DONE | test_grant_revoke, test_utility_statements |
| XML functions (SQL:2003) | OOS | clean ParseError; revisit on demand |
| Polymorphic table functions (SQL:2016) | OOS | clean ParseError |
| Oracle PL/SQL packages | OOS | clean ParseError |

## SQL — dialect-specific constructs

| Feature | Status | Evidence / plan |
|---|---|---|
| Dialect-aware lexing (TokenizerConfig) | DONE | test_tokenizer |
| LIMIT / TOP / OFFSET-FETCH / FIRST-SKIP mapping | DONE | test_bugfix_regressions, test_dialect_feature_combinations |
| Boolean spelling, quoting styles, ILIKE polyfill | DONE | test_dialect_feature_combinations |
| MySQL fulltext `MATCH ... AGAINST` | GAP (wave 2) | historically claimed missing |
| BigQuery STRUCT literal / ARRAY subscript edge cases | GAP (wave 2) | |
| Snowflake `FLATTEN` table function | GAP (wave 2) | lateral flatten in FROM |
| PG `?` key-exists fixpoint (lexes as operator) | DONE (documented exclusion) | test_roundtrip_property header |
| First-class set: ANSI, PG, MySQL, SQLite, MSSQL, Snowflake | DONE | matrix tests |
| Promote Oracle, DB2, BigQuery, DuckDB | GAP (issue #3) | |

## SQL — optimizer

| Feature | Status | Evidence / plan |
|---|---|---|
| Constant folding (int, string ||) with guards | DONE | test_optimizer |
| Boolean simplification | DONE | test_optimizer |
| WHERE TRUE pruning | DONE | test_optimizer |
| Predicate/projection pushdown, join reordering | OOS | old claims; needs schema/cardinality model to be real |

## MIME

| Feature | Status | Evidence / plan |
|---|---|---|
| Single pipeline entry (`parse_message`) w/ policies | DONE | test_pipeline, test_mime_anomalies |
| CRLF/LF, folded headers, comments, address groups | DONE | test_mime_parser, test_header_comments, test_address_groups |
| RFC 2046 multipart (anchored boundaries, limits) | DONE | test_boundary_recovery, test_mime_multipart |
| RFC 2231 continuations (decode) | DONE | test_rfc2231_continuations |
| base64 / quoted-printable **decode** (strict) | DONE | test_mime_encoding |
| base64 / quoted-printable / RFC 2047 **encode** | GAP (wave 3) | "full encode/decode" was claimed; only decode exists |
| Charsets: ISO-8859-1, Windows-1252 → UTF-8 | DONE | test_mime_encoding |
| UTF-16 (BE/LE, BOM) → UTF-8 | GAP (wave 3) | no ICU needed |
| Asian charsets (Shift-JIS, EUC-KR, GB2312) | OOS | reported as unknown-charset, never mislabeled |
| message/partial detection | GAP (wave 3) | detect + anomaly; reassembly OOS |
| Corpus benchmark (SpamAssassin/Enron) | GAP (issue #4) | |

## Engineering standards

| Item | Status | Plan |
|---|---|---|
| CI: GCC+Clang, ASan/UBSan, Werror, install test | DONE | .github/workflows/ci.yml |
| Fuzzers (parser, roundtrip contract, MIME) | DONE | fuzz/ |
| Coverage report in CI | DONE | ci.yml coverage job |
| Benchmarks re-run with current code, numbers recorded | GAP (wave 4) | bench preset; publish in bench/ |
| Repo-wide clang-format + .git-blame-ignore-revs | GAP (wave 4) | |
| clang-tidy clean | GAP (wave 4) | local run + fix; CI job optional |
| SECURITY.md (reporting, threat model) | GAP (wave 4) | |
| Doxygen config for public headers | GAP (wave 4) | |
