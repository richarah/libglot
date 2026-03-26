#include <benchmark/benchmark.h>
#include <libglot/sql/parser.h>
#include <libglot/sql/optimizer.h>
#include <libglot/util/arena.h>

using namespace libglot::sql;

// ============================================================================
// Constant Folding Benchmarks
// ============================================================================

static void BM_ConstantFolding_Simple(benchmark::State& state) {
    for (auto _ : state) {
        libglot::Arena arena;
        SQLParser parser(arena, "SELECT 1 + 2 + 3 + 4 + 5 FROM users");
        auto* stmt = parser.parse_select();

        SQLOptimizer opt(arena);
        benchmark::DoNotOptimize(opt.fold_constants(stmt));
    }
}
BENCHMARK(BM_ConstantFolding_Simple);

static void BM_ConstantFolding_Complex(benchmark::State& state) {
    for (auto _ : state) {
        libglot::Arena arena;
        SQLParser parser(arena, "SELECT (10 * 5) + (20 / 4) - (3 * 2) FROM users WHERE (100 + 50) > price");
        auto* stmt = parser.parse_select();

        SQLOptimizer opt(arena);
        benchmark::DoNotOptimize(opt.fold_constants(stmt));
    }
}
BENCHMARK(BM_ConstantFolding_Complex);

// ============================================================================
// Expression Simplification Benchmarks
// ============================================================================

static void BM_ExpressionSimplification_Boolean(benchmark::State& state) {
    for (auto _ : state) {
        libglot::Arena arena;
        SQLParser parser(arena, "SELECT * FROM users WHERE active = TRUE AND deleted = FALSE");
        auto* stmt = parser.parse_select();

        SQLOptimizer opt(arena);
        benchmark::DoNotOptimize(opt.simplify_expressions(stmt));
    }
}
BENCHMARK(BM_ExpressionSimplification_Boolean);

// ============================================================================
// Predicate Pushdown Benchmarks
// ============================================================================

static void BM_PredicatePushdown(benchmark::State& state) {
    for (auto _ : state) {
        libglot::Arena arena;
        SQLParser parser(arena, "SELECT * FROM (SELECT id, name FROM users) WHERE id > 100");
        auto* stmt = parser.parse_select();

        SQLOptimizer opt(arena);
        benchmark::DoNotOptimize(opt.pushdown_predicates(static_cast<SelectStmt*>(stmt)));
    }
}
BENCHMARK(BM_PredicatePushdown);

// ============================================================================
// Full Optimization Pipeline Benchmarks
// ============================================================================

static void BM_FullOptimization_Simple(benchmark::State& state) {
    for (auto _ : state) {
        libglot::Arena arena;
        SQLParser parser(arena, "SELECT id, name FROM users WHERE active = TRUE");
        auto* stmt = parser.parse_select();

        SQLOptimizer opt(arena);
        benchmark::DoNotOptimize(opt.optimize(stmt));
    }
}
BENCHMARK(BM_FullOptimization_Simple);

static void BM_FullOptimization_Complex(benchmark::State& state) {
    for (auto _ : state) {
        libglot::Arena arena;
        SQLParser parser(arena,
            "SELECT u.id, u.name, COUNT(*) "
            "FROM (SELECT * FROM users WHERE created_at > '2024-01-01') u "
            "JOIN orders o ON u.id = o.user_id "
            "WHERE u.active = TRUE AND (1 + 1) = 2 "
            "GROUP BY u.id, u.name "
            "HAVING COUNT(*) > 10");
        auto* stmt = parser.parse_select();

        SQLOptimizer opt(arena);
        benchmark::DoNotOptimize(opt.optimize(stmt));
    }
}
BENCHMARK(BM_FullOptimization_Complex);

// ============================================================================
// Projection Pushdown Benchmarks
// ============================================================================

static void BM_ProjectionPushdown(benchmark::State& state) {
    for (auto _ : state) {
        libglot::Arena arena;
        SQLParser parser(arena, "SELECT id, name FROM (SELECT * FROM users)");
        auto* stmt = parser.parse_select();

        SQLOptimizer opt(arena);
        benchmark::DoNotOptimize(opt.pushdown_projections(static_cast<SelectStmt*>(stmt)));
    }
}
BENCHMARK(BM_ProjectionPushdown);

// ============================================================================
// Main
// ============================================================================

BENCHMARK_MAIN();
