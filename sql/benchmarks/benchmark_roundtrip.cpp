/// ============================================================================
/// Phase C2 Benchmark: SQL Roundtrip Performance Baseline
/// ============================================================================
///
/// Establishes performance baseline for libglot-core SQL implementation.
///
/// Benchmark methodology (per spec):
/// - 10,000 iterations per query
/// - 10 repetitions per query
/// - Drop top and bottom outliers, take median of remaining 8
/// - Report wall-clock time in nanoseconds
///
/// Note: libsqlglot comparison deferred to Phase A due to C++26 reflection
///       requirement (not available in current GCC 15). This benchmark
///       establishes shim baseline performance.
///
/// Gate condition: Establish baseline numbers, validate zero-cost abstraction.
/// ============================================================================

#include <benchmark/benchmark.h>
#include "../include/libglot/sql/parser.h"
#include "../include/libglot/sql/generator.h"

using namespace libglot::sql;

// ============================================================================
// Test Queries - Phase C2 Required Queries
// ============================================================================

/// Simple query 1: Literal select
static constexpr std::string_view kSimpleSelect1 = "SELECT 1";

/// Simple query 2: Column select
static constexpr std::string_view kSimpleSelectCol = "SELECT col FROM t";

/// Simple query 3: Multi-column with WHERE
static constexpr std::string_view kSimpleSelectMulti =
    "SELECT a, b, c FROM t WHERE x = 1";

/// Representative query from Phase C1 (baseline)
static constexpr std::string_view kRepresentativeQuery =
    "SELECT col AS alias FROM table WHERE col = 1 ORDER BY col LIMIT 10";

/// Medium complexity (supported by shim)
static constexpr std::string_view kMediumQuery =
    "SELECT a, b, c FROM t1 WHERE x > 10 ORDER BY a LIMIT 100";

// Note: Complex ORM-style queries with JOINs, GROUP BY, HAVING not yet supported
// in Phase C1 shim. Will be benchmarked in Phase A.

// Note: Stored procedures not supported. Will be added in Phase A.

// ============================================================================
// libglot-core SQL Implementation Benchmarks
// ============================================================================
//
// Methodology per spec:
// - Iterations(10000) ensures 10,000 iterations per run
// - Repetitions(10) runs the benchmark 10 times
// - Google Benchmark reports aggregate statistics including median
// ============================================================================

// Simple Query 1: SELECT 1
static void BM_SimpleSelect1_Parse(benchmark::State& state) {
    for (auto _ : state) {
        libglot::Arena arena;
        SQLParser parser(arena, kSimpleSelect1);
        auto* ast = parser.parse_top_level();
        benchmark::DoNotOptimize(ast);
        benchmark::ClobberMemory();
    }
    state.SetLabel("simple_select_1");
}
BENCHMARK(BM_SimpleSelect1_Parse)->Iterations(10000)->Repetitions(10);

static void BM_SimpleSelect1_Roundtrip(benchmark::State& state) {
    for (auto _ : state) {
        libglot::Arena arena;
        SQLParser parser(arena, kSimpleSelect1);
        auto* ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::ANSI);
        std::string sql = gen.generate(ast);
        benchmark::DoNotOptimize(sql);
        benchmark::ClobberMemory();
    }
    state.SetLabel("simple_select_1");
}
BENCHMARK(BM_SimpleSelect1_Roundtrip)->Iterations(10000)->Repetitions(10);

// Simple Query 2: SELECT col FROM t
static void BM_SimpleSelectCol_Parse(benchmark::State& state) {
    for (auto _ : state) {
        libglot::Arena arena;
        SQLParser parser(arena, kSimpleSelectCol);
        auto* ast = parser.parse_top_level();
        benchmark::DoNotOptimize(ast);
        benchmark::ClobberMemory();
    }
    state.SetLabel("simple_select_col");
}
BENCHMARK(BM_SimpleSelectCol_Parse)->Iterations(10000)->Repetitions(10);

static void BM_SimpleSelectCol_Roundtrip(benchmark::State& state) {
    for (auto _ : state) {
        libglot::Arena arena;
        SQLParser parser(arena, kSimpleSelectCol);
        auto* ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::ANSI);
        std::string sql = gen.generate(ast);
        benchmark::DoNotOptimize(sql);
        benchmark::ClobberMemory();
    }
    state.SetLabel("simple_select_col");
}
BENCHMARK(BM_SimpleSelectCol_Roundtrip)->Iterations(10000)->Repetitions(10);

// Simple Query 3: SELECT a, b, c FROM t WHERE x = 1
static void BM_SimpleSelectMulti_Parse(benchmark::State& state) {
    for (auto _ : state) {
        libglot::Arena arena;
        SQLParser parser(arena, kSimpleSelectMulti);
        auto* ast = parser.parse_top_level();
        benchmark::DoNotOptimize(ast);
        benchmark::ClobberMemory();
    }
    state.SetLabel("simple_select_multi");
}
BENCHMARK(BM_SimpleSelectMulti_Parse)->Iterations(10000)->Repetitions(10);

