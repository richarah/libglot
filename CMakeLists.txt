cmake_minimum_required(VERSION 3.28)
project(libglot VERSION 0.2.0 LANGUAGES CXX)

# libglot is written against C++20. The standard is enforced per-target via
# target_compile_features(cxx_std_20) in each module so it propagates to
# consumers; nothing here should require a newer standard.

# ============================================================================
# Build Options
# ============================================================================
option(LIBGLOT_BUILD_BENCHMARKS "Build benchmarks (fetches Google Benchmark)" OFF)
option(LIBGLOT_BUILD_FUZZERS "Build libFuzzer harnesses (requires Clang)" OFF)
option(LIBGLOT_BUILD_SQL "Build libglot-sql" ON)
option(LIBGLOT_BUILD_MIME "Build libglot-mime" ON)
option(LIBGLOT_WERROR "Treat warnings as errors" OFF)
option(LIBGLOT_ENABLE_ASAN "Enable AddressSanitizer + UBSan" OFF)
option(LIBGLOT_ENABLE_TSAN "Enable ThreadSanitizer" OFF)
option(LIBGLOT_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(LIBGLOT_ENABLE_LTO "Enable Link-Time Optimization" OFF)

# ============================================================================
# Warnings
# ============================================================================
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
    if(LIBGLOT_WERROR)
        add_compile_options(-Werror)
    endif()
elseif(MSVC)
    add_compile_options(/W4)
    if(LIBGLOT_WERROR)
        add_compile_options(/WX)
    endif()
endif()

# ============================================================================
# Sanitizers
# ============================================================================
if(LIBGLOT_ENABLE_ASAN)
    add_compile_options(-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

if(LIBGLOT_ENABLE_TSAN)
    add_compile_options(-fsanitize=thread -fno-omit-frame-pointer)
    add_link_options(-fsanitize=thread)
endif()

if(LIBGLOT_ENABLE_UBSAN AND NOT LIBGLOT_ENABLE_ASAN)
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
    else()
        message(WARNING "LTO not supported: ${ipo_error}")
    endif()
endif()

# ============================================================================
# Build accelerators (optional, local convenience)
# ============================================================================
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    find_program(MOLD_LINKER mold)
    if(MOLD_LINKER)
        add_link_options(-fuse-ld=mold)
    endif()
endif()

# ============================================================================
# Testing
# ============================================================================
# include(CTest) defines BUILD_TESTING (default ON) and calls enable_testing()
# at the top level so `ctest` from the build root discovers every test.
include(CTest)

if(BUILD_TESTING)
    find_package(Catch2 3 QUIET)
    if(NOT Catch2_FOUND)
        include(FetchContent)
        FetchContent_Declare(Catch2
            GIT_REPOSITORY https://github.com/catchorg/Catch2.git
            GIT_TAG v3.5.0
            GIT_SHALLOW TRUE
            SYSTEM
        )
        FetchContent_MakeAvailable(Catch2)
        list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
    endif()
    include(Catch)
endif()

# ============================================================================
# Modules
# ============================================================================
add_subdirectory(core)

if(LIBGLOT_BUILD_SQL)
    add_subdirectory(sql)
endif()

if(LIBGLOT_BUILD_MIME)
    add_subdirectory(mime)
endif()

if(LIBGLOT_BUILD_FUZZERS)
    add_subdirectory(fuzz)
endif()

option(LIBGLOT_BUILD_EXAMPLES "Build example programs" ON)
if(LIBGLOT_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# ============================================================================
# Installation
# ============================================================================
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

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
message(STATUS "libglot ${PROJECT_VERSION}")
message(STATUS "  Compiler:         ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  Build type:       ${CMAKE_BUILD_TYPE}")
message(STATUS "  Tests:            ${BUILD_TESTING}")
message(STATUS "  Benchmarks:       ${LIBGLOT_BUILD_BENCHMARKS}")
message(STATUS "  SQL module:       ${LIBGLOT_BUILD_SQL}")
message(STATUS "  MIME module:      ${LIBGLOT_BUILD_MIME}")
message(STATUS "  Werror:           ${LIBGLOT_WERROR}")
message(STATUS "  ASan+UBSan:       ${LIBGLOT_ENABLE_ASAN}")
message(STATUS "  TSan:             ${LIBGLOT_ENABLE_TSAN}")
message(STATUS "")
