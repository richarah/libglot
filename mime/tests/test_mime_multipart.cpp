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

TEST_CASE("MIME Multipart: CRLF multipart message", "[mime][multipart][crlf]") {
    libglot::Arena arena;
    std::string source =
        "Content-Type: multipart/mixed; boundary=\"bnd\"\r\n"
        "\r\n"
        "preamble to be discarded\r\n"
        "--bnd\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n"
        "Part one content\r\n"
        "--bnd\r\n"
        "Content-Type: text/html\r\n"
        "\r\n"
        "<p>Part two</p>\r\n"
        "--bnd--\r\n"
        "epilogue to be discarded\r\n";

    MimeParserExtended parser(arena, source);
    auto* msg = parser.parse_message_multipart();

    REQUIRE(msg != nullptr);
    REQUIRE(msg->parts.size() == 2);

    // The CRLF before a delimiter belongs to the delimiter, not the part
    REQUIRE(msg->parts[0]->body == "Part one content");
    REQUIRE(msg->parts[1]->body == "<p>Part two</p>");
}

TEST_CASE("MIME Multipart: Boundary text inside part content does not split", "[mime][multipart][boundary]") {
    libglot::Arena arena;
    std::string source =
        "Content-Type: multipart/mixed; boundary=xyz\n"
        "\n"
        "--xyz\n"
        "Content-Type: text/plain\n"
        "\n"
        "This line mentions --xyz mid-line and must not split\n"
        "--xyzlonger is a prefix match and must not split either\n"
        "--xyz\n"
        "\n"
        "second part\n"
        "--xyz--\n";

    MimeParserExtended parser(arena, source);
    auto* msg = parser.parse_message_multipart();

    REQUIRE(msg != nullptr);
    REQUIRE(msg->parts.size() == 2);
    REQUIRE(msg->parts[0]->body.find("mentions --xyz mid-line") != std::string_view::npos);
    REQUIRE(msg->parts[0]->body.find("--xyzlonger") != std::string_view::npos);
    REQUIRE(msg->parts[1]->body.find("second part") != std::string_view::npos);
}

TEST_CASE("MIME Multipart: Whitespace after boundary marker", "[mime][multipart][boundary]") {
    libglot::Arena arena;
    std::string source =
        "Content-Type: multipart/mixed; boundary=pad\n"
        "\n"
        "--pad  \n"
        "\n"
        "part one\n"
        "--pad \t \n"
        "\n"
        "part two\n"
        "--pad-- \n";

    MimeParserExtended parser(arena, source);
    auto* msg = parser.parse_message_multipart();

    REQUIRE(msg != nullptr);
    REQUIRE(msg->parts.size() == 2);
    REQUIRE(msg->parts[0]->body == "part one");
    REQUIRE(msg->parts[1]->body == "part two");
}

TEST_CASE("MIME Multipart: Missing final boundary still returns parts", "[mime][multipart][boundary]") {
    libglot::Arena arena;
    std::string source =
        "Content-Type: multipart/mixed; boundary=nofinal\n"
        "\n"
        "--nofinal\n"
        "Content-Type: text/plain\n"
        "\n"
        "part one\n"
        "--nofinal\n"
        "Content-Type: text/plain\n"
        "\n"
        "part two, message truncated before close delimiter\n";

    MimeParserExtended parser(arena, source);
    auto* msg = parser.parse_message_multipart();

    REQUIRE(msg != nullptr);
    REQUIRE(msg->parts.size() == 2);
    REQUIRE(msg->parts[1]->body.find("part two") != std::string_view::npos);

    // The missing close delimiter is anomaly-worthy
    bool has_missing_final = false;
    for (const auto& rec : parser.anomalies().records) {
        if (rec.kind == AnomalyKind::MissingFinalBoundary) {
            has_missing_final = true;
        }
    }
    REQUIRE(has_missing_final);
}

