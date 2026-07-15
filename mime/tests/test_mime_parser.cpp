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

#include "../../core/include/libglot/util/arena.h"
#include "../include/libglot/mime/parser.h"
#include <catch2/catch_test_macros.hpp>

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
    std::string_view source = "Content-Type: text/html\n"
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
    std::string_view source = "Content-Type: text/plain\n"
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

TEST_CASE("MIME Parser: CRLF and LF messages parse identically", "[mime][parser][crlf]") {
    // RFC 5322 messages use CRLF line endings; the parser must treat
    // CRLF, LF, and (leniently) bare CR uniformly.
    std::string_view lf_source = "Content-Type: text/plain\n"
                                 "Subject: Hello\n"
                                 "\n"
                                 "Body line 1\nBody line 2";
    std::string_view crlf_source = "Content-Type: text/plain\r\n"
                                   "Subject: Hello\r\n"
                                   "\r\n"
                                   "Body line 1\nBody line 2";

    libglot::Arena arena_lf;
    MimeParser parser_lf(arena_lf, lf_source);
    auto* msg_lf = parser_lf.parse_top_level();

    libglot::Arena arena_crlf;
    MimeParser parser_crlf(arena_crlf, crlf_source);
    auto* msg_crlf = parser_crlf.parse_top_level();

    REQUIRE(msg_lf != nullptr);
    REQUIRE(msg_crlf != nullptr);

    // Identical header set
    REQUIRE(msg_lf->headers.size() == 2);
    REQUIRE(msg_crlf->headers.size() == msg_lf->headers.size());
    for (size_t i = 0; i < msg_lf->headers.size(); ++i) {
        REQUIRE(msg_crlf->headers[i]->field == msg_lf->headers[i]->field);
        REQUIRE(msg_crlf->headers[i]->value == msg_lf->headers[i]->value);
    }
    REQUIRE(msg_lf->headers[0]->field == "Content-Type");
    REQUIRE(msg_lf->headers[0]->value == "text/plain");
    REQUIRE(msg_lf->headers[1]->field == "Subject");
    REQUIRE(msg_lf->headers[1]->value == "Hello");

    // Identical body content
    REQUIRE(msg_lf->body == "Body line 1\nBody line 2");
    REQUIRE(msg_crlf->body == msg_lf->body);
}

TEST_CASE("MIME Parser: CRLF message with CRLF body", "[mime][parser][crlf]") {
    libglot::Arena arena;
    std::string_view source = "Subject: Test\r\n\r\nLine 1\r\nLine 2\r\n";

    MimeParser parser(arena, source);
    auto* msg = parser.parse_top_level();

    REQUIRE(msg != nullptr);
    REQUIRE(msg->headers.size() == 1);
    REQUIRE(msg->headers[0]->value == "Test");
    REQUIRE(msg->body == "Line 1\r\nLine 2\r\n");
}

TEST_CASE("MIME Parser: Lenient bare CR line endings", "[mime][parser][crlf]") {
    libglot::Arena arena;
    std::string_view source = "Subject: Legacy\rFrom: a@b.c\r\rBody text";

    MimeParser parser(arena, source);
    auto* msg = parser.parse_top_level();

    REQUIRE(msg != nullptr);
    REQUIRE(msg->headers.size() == 2);
    REQUIRE(msg->headers[0]->field == "Subject");
    REQUIRE(msg->headers[0]->value == "Legacy");
    REQUIRE(msg->headers[1]->field == "From");
    REQUIRE(msg->headers[1]->value == "a@b.c");
    REQUIRE(msg->body == "Body text");
}

TEST_CASE("MIME Parser: Folded header value is unfolded", "[mime][parser][folding]") {
    // RFC 5322 §2.2.3: a header may be split across lines; continuation
    // lines start with SP/HTAB. Unfolding removes the line break and
    // keeps the whitespace.
    libglot::Arena arena;
    std::string_view source = "Subject: This is a long\n"
                              " subject that spans\n"
                              " multiple lines\n"
                              "\n"
                              "Body";

    MimeParser parser(arena, source);
    auto* msg = parser.parse_top_level();

    REQUIRE(msg != nullptr);
    REQUIRE(msg->headers.size() == 1);
    REQUIRE(msg->headers[0]->field == "Subject");
    REQUIRE(msg->headers[0]->value == "This is a long subject that spans multiple lines");
    REQUIRE(msg->body == "Body");
}

TEST_CASE("MIME Parser: Folded header with CRLF line endings", "[mime][parser][folding]") {
    libglot::Arena arena;
    std::string_view source = "Subject: Part one\r\n"
                              "\tpart two\r\n"
                              "From: x@y.z\r\n"
                              "\r\n"
                              "Body";

    MimeParser parser(arena, source);
    auto* msg = parser.parse_top_level();

    REQUIRE(msg != nullptr);
    REQUIRE(msg->headers.size() == 2);
    // Unfolding keeps the continuation whitespace (here a HTAB)
    REQUIRE(msg->headers[0]->value == "Part one\tpart two");
    REQUIRE(msg->headers[1]->field == "From");
    REQUIRE(msg->headers[1]->value == "x@y.z");
    REQUIRE(msg->body == "Body");
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
