# Roadmap: structural wins, then breadth

Execution order matters here. Each stage is a prerequisite for the one
below it: doing them out of order means doing the work twice.

Status is tracked per stage; `docs/FEATURE_MATRIX.md` remains the
row-by-row source of truth for what is DONE vs OOS.

## Stage 1 - Dialect family inheritance (issue #5) [STRUCTURAL] - DONE

45 dialect rows are hand-maintained; promoting a dialect currently means
duplicating what its family already does. Express families in
`dialect_traits.h` (base profile + delta, constexpr, no runtime cost) so a
family member is a delta plus a conformance suite, correct by construction.

**Blocks stage 2.** Doing stage 2 first would produce 12 copy-pasted
dialects that then have to be rewritten.

## Stage 2 - Promote the family-member dialects (issue #3 follow-on) - DONE

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

## Stage 3 - MIME envelope gaps (issue #6) - DONE

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

### Differential residual, classified field-by-field (2026-07-18) - DONE

Re-ran the exact 500-message raw-sample differential (79.07% agreement)
and diagnosed every disagreeing field individually, rather than leaving
the residual as one unexamined number. Two real libglot bugs turned up
and were fixed; everything else sorted into "out of scope" or "both
parsers are defensible, here's why."

**Bug 1 - a Reject-severity anomaly on any header silently dropped Date
and Message-ID/threading parsing for the whole message.** `finish_message`
checked `rejected_` (set by e.g. `InvalidUtf8Header` on a raw 8-bit
Subject - common in real spam, nothing to do with the Date header)
*before* calling `parse_date_header`/`parse_threading_headers`, even
though the code comment's own stated intent was to stop body/multipart
*descent*, not header-derived extraction that needs no further descent.
9/104 disagreeing messages in the sample had a perfectly valid Date
silently discarded this way. Fixed by moving both calls ahead of the
`rejected_` check; multipart descent below is still correctly gated.

**Bug 2 - `DateTimeParser` silently accepted trailing garbage after a
resolved zone.** `Fri, 23 Aug 2002 22:46:34 GMT+1` matched zone "GMT"
(known, offset 0) and silently discarded the "+1", reporting a confident
UTC offset the header never actually specified - worse than declining to
parse, which is what the same parser already does for other malformed
zones (`19:21:44 01800`, stage-4 residual). Fixed narrowly: a numeric
zone (`+0700`) now requires nothing follow it; a resolved alpha zone
allows further *space-separated words* (real mail spells out "Eastern
Daylight Time", and RFC 5322 §4.3 already treats any unrecognized
all-alpha obs-zone as equivalent to "-0000" - that leniency is
deliberate and stays), but rejects anything glued on with no separating
whitespace. Verified both the fix and the non-regression by hand
(`GMT+1` -> invalid, `Eastern Daylight Time` -> still valid/unknown-tz)
before re-running the full suite.

Both fixes: 0 regressions across the 500-message sample and the full
1309-test suite. Message-level agreement stayed at 79.07% (the 9
Date-fixed messages still disagree on other fields, mostly charset), but
the Date field itself dropped from 29 to 20 residual disagreements -
correctness fixed independent of whether it moved the headline number.

**Charset gaps closed** (the same audit surfaced these independently of
the two bugs above): ISO-8859-9 (Latin-5, Turkish), ISO-8859-2 (Latin-2,
Central European), and KOI8-R (Cyrillic) were all present in real
messages in the 500-sample and reported undecodable. ISO-8859-9 is a
Latin-1 delta like Latin-9 (6 substitutions); ISO-8859-2 and KOI8-R are
not and get their own 128-entry tables (generated from Python's own
codecs, cross-checked byte-for-byte rather than transcribed from memory).
Wiring these in required finding *three* separate charset dispatch
points that have to be kept in sync by hand (`decoded_body_utf8`,
`EncodedWordDecoder::decode`'s RFC 2047 path, and the `to_utf8`
switch) - missing the encoded-word one was caught by a dedicated test
per charset, not by inspection, and is exactly the kind of gap that
would otherwise resurface for the next charset added. Raw SpamAssassin
corpus (3,303 messages): text-decode rate 98.95% -> 99.25%. Regression
tests in `mime/tests/test_charset_regional.cpp`.

**Classified as out of scope or defensible, not bugs:**

- **Subject/body charsets in Asian encodings** (ISO-2022-JP, GB2312/GBK,
  Big5): matches the existing "Asian charsets OOS" non-goal exactly -
  ISO-2022-JP's raw escape sequences (`\x1b$B...\x1b(J`) pass through
  unconverted, same honest-unknown-charset behavior as the others.
