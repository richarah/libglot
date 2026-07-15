/// ============================================================================
/// message/partial Detection Tests (RFC 2046 Section 5.2.2)
/// ============================================================================
///
/// Exercises MessagePartialParser directly (id/number/total parameter
/// parsing, defensive handling of malformed numeric parameters) and its
/// wiring into the parse_message() pipeline: Content-Type: message/partial
/// is detected, the reference is attached to the message, and a
/// MessagePartialDetected anomaly is recorded so callers know reassembly
/// with sibling fragments is required. Reassembly itself is out of scope.
/// ============================================================================

#include <catch2/catch_test_macros.hpp>
#include "../include/libglot/mime/mime.h"
#include "../../core/include/libglot/util/arena.h"

using namespace libglot::mime;

// ============================================================================
// MessagePartialParser (standalone)
// ============================================================================

TEST_CASE("Message/Partial: parses id/number/total", "[mime][message_partial]") {
    std::vector<std::pair<std::string_view, std::string_view>> params = {
        {"id", "abc123@example.com"},
        {"number", "2"},
        {"total", "3"},
    };

    auto ref = MessagePartialParser::parse(params);

    REQUIRE(ref.id == "abc123@example.com");
    REQUIRE(ref.number == 2);
    REQUIRE(ref.total == 3);
}

TEST_CASE("Message/Partial: case-insensitive parameter keys", "[mime][message_partial]") {
    std::vector<std::pair<std::string_view, std::string_view>> params = {
        {"ID", "xyz"},
        {"Number", "1"},
        {"Total", "5"},
    };

    auto ref = MessagePartialParser::parse(params);

    REQUIRE(ref.id == "xyz");
    REQUIRE(ref.number == 1);
    REQUIRE(ref.total == 5);
}

TEST_CASE("Message/Partial: empty parameters yield a zeroed reference", "[mime][message_partial]") {
    std::vector<std::pair<std::string_view, std::string_view>> params = {};

    auto ref = MessagePartialParser::parse(params);

    REQUIRE(ref.id.empty());
    REQUIRE(ref.number == 0);
    REQUIRE(ref.total == 0);
}

TEST_CASE("Message/Partial: non-numeric number/total do not throw and default to 0", "[mime][message_partial][security]") {
    std::vector<std::pair<std::string_view, std::string_view>> params = {
        {"id", "abc"},
        {"number", "abc"},
        {"total", "xyz"},
    };

    MessagePartialRef ref;
    REQUIRE_NOTHROW(ref = MessagePartialParser::parse(params));
    REQUIRE(ref.number == 0);
    REQUIRE(ref.total == 0);
}

TEST_CASE("Message/Partial: negative, zero, trailing-garbage, and out-of-range numbers are ignored", "[mime][message_partial][security]") {
    {
        std::vector<std::pair<std::string_view, std::string_view>> params = {
            {"number", "-1"}, {"total", "3"},
        };
        auto ref = MessagePartialParser::parse(params);
        REQUIRE(ref.number == 0);  // negative rejected
        REQUIRE(ref.total == 3);
    }
    {
        std::vector<std::pair<std::string_view, std::string_view>> params = {
            {"number", "0"},
        };
        auto ref = MessagePartialParser::parse(params);
        REQUIRE(ref.number == 0);  // zero is not a valid 1-based fragment number
    }
    {
        std::vector<std::pair<std::string_view, std::string_view>> params = {
            {"number", "2abc"},
        };
        auto ref = MessagePartialParser::parse(params);
        REQUIRE(ref.number == 0);  // trailing garbage after digits rejected
    }
    {
        std::vector<std::pair<std::string_view, std::string_view>> params = {
            {"total", "99999999999999999999999999999999"},
        };
        MessagePartialRef ref;
        REQUIRE_NOTHROW(ref = MessagePartialParser::parse(params));
        REQUIRE(ref.total == 0);  // overflow rejected, never throws
    }
}

// ============================================================================
// Pipeline wiring: detection, parameters, anomaly
// ============================================================================

TEST_CASE("Pipeline: message/partial is detected and parameters attached", "[mime][pipeline][message_partial]") {
    libglot::Arena arena;
    std::string_view source =
        "Content-Type: message/partial; id=\"frag-1@example.com\"; number=1; total=3\n"
        "\n"
        "First fragment of a large message.\n";

    auto result = parse_message(arena, source);

    REQUIRE(result.message != nullptr);
    REQUIRE(result.message->message_partial != nullptr);
    REQUIRE(result.message->message_partial->id == "frag-1@example.com");
    REQUIRE(result.message->message_partial->number == 1);
    REQUIRE(result.message->message_partial->total == 3);
}

TEST_CASE("Pipeline: message/partial records the MessagePartialDetected anomaly", "[mime][pipeline][message_partial][anomalies]") {
    libglot::Arena arena;
    std::string_view source =
        "Content-Type: message/partial; id=\"frag-2@example.com\"; number=2; total=3\n"
        "\n"
        "Second fragment.\n";

    auto result = parse_message(arena, source);

    REQUIRE(result.message != nullptr);
    REQUIRE(result.has_anomaly(AnomalyKind::MessagePartialDetected));
    // Structural severity: not itself rejected under the standard policy
    REQUIRE(!result.rejected);
}

TEST_CASE("Pipeline: message/partial is detected case-insensitively and with extra parameters", "[mime][pipeline][message_partial]") {
    libglot::Arena arena;
    std::string_view source =
        "Content-Type: Message/Partial; id=xyz; number=3; total=3\n"
        "\n"
        "Last fragment.\n";

    auto result = parse_message(arena, source);

    REQUIRE(result.message != nullptr);
    REQUIRE(result.message->message_partial != nullptr);
    REQUIRE(result.message->message_partial->id == "xyz");
    REQUIRE(result.message->message_partial->number == 3);
    REQUIRE(result.message->message_partial->total == 3);
    REQUIRE(result.has_anomaly(AnomalyKind::MessagePartialDetected));
}

TEST_CASE("Pipeline: normal (non-partial) messages have no message_partial and no anomaly", "[mime][pipeline][message_partial]") {
    libglot::Arena arena;
    std::string_view source =
        "Content-Type: text/plain; charset=utf-8\n"
        "Subject: not a fragment\n"
        "\n"
        "Ordinary body.\n";

    auto result = parse_message(arena, source);

    REQUIRE(result.message != nullptr);
    REQUIRE(result.message->message_partial == nullptr);
    REQUIRE(!result.has_anomaly(AnomalyKind::MessagePartialDetected));
}

TEST_CASE("Pipeline: message/external-body is unaffected by message/partial wiring", "[mime][pipeline][message_partial]") {
    libglot::Arena arena;
    std::string_view source =
        "Content-Type: message/external-body; access-type=ftp; name=file.txt; site=ftp.example.com\n"
        "\n"
        "\n";

    auto result = parse_message(arena, source);

    REQUIRE(result.message != nullptr);
    REQUIRE(result.message->external_body != nullptr);
    REQUIRE(result.message->message_partial == nullptr);
    REQUIRE(!result.has_anomaly(AnomalyKind::MessagePartialDetected));
}

TEST_CASE("Message/Partial: severity is Structural, not Security/DoS", "[mime][message_partial][anomalies]") {
    REQUIRE(AnomalyConfig::get_severity(AnomalyKind::MessagePartialDetected) == AnomalySeverity::Structural);
}

TEST_CASE("Message/Partial: anomaly kind name is registered", "[mime][message_partial][anomalies]") {
    REQUIRE(anomaly_kind_name(AnomalyKind::MessagePartialDetected) == "MessagePartialDetected");
}
