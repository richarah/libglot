#include <catch2/catch_test_macros.hpp>
#include "../include/libglot/mime/parser_extended.h"
#include "../../core/include/libglot/util/arena.h"

using namespace libglot::mime;

TEST_CASE("MIME Multipart: Parse simple multipart/mixed", "[mime][multipart]") {
    libglot::Arena arena;
    std::string_view source = R"(Content-Type: multipart/mixed; boundary="boundary123"

--boundary123
Content-Type: text/plain

This is part 1
--boundary123
Content-Type: text/html

<p>This is part 2</p>
--boundary123--
)";

    MimeParserExtended parser(arena, source);
    auto* msg = parser.parse_message_multipart();

    REQUIRE(msg != nullptr);
    REQUIRE(msg->headers.size() == 1);
    REQUIRE(msg->headers[0]->field == "Content-Type");
    REQUIRE(msg->headers[0]->value.find("multipart/mixed") != std::string_view::npos);

    // Check parameters
    REQUIRE(msg->headers[0]->parameters.size() == 1);
    REQUIRE(msg->headers[0]->parameters[0].first == "boundary");
    REQUIRE(msg->headers[0]->parameters[0].second == "boundary123");

    // Check parts
    REQUIRE(msg->parts.size() == 2);

    // Part 1
    REQUIRE(msg->parts[0]->headers.size() == 1);
    REQUIRE(msg->parts[0]->headers[0]->field == "Content-Type");
    REQUIRE(msg->parts[0]->headers[0]->value.find("text/plain") != std::string_view::npos);
    REQUIRE(msg->parts[0]->body.find("This is part 1") != std::string_view::npos);

    // Part 2
    REQUIRE(msg->parts[1]->headers.size() == 1);
    REQUIRE(msg->parts[1]->headers[0]->field == "Content-Type");
    REQUIRE(msg->parts[1]->headers[0]->value.find("text/html") != std::string_view::npos);
    REQUIRE(msg->parts[1]->body.find("<p>This is part 2</p>") != std::string_view::npos);
}

TEST_CASE("MIME Multipart: Parse header parameters", "[mime][parameters]") {
    libglot::Arena arena;
    std::string_view source = "Content-Type: text/plain; charset=utf-8; format=flowed\n\nBody";

    MimeParserExtended parser(arena, source);
    auto* msg = parser.parse_message_multipart();

    REQUIRE(msg != nullptr);
    REQUIRE(msg->headers.size() == 1);

    auto* header = msg->headers[0];
    REQUIRE(header->field == "Content-Type");
    REQUIRE(header->parameters.size() == 2);
    REQUIRE(header->parameters[0].first == "charset");
    REQUIRE(header->parameters[0].second == "utf-8");
    REQUIRE(header->parameters[1].first == "format");
    REQUIRE(header->parameters[1].second == "flowed");
}

TEST_CASE("MIME Multipart: Parse quoted parameter values", "[mime][parameters]") {
    libglot::Arena arena;
    std::string_view source = R"(Content-Disposition: attachment; filename="document with spaces.pdf"

Body)";

    MimeParserExtended parser(arena, source);
    auto* msg = parser.parse_message_multipart();

    REQUIRE(msg != nullptr);
    REQUIRE(msg->headers.size() == 1);

    auto* header = msg->headers[0];
    REQUIRE(header->field == "Content-Disposition");
    REQUIRE(header->parameters.size() == 1);
    REQUIRE(header->parameters[0].first == "filename");
    REQUIRE(header->parameters[0].second == "document with spaces.pdf");
}

TEST_CASE("MIME Multipart: Nested multipart", "[mime][multipart][nested]") {
    libglot::Arena arena;
    std::string_view source = R"(Content-Type: multipart/mixed; boundary="outer"

--outer
Content-Type: multipart/alternative; boundary="inner"

--inner
Content-Type: text/plain

Plain text version
--inner
Content-Type: text/html

<p>HTML version</p>
--inner--
--outer
Content-Type: application/pdf

PDF data here
--outer--
)";

    MimeParserExtended parser(arena, source);
    auto* msg = parser.parse_message_multipart();

    REQUIRE(msg != nullptr);
    REQUIRE(msg->parts.size() == 2);

    // First part should be multipart/alternative with 2 nested parts
    REQUIRE(msg->parts[0]->headers.size() == 1);
    REQUIRE(msg->parts[0]->headers[0]->value.find("multipart/alternative") != std::string_view::npos);
    REQUIRE(msg->parts[0]->parts.size() == 2);
    REQUIRE(msg->parts[0]->parts[0]->body.find("Plain text version") != std::string_view::npos);
    REQUIRE(msg->parts[0]->parts[1]->body.find("HTML version") != std::string_view::npos);

    // Second part should be application/pdf
    REQUIRE(msg->parts[1]->headers.size() == 1);
    REQUIRE(msg->parts[1]->headers[0]->value.find("application/pdf") != std::string_view::npos);
    REQUIRE(msg->parts[1]->body.find("PDF data here") != std::string_view::npos);
}

TEST_CASE("MIME Multipart: Empty parts", "[mime][multipart]") {
    libglot::Arena arena;
    std::string_view source = R"(Content-Type: multipart/mixed; boundary="test"

--test
Content-Type: text/plain

--test
Content-Type: text/html

<p>Some content</p>
--test--
)";

    MimeParserExtended parser(arena, source);
    auto* msg = parser.parse_message_multipart();

    REQUIRE(msg != nullptr);
    REQUIRE(msg->parts.size() == 2);

    // First part has empty body (or only whitespace)
    bool first_part_empty = msg->parts[0]->body.empty() ||
                           msg->parts[0]->body.find_first_not_of(" \r\n\t") == std::string_view::npos;
    REQUIRE(first_part_empty);

    // Second part has content
    REQUIRE(msg->parts[1]->body.find("<p>Some content</p>") != std::string_view::npos);
}
