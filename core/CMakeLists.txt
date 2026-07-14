# libglot-core - domain-agnostic parser/transpiler infrastructure.
# Header-only: concepts (TokenSpec, AstNode, GrammarSpec, DialectTraits),
# arena allocation, string interning, and the ParserBase/GeneratorBase CRTP
# skeleton shared by the SQL and MIME modules.

add_library(libglot-core INTERFACE)
add_library(libglot::core ALIAS libglot-core)

target_include_directories(libglot-core
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(libglot-core INTERFACE cxx_std_20)

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
