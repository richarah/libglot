cmake_minimum_required(VERSION 3.28)

# ============================================================================
# libglot-core - Domain-agnostic parser/transpiler infrastructure
# ============================================================================
# Header-only library providing:
# - C++26 concepts for TokenSpec, AstNode, GrammarSpec, DialectTraits
# - Arena allocation for AST nodes
# - String interning (thread-safe + local)
# - Error recovery framework
# - Perfect hash infrastructure
# - Template-based tokenizer, parser, generator
#
# Zero dependencies beyond C++26 standard library.
# Zero runtime overhead - all abstractions resolved at compile-time.
# ============================================================================

project(libglot-core VERSION 0.1.0 LANGUAGES CXX)

# ============================================================================
# Header-Only Library Interface
# ============================================================================

add_library(libglot-core INTERFACE)
add_library(libglot::core ALIAS libglot-core)

target_include_directories(libglot-core
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Note: CMake 3.28 doesn't support cxx_std_26, using manual -std=c++2c flag
# target_compile_features(libglot-core
#     INTERFACE
#         cxx_std_26
# )

# ============================================================================
# Compiler-Specific Optimizations
# ============================================================================

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(libglot-core
        INTERFACE
            # Aggressive inlining for zero-cost abstractions
            $<$<CONFIG:Release>:-finline-functions>
            $<$<CONFIG:Release>:-finline-limit=1000>

            # Template instantiation depth (for complex grammars)
            -ftemplate-depth=1024

            # Concepts diagnostics
            -fconcepts-diagnostics-depth=3
    )
endif()

# ============================================================================
# Tests (if enabled)
# ============================================================================

# Core library is header-only, tests are in domain implementations (sql, mime)

# ============================================================================
# Installation
# ============================================================================

include(GNUInstallDirs)

install(TARGETS libglot-core
        EXPORT libglot-core-targets
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/libglot
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.h"
)

install(EXPORT libglot-core-targets
        FILE libglot-core-targets.cmake
        NAMESPACE libglot::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/libglot
)

# ============================================================================
# Summary
# ============================================================================

message(STATUS "libglot-core configured (header-only library)")
message(STATUS "  Concepts: TokenSpec, AstNode, GrammarSpec, DialectTraits")
message(STATUS "  Infrastructure: Arena, StringPool, ErrorRecovery")
message(STATUS "  Templates: Tokenizer<Spec>, Parser<Spec>, Generator<Spec>")
