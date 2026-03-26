cmake_minimum_required(VERSION 3.28)

# ============================================================================
# libglot-sql Benchmarks (Phase C2)
# ============================================================================

# Find or download Google Benchmark
find_package(benchmark QUIET)

if(NOT benchmark_FOUND)
    message(STATUS "Google Benchmark not found, using FetchContent")
    include(FetchContent)
    FetchContent_Declare(
        benchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG v1.8.3
        GIT_SHALLOW TRUE
    )
    # Don't build benchmark tests
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(benchmark)
endif()

# ============================================================================
# Benchmark: Roundtrip (Phase C2 Gate Condition)
# ============================================================================

add_executable(benchmark_roundtrip benchmark_roundtrip.cpp)

target_link_libraries(benchmark_roundtrip
    PRIVATE
        libglot::sql
        libglot::core
        benchmark::benchmark
)

# Enable optimizations for benchmarking
target_compile_options(benchmark_roundtrip PRIVATE
    -O3
    -march=native
    -DNDEBUG
)

# ============================================================================
# Summary
# ============================================================================

message(STATUS "libglot-sql benchmarks configured:")
message(STATUS "  - benchmark_roundtrip: Performance validation vs libsqlglot")
