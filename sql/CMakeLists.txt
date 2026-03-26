cmake_minimum_required(VERSION 3.28)

# ============================================================================
# libglot-sql - SQL domain implementation over libglot-core
# ============================================================================
# Static library with SQL-specific implementation:
# - SQLTokenSpec, SQLGrammarSpec, SQLDialectTraits
# - SQLParser, SQLGenerator
# - Minimal AST nodes for Phase C1
# ============================================================================

project(libglot-sql VERSION 0.1.0 LANGUAGES CXX)

# ============================================================================
# SQL Domain Library (Header-Only for Phase C1)
# ============================================================================

add_library(libglot-sql INTERFACE)
add_library(libglot::sql ALIAS libglot-sql)

target_include_directories(libglot-sql
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(libglot-sql
    INTERFACE
        libglot::core
)

# Note: CMake 3.28 doesn't support cxx_std_26, using manual -std=c++2c flag
# target_compile_features(libglot-sql
#     INTERFACE
#         cxx_std_26
# )

# ============================================================================
# Tests (if enabled)
# ============================================================================

if(LIBGLOT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ============================================================================
# Benchmarks (if enabled)
# ============================================================================

if(LIBGLOT_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

# ============================================================================
# Summary
# ============================================================================

message(STATUS "libglot-sql configured (Phase C1: minimal SQL shim)")
message(STATUS "  SQLTokenSpec: adapts libsqlglot token types")
message(STATUS "  SQLParser: minimal SELECT support")
message(STATUS "  SQLGenerator: dialect-aware (ANSI, PostgreSQL, MySQL)")
