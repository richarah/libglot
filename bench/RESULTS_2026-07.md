# Benchmark results — 2026-07-15

Measured after the full quality overhaul (waves 1–3 included), Release
build (`-O2`, GCC 15.2, `-march=native`), WSL2 on a 12-core VM, Google
Benchmark `iterations:10000/repeats:10`. Numbers are means (median in
parentheses where it differs materially). Run with:

```
cmake --preset bench && cmake --build --preset bench -j3
./build/bench/sql/benchmarks/benchmark_roundtrip
./build/bench/mime/benchmarks/bench_mime_parsing
```

## SQL (`benchmark_roundtrip`)

| Benchmark | Time |
|---|---|
| `SELECT 1` parse | 1.42 µs |
| `SELECT 1` parse+generate roundtrip | 1.66 µs |
| `SELECT col FROM t` parse | 1.44 µs |
| `SELECT col FROM t` roundtrip | 1.82 µs (median 1.60 µs) |
| Multi-column SELECT parse | 2.49 µs |
| Multi-column SELECT roundtrip | 2.93 µs |
| Representative query parse | 2.64 µs |
| Representative query roundtrip | 3.31 µs |
| Medium query parse | 2.73 µs |
| Medium query roundtrip | 3.30 µs |
| Transpile → PostgreSQL | 0.60 µs (median 0.56 µs) |
| Arena allocation batch | 1.30 µs |

Context: the March 2026 pre-overhaul baseline (`PHASE_C2_RESULTS.md`,
historical) reported ~1.3 µs for the simplest parse on different hardware
and a parser with far fewer features (no schema in CREATE TABLE, no
parenthesization, silent drops). Parse cost has stayed in the same
microsecond band while correctness features were added.

## MIME (`bench_mime_parsing`)

| Benchmark | Time |
|---|---|
| Simple message parse (full pipeline) | 2.02 µs |
| Multipart message parse | 2.59 µs |
| Nested multipart parse | 3.29 µs |
| base64 decode (small) | 36 ns |
| base64 decode (large) | 207 ns |
| Quoted-printable decode | 51 ns |
| RFC 2047 decode | 107 ns |
| ISO-8859-1 → UTF-8 | 2.35 µs |
| Windows-1252 → UTF-8 | 5.27 µs |
| UTF-8 validation | 45 ns |

Notes: measurements taken on a shared/virtualized machine (load average ~6
during runs); treat ±25 % as noise. CV on the noisiest SQL series was
~24 %. For regression tracking, compare medians from the same machine.

## libglot vs Python sqlglot (measured 2026-07-16)

Head-to-head on the same machine, same queries, same session. libglot:
Release `-O2`, GCC 15, 20,000 iterations/query. sqlglot 30.12.0 on CPython,
2,000 iterations/query. "parse" = source to AST; "transpile" = parse +
generate (PostgreSQL in, T-SQL out). Times are ns/op.

| Query | libglot parse | sqlglot parse | speedup | libglot transpile | sqlglot transpile | speedup |
|---|---|---|---|---|---|---|
| `SELECT 1` | 1.3 µs | 42.8 µs | 33x | 1.5 µs | 80.9 µs | 47x |
| `SELECT col FROM t` | 1.5 µs | 58.9 µs | 36x | 1.7 µs | 102.1 µs | 64x |
| SELECT + WHERE + ORDER BY + LIMIT | 3.0 µs | 142.0 µs | 46x | 3.5 µs | 266.5 µs | 73x |
| JOIN + GROUP BY + HAVING + ORDER BY | 5.2 µs | 310.8 µs | 58x | 6.2 µs | 559.5 µs | 81x |
| CTE + window function | 4.4 µs | 275.0 µs | 56x | 6.0 µs | 577.2 µs | 93x |

**Summary: 33-58x faster on parse, 47-93x faster on transpile.** The margin
widens with query complexity, and is larger for transpile than for parse
(generation is where the interpreted implementation pays most).

Caveats, so these numbers are not oversold:
- Different feature sets. sqlglot supports far more dialects and does work
  libglot does not (e.g. a full optimizer, schema binding). This measures the
  common path: parse, and parse+generate.
- Measured on one shared/virtualized machine (WSL2, 12 cores); treat +/-25%
  as noise. Re-run with `bench/` to reproduce.
- The historical "126-252x faster than Python" figure in this repo predates
  the overhaul, was never reproducible here, and is superseded by the table
  above.

## MIME corpus (measured 2026-07-17, re-measured 2026-07-18 after the stage-5 follow-up work)

