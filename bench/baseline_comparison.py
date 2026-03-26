#!/usr/bin/env python3
"""
Phase C2 Benchmark: libsqlglot vs libglot-sql Shim Comparison

Methodology:
- 10,000 iterations per query
- 10 repetitions
- Drop top and bottom outliers
- Report median of remaining 8
- Output to baseline_comparison.csv

Gate condition: All queries supported by shim must be within 2% of libsqlglot
"""

import sys
import time
import statistics
import csv
from pathlib import Path
from typing import List, Tuple, Optional

# Try to import libsqlglot C++
try:
    # Add the build directory to path for the Python module
    sys.path.insert(0, str(Path(__file__).parent.parent / "libsqlglot" / "build" / "src" / "python"))
    import libsqlglot
    HAS_LIBSQLGLOT = True
except ImportError as e:
    HAS_LIBSQLGLOT = False
    print(f"Warning: libsqlglot not available: {e}")
    print("Build libsqlglot first: cd libsqlglot && cmake -B build && cmake --build build")

# Try to import libglot-sql shim (through subprocess for now since it's C++ only)
# We'll benchmark via the compiled benchmark binary
HAS_SHIM = Path(__file__).parent.parent / "build" / "sql" / "benchmarks" / "benchmark_roundtrip"
HAS_SHIM = HAS_SHIM.exists()

# Test queries as specified in the requirements
TEST_QUERIES = [
    # Simple queries (shim supports these)
    ("simple_select_1", "SELECT 1", True),
    ("simple_select_col", "SELECT col FROM t", True),
    ("simple_select_multi", "SELECT a, b, c FROM t WHERE x = 1", True),

    # Medium query (shim supports basic JOINs)
    ("medium_join",
     "SELECT a, b, c FROM t1 JOIN t2 ON t1.id = t2.id WHERE x > 10 ORDER BY a LIMIT 100",
     True),

    # Complex ORM-style query (shim supports this)
    ("complex_orm",
     """SELECT t1.a, t2.b, t3.c
        FROM schema1.table1 AS t1
        INNER JOIN schema2.table2 AS t2 ON t1.id = t2.fk_id
        LEFT JOIN schema3.table3 AS t3 ON t2.id = t3.fk_id
        WHERE t1.status = 'active'
          AND t2.created_at > '2024-01-01'
          AND (t3.value IS NOT NULL OR t3.fallback = 0)
        GROUP BY t1.a, t2.b, t3.c
        HAVING COUNT(*) > 5
        ORDER BY t1.a ASC, t2.b DESC
        LIMIT 50 OFFSET 200""",
     True),

    # Stored procedure (shim does NOT support - will be tested after Phase A)
    ("stored_procedure",
     """CREATE OR REPLACE FUNCTION get_user(p_id INT)
        RETURNS TABLE(name TEXT) AS $$
        BEGIN
            RETURN QUERY SELECT u.name FROM users u WHERE u.id = p_id;
        END;
        $$ LANGUAGE plpgsql""",
     False),  # Not supported by shim yet
]

def benchmark_libsqlglot(sql: str, iterations: int = 10000) -> List[float]:
    """
    Benchmark libsqlglot C++ for a single query.
    Returns list of times (one per repetition).
    """
    if not HAS_LIBSQLGLOT:
        return []

    repetition_times = []

    for rep in range(10):  # 10 repetitions
        times = []
        for _ in range(iterations):
            start = time.perf_counter_ns()
            try:
                result = libsqlglot.transpile(sql)
            except Exception as e:
                # Query not supported
                return []
            elapsed = time.perf_counter_ns() - start
            times.append(elapsed)

        # Take median of this repetition
        repetition_times.append(statistics.median(times))

    return repetition_times

def benchmark_shim_python_driver(sql: str, iterations: int = 10000) -> List[float]:
    """
    Benchmark libglot-sql shim via Python.
    Since we don't have Python bindings yet, we'll estimate based on the
    benchmark binary's output. For now, return empty to mark as "will benchmark via C++".
    """
    # TODO: This will be implemented in Phase A when Python bindings are created
    return []

