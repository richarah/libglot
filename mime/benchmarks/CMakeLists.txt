# libglot-mime benchmarks (Google Benchmark). Built only when
# LIBGLOT_BUILD_BENCHMARKS=ON. Google Benchmark is provided by the SQL
# benchmarks directory when both are enabled; fetch it here if absent.

find_package(benchmark QUIET)
if(NOT benchmark_FOUND AND NOT TARGET benchmark::benchmark)
    include(FetchContent)
    FetchContent_Declare(benchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG v1.8.3
        GIT_SHALLOW TRUE
        SYSTEM
    )
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(benchmark)
endif()

add_executable(bench_mime_parsing bench_mime_parsing.cpp)
target_link_libraries(bench_mime_parsing PRIVATE libglot::mime benchmark::benchmark)
