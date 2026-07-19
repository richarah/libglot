# MIME parsing: libglot vs. reference implementations

Head-to-head wall-clock comparison against three other real MIME parsers,
each parsing the *same* real-mail corpus with the *same* protocol per
message: read file bytes, parse, walk every header (top-level and every
part, recursively), and decode every text/html body. This forces each
implementation to do comparable work — a driver that only checked
`Content-Type` and stopped would look artificially fast.

Results are in `../RESULTS_2026-07.md`, under "MIME parsing: libglot vs.
other MIME parsers". This directory holds the driver sources so the
comparison is reproducible; it does not vendor the corpora or the
downloaded JARs.

## Compared implementations

| Implementation | Version | Language | Why this one |
|---|---|---|---|
| libglot | this repo | C++20 | — |
| Python `email` (stdlib) | 3.12 | Python | Already this repo's differential-testing reference implementation (`scripts/mime_diff.py`) |
| Apache James Mime4j | 0.8.14 (`apache-mime4j-dom`) | Java | Already vendored in this repo for the RFC conformance suite (`mime/tests/data/mime4j/`); a mature, production reference (Apache James, and derivatives) |
| `mail-parser` (stalwart-labs) | 0.11.5 | Rust | Explicitly benchmark-oriented in its own README; the closest thing to a "standard" speed reference in this specific space |

## Corpus

Same real-mail corpora already used for libglot's own correctness and
parse-rate figures elsewhere in this repo (`docs/ROADMAP.md`,
`../RESULTS_2026-07.md`'s "MIME corpus" table) — not synthetic messages:

- **SpamAssassin public corpus**, mbox-split into individual messages
  with `scripts/mime_diff.py`'s `split_mbox` (the same splitter the
  differential harness uses) so every driver sees byte-identical
  message boundaries: 3,303 messages.
- **Enron corpus** (maildir, one message per file already): a sorted
  50,000-file subset, not the full 517,401-message corpus, to keep the
  Python run (the slowest by roughly two orders of magnitude) inside a
  couple of minutes. Enough to move well past JIT/interpreter
  warmup noise; see docs/ROADMAP.md and bench/RESULTS_2026-07.md for
  libglot's own separately-measured *full*-corpus numbers.

Neither corpus is included here (see `docs/ROADMAP.md` for where they
came from); point each driver at your own copies.

## Reproducing

```bash
# 1. libglot (Release, -O2, matching this repo's own benchmark convention)
g++ -std=c++20 -O2 -DNDEBUG -Imime/include -Icore/include \
    bench/mime_comparison/bench_libglot.cpp -o bench_libglot
./bench_libglot <filelist>

# 2. Python (stdlib only, no install needed)
python3 bench/mime_comparison/bench_python.py <filelist>

# 3. Rust (mail-parser via crates.io)
cd bench/mime_comparison/rust && cargo build --release
./target/release/mimebench <filelist>

# 4. Java (Apache James Mime4j via Maven Central, no build tool needed)
mkdir -p lib && cd lib
curl -sO https://repo1.maven.org/maven2/org/apache/james/apache-mime4j-dom/0.8.14/apache-mime4j-dom-0.8.14.jar
curl -sO https://repo1.maven.org/maven2/org/apache/james/apache-mime4j-core/0.8.14/apache-mime4j-core-0.8.14.jar
curl -sO https://repo1.maven.org/maven2/commons-io/commons-io/2.22.0/commons-io-2.22.0.jar
cd ..
CP="lib/apache-mime4j-dom-0.8.14.jar:lib/apache-mime4j-core-0.8.14.jar:lib/commons-io-2.22.0.jar"
javac -cp "$CP" -d out BenchMime4j.java
java -cp "$CP:out" BenchMime4j <filelist>
```

`<filelist>` is a newline-separated list of message file paths (one
message per file — pre-split any mbox files first, e.g. with
`scripts/mime_diff.py`'s `split_mbox`, so every driver parses the exact
same message boundaries).

## A real fairness bug found while building this

The first Java run showed 2,478/50,000 "failures" against 0-5 for every
other implementation. Not a mime4j weakness — `DefaultMessageBuilder`'s
*default* `MimeConfig` caps line length at 1000 bytes (a conservative
default), and plenty of real Enron mail has longer unwrapped lines.
`BenchMime4j.java` explicitly sets `MimeConfig.PERMISSIVE` to remove
that artificial ceiling, which is what the numbers here reflect. Worth
keeping in mind before citing any single-library "it failed on N%
of my corpus" number without checking whether that's the parser or just
its default configuration.
