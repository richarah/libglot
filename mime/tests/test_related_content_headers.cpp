/// ============================================================================
/// multipart/related "start" (RFC 2387) + Content-ID/Location/Description/
/// Language Header Tests
/// ============================================================================
///
/// Exercises MimeParserExtended::resolve_related_start (parser_extended.h):
/// multipart/related's optional "start" parameter is resolved against each
/// part's Content-ID header and attached as Message::related_root, falling
/// back to the first part when "start" is absent or unresolved (recording
/// AnomalyKind::InvalidRelatedStart in the unresolved case). Also covers
/// RFC 5322 comment-stripping now applied to Content-ID/Content-Location/
/// Content-Description/Content-Language (previously only Content-Disposition
/// was in MimeParserExtended::is_structured_field).
/// ============================================================================

#include "../../core/include/libglot/util/arena.h"
#include "../include/libglot/mime/mime.h"
#include <catch2/catch_test_macros.hpp>

using namespace libglot::mime;

TEST_CASE("multipart/related: start resolves to the matching Content-ID",
          "[mime][related]") {
    libglot::Arena arena;
    std::string_view source =
        "Content-Type: multipart/related; boundary=\"b\"; "
        "start=\"<root.part@example.com>\"; type=\"text/html\"\n"
        "\n"
        "--b\n"
        "Content-Type: image/png\n"
        "Content-ID: <image1@example.com>\n"
        "\n"
        "fake-image-bytes\n"
        "--b\n"
        "Content-Type: text/html\n"
        "Content-ID: <root.part@example.com>\n"
        "\n"
        "<html><body><img src=\"cid:image1@example.com\"></body></html>\n"
        "--b--\n";

    auto result = parse_message(arena, source);
    Message* msg = result.message;

    REQUIRE(msg != nullptr);
    REQUIRE(!result.rejected);
    REQUIRE(!result.has_anomaly(AnomalyKind::InvalidRelatedStart));
    REQUIRE(msg->parts.size() == 2);
    REQUIRE(msg->related_root == msg->parts[1]);

    const Header* ct = find_header(*msg->related_root, "Content-Type");
    REQUIRE(ct != nullptr);
    REQUIRE(ct->value == "text/html");
}

TEST_CASE("multipart/related: absent start falls back to the first part",
          "[mime][related]") {
    libglot::Arena arena;
    std::string_view source = "Content-Type: multipart/related; boundary=\"b\"\n"
                              "\n"
                              "--b\n"
                              "Content-Type: text/html\n"
                              "Content-ID: <root@example.com>\n"
                              "\n"
                              "<html></html>\n"
                              "--b\n"
                              "Content-Type: image/png\n"
                              "Content-ID: <img@example.com>\n"
                              "\n"
                              "bytes\n"
                              "--b--\n";

    auto result = parse_message(arena, source);
    Message* msg = result.message;

    REQUIRE(msg != nullptr);
    REQUIRE(!result.has_anomaly(AnomalyKind::InvalidRelatedStart));
    REQUIRE(msg->related_root == msg->parts[0]);
}

TEST_CASE("multipart/related: unresolved start falls back to the first part "
          "and records an anomaly",
          "[mime][related]") {
    libglot::Arena arena;
    std::string_view source =
        "Content-Type: multipart/related; boundary=\"b\"; start=\"<does-not-exist@example.com>\"\n"
        "\n"
        "--b\n"
        "Content-Type: text/html\n"
        "Content-ID: <root@example.com>\n"
        "\n"
        "<html></html>\n"
        "--b\n"
        "Content-Type: image/png\n"
        "Content-ID: <img@example.com>\n"
        "\n"
        "bytes\n"
        "--b--\n";

    auto result = parse_message(arena, source);
    Message* msg = result.message;

    REQUIRE(msg != nullptr);
    REQUIRE(result.has_anomaly(AnomalyKind::InvalidRelatedStart));
    REQUIRE(msg->related_root == msg->parts[0]);
}

TEST_CASE("multipart/related: absent for non-related multipart", "[mime][related]") {
    libglot::Arena arena;
    std::string_view source = "Content-Type: multipart/mixed; boundary=\"b\"\n"
                              "\n"
                              "--b\n"
                              "Content-Type: text/plain\n"
                              "\n"
                              "body\n"
                              "--b--\n";

    auto result = parse_message(arena, source);
    REQUIRE(result.message != nullptr);
    REQUIRE(result.message->related_root == nullptr);
}

// ============================================================================
// Content-ID / Content-Location / Content-Description / Content-Language:
// RFC 5322 comments are now stripped for these fields too (previously only
// Content-Disposition was handled).
// ============================================================================

TEST_CASE("Content-ID: comment is stripped like other structured fields",
          "[mime][related][headers]") {
    libglot::Arena arena;
    std::string_view source = "Content-Type: text/plain\n"
                              "Content-ID: <id@example.com> (auto-generated)\n"
                              "\n"
                              "body\n";

    auto result = parse_message(arena, source);
    REQUIRE(result.message != nullptr);
    const Header* cid = find_header(*result.message, "Content-ID");
    REQUIRE(cid != nullptr);
    REQUIRE(cid->value == "<id@example.com> ");
}

TEST_CASE("Content-Location/Content-Description/Content-Language are readable",
          "[mime][related][headers]") {
    libglot::Arena arena;
    std::string_view source = "Content-Type: text/plain\n"
                              "Content-Location: http://example.com/resource.txt\n"
                              "Content-Description: A short description\n"
                              "Content-Language: en-US\n"
                              "\n"
                              "body\n";

    auto result = parse_message(arena, source);
    REQUIRE(result.message != nullptr);

    const Header* loc = find_header(*result.message, "Content-Location");
    REQUIRE(loc != nullptr);
    REQUIRE(loc->value == "http://example.com/resource.txt");

    const Header* desc = find_header(*result.message, "Content-Description");
    REQUIRE(desc != nullptr);
    REQUIRE(desc->value == "A short description");

    const Header* lang = find_header(*result.message, "Content-Language");
    REQUIRE(lang != nullptr);
    REQUIRE(lang->value == "en-US");
}
