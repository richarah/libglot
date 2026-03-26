cmake_minimum_required(VERSION 3.28)
project(libglot VERSION 0.1.0 LANGUAGES CXX)

# ============================================================================
# C++26 Requirement - Zero-cost abstractions via concepts, constexpr, CRTP
# ============================================================================
# Note: CMake 3.28 doesn't recognize CXX_STANDARD 26, use manual flags
# set(CMAKE_CXX_STANDARD 26)
# set(CMAKE_CXX_STANDARD_REQUIRED ON)
# set(CMAKE_CXX_EXTENSIONS OFF)

# Manual C++26 flag (CMake 3.30+ will support cxx_std_26)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-std=c++2c)
elseif(MSVC)
    add_compile_options(/std:c++latest)
endif()

# ============================================================================
# Compiler Requirements
# ============================================================================
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "14.0")
        message(FATAL_ERROR "GCC 14+ required for C++26 support")
    endif()
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "18.0")
        message(FATAL_ERROR "Clang 18+ required for C++26 support")
    endif()
endif()

# ============================================================================
# Build Options
# ============================================================================
option(LIBGLOT_BUILD_TESTS "Build test suite" ON)
option(LIBGLOT_BUILD_BENCHMARKS "Build benchmarks" ON)
option(LIBGLOT_BUILD_BINDINGS "Build Python bindings (pybind11)" OFF)
option(LIBGLOT_BUILD_SQL "Build libglot-sql" ON)
option(LIBGLOT_BUILD_MIME "Build libglot-mime" ON)  # Phase B: multi-domain validation
option(LIBGLOT_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(LIBGLOT_ENABLE_TSAN "Enable ThreadSanitizer" OFF)
option(LIBGLOT_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(LIBGLOT_ENABLE_LTO "Enable Link-Time Optimization" OFF)
option(LIBGLOT_ENABLE_PGO_GENERATE "Generate PGO profile" OFF)
option(LIBGLOT_ENABLE_PGO_USE "Use PGO profile" OFF)

# ============================================================================
# Compiler Warnings - Paranoid mode for safety
# ============================================================================
# Note: For Phase C1, we temporarily relax some warnings to focus on
# validating the architecture. Full warning compliance is deferred to Phase A.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -Wall -Wextra -Wpedantic
        # Temporarily disable -Werror for Phase C1 validation
        # -Werror will be re-enabled in Phase A
    )
endif()

# ============================================================================
# Sanitizers
# ============================================================================
if(LIBGLOT_ENABLE_ASAN)
    add_compile_options(-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
    # Workaround for GCC 15 false positive in std::regex (Google Benchmark dependency)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        add_compile_options(-Wno-error=maybe-uninitialized)
    endif()
endif()

if(LIBGLOT_ENABLE_TSAN)
    add_compile_options(-fsanitize=thread -fno-omit-frame-pointer)
    add_link_options(-fsanitize=thread)
endif()

if(LIBGLOT_ENABLE_UBSAN)
    add_compile_options(-fsanitize=undefined -fno-sanitize-recover=all)
    add_link_options(-fsanitize=undefined)
endif()

# ============================================================================
# Link-Time Optimization
# ============================================================================
if(LIBGLOT_ENABLE_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)
    if(ipo_supported)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
        message(STATUS "LTO enabled")
    else()
        message(WARNING "LTO not supported: ${ipo_error}")
    endif()
endif()

# ============================================================================
# Profile-Guided Optimization
# ============================================================================
if(LIBGLOT_ENABLE_PGO_GENERATE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(-fprofile-generate)
        add_link_options(-fprofile-generate)
        message(STATUS "PGO profile generation enabled")
    endif()
endif()

if(LIBGLOT_ENABLE_PGO_USE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(-fprofile-use -fprofile-correction)
        add_link_options(-fprofile-use)
        message(STATUS "PGO profile usage enabled")
    endif()
endif()

# ============================================================================
# ccache Support
# ============================================================================
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    message(STATUS "ccache found: ${CCACHE_PROGRAM}")
endif()

# ============================================================================
# Linker Selection (prefer mold > lld > ld)
# ============================================================================
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    find_program(MOLD_LINKER mold)
    if(MOLD_LINKER)
        add_link_options(-fuse-ld=mold)
        message(STATUS "Using mold linker")
    else()
        find_program(LLD_LINKER lld)
        if(LLD_LINKER)
            add_link_options(-fuse-ld=lld)
            message(STATUS "Using lld linker")
        else()
            message(STATUS "Using default linker (consider installing mold or lld)")
        endif()
    endif()
endif()

# ============================================================================
# Subdirectories
# ============================================================================
# libglot-core (header-only, no dependencies)
add_subdirectory(core)

# libglot-sql (depends on core)
if(LIBGLOT_BUILD_SQL)
    add_subdirectory(sql)
endif()

# libglot-mime (depends on core)
if(LIBGLOT_BUILD_MIME)
    add_subdirectory(mime)
endif()

# Python bindings (optional)
if(LIBGLOT_BUILD_BINDINGS)
    add_subdirectory(bindings)
endif()

# ============================================================================
# Installation
# ============================================================================
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Install core headers
install(DIRECTORY core/include/libglot
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.h")

# Generate and install package config
configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/libglotConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/libglotConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/libglot
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/libglotConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/libglotConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/libglotConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/libglot
)

# ============================================================================
# Summary
# ============================================================================
message(STATUS "")
message(STATUS "libglot configuration summary:")
message(STATUS "  Version:          ${PROJECT_VERSION}")
message(STATUS "  C++ Standard:     C++${CMAKE_CXX_STANDARD}")
message(STATUS "  Compiler:         ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  Build type:       ${CMAKE_BUILD_TYPE}")
message(STATUS "  Build tests:      ${LIBGLOT_BUILD_TESTS}")
message(STATUS "  Build benchmarks: ${LIBGLOT_BUILD_BENCHMARKS}")
message(STATUS "  Build SQL:        ${LIBGLOT_BUILD_SQL}")
message(STATUS "  Build MIME:       ${LIBGLOT_BUILD_MIME}")
message(STATUS "  Build bindings:   ${LIBGLOT_BUILD_BINDINGS}")
message(STATUS "  LTO:              ${LIBGLOT_ENABLE_LTO}")
message(STATUS "  ASan:             ${LIBGLOT_ENABLE_ASAN}")
message(STATUS "  TSan:             ${LIBGLOT_ENABLE_TSAN}")
message(STATUS "  UBSan:            ${LIBGLOT_ENABLE_UBSAN}")
message(STATUS "")
