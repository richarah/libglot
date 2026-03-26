cmake_minimum_required(VERSION 3.28)

# ============================================================================
# libglot-mime Tests (Phase B)
# ============================================================================

add_executable(test_mime_parser test_mime_parser.cpp)

target_link_libraries(test_mime_parser
    PRIVATE
        libglot::mime
        libglot::core
        Catch2::Catch2WithMain
)

# Add test to CTest
add_test(NAME test_mime_parser COMMAND test_mime_parser)

message(STATUS "libglot-mime tests configured:")
message(STATUS "  - test_mime_parser: Parse MIME headers")
