/// ============================================================================
/// Message-ID / In-Reply-To / References Tests (RFC 5322 Section 3.6.4)
/// ============================================================================
///
/// Exercises MessageIdParser (complete_features.h) directly, and its wiring
/// into the parse_message() pipeline (Message::message_id, in_reply_to,
/// references), covering multiple references, folded values (already
/// unfolded upstream), comments (already stripped upstream, since these
/// fields are in MimeParserExtended::is_structured_field), and malformed
/// msg-ids -- which must record an anomaly, never throw.
/// ============================================================================

#include "../../core/include/libglot/util/arena.h"
#include "../include/libglot/mime/mime.h"
#include <catch2/catch_test_macros.hpp>

using namespace libglot::mime;

// ============================================================================
// MessageIdParser (standalone)
// ============================================================================

TEST_CASE("MessageId: parses a single well-formed msg-id", "[mime][msgid]") {
    auto id = MessageIdParser::parse_one("<1234.5678@example.com>");
    REQUIRE(id.valid);
    REQUIRE(id.value == "1234.5678@example.com");
}

TEST_CASE("MessageId: parse_list finds multiple msg-ids", "[mime][msgid]") {
    auto ids = MessageIdParser::parse_list(
        "<a1@example.com> <a2@example.org> <a3@sub.example.net>");
    REQUIRE(ids.size() == 3);
    REQUIRE(ids[0].valid);
    REQUIRE(ids[0].value == "a1@example.com");
    REQUIRE(ids[1].valid);
    REQUIRE(ids[1].value == "a2@example.org");
    REQUIRE(ids[2].valid);
    REQUIRE(ids[2].value == "a3@sub.example.net");
}

TEST_CASE("MessageId: malformed ids do not throw and are marked invalid", "[mime][msgid]") {
    REQUIRE(!MessageIdParser::parse_one("no-brackets@example.com").valid);
    REQUIRE(!MessageIdParser::parse_one("<missing-at-sign>").valid);
    REQUIRE(!MessageIdParser::parse_one("<@example.com>").valid);   // empty local-part
    REQUIRE(!MessageIdParser::parse_one("<local@>").valid);          // empty domain
    REQUIRE(!MessageIdParser::parse_one("<>").valid);                // empty entirely
    REQUIRE(!MessageIdParser::parse_one("<<nested@example.com>>").valid);
    REQUIRE(!MessageIdParser::parse_one("").valid);
}

TEST_CASE("MessageId: unterminated angle bracket does not throw", "[mime][msgid]") {
    auto ids = MessageIdParser::parse_list("<truncated@example.com");
    REQUIRE(ids.size() == 1);
    REQUIRE(!ids[0].valid);
}

// ============================================================================
// Pipeline wiring
// ============================================================================

TEST_CASE("Threading headers: Message-ID is parsed onto the message", "[mime][msgid][pipeline]") {
    libglot::Arena arena;
    std::string_view source = "Message-ID: <root-msg@example.com>\n\nbody\n";

    auto result = parse_message(arena, source);
    REQUIRE(result.message != nullptr);
    REQUIRE(result.message->message_id != nullptr);
    REQUIRE(result.message->message_id->valid);
    REQUIRE(result.message->message_id->value == "root-msg@example.com");
    REQUIRE(!result.has_anomaly(AnomalyKind::InvalidMessageIdSyntax));
}

TEST_CASE("Threading headers: References carries an ordered list", "[mime][msgid][pipeline]") {
    libglot::Arena arena;
    std::string_view source = "References: <m1@example.com> <m2@example.com> <m3@example.com>\n"
                              "\n"
                              "body\n";

    auto result = parse_message(arena, source);
    REQUIRE(result.message != nullptr);
    REQUIRE(result.message->references != nullptr);
    REQUIRE(result.message->references->size() == 3);
    REQUIRE((*result.message->references)[0].value == "m1@example.com");
    REQUIRE((*result.message->references)[1].value == "m2@example.com");
    REQUIRE((*result.message->references)[2].value == "m3@example.com");
}

TEST_CASE("Threading headers: In-Reply-To is parsed as a list", "[mime][msgid][pipeline]") {
    libglot::Arena arena;
    std::string_view source = "In-Reply-To: <parent@example.com>\n\nbody\n";

    auto result = parse_message(arena, source);
    REQUIRE(result.message != nullptr);
    REQUIRE(result.message->in_reply_to != nullptr);
    REQUIRE(result.message->in_reply_to->size() == 1);
    REQUIRE((*result.message->in_reply_to)[0].value == "parent@example.com");
}

TEST_CASE("Threading headers: folded References value is unfolded upstream",
          "[mime][msgid][pipeline]") {
    libglot::Arena arena;
    std::string_view source = "References: <m1@example.com>\n"
                              " <m2@example.com>\n"
                              "\n"
                              "body\n";

    auto result = parse_message(arena, source);
    REQUIRE(result.message != nullptr);
    REQUIRE(result.message->references != nullptr);
    REQUIRE(result.message->references->size() == 2);
    REQUIRE((*result.message->references)[0].value == "m1@example.com");
    REQUIRE((*result.message->references)[1].value == "m2@example.com");
}

TEST_CASE("Threading headers: comment in Message-ID is stripped upstream",
          "[mime][msgid][pipeline]") {
    libglot::Arena arena;
    std::string_view source = "Message-ID: <id@example.com> (generated)\n\nbody\n";

    auto result = parse_message(arena, source);
    REQUIRE(result.message != nullptr);
    REQUIRE(result.message->message_id != nullptr);
    REQUIRE(result.message->message_id->valid);
    REQUIRE(result.message->message_id->value == "id@example.com");
}

TEST_CASE("Threading headers: malformed Message-ID records an anomaly, never throws",
          "[mime][msgid][pipeline]") {
    libglot::Arena arena;
    std::string_view source = "Message-ID: not-a-valid-msgid\n\nbody\n";

    auto result = parse_message(arena, source);
    REQUIRE(result.message != nullptr);
    REQUIRE(result.has_anomaly(AnomalyKind::InvalidMessageIdSyntax));
}

TEST_CASE("Threading headers: malformed References entries are each flagged",
          "[mime][msgid][pipeline]") {
    libglot::Arena arena;
    std::string_view source = "References: <good@example.com> <bad-no-domain@>\n\nbody\n";

    auto result = parse_message(arena, source);
    REQUIRE(result.message != nullptr);
    REQUIRE(result.message->references != nullptr);
    REQUIRE(result.message->references->size() == 2);
    REQUIRE((*result.message->references)[0].valid);
    REQUIRE(!(*result.message->references)[1].valid);
    REQUIRE(result.has_anomaly(AnomalyKind::InvalidMessageIdSyntax));
}

TEST_CASE("Threading headers: absent when the headers are absent", "[mime][msgid][pipeline]") {
    libglot::Arena arena;
    std::string_view source = "Subject: no threading headers here\n\nbody\n";

    auto result = parse_message(arena, source);
    REQUIRE(result.message != nullptr);
    REQUIRE(result.message->message_id == nullptr);
    REQUIRE(result.message->in_reply_to == nullptr);
    REQUIRE(result.message->references == nullptr);
}