- **Raw undeclared 8-bit bytes in headers** (no RFC 2047 encoding, just a
  legacy charset's bytes directly in Subject/From): libglot preserves the
  bytes (escaped as private-use codepoints by `mime_dump`'s JSON output
  so they survive the round trip); Python's strict header decode
  substitutes U+FFFD and loses the original bytes. Same convention
  already documented for body text, now confirmed to extend to headers.
- **A "strict" base64 decode disagreement that turned out to be Python's
  bug, not libglot's**: two large body-length mismatches (29,847 vs
  39,798 bytes, 81,105 vs 108,142) traced to base64 payloads with a
  handful of stray non-alphabet bytes. RFC 2045 §6.8 is unambiguous -
  decoders "must ignore" characters outside the base64 alphabet.
  libglot does; Python's `base64.b64decode` raises, and the stdlib
  `email` package's fallback path returns the *undecoded* payload
  (explaining why Python's "decoded" length was suspiciously close to
  the encoded length). Verified with `base64.b64decode(..., validate=True)`
  reproducing the same "Only base64 data is allowed" error standalone.
- **to/from address fields (31 disagreements)**: all traced to
  `tools/mime_dump.cpp`'s `canonical_address_values`, a deliberately
  simplified ad-hoc splitter (its own comment says "not a
  re-serialization of RFC 5322 syntax") - not the library's real
  `AddressGroupParser`/structured address AST. Covers: empty
  `undisclosed-recipients:;` groups (Python's address list comes back
  empty for these, the harness has no group-aware fallback so the field
  is omitted instead of matching); double literal spaces inside a
  quoted-string display name (`"MR.IKE  EJOH"`) that the harness's own
  `collapse_ws` erases on the Python side, even though libglot correctly
  preserves quoted-string content verbatim and Python's own parser also
  preserved it before the harness normalized it away; an RFC
  2047-encoded-word glued directly onto an addr-spec local-part with no
  angle brackets (`joko@rs.128.ne.jp@FreeBSD.ORG`) that the dump tool's
  angle-bracket-only quoting logic doesn't re-quote (Python does); and a
  garbled spamware `To:` header (`<C:\`Bulk...txt@dogma.slashnull.org>`)
  where Python truncates to `C` and libglot keeps the whole malformed
  addr-spec - neither is "more correct" for input this broken.
- **Message-ID with a trailing RFC 5322 CFWS comment** (`<id> (added by
  ...)`- a relay convention): libglot correctly stops at the closing
  `>` per the msg-id grammar; the harness's Python-side comparison is a
  naive strip that includes the trailing comment. Harness gap, not a
  parser bug.
- **`parts.count` for a message that also tripped `InvalidUtf8Header`**:
  correctly gated behind `rejected_` (unlike Date/threading, multipart
  descent should stop under a Reject-severity anomaly) - working as
  designed, not a consequence of either bug above.
- **A 1-byte body-length difference** on one message: a boundary
  immediately preceded by content with no trailing newline - a known,
  minor CRLF-before-boundary convention ambiguity (RFC 2046), not
  systemic (1/500).

### RFC conformance suite imported from Apache James Mime4j (2026-07-18) - DONE

Vendored the 32 hand-crafted `.msg` conformance fixtures from Apache James
Mime4j's own test suite (`mime/tests/data/mime4j/`, Apache License 2.0 -
its `mimetools-testmsgs/` sibling directory is Artistic-licensed and was
deliberately excluded). Chosen over cpython's `test_email` suite: mime4j's
fixtures are self-contained message files with a matching expected-output
XML; cpython's equivalent coverage is ~493 test methods with fixtures as
inline Python string literals welded to Python-specific assertions, an
order of magnitude more translation work per fixture for the same kind of
edge case.

Read from disk at test time (`mime/tests/test_rfc_conformance_mime4j.cpp`,
32 fixtures / 31 TEST_CASEs / 177 assertions) rather than retyped as string
literals, so CRLFs and long boundary strings are never hand-transcribed.
Assertions check libglot's own verified behavior, not a mechanical
reproduction of mime4j's tree (its internal model and leniency choices
differ from libglot's by design in places - each divergence is explained
in the test file).

**A third real bug found and fixed**: `message/rfc822` parts never
transfer-decoded their body before recursing into it as a nested message.
RFC 2046 §5.2.1 permits only 7bit/8bit/binary there, but real senders
sometimes base64-encode one anyway (mime4j's
`base64encoded-rfc822message*.msg` fixtures exist for exactly this) -
libglot was parsing the still-base64 bytes as headers+body directly,
finding no real header lines (no `:` in base64 text) and silently
producing an empty nested message instead of the real, recoverable
content. Fixed in `finish_message`'s `message/rfc822` branch: the part's
own Content-Transfer-Encoding is decoded (base64/quoted-printable; other
values pass through unchanged) before recursing, with the decoded bytes
copied into the arena (`Message::body` and everything under it are
string_views into the original source buffer, which a decoded temporary
is not). Verified two and three levels of nesting decode correctly,
including a base64-encoded message/rfc822 whose decoded content is itself
multipart. 0 regressions across the resulting 1340-test suite.

