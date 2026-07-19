# libglot-sql - SQL parser, generator, and dialect transpilation built on
# libglot-core. Header-only; the tokenizer lives in include/libglot/sql/lex/.

add_library(libglot-sql INTERFACE)
add_library(libglot::sql ALIAS libglot-sql)
# Export as libglot::sql (matching the in-tree alias), not libglot::libglot-sql
set_target_properties(libglot-sql PROPERTIES EXPORT_NAME sql)

target_include_directories(libglot-sql
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(libglot-sql INTERFACE libglot::core)
target_compile_features(libglot-sql INTERFACE cxx_std_20)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

if(LIBGLOT_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

include(GNUInstallDirs)

install(TARGETS libglot-sql
        EXPORT libglot-sql-targets
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/libglot
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.h"
)

install(EXPORT libglot-sql-targets
        FILE libglot-sql-targets.cmake
        NAMESPACE libglot::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/libglot
)