| Corpus | Messages | Parse | Text decoded | Notes |
|---|---|---|---|---|
| Enron (full, maildir) | 517,401 | **100.00%** | **100.00%** | up from 99.99% parse after RFC 5322 obsolete-header-grammar tolerance (docs/ROADMAP.md); peak RSS **11.5 MB** |
| SpamAssassin (raw, mbox-split) | 3,303 | **99.58%** | **99.26%** | `--mbox`, no preprocessing; parse up from 98.61% (obsolete-header tolerance), decode up from 98.95% (ISO-8859-9/-2, KOI8-R) - both docs/ROADMAP.md stage 5 differential follow-up |
| Committed corpus | 6 | 100% | 100% | CI-gated, plus 100% differential agreement |

Flat 11.5 MB peak RSS across half a million messages is the arena
allocator behaving: memory is bounded by the largest single message, not
by corpus size.

Reproduce: `tools/mime_corpus [--mbox] --min-success 0.0 <dir>`.

## MIME parsing: libglot vs. other MIME parsers (measured 2026-07-19)

Head-to-head against three real reference implementations, on the exact
same real-mail corpora (SpamAssassin, mbox-split, 3,303 messages; a
sorted 50,000-message subset of Enron), same machine, same session.
Every driver does the same amount of work per message: read the file,
parse, walk every header (recursively through every part), decode every
text/html body. Full methodology, driver sources, and a fairness bug
found and fixed while building this (Java's default line-length limit
was silently rejecting long-but-valid lines) are in
`bench/mime_comparison/README.md`. Median of 3 runs (2 for Python's
Enron figure, since each run already takes ~60s); msg/s = messages/sec
over the whole read+parse+walk loop.

| Implementation | Version | SpamAssassin (3,303 msgs) | Enron subset (50,000 msgs) |
|---|---|---|---|
| **libglot** | this repo | **49,197 msg/s** | **88,877 msg/s** |
| `mail-parser` (Rust) | 0.11.5 | 18,565 msg/s | 46,835 msg/s |
| Apache James Mime4j (Java) | 0.8.14 | 7,020 msg/s | 41,336 msg/s |
| Python `email` (stdlib) | 3.12 | 699 msg/s | ~807 msg/s |

| Implementation | Speedup vs. libglot (SpamAssassin) | Speedup vs. libglot (Enron subset) |
|---|---|---|
| `mail-parser` (Rust) | 2.6x slower | 1.9x slower |
| Apache James Mime4j (Java) | 7.0x slower | 2.1x slower |
| Python `email` (stdlib) | 70x slower | 110x slower |

**libglot is faster than all three, including a Rust parser built and
benchmarked specifically for speed.** The margin over `mail-parser`
narrows on the larger Enron sample (2.6x -> 1.9x) - plausible causes
include Rust's allocator/`Cow`-based ownership model amortizing better
at this message-size distribution, and the two corpora differing in
average message complexity (Enron is largely plain single-part
corporate mail; SpamAssassin skews toward elaborately-nested spam
HTML/multipart structures, which is exactly where arena-allocated
tree-building has more room to win over per-node heap allocation) -
not confirmed further than that.

Caveats, so these numbers are not oversold:
- **libglot does strictly more work per message than any of the other
  three**: every parse also runs this repo's full anomaly-detection
  pipeline (DoS/security/structural checks - dozens of `AnomalyKind`
  cases) and enforces `ParserLimits::standard()`'s nesting/part-count
  caps, none of which the other libraries have an equivalent for. If
  anything this understates libglot's relative efficiency, not
  overstates it.
- Different feature/charset scope: Java and Rust both decode through
  their host language's own comprehensive charset libraries; libglot
  decodes through its own curated table (UTF-8/16, ISO-8859-1/-2/-9/-15,
  Windows-1252, KOI8-R, US-ASCII - see `docs/FEATURE_MATRIX.md`).
  Not every message exercises a charset all four sides agree how to
  handle, so per-message work is comparable but not bit-for-bit
  identical.
- Measured on one shared/virtualized machine (12-core, WSL2); treat
  ±25% as noise, same caveat as the SQL-vs-sqlglot table above.
- The Enron figure is a 50,000-message subset, not the full
  517,401-message corpus libglot's own numbers elsewhere in this file
  use - see `bench/mime_comparison/README.md` for why (Python's runtime
  at full scale).
- Every implementation's *default* configuration/limits differ (this is
  real, not an oversight to paper over): Java's default line-length cap
  had to be raised to a permissive setting to stop it from rejecting
  valid long lines that libglot's and the others' defaults tolerate
  fine - see the README for the full story. A parser's defaults are
  part of what you're choosing when you pick a library, not just its
  raw speed.
