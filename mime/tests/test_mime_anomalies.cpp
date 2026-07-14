/// ============================================================================
/// MIME Anomaly Detection Tests
/// ============================================================================
///
/// Exercises MimeParserWithAnomalies (parser_with_anomalies.h) and the
/// anomaly plumbing shared with MimeParserExtended:
/// - duplicate Content-Type headers
/// - missing final multipart boundary
/// - invalid RFC 2231 percent-encoding
/// - multipart nesting depth exceeded
/// ============================================================================

#include <catch2/catch_test_macros.hpp>
#include "../include/libglot/mime/parser_with_anomalies.h"
#include "../include/libglot/mime/complete_features.h"
#include "../../core/include/libglot/util/arena.h"

using namespace libglot::mime;

namespace {

bool has_anomaly(const AnomalyReport& report, AnomalyKind kind) {
    for (const auto& rec : report.records) {
        if (rec.kind == kind) {
            return true;
        }
    }
    return false;
}

} // namespace

TEST_CASE("Anomalies: Duplicate Content-Type header is reported", "[mime][anomalies]") {
    libglot::Arena arena;
    std::string_view source =
        "Content-Type: text/plain\n"
        "Content-Type: text/html\n"
        "Subject: duplicate headers\n"
        "\n"
        "Body\n";

    MimeParserWithAnomalies parser(arena, source);
    auto* msg = parser.parse_with_anomaly_detection();

    REQUIRE(msg != nullptr);
    REQUIRE(msg->headers.size() == 3);

    const auto& report = parser.anomaly_report();
    REQUIRE(has_anomaly(report, AnomalyKind::DuplicateContentType));

    // DuplicateContentType has Security severity
    REQUIRE(report.has_critical_anomalies());
    REQUIRE(report.count_at_severity(AnomalySeverity::Security) >= 1);
}

TEST_CASE("Anomalies: Clean message reports no critical anomalies", "[mime][anomalies]") {
    libglot::Arena arena;
    std::string_view source =
        "Content-Type: text/plain; charset=utf-8\n"
        "Subject: all good\n"
        "\n"
        "Body\n";

    MimeParserWithAnomalies parser(arena, source);
    auto* msg = parser.parse_with_anomaly_detection();

    REQUIRE(msg != nullptr);
    REQUIRE(!parser.anomaly_report().has_critical_anomalies());
    REQUIRE(!has_anomaly(parser.anomaly_report(), AnomalyKind::DuplicateContentType));
}

TEST_CASE("Anomalies: Missing final boundary is reported from the parse path", "[mime][anomalies]") {
    libglot::Arena arena;
    std::string_view source =
        "MIME-Version: 1.0\n"
        "Content-Type: multipart/mixed; boundary=frag\n"
        "\n"
        "--frag\n"
        "Content-Type: text/plain\n"
        "\n"
        "part one\n"
        "--frag\n"
        "Content-Type: text/plain\n"
        "\n"
        "truncated message, no close delimiter\n";

    MimeParserWithAnomalies parser(arena, source);
    auto* msg = parser.parse_with_anomaly_detection();

    REQUIRE(msg != nullptr);
    REQUIRE(msg->parts.size() == 2);
    REQUIRE(has_anomaly(parser.anomaly_report(), AnomalyKind::MissingFinalBoundary));
}

TEST_CASE("Anomalies: Properly terminated multipart has no boundary anomaly", "[mime][anomalies]") {
    libglot::Arena arena;
    std::string_view source =
        "MIME-Version: 1.0\n"
        "Content-Type: multipart/mixed; boundary=ok\n"
        "\n"
        "--ok\n"
        "\n"
        "part\n"
        "--ok--\n";

    MimeParserWithAnomalies parser(arena, source);
    auto* msg = parser.parse_with_anomaly_detection();

    REQUIRE(msg != nullptr);
    REQUIRE(msg->parts.size() == 1);
    REQUIRE(!has_anomaly(parser.anomaly_report(), AnomalyKind::MissingFinalBoundary));
}

TEST_CASE("Anomalies: Missing boundary parameter is reported", "[mime][anomalies]") {
    libglot::Arena arena;
    std::string_view source =
        "Content-Type: multipart/mixed\n"
        "\n"
        "Body without any boundary\n";

    MimeParserWithAnomalies parser(arena, source);
    auto* msg = parser.parse_with_anomaly_detection();

    REQUIRE(msg != nullptr);
    REQUIRE(has_anomaly(parser.anomaly_report(), AnomalyKind::MissingBoundaryParameter));
}

TEST_CASE("Anomalies: Nesting depth exceeded is reported from the parse path", "[mime][anomalies][limits]") {
    // Build a multipart message nested 20 levels deep, then cap depth at 5
    std::string content = "Content-Type: text/plain\n\nleaf";
    for (int level = 20; level >= 1; --level) {
        std::string b = "n" + std::to_string(level);
        content = "Content-Type: multipart/mixed; boundary=" + b + "\n\n"
                  "--" + b + "\n" + content + "\n--" + b + "--\n";
    }

    libglot::Arena arena;
    ParserLimits limits = ParserLimits::standard();
    limits.max_nesting_depth = 5;

    MimeParserWithAnomalies parser(arena, content, AnomalyConfig::standard(), limits);
    auto* msg = parser.parse_with_anomaly_detection();

    REQUIRE(msg != nullptr);
    REQUIRE(has_anomaly(parser.anomaly_report(), AnomalyKind::ExcessiveNestingDepth));

    // DoS severity counts as critical
    REQUIRE(parser.anomaly_report().has_critical_anomalies());
}

TEST_CASE("Anomalies: Invalid RFC 2231 percent-encoding is reported via CompleteMimeParser", "[mime][anomalies][rfc2231]") {
    libglot::Arena arena;
    std::string_view source =
        "Content-Type: application/pdf; filename*0*=\"utf-8''bad%ZZname.pdf\"\n"
        "\n"
        "Body\n";

    CompleteMimeParser parser(arena, source);
    Message* msg = nullptr;
    REQUIRE_NOTHROW(msg = parser.parse_complete());

    REQUIRE(msg != nullptr);
    REQUIRE(has_anomaly(parser.anomalies(), AnomalyKind::InvalidParameterSyntax));
}

TEST_CASE("Anomalies: Severity lookup is exposed via AnomalyConfig", "[mime][anomalies]") {
    // Regression check for the previous compile error: get_severity is a
    // static member of AnomalyConfig and must be called qualified.
    REQUIRE(AnomalyConfig::get_severity(AnomalyKind::DuplicateContentType) == AnomalySeverity::Security);
    REQUIRE(AnomalyConfig::get_severity(AnomalyKind::ExcessiveNestingDepth) == AnomalySeverity::DoS);
    REQUIRE(AnomalyConfig::get_severity(AnomalyKind::MissingFinalBoundary) == AnomalySeverity::Structural);
}
