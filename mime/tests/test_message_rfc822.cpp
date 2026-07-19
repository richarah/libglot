/// ============================================================================
/// message/rfc822 Nesting Tests (RFC 2046 Section 5.2.1)
/// ============================================================================
///
/// Exercises MimeParserExtended::parse_encapsulated_message (parser_extended.h)
/// wired into finish_message(): a Content-Type: message/rfc822 part has its
/// body parsed as a full nested Message (headers + body, recursively run
/// through the same pipeline) and attached via Message::encapsulated. The
/// same nesting-depth/part-count DoS limits enforced for multipart apply to
/// chains of message/rfc822, recording the same anomaly kinds.
/// ============================================================================

#include "../../core/include/libglot/util/arena.h"
#include "../include/libglot/mime/mime.h"
#include <catch2/catch_test_macros.hpp>

#include <string>

using namespace libglot::mime;

TEST_CASE("message/rfc822: forwarded mail with headers and body is recursed into",
          "[mime][rfc822]") {
    libglot::Arena arena;
    std::string_view source = "Content-Type: message/rfc822\n"
                              "\n"
                              "From: alice@example.com\n"
                              "To: bob@example.com\n"
                              "Subject: Original message\n"
                              "\n"
                              "This is the body of the forwarded message.\n";

    auto result = parse_message(arena, source);
    Message* msg = result.message;

    REQUIRE(msg != nullptr);
    REQUIRE(!result.rejected);
    REQUIRE(msg->encapsulated != nullptr);

    const Message* nested = msg->encapsulated;
    const Header* from = find_header(*nested, "From");
    const Header* subject = find_header(*nested, "Subject");
    REQUIRE(from != nullptr);
    REQUIRE(from->value == "alice@example.com");
    REQUIRE(subject != nullptr);
    REQUIRE(subject->value == "Original message");
    REQUIRE(nested->body == "This is the body of the forwarded message.\n");
}

TEST_CASE("message/rfc822: nested two deep", "[mime][rfc822]") {
    libglot::Arena arena;
    std::string_view source = "Content-Type: message/rfc822\n"
                              "\n"
                              "Subject: Outer forward\n"
                              "Content-Type: message/rfc822\n"
                              "\n"
                              "Subject: Innermost message\n"
                              "\n"
                              "Innermost body.\n";

    auto result = parse_message(arena, source);
    Message* msg = result.message;

    REQUIRE(msg != nullptr);
    REQUIRE(!result.rejected);
    REQUIRE(msg->encapsulated != nullptr);

    Message* level1 = msg->encapsulated;
    const Header* subject1 = find_header(*level1, "Subject");
    REQUIRE(subject1 != nullptr);
    REQUIRE(subject1->value == "Outer forward");
    REQUIRE(level1->encapsulated != nullptr);

    Message* level2 = level1->encapsulated;
    const Header* subject2 = find_header(*level2, "Subject");
    REQUIRE(subject2 != nullptr);
    REQUIRE(subject2->value == "Innermost message");
    REQUIRE(level2->body == "Innermost body.\n");
}

TEST_CASE("message/rfc822: a part inside multipart/mixed", "[mime][rfc822]") {
    libglot::Arena arena;
    std::string_view source = "MIME-Version: 1.0\n"
                              "Content-Type: multipart/mixed; boundary=\"outer\"\n"
                              "\n"
                              "--outer\n"
                              "Content-Type: text/plain\n"
                              "\n"
                              "Please see the attached forwarded message.\n"
                              "--outer\n"
                              "Content-Type: message/rfc822\n"
                              "\n"
                              "From: carol@example.com\n"
                              "Subject: Fwd: Attached\n"
                              "\n"
                              "Attached message body.\n"
                              "--outer--\n";

    auto result = parse_message(arena, source);
    Message* msg = result.message;

    REQUIRE(msg != nullptr);
    REQUIRE(!result.rejected);
    REQUIRE(msg->parts.size() == 2);

    Message* plain_part = msg->parts[0];
    REQUIRE(plain_part->encapsulated == nullptr);

    Message* rfc822_part = msg->parts[1];
    REQUIRE(rfc822_part->encapsulated != nullptr);
    const Header* from = find_header(*rfc822_part->encapsulated, "From");
    REQUIRE(from != nullptr);
    REQUIRE(from->value == "carol@example.com");
    // The line break immediately before the closing boundary belongs to the
    // delimiter (RFC 2046 boundary.h), not to the part's content.
    REQUIRE(rfc822_part->encapsulated->body == "Attached message body.");
}

TEST_CASE("message/rfc822: depth-limit enforcement stops recursion cleanly",
          "[mime][rfc822][limits]") {
    // Build a chain of nested message/rfc822 parts deeper than the limit.
    std::string chain = "Subject: leaf\n\nleaf body\n";
    constexpr int kChainDepth = 10;
    for (int i = 0; i < kChainDepth; ++i) {
        chain = "Content-Type: message/rfc822\n\n" + chain;
    }

    libglot::Arena arena;
    ParseOptions options;
    options.limits.max_nesting_depth = 3;

    auto result = parse_message(arena, chain, options);
    Message* msg = result.message;

    REQUIRE(msg != nullptr);
    REQUIRE(result.has_anomaly(AnomalyKind::ExcessiveNestingDepth));
    // DoS-severity anomaly under the standard config marks the parse
    // rejected, exactly like the equivalent multipart nesting-depth case.
    REQUIRE(result.rejected);

    // Recursion stops at the configured depth: walk down until encapsulated
    // becomes null and confirm it does not reach kChainDepth levels.
    Message* cur = msg;
    int depth = 0;
    while (cur->encapsulated != nullptr) {
        cur = cur->encapsulated;
        ++depth;
    }
    REQUIRE(depth < kChainDepth);
    REQUIRE(depth == 3);
}

TEST_CASE("message/rfc822: absent for ordinary content types", "[mime][rfc822]") {
    libglot::Arena arena;
    std::string_view source = "Content-Type: text/plain\n\nordinary body\n";

    auto result = parse_message(arena, source);
    REQUIRE(result.message != nullptr);
    REQUIRE(result.message->encapsulated == nullptr);
}