def format_time_ns(ns: float) -> str:
    """Format nanoseconds in appropriate units"""
    if ns < 1000:
        return f"{ns:.2f} ns"
    elif ns < 1_000_000:
        return f"{ns / 1000:.2f} µs"
    elif ns < 1_000_000_000:
        return f"{ns / 1_000_000:.2f} ms"
    else:
        return f"{ns / 1_000_000_000:.2f} s"

def main():
    print("=" * 80)
    print("Phase C2: libsqlglot vs libglot-sql Shim Baseline Comparison")
    print("=" * 80)
    print()

    if not HAS_LIBSQLGLOT:
        print("ERROR: libsqlglot C++ not available. Cannot proceed.")
        print("Build libsqlglot first:")
        print("  cd libsqlglot && cmake -B build && cmake --build build")
        return 1

    print(f"✓ libsqlglot C++ available")
    if HAS_SHIM:
        print(f"✓ libglot-sql shim available")
    else:
        print(f"⚠ libglot-sql shim not built yet")
    print()

    # Prepare CSV output
    csv_file = Path(__file__).parent / "baseline_comparison.csv"
    results = []

    print("Running benchmarks (10,000 iterations × 10 repetitions per query)...")
    print()

    for query_id, sql, shim_supported in TEST_QUERIES:
        print(f"Benchmarking: {query_id}")
        query_length = len(sql)

        # Benchmark libsqlglot
        print(f"  libsqlglot...", end='', flush=True)
        libsqlglot_times = benchmark_libsqlglot(sql, iterations=10000)

        if not libsqlglot_times:
            print(" SKIP (not supported)")
            results.append({
                'query_id': query_id,
                'query_length': query_length,
                'libsqlglot_median_ns': 'N/A',
                'shim_median_ns': 'N/A',
                'delta_percent': 'N/A',
                'note': 'Not supported by libsqlglot'
            })
            continue

        # Drop extremes, take median
        libsqlglot_times_sorted = sorted(libsqlglot_times)
        libsqlglot_middle8 = libsqlglot_times_sorted[1:-1]
        libsqlglot_median = statistics.median(libsqlglot_middle8)
        print(f" {format_time_ns(libsqlglot_median)}")

        # Benchmark shim
        if not shim_supported:
            print(f"  shim: N/A (will be tested after Phase A migration)")
            results.append({
                'query_id': query_id,
                'query_length': query_length,
                'libsqlglot_median_ns': f"{libsqlglot_median:.2f}",
                'shim_median_ns': 'N/A',
                'delta_percent': 'N/A',
                'note': 'Shim does not support this query yet (Phase A)'
            })
        else:
            # For Phase C1 shim, we need to benchmark via the C++ binary
            # For now, we'll note that and do manual benchmarking
            print(f"  shim: (benchmark via C++ binary - see Phase C2.2)")
            results.append({
                'query_id': query_id,
                'query_length': query_length,
                'libsqlglot_median_ns': f"{libsqlglot_median:.2f}",
                'shim_median_ns': 'PENDING',
                'delta_percent': 'PENDING',
                'note': 'Shim benchmark pending - run benchmark_roundtrip binary'
            })

        print()

    # Write CSV
    with open(csv_file, 'w', newline='') as f:
        fieldnames = ['query_id', 'query_length', 'libsqlglot_median_ns',
                      'shim_median_ns', 'delta_percent', 'note']
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(results)

    print(f"✓ Results written to: {csv_file}")
    print()
    print("=" * 80)
    print("Phase C2.1 Complete: libsqlglot baseline captured")
    print("=" * 80)
    print()
    print("Next step: Run C++ benchmark binary to get shim numbers:")
    print("  ./build/sql/benchmarks/benchmark_roundtrip --benchmark_format=csv")
    print()
    print("Then manually compare and validate 2% threshold.")

    return 0

if __name__ == "__main__":
    sys.exit(main())
