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
