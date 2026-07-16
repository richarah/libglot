# Roadmap: structural wins, then breadth

Execution order matters here. Each stage is a prerequisite for the one
below it: doing them out of order means doing the work twice.

Status is tracked per stage; `docs/FEATURE_MATRIX.md` remains the
row-by-row source of truth for what is DONE vs OOS.

## Stage 1 - Dialect family inheritance (issue #5) [STRUCTURAL]

45 dialect rows are hand-maintained; promoting a dialect currently means
duplicating what its family already does. Express families in
`dialect_traits.h` (base profile + delta, constexpr, no runtime cost) so a
family member is a delta plus a conformance suite, correct by construction.

**Blocks stage 2.** Doing stage 2 first would produce 12 copy-pasted
dialects that then have to be rewritten.

## Stage 2 - Promote the family-member dialects (issue #3 follow-on)

With families in place, promote the ~12 near-free members:

- PostgreSQL family: Redshift, Greenplum, TimescaleDB, CockroachDB,
  YugabyteDB, Citus, RisingWave, Materialize
- MySQL family: MariaDB, TiDB, SingleStore
- T-SQL family: Azure Synapse

Each: delta + conformance suite + fixpoint corpus entries. Result: ~22
first-class dialects covering the overwhelming majority of real usage.

The remaining ~15 (Informix, Firebird, SAP HANA, Dremio, MonetDB, Drill,
Spanner, QuestDB, H2, HSQLDB, Derby, ClickHouse, Teradata, Vertica,
Netezza, Exasol, Presto/Trino, Athena, Hive/Spark/Databricks/Impala) are
genuine per-dialect work. They stay honestly labelled as quoting/traits
only until individually promoted - we do not pad the headline number.

## Stage 3 - MIME envelope gaps (issue #6)

Ordered by real-world frequency:

1. `message/rfc822` recursion (forwarded/attached mail; today not recursed)
2. `Date:` parsing (RFC 5322 date-time; today an opaque string)
3. `multipart/report` (RFC 6522 DSNs/bounces)
4. `Message-ID` / `In-Reply-To` / `References` (msg-id syntax; threading)
5. `multipart/related` `start` (RFC 2387), `Content-ID`/`Location`/
   `Description`/`Language`
6. RFC 6532 internationalized (raw UTF-8) headers
7. `multipart/signed` / `encrypted` (RFC 1847) - requires byte-exact
   canonical preservation of the signed part, or signatures break

## Stage 4 - Differential testing (issue #7) [STRUCTURAL] - DONE

Run libglot and a mature implementation (Python stdlib `email`) over the
same corpus; diff parsed structure (part count, content types, header
values, decoded bodies, filenames). Turns every corpus message into an
assertion instead of a smoke test. Gate CI on the committed corpus; feed
disagreements into the fuzz corpus.

**Deliberately after stage 3**: a differential oracle run before the
envelope gaps are closed would report a flood of known-missing features
rather than real bugs.

### Measured results (2026-07-16)

- Committed corpus (`tests/corpus/mime`, 6 messages): **100% agreement**,
  gated in CI by the `mime-differential` job.
- 500 real SpamAssassin messages: **92.2% agreement** (405/500 before a
  harness fix; the first run's dominant "disagreement" was the harness's
  own bug - mime_dump applies RFC 2045's absent-header `charset=us-ascii`
  default and the Python side did not, so the two sides differed by
  convention rather than on content).
- Robustness over 3,302 SpamAssassin messages: **98.64% parse**, 98.2% of
  text parts decoded. The raw figure is 11% because those files are mbox:
  each begins with a `From <addr> <date>` separator line, which is a
  storage envelope, not RFC 5322 content. Stripping it is a tooling
  concern - and proper mbox support means *splitting* one file into many
  messages (some corpus files carry a `From ` line mid-file), not dropping
  line 1. Tracked as issue #9; the parser stays strict by design.

### Residual disagreements, classified

- **ISO-8859-15 bodies** (7 of 500): libglot has no ISO-8859-15 decoder, so
  it reports the charset as unknown rather than mislabelling the bytes.
  Latin-9 is Latin-1 with eight substitutions, so this is a cheap, honest
  win - issue #8.
- **us-ascii-declared bodies containing 8-bit bytes**: Python's strict
  decode raises and yields no text; libglot passes the bytes through. Both
  defensible; libglot is the more useful behaviour here.
- **A malformed date zone** (`19:21:44 01800`): Python resolves it to
  +18:00; libglot declines to parse and records `InvalidDateFormat`.
  Python is being extremely lenient with a zone that is not valid syntax.
- **Address/subject formatting**: display-name and folding conventions
  between the two canonicalizations, not content differences.

The oracle has not yet found a libglot correctness bug - which is itself
the useful result, given it found several in the harness.

## Stage 5 - Corpus breadth

With a differential oracle in place, scale up: SpamAssassin (already
best-effort in CI), Enron (~500k messages, the real scale test), Apache
James mime4j and Python `email` test suites (RFC edge cases with known
expected outputs), and parser-differential/security corpora. Publish real
success and agreement rates; retire estimates.

## Non-goals (unchanged)

XML functions, PL/SQL packages, cost-based optimization, Asian charsets,
`message/partial` reassembly. These fail cleanly rather than silently
emitting something wrong, and are listed OOS in the feature matrix.
