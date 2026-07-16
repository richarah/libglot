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

## Stage 4 - Differential testing (issue #7) [STRUCTURAL]

Run libglot and a mature implementation (Python stdlib `email`) over the
same corpus; diff parsed structure (part count, content types, header
values, decoded bodies, filenames). Turns every corpus message into an
assertion instead of a smoke test. Gate CI on the committed corpus; feed
disagreements into the fuzz corpus.

**Deliberately after stage 3**: a differential oracle run before the
envelope gaps are closed would report a flood of known-missing features
rather than real bugs.

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