**Two further gaps found, not fixed in this pass** (bigger, riskier
changes than the ones above; tracked here rather than silently patched
alongside a test-suite import):
- **No preamble/epilogue modeling.** `Message` has no concept of RFC
  2046's preamble/epilogue at all - content before the first boundary and
  after the last is simply absent from the AST. Harmless when a boundary
  splits normally (RFC says readers should ignore both anyway), but
  visible when nothing splits at all (see below).
- **A multipart whose only boundary occurrence is the close delimiter
  (`--boundary--` with no preceding `--boundary`) is not recognized as
  multipart at all.** RFC 2046 explicitly permits a zero-body-part
  multipart; mime4j reports 0 parts plus preamble/epilogue text for this.
  libglot's splitter appears to require an opening delimiter before a
  close ends a part sequence, so finding only the close, it falls back to
  reporting the whole body undivided (`multipartnopart.msg`,
  `missing-inner-start-boundary.msg`).

**Confirmed as deliberate strictness, not bugs** (same "decline rather
than guess wrong" philosophy as the differential-residual findings
above): a boundary line followed by anything other than linear whitespace
(RFC 2046's `transport-padding` is `*LWSP-char`, not arbitrary text) is
correctly not recognized as a delimiter, even though mime4j tolerates
trailing garbage there (`ending-boundaries.msg`); and RFC 5322 §4's
obsolete header grammar (whitespace before `:`, blank lines inside a
fold) is currently rejected outright rather than tolerated, a real gap
but a much larger one (touches the core header tokenizer) than anything
else found this pass, so left as an open follow-up rather than attempted
here (`obsolete.msg`).

### Security / adversarial-input corpus (2026-07-18) - DONE

Added `mime/tests/test_security_corpus.cpp`: hand-crafted attack-shaped
inputs with a specific expected defensive outcome each (null-byte
smuggling, filename path traversal, an RFC 2047 encoding-based evasion of
the filename check, and a boundary-confusion shape complementing mime4j's
`boundary-name-clash.msg`), run deterministically in CI - distinct from
`fuzz/fuzz_mime_parser.cpp`'s randomized, time-boxed mutation fuzzing.

**Found three genuinely dead anomaly detectors and implemented them.**
`AnomalyKind::NullByteInHeader`, `NullInBase64`, and `InvalidFilenameChars`
all existed in `anomalies.h` - Security-severity classification, display
names, doc comments describing exactly what they should catch - but
`record_anomaly` was never called for any of the three anywhere in the
parser. Writing adversarial test cases for them immediately surfaced this
(the anomaly simply never fired). Implemented all three:
- `NullByteInHeader`: a literal NUL in any header value (`enhance_header`).
- `NullInBase64`: a NUL in a body whose Content-Transfer-Encoding is
  declared `base64` (valid base64 text cannot contain one) - checked on
  the raw encoded body, not after decoding, since the decoder's RFC 2045
  §6.8-mandated leniency (ignore out-of-alphabet characters) would
  otherwise silently drop it before any check could see it.
- `InvalidFilenameChars`: a NUL byte or path separator (`/`, `\`) in a
  Content-Disposition `filename` or Content-Type's legacy `name`
  parameter, checked on the RFC 2047-decoded value (an encoded-word's
  base64 payload legitimately contains `/` as an alphabet character, a
  real false positive hit while building this corpus and fixed by
  decoding first - which also closes an evasion, since an attacker can no
  longer hide `../` from the check by RFC-2047-encoding it).

All three verified to add **zero false positives** over the full
517,401-message Enron corpus and the 3,303-message raw SpamAssassin
corpus - and `InvalidFilenameChars` does fire once for real on the latter,
a genuine MHT-style attachment (`Content-Type: image/jpeg;
name="./MassMail-1509_files/image002.jpg"`) whose Content-Type `name`
parameter carries an embedded relative path, exactly the shape RFC 2183
and this check exist to flag. Two more anomaly-adjacent controls were
found dead by the same method and are **not** fixed here (documented
rather than silently expanded into): `AnomalyKind::DuplicateFilenameParameter`,
and `ParserLimits::max_filename_length` (defined per config tier in
`limits.h`, never checked against an actual filename anywhere).

0 regressions: 1353/1353 tests, committed corpus still 100%, 79.48%/
99.25% SpamAssassin figures unchanged (none of the new checks fire on
real, non-adversarial mail at that scale).

### Remaining

The two mime4j-discovered gaps (preamble/epilogue modeling +
zero-body-part multipart recognition; obsolete RFC 5322 header grammar
tolerance), plus the two now-identified dead controls
(`DuplicateFilenameParameter`, `max_filename_length` enforcement).

## Non-goals (unchanged)

XML functions, PL/SQL packages, cost-based optimization, Asian charsets,
`message/partial` reassembly. These fail cleanly rather than silently
emitting something wrong, and are listed OOS in the feature matrix.
