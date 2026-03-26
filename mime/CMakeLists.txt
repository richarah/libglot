cmake_minimum_required(VERSION 3.28)

# ============================================================================
# libglot-mime - MIME domain implementation over libglot-core
# ============================================================================
# Demonstrates that libglot-core can support multiple domain languages.
# Implements minimal MIME header parsing.
# ============================================================================

project(libglot-mime VERSION 0.1.0 LANGUAGES CXX)

# ============================================================================
# MIME Domain Library (Header-Only)
# ============================================================================

add_library(libglot-mime INTERFACE)
add_library(libglot::mime ALIAS libglot-mime)

target_include_directories(libglot-mime
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(libglot-mime
    INTERFACE
        libglot::core
)

# ============================================================================
# Tests (if enabled)
# ============================================================================

if(LIBGLOT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ============================================================================
# Summary
# ============================================================================

message(STATUS "libglot-mime configured (Phase B: multi-domain validation)")
message(STATUS "  MimeTokenizer: Simple header tokenization")
message(STATUS "  MimeParser: Header parsing using ParserBase<Spec>")
