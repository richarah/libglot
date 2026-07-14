# libglot-sql benchmarks (Google Benchmark). Built only when
# LIBGLOT_BUILD_BENCHMARKS=ON.

find_package(benchmark QUIET)
if(NOT benchmark_FOUND)
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

add_executable(benchmark_roundtrip benchmark_roundtrip.cpp)
target_link_libraries(benchmark_roundtrip PRIVATE libglot::sql benchmark::benchmark)
target_compile_options(benchmark_roundtrip PRIVATE $<$<CONFIG:Release>:-march=native>)
