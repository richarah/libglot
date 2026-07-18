# libglot-mime test suite. Each source file is one Catch2 executable;
# individual TEST_CASEs are registered with CTest via catch_discover_tests.

set(MIME_TESTS
    test_address_groups
    test_boundary_recovery
    test_charset_latin9
    test_charset_regional
    test_charset_utf16
    test_date_parsing
    test_external_body
    test_header_comments
    test_message_partial
    test_message_rfc822
    test_mime_anomalies
    test_mime_encoding
    test_mime_multipart
    test_mime_parser
    test_multipart_report
    test_multipart_signed
    test_pipeline
    test_related_content_headers
    test_rfc2231_continuations
    test_rfc_conformance_mime4j
    test_security_corpus
    test_threading_headers
    test_utf8_headers
)

foreach(test_name IN LISTS MIME_TESTS)
    add_executable(${test_name} ${test_name}.cpp)
    target_link_libraries(${test_name} PRIVATE libglot::mime Catch2::Catch2WithMain)
    catch_discover_tests(${test_name} TEST_PREFIX "mime.")
endforeach()

# Vendored mime4j fixtures (mime/tests/data/mime4j/) are read from disk at
# test time rather than embedded as string literals, so the exact bytes
# (CRLFs, long boundary strings) are never retyped by hand.
target_compile_definitions(test_rfc_conformance_mime4j PRIVATE
    MIME4J_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data/mime4j")
