# libFuzzer harnesses. Built only with LIBGLOT_BUILD_FUZZERS=ON, which
# requires Clang (libFuzzer ships with it).

if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "LIBGLOT_BUILD_FUZZERS requires Clang (libFuzzer)")
endif()

set(FUZZERS
    fuzz_sql_parser
    fuzz_sql_roundtrip
    fuzz_mime_parser
)

foreach(fuzzer IN LISTS FUZZERS)
    add_executable(${fuzzer} ${fuzzer}.cpp)
    target_link_libraries(${fuzzer} PRIVATE libglot::sql libglot::mime)
    target_compile_options(${fuzzer} PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer)
    target_link_options(${fuzzer} PRIVATE -fsanitize=fuzzer,address,undefined)
endforeach()