static void BM_SimpleSelectMulti_Roundtrip(benchmark::State& state) {
    for (auto _ : state) {
        libglot::Arena arena;
        SQLParser parser(arena, kSimpleSelectMulti);
        auto* ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::ANSI);
        std::string sql = gen.generate(ast);
        benchmark::DoNotOptimize(sql);
        benchmark::ClobberMemory();
    }
    state.SetLabel("simple_select_multi");
}
BENCHMARK(BM_SimpleSelectMulti_Roundtrip)->Iterations(10000)->Repetitions(10);

// Representative Query: SELECT col AS alias FROM table WHERE col = 1 ORDER BY col LIMIT 10
static void BM_Representative_Parse(benchmark::State& state) {
    for (auto _ : state) {
        libglot::Arena arena;
        SQLParser parser(arena, kRepresentativeQuery);
        auto* ast = parser.parse_top_level();
        benchmark::DoNotOptimize(ast);
        benchmark::ClobberMemory();
    }
    state.SetLabel("representative");
}
BENCHMARK(BM_Representative_Parse)->Iterations(10000)->Repetitions(10);

static void BM_Representative_Roundtrip(benchmark::State& state) {
    for (auto _ : state) {
        libglot::Arena arena;
        SQLParser parser(arena, kRepresentativeQuery);
        auto* ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::ANSI);
        std::string sql = gen.generate(ast);
        benchmark::DoNotOptimize(sql);
        benchmark::ClobberMemory();
    }
    state.SetLabel("representative");
}
BENCHMARK(BM_Representative_Roundtrip)->Iterations(10000)->Repetitions(10);

// Medium Query: SELECT a, b, c FROM t1 WHERE x > 10 ORDER BY a LIMIT 100
static void BM_Medium_Parse(benchmark::State& state) {
    for (auto _ : state) {
        libglot::Arena arena;
        SQLParser parser(arena, kMediumQuery);
        auto* ast = parser.parse_top_level();
        benchmark::DoNotOptimize(ast);
        benchmark::ClobberMemory();
    }
    state.SetLabel("medium");
}
BENCHMARK(BM_Medium_Parse)->Iterations(10000)->Repetitions(10);

static void BM_Medium_Roundtrip(benchmark::State& state) {
    for (auto _ : state) {
        libglot::Arena arena;
        SQLParser parser(arena, kMediumQuery);
        auto* ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::ANSI);
        std::string sql = gen.generate(ast);
        benchmark::DoNotOptimize(sql);
        benchmark::ClobberMemory();
    }
    state.SetLabel("medium");
}
BENCHMARK(BM_Medium_Roundtrip)->Iterations(10000)->Repetitions(10);

// ============================================================================
// Dialect Transpilation Benchmarks
// ============================================================================

/// Transpile to MySQL
static void BM_Transpile_MySQL(benchmark::State& state) {
    libglot::Arena arena;
    SQLParser parser(arena, kRepresentativeQuery);
    auto* ast = parser.parse_top_level();

    for (auto _ : state) {
        SQLGenerator gen(SQLDialect::MySQL);
        std::string sql = gen.generate(ast);
        benchmark::DoNotOptimize(sql);
        benchmark::ClobberMemory();
    }
    state.SetLabel("mysql_transpile");
}
BENCHMARK(BM_Transpile_MySQL)->Iterations(10000)->Repetitions(10);

/// Transpile to PostgreSQL
static void BM_Transpile_PostgreSQL(benchmark::State& state) {
    libglot::Arena arena;
    SQLParser parser(arena, kRepresentativeQuery);
    auto* ast = parser.parse_top_level();

    for (auto _ : state) {
        SQLGenerator gen(SQLDialect::PostgreSQL);
        std::string sql = gen.generate(ast);
        benchmark::DoNotOptimize(sql);
        benchmark::ClobberMemory();
    }
    state.SetLabel("postgresql_transpile");
}
BENCHMARK(BM_Transpile_PostgreSQL)->Iterations(10000)->Repetitions(10);

// ============================================================================
// Infrastructure Benchmarks
// ============================================================================

/// Arena allocation overhead baseline
static void BM_ArenaAllocation(benchmark::State& state) {
    for (auto _ : state) {
        libglot::Arena arena;
        // Allocate typical AST node count for representative query
        for (int i = 0; i < 20; ++i) {
            auto* node = arena.create<SelectStmt>();
            benchmark::DoNotOptimize(node);
        }
        benchmark::ClobberMemory();
    }
    state.SetLabel("arena_alloc");
}
BENCHMARK(BM_ArenaAllocation)->Iterations(10000)->Repetitions(10);

// ============================================================================
// Main
// ============================================================================

BENCHMARK_MAIN();
