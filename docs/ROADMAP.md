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

## Stage 5 - Corpus breadth - DONE (partial; see remaining work)

Closed issues #8 (ISO-8859-15) and #9 (real mbox support in tooling), then
re-measured everything against raw, unmodified SpamAssassin.

### Measured (2026-07-17), raw corpus, mbox-split

- **3,303 messages** from 3,302 raw files (one file really was a
  multi-message mbox, which is why splitting rather than stripping was the
  right call): **98.61% parse**, **98.95% of text parts decoded** (up from
  98.20% - ISO-8859-15 support accounts for the gain).
- Differential vs Python `email`, 500-message raw sample: **79.07%
  agreement**. This is NOT comparable to the earlier 92.2%: that sample was
  a different, tidier 500 (pre-stripped easy_ham), while this one is raw
  and includes hard_ham/spam. Sample composition, not a regression.
- Committed corpus: **100%**, gated in CI.

### Residual disagreements (500-message raw sample)

Dominated by canonicalization convention in the *harness*, not proven
libglot bugs: `body_text` (50, mostly charset scope and us-ascii-declared
bodies holding 8-bit bytes where Python's strict decode fails and libglot
passes through), `date` (29), `subject` (22), `to`/`from` (31 combined -
display-name and folding conventions between the two canonicalizations).

Two harness bugs were found and fixed while measuring, each worth several
points of apparent agreement on its own: the RFC 2045 absent-header
charset default (applied by mime_dump, not by the Python side), and year
zero-padding (`strftime("%Y")` renders year 102 as "102"; mime_dump pads
to "0102"). Neither was a parser difference.

Verified along the way: libglot applies RFC 5322 4.3's three-digit-year
rule (`102` -> 2002) and Python's `parsedate_to_datetime` does not - a real
difference, though not the one the corpus exercises (that mail carries a
four-digit `0102`, which both read literally as year 102).

### Enron at scale (2026-07-17) - DONE

517,401 messages (the full Enron corpus, maildir: one message per file, no
mbox envelope):

- **99.99% parse** (517,347; 54 parse errors, 87 policy rejections)
- **100.00% of text parts decoded** - up from 92.64%
- 121 anomalies total, which is correct rather than suspicious: Enron mail
  is machine-generated by JavaMail and carries well-formed MIME-Version /
  Content-Type / Content-Transfer-Encoding headers
- 3m11s wall clock (~2,700 messages/sec), **peak RSS 11.5 MB** - flat
  memory across half a million messages, i.e. the arena is not leaking

**Real bug found by running at scale**: ~10% of Enron mail labels its
charset `ansi_x3.4-1968`, which is IANA's *primary* name for US-ASCII
("US-ASCII" is one of its aliases). libglot did not recognize it, so ~38,000
messages' bodies were reported undecodable. The alias table now carries the
full IANA alias set for US-ASCII (plus Latin-1/Latin-9/Windows-1252/UTF-16
aliases), and lookup is case-insensitive inside `detect_charset` per RFC
2045 5.1 rather than relying on each caller to normalize. Text decode over
Enron went 92.64% -> 100.00%. Regression tests in
`mime/tests/test_charset_latin9.cpp`.

### Remaining

Per-field classification of the differential residual (each class needs
individual diagnosis before it can be called a bug or a convention),
mime4j / Python `email` RFC test suites, and security/parser-differential
corpora.

## Non-goals (unchanged)

XML functions, PL/SQL packages, cost-based optimization, Asian charsets,
`message/partial` reassembly. These fail cleanly rather than silently
emitting something wrong, and are listed OOS in the feature matrix.
