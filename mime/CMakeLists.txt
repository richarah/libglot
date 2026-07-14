# libglot-mime - MIME/email message parser built on libglot-core.
# Header-only.

add_library(libglot-mime INTERFACE)
add_library(libglot::mime ALIAS libglot-mime)

target_include_directories(libglot-mime
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(libglot-mime INTERFACE libglot::core)
target_compile_features(libglot-mime INTERFACE cxx_std_20)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

if(LIBGLOT_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

include(GNUInstallDirs)

install(TARGETS libglot-mime
        EXPORT libglot-mime-targets
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/libglot
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.h"
)

install(EXPORT libglot-mime-targets
        FILE libglot-mime-targets.cmake
        NAMESPACE libglot::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/libglot
)
