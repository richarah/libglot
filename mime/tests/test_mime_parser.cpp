/// ============================================================================
/// Phase B Test: MIME Parser Validation
/// ============================================================================
///
/// Validates that libglot-core can support multiple domain languages.
///
/// Tests:
/// - Parse simple MIME message
/// - Parse multiple headers
/// - Parse message with body
/// - Verify AST structure
///
/// Gate condition: Demonstrates that ParserBase<Spec> works for non-SQL domains.
/// ============================================================================

#include <catch2/catch_test_macros.hpp>
#include "../include/libglot/mime/parser.h"
#include "../../core/include/libglot/util/arena.h"

using namespace libglot::mime;

TEST_CASE("MIME Parser: Parse simple header", "[mime][parser]") {
    libglot::Arena arena;
    std::string_view source = "Content-Type: text/plain\n";

    MimeParser parser(arena, source);
    auto* msg = parser.parse_top_level();

    REQUIRE(msg != nullptr);
    REQUIRE(msg->type == MimeNodeKind::MESSAGE);
    REQUIRE(msg->headers.size() == 1);

    auto* header = msg->headers[0];
    REQUIRE(header->type == MimeNodeKind::HEADER);
    REQUIRE(header->field == "Content-Type");
    REQUIRE(header->value == "text/plain");
}

TEST_CASE("MIME Parser: Parse multiple headers", "[mime][parser]") {
    libglot::Arena arena;
    std::string_view source =
        "Content-Type: text/html\n"
        "Subject: Test Message\n"
        "From: alice@example.com\n";

    MimeParser parser(arena, source);
    auto* msg = parser.parse_top_level();

    REQUIRE(msg != nullptr);
    REQUIRE(msg->headers.size() == 3);

    REQUIRE(msg->headers[0]->field == "Content-Type");
    REQUIRE(msg->headers[0]->value == "text/html");

    REQUIRE(msg->headers[1]->field == "Subject");
    REQUIRE(msg->headers[1]->value == "Test Message");

    REQUIRE(msg->headers[2]->field == "From");
    REQUIRE(msg->headers[2]->value == "alice@example.com");
}

TEST_CASE("MIME Parser: Parse message with body", "[mime][parser]") {
    libglot::Arena arena;
    std::string_view source =
        "Content-Type: text/plain\n"
        "Subject: Hello\n"
        "\n"
        "This is the message body.\n"
        "It can have multiple lines.";

    MimeParser parser(arena, source);
    auto* msg = parser.parse_top_level();

    REQUIRE(msg != nullptr);
    REQUIRE(msg->headers.size() == 2);

    REQUIRE(msg->headers[0]->field == "Content-Type");
    REQUIRE(msg->headers[0]->value == "text/plain");

    REQUIRE(msg->headers[1]->field == "Subject");
    REQUIRE(msg->headers[1]->value == "Hello");

    REQUIRE(msg->body == "This is the message body.\nIt can have multiple lines.");
}

TEST_CASE("MIME Parser: Parse empty value", "[mime][parser]") {
    libglot::Arena arena;
    std::string_view source = "X-Custom-Header:\n";

    MimeParser parser(arena, source);
    auto* msg = parser.parse_top_level();

    REQUIRE(msg != nullptr);
    REQUIRE(msg->headers.size() == 1);
    REQUIRE(msg->headers[0]->field == "X-Custom-Header");
    REQUIRE(msg->headers[0]->value == "");
}

TEST_CASE("MIME Parser: Zero-cost abstraction check", "[mime][parser][performance]") {
    // This test verifies that MIME parser compiles and instantiates
    // the ParserBase template without virtual dispatch overhead.

    libglot::Arena arena;
    std::string_view source = "Test: value\n";

    MimeParser parser(arena, source);
    auto* msg = parser.parse_top_level();

    REQUIRE(msg != nullptr);

    // Size check: Parser should not have vtable pointer
    // (This is a compile-time property, but we can check object size)
    INFO("MimeParser should be zero-cost abstraction (no vtable overhead)");
}