TEST_CASE("MIME Multipart: Folded Content-Type header in part", "[mime][multipart][folding]") {
    libglot::Arena arena;
    std::string source =
        "Content-Type: multipart/mixed;\n"
        " boundary=\"folded\"\n"
        "\n"
        "--folded\n"
        "Content-Type: text/plain;\n"
        " charset=utf-8\n"
        "\n"
        "part body\n"
        "--folded--\n";

    MimeParserExtended parser(arena, source);
    auto* msg = parser.parse_message_multipart();

    REQUIRE(msg != nullptr);

    // Top-level folded Content-Type reassembled exactly
    REQUIRE(msg->headers.size() == 1);
    REQUIRE(msg->headers[0]->value == "multipart/mixed; boundary=\"folded\"");
    REQUIRE(msg->headers[0]->parameters.size() == 1);
    REQUIRE(msg->headers[0]->parameters[0].first == "boundary");
    REQUIRE(msg->headers[0]->parameters[0].second == "folded");

    // Folded part header reassembled and parameters parsed
    REQUIRE(msg->parts.size() == 1);
    REQUIRE(msg->parts[0]->headers.size() == 1);
    REQUIRE(msg->parts[0]->headers[0]->value == "text/plain; charset=utf-8");
    REQUIRE(msg->parts[0]->headers[0]->parameters.size() == 1);
    REQUIRE(msg->parts[0]->headers[0]->parameters[0].first == "charset");
    REQUIRE(msg->parts[0]->headers[0]->parameters[0].second == "utf-8");
    REQUIRE(msg->parts[0]->body == "part body");
}

namespace {

/// Build a multipart message nested `depth` levels deep
std::string build_nested_multipart(int depth) {
    std::string content = "Content-Type: text/plain\n\nleaf content";
    for (int level = depth; level >= 1; --level) {
        std::string b = "b" + std::to_string(level);
        content = "Content-Type: multipart/mixed; boundary=" + b + "\n\n"
                  "--" + b + "\n" + content + "\n--" + b + "--\n";
    }
    return content;
}

/// Depth of the parsed multipart tree (0 = no parts)
int multipart_depth(const Message* msg) {
    int deepest = 0;
    for (const auto* part : msg->parts) {
        deepest = std::max(deepest, multipart_depth(part));
    }
    return msg->parts.empty() ? 0 : deepest + 1;
}

} // namespace

TEST_CASE("MIME Multipart: 100-deep nesting parses without stack overflow", "[mime][multipart][limits]") {
    libglot::Arena arena;
    std::string source = build_nested_multipart(100);

    MimeParserExtended parser(arena, source);
    auto* msg = parser.parse_message_multipart();

    REQUIRE(msg != nullptr);
    REQUIRE(multipart_depth(msg) == 100);

    // Within the default limits: no DoS anomaly
    for (const auto& rec : parser.anomalies().records) {
        REQUIRE(rec.kind != AnomalyKind::ExcessiveNestingDepth);
    }
}

TEST_CASE("MIME Multipart: Nesting depth limit stops descent cleanly", "[mime][multipart][limits]") {
    libglot::Arena arena;
    std::string source = build_nested_multipart(100);

    ParserLimits limits = ParserLimits::standard();
    limits.max_nesting_depth = 10;

    MimeParserExtended parser(arena, source, limits);
    auto* msg = parser.parse_message_multipart();

    REQUIRE(msg != nullptr);
    REQUIRE(multipart_depth(msg) <= 10);

    bool has_depth_anomaly = false;
    for (const auto& rec : parser.anomalies().records) {
        if (rec.kind == AnomalyKind::ExcessiveNestingDepth) {
            has_depth_anomaly = true;
        }
    }
    REQUIRE(has_depth_anomaly);
}

TEST_CASE("MIME Multipart: Part count limit stops parsing cleanly", "[mime][multipart][limits]") {
    libglot::Arena arena;
    std::string source = "Content-Type: multipart/mixed; boundary=many\n\n";
    for (int i = 0; i < 50; ++i) {
        source += "--many\n\npart " + std::to_string(i) + "\n";
    }
    source += "--many--\n";

    ParserLimits limits = ParserLimits::standard();
    limits.max_total_parts = 20;

    MimeParserExtended parser(arena, source, limits);
    auto* msg = parser.parse_message_multipart();

    REQUIRE(msg != nullptr);
    REQUIRE(msg->parts.size() == 20);

    bool has_count_anomaly = false;
    for (const auto& rec : parser.anomalies().records) {
        if (rec.kind == AnomalyKind::ExcessivePartCount) {
            has_count_anomaly = true;
        }
    }
    REQUIRE(has_count_anomaly);
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
