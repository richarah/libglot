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
| MERGE ... WHEN NOT MATCHED BY SOURCE (T-SQL) | DONE | test_merge_extended, test_roundtrip_property ("MERGE WHEN NOT MATCHED BY SOURCE"); `MergeStmt` reworked from single UPDATE/INSERT slots into an ordered `when_clauses` list (`MergeWhenClause`: match kind, optional `AND` condition, action) so WHEN MATCHED THEN DELETE and a WHEN MATCHED AND cond THEN ... condition are also modeled, not just the T-SQL-specific clause; NOT_MATCHED_BY_SOURCE throws std::logic_error outside SQL Server/Azure Synapse |
| OUTPUT / RETURNING (cross-dialect) | DONE | test_output_clause |
| CREATE TABLE full column/constraint schema | DONE | test_schema_type, test_fk_check_constraints |
| CREATE TABLE trailing table options (ENGINE=, DISTSTYLE, ...) | DONE | test_table_options, test_roundtrip_property ("CREATE TABLE trailing table options"); modeled as an ordered `(name, value, has_equals)` list on `CreateTableStmt`, regenerated verbatim - never dialect-gated (every dialect's own trailing syntax round-trips); name/value boundaries are a documented best-effort heuristic (a small whitelist of recognized option-start keywords, paren-depth aware) covering the forms in the spec, not a full per-dialect option grammar |
| CREATE/ALTER/DROP SEQUENCE, NEXTVAL/CURRVAL | DONE | test_sequences, test_roundtrip_property ("sequences"); NEXTVAL('seq')/CURRVAL('seq') function-style and Oracle's member-style `seq.NEXTVAL`/`seq.CURRVAL` both canonicalize to one `SequenceRefExpr` node, regenerated per dialect (Oracle member-style, function-style elsewhere); MySQL/SQLite throw std::logic_error (no sequence object) |
| Temporal tables (`FOR SYSTEM_TIME AS OF ...`) | DONE | test_temporal_tables, test_roundtrip_property ("temporal tables"); all four SQL:2011 forms (AS OF / FROM..TO / BETWEEN..AND / CONTAINED IN / ALL) parse onto `TableRef`; generates for SQL Server, Azure Synapse, MariaDB; every other dialect throws std::logic_error |
| CONNECT BY / START WITH (Oracle, Snowflake) | DONE | test_connect_by; non-native dialects throw |
| CONNECT BY → recursive CTE lowering | DONE (issue #2) | test_connect_by_lowering; `sql/include/libglot/sql/transforms.h::lower_connect_by` rewrites `START WITH s CONNECT BY [PRIOR] c` into `WITH RECURSIVE hierarchy AS (anchor WHERE s UNION ALL recursive JOIN hierarchy ON c')`, `PRIOR expr` in `c` qualified by the CTE (parent row), everything else by the source alias (child row); a `level` column is added only when `LEVEL` is referenced anywhere in the query; `WHERE` is re-applied on the outer SELECT (Oracle semantics: filtered after the hierarchy is built), never inside the anchor. Opt-in via `SQLGenerator(dialect, libglot::Arena* transform_arena)` - without an arena, non-Oracle/Snowflake dialects still throw std::logic_error as before (message now also points at the arena). No clean lowering exists for CONNECT BY NOCYCLE (needs a key column to build a cycle path, not derivable from syntax alone), ORDER SIBLINGS BY (depends on the traversal path), or a JOIN/multi-table FROM combined with CONNECT BY - all three throw std::logic_error with a specific message; CONNECT_BY_ROOT/SYS_CONNECT_BY_PATH are moot since the parser doesn't accept them in expression position at all |
| Procedural SQL (IF/WHILE/FOR, cursors, RAISE map) | DONE | test_procedure_dialects, test_for_keyword |
| FOR record IN SELECT loops, REVERSE | DONE | test_for_keyword, test_roundtrip_property ("FOR record/REVERSE loop forms"); `ForLoop` extended with `reverse` and a `query` slot (mutually exclusive with the range form); record iteration generates PostgreSQL's bare `FOR rec IN SELECT ...` and Oracle's parenthesized `FOR rec IN (SELECT ...)`, and throws std::logic_error for the T-SQL lowering (no direct equivalent); REVERSE lowers to a descending WHILE for T-SQL |
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
| MySQL fulltext `MATCH ... AGAINST` | DONE | test_fulltext_match, test_roundtrip_property ("MySQL fulltext"); dedicated `MatchAgainst` node covers all four AGAINST modifiers (bare/NATURAL LANGUAGE MODE/+WITH QUERY EXPANSION/BOOLEAN MODE/WITH QUERY EXPANSION alone); MySQL/MariaDB only, everything else throws std::logic_error. Parsing the search argument required suppressing the generic `expr IN (...)` postfix (a scoped `no_in_postfix_` flag) so `AGAINST('x' IN NATURAL LANGUAGE MODE)` doesn't misparse "IN" as the value-list operator |
| BigQuery STRUCT literal / ARRAY subscript edge cases | DONE | test_struct_array_subscript, test_roundtrip_property ("BigQuery STRUCT ... array subscript"); `STRUCT(...)` (already parsed generically as a FunctionCall) now throws std::logic_error for every dialect but BigQuery at generation time; `ArrayIndex` gained a `subscript` field (NONE/OFFSET/ORDINAL/SAFE_OFFSET) so `arr[OFFSET(0)]`/`arr[ORDINAL(1)]`/`arr[SAFE_OFFSET(0)]` generate only for BigQuery while plain `arr[index]` is untouched everywhere. Required adding a BigQuery `TokenizerConfig` (bracket_identifiers=false) - BigQuery previously inherited the ANSI default bracket-quoted-identifier lexing, which made `identifier[...]` unparseable as a subscript at all; PostgreSQL/MySQL/ANSI still can't lex bare `ident[...]` subscripting (pre-existing, asserted in test_tokenizer.cpp) and are out of scope here |
| Snowflake `FLATTEN` table function | DONE | test_flatten, test_roundtrip_property ("Snowflake LATERAL FLATTEN"); `LATERAL FLATTEN(INPUT => expr [, PATH => '...'] [, OUTER => bool])` parses onto a dedicated `FlattenClause` wrapped in the existing `LateralJoin` node; required a new `=>` token (FAT_ARROW) in the tokenizer. Snowflake only; every other dialect throws std::logic_error |
| PG `?` key-exists fixpoint (lexes as operator) | DONE (documented exclusion) | test_roundtrip_property header |
| First-class set: ANSI, PG, MySQL, SQLite, MSSQL, Snowflake, Oracle, DB2, BigQuery, DuckDB | DONE | test_dialect_feature_combinations, test_dialect_{oracle,db2,bigquery,duckdb}, test_roundtrip_property |
| Promote Oracle, DB2, BigQuery, DuckDB | DONE (issue #3) | one conformance suite per dialect with exact-string roundtrips and fixpoints: test_dialect_oracle, test_dialect_db2, test_dialect_bigquery, test_dialect_duckdb. Remaining dialects in the 45-entry enum are still quoting/traits only and are documented as such |

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
| base64 / quoted-printable / RFC 2047 **encode** | DONE | test_mime_encoding; `TransferEncoding::encode_base64` (RFC 2045, 76-char CRLF-wrapped, exact-string + binary-data round-trip + 75/76/77-char wrap-boundary cases) and `encode_base64_raw` (unwrapped, used standalone and by encoded-words); `TransferEncoding::encode_quoted_printable` (non-printables and `=` escaped, trailing space/tab escaped, existing CR/LF passed through untouched as hard breaks, soft `=\r\n` breaks so no line exceeds 76 cols, 75/76/77-char boundary cases); `EncodedWordDecoder::encode_word` (RFC 2047 `=?UTF-8?B?...?=` / `?Q?`, splits into multiple encoded-words on the 75-char limit at UTF-8 codepoint boundaries, non-ASCII-subject and emoji round-trip tests) |
| Charsets: ISO-8859-1, Windows-1252 → UTF-8 | DONE | test_mime_encoding |
| UTF-16 (BE/LE, BOM) → UTF-8 | DONE | test_charset_utf16; `CharsetConverter::utf16_to_utf8` (RFC 2781) - FEFF/FFFE BOM detection (consumed, overrides the passed-in default), big-endian default per RFC 2781 when no BOM, surrogate-pair combination (emoji), unpaired high/low surrogates and a truncated trailing byte replaced with U+FFFD (never throws, output re-validated with `is_valid_utf8`); wired into `Charset::UTF16`/`UTF16BE`/`UTF16LE` (`to_utf8`) and `decoded_body_utf8()` so `charset=UTF-16`/`UTF-16BE`/`UTF-16LE` parts decode through the normal pipeline |
| Asian charsets (Shift-JIS, EUC-KR, GB2312) | OOS | reported as unknown-charset, never mislabeled |
| message/partial detection | DONE | test_message_partial; `Content-Type: message/partial` detected in `finish_message` (parser_extended.h), `id`/`number`/`total` parsed onto a new `MessagePartialRef` (complete_features.h, `Message::message_partial`) with `std::from_chars`-based defensive numeric parsing (malformed/negative/overflowing values default to 0, never throws); records the new `AnomalyKind::MessagePartialDetected` (Structural severity) so callers know reassembly with sibling fragments is required; absent for normal messages and for `message/external-body`; reassembly itself is out of scope |
| Corpus benchmark (SpamAssassin/Enron) | DONE (issue #4) | tools/mime_corpus runs any message directory through the pipeline and reports parse success, policy rejections, text-decode rate and an anomaly histogram; exits non-zero below --min-success. CI: committed corpus (tests/corpus/mime) gated at 100%, SpamAssassin public corpus run best-effort and reported |

## Engineering standards

| Item | Status | Plan |
|---|---|---|
| CI: GCC+Clang, ASan/UBSan, Werror, install test | DONE | .github/workflows/ci.yml |
| Fuzzers (parser, roundtrip contract, MIME) | DONE | fuzz/ |
| Coverage report in CI | DONE | ci.yml coverage job |
| Benchmarks re-run with current code, numbers recorded | DONE | bench/RESULTS_2026-07.md |
| Repo-wide clang-format + .git-blame-ignore-revs | DONE | style commit listed in .git-blame-ignore-revs; `git config blame.ignoreRevsFile .git-blame-ignore-revs` |
| clang-tidy | DONE | blocking CI job (clang-tidy-18, warnings-as-errors) over every first-party TU, which transitively covers the whole public header surface. Correctness findings fixed: int-widening in mime/limits.h and core/util/arena.h size constants, char-narrowing in mime/encoding.h and sql/lex/keywords.h, vestigial cross-namespace forward declarations in sql/lex/fwd.h, exception escaping from an example's main. Style-tier checks that conflict with the project's deliberate idiom (constexpr C-arrays, constructor init lists, single-statement ifs) are disabled in .clang-tidy with rationale; fuzz/.clang-tidy scopes off bugprone-empty-catch, since swallowing expected parse errors is the point of a fuzz target |
| SECURITY.md (reporting, threat model) | DONE | SECURITY.md |
| Doxygen config for public headers | DONE | Doxyfile (output docs/api/) |
