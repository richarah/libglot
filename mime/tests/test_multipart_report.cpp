/// ============================================================================
/// multipart/report (RFC 6522) / message/delivery-status (RFC 3464) Tests
/// ============================================================================
///
/// Exercises the multipart/report detection and required report-type
/// parameter check wired into MimeParserExtended::finish_message
/// (parser_extended.h), plus DeliveryStatusParser (complete_features.h)
/// both standalone and through the pipeline (Message::delivery_status),
/// over a realistic bounce (DSN) message end-to-end.
/// ============================================================================

#include "../../core/include/libglot/util/arena.h"
#include "../include/libglot/mime/mime.h"
#include <catch2/catch_test_macros.hpp>

using namespace libglot::mime;

namespace {

std::string_view value_of(const std::vector<std::pair<std::string, std::string>>& fields,
                          std::string_view field) {
    for (const auto& [name, val] : fields) {
        if (name == field) {
            return val;
        }
    }
    return "<missing>";
}

} // namespace

// ============================================================================
// DeliveryStatusParser (standalone)
// ============================================================================

TEST_CASE("DeliveryStatus: per-message group then per-recipient groups", "[mime][dsn]") {
    std::string_view body = "Reporting-MTA: dns; mail.example.com\n"
                            "Arrival-Date: Thu, 19 Jul 2026 10:00:00 -0400\n"
                            "\n"
                            "Final-Recipient: rfc822; user1@example.org\n"
                            "Action: failed\n"
                            "Status: 5.1.1\n"
                            "\n"
                            "Final-Recipient: rfc822; user2@example.org\n"
                            "Action: delayed\n"
                            "Status: 4.4.7\n";

    auto ref = DeliveryStatusParser::parse(body);

    REQUIRE(value_of(ref.message_fields, "Reporting-MTA") == "dns; mail.example.com");
    REQUIRE(value_of(ref.message_fields, "Arrival-Date") == "Thu, 19 Jul 2026 10:00:00 -0400");

    REQUIRE(ref.recipient_fields.size() == 2);
    REQUIRE(value_of(ref.recipient_fields[0], "Final-Recipient") == "rfc822; user1@example.org");
    REQUIRE(value_of(ref.recipient_fields[0], "Action") == "failed");
    REQUIRE(value_of(ref.recipient_fields[0], "Status") == "5.1.1");
    REQUIRE(value_of(ref.recipient_fields[1], "Final-Recipient") == "rfc822; user2@example.org");
    REQUIRE(value_of(ref.recipient_fields[1], "Action") == "delayed");
}

TEST_CASE("DeliveryStatus: folded continuation lines are joined", "[mime][dsn]") {
    std::string_view body = "Reporting-MTA: dns; mail.example.com\n"
                            "\n"
                            "Final-Recipient: rfc822; user@example.org\n"
                            "Diagnostic-Code: smtp; 550 5.1.1 User unknown\n"
                            " (extended details continue here)\n";

    auto ref = DeliveryStatusParser::parse(body);
    REQUIRE(ref.recipient_fields.size() == 1);
    REQUIRE(value_of(ref.recipient_fields[0], "Diagnostic-Code") ==
            "smtp; 550 5.1.1 User unknown (extended details continue here)");
}

// ============================================================================
// Pipeline: a realistic bounce end-to-end
// ============================================================================

TEST_CASE("multipart/report: realistic DSN bounce end-to-end", "[mime][dsn][pipeline]") {
    libglot::Arena arena;
    std::string_view source =
        "From: Mail Delivery Subsystem <MAILER-DAEMON@example.com>\n"
        "To: sender@example.com\n"
        "Subject: Undeliverable mail\n"
        "MIME-Version: 1.0\n"
        "Content-Type: multipart/report; report-type=delivery-status; boundary=\"RAA14128\"\n"
        "\n"
        "--RAA14128\n"
        "Content-Type: text/plain; charset=us-ascii\n"
        "\n"
        "This is an automatically generated delivery status notification.\n"
        "--RAA14128\n"
        "Content-Type: message/delivery-status\n"
        "\n"
        "Reporting-MTA: dns; mail.example.com\n"
        "Arrival-Date: Thu, 19 Jul 2026 10:00:00 -0400\n"
        "\n"
        "Final-Recipient: rfc822; user@example.org\n"
        "Action: failed\n"
        "Status: 5.1.1\n"
        "Diagnostic-Code: smtp; 550 5.1.1 User unknown\n"
        "--RAA14128\n"
        "Content-Type: message/rfc822\n"
        "\n"
        "From: sender@example.com\n"
        "To: user@example.org\n"
        "Subject: Original message\n"
        "\n"
        "Original body.\n"
        "--RAA14128--\n";

    auto result = parse_message(arena, source);
    Message* msg = result.message;

    REQUIRE(msg != nullptr);
    REQUIRE(!result.rejected);
    REQUIRE(!result.has_anomaly(AnomalyKind::MissingReportTypeParameter));
    REQUIRE(msg->parts.size() == 3);

    Message* human_readable = msg->parts[0];
    REQUIRE(human_readable->delivery_status == nullptr);

    Message* dsn_part = msg->parts[1];
    REQUIRE(dsn_part->delivery_status != nullptr);
    REQUIRE(value_of(dsn_part->delivery_status->message_fields, "Reporting-MTA") ==
            "dns; mail.example.com");
    REQUIRE(dsn_part->delivery_status->recipient_fields.size() == 1);
    REQUIRE(value_of(dsn_part->delivery_status->recipient_fields[0], "Final-Recipient") ==
            "rfc822; user@example.org");
    REQUIRE(value_of(dsn_part->delivery_status->recipient_fields[0], "Action") == "failed");

    Message* original_part = msg->parts[2];
    REQUIRE(original_part->encapsulated != nullptr);
    const Header* subject = find_header(*original_part->encapsulated, "Subject");
    REQUIRE(subject != nullptr);
    REQUIRE(subject->value == "Original message");
}

TEST_CASE("multipart/report: missing report-type parameter is flagged",
          "[mime][dsn][pipeline]") {
    libglot::Arena arena;
    std::string_view source = "Content-Type: multipart/report; boundary=\"b\"\n"
                              "\n"
                              "--b\n"
                              "Content-Type: text/plain\n"
                              "\n"
                              "body\n"
                              "--b--\n";

    auto result = parse_message(arena, source);
    REQUIRE(result.message != nullptr);
    REQUIRE(result.has_anomaly(AnomalyKind::MissingReportTypeParameter));
}

TEST_CASE("multipart/report: absent for ordinary multipart/mixed", "[mime][dsn][pipeline]") {
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
    REQUIRE(!result.has_anomaly(AnomalyKind::MissingReportTypeParameter));
}
