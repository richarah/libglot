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

# ============================================================================
# MIME Multipart Tests
# ============================================================================

add_executable(test_mime_multipart test_mime_multipart.cpp)

target_link_libraries(test_mime_multipart
    PRIVATE
        libglot::mime
        libglot::core
        Catch2::Catch2WithMain
)

add_test(NAME test_mime_multipart COMMAND test_mime_multipart)

# ============================================================================
# MIME Encoding Tests
# ============================================================================

add_executable(test_mime_encoding test_mime_encoding.cpp)

target_link_libraries(test_mime_encoding
    PRIVATE
        libglot::mime
        libglot::core
        Catch2::Catch2WithMain
)

add_test(NAME test_mime_encoding COMMAND test_mime_encoding)

message(STATUS "libglot-mime tests configured:")
message(STATUS "  - test_mime_parser: Parse MIME headers (26 assertions)")
message(STATUS "  - test_mime_multipart: Multipart parsing (44 assertions)")
message(STATUS "  - test_mime_encoding: Transfer encodings (30+ assertions)")
