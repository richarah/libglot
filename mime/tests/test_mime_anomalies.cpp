/// ============================================================================
/// MIME Anomaly Detection Tests
/// ============================================================================
///
/// Exercises the anomaly plumbing of the single parse_message() pipeline
/// (mime.h / parser_extended.h):
/// - duplicate Content-Type headers
/// - missing final multipart boundary
/// - invalid RFC 2231 percent-encoding
/// - multipart nesting depth exceeded
/// ============================================================================

#include <catch2/catch_test_macros.hpp>
#include "../include/libglot/mime/mime.h"
#include "../../core/include/libglot/util/arena.h"

using namespace libglot::mime;

TEST_CASE("Anomalies: Duplicate Content-Type header is reported", "[mime][anomalies]") {
    libglot::Arena arena;
    std::string_view source =
        "Content-Type: text/plain\n"
        "Content-Type: text/html\n"
        "Subject: duplicate headers\n"
        "\n"
        "Body\n";

    auto result = parse_message(arena, source);

    REQUIRE(result.message != nullptr);
    REQUIRE(result.message->headers.size() == 3);

    REQUIRE(result.has_anomaly(AnomalyKind::DuplicateContentType));

    // DuplicateContentType has Security severity
    REQUIRE(result.report.has_critical_anomalies());
    REQUIRE(result.report.count_at_severity(AnomalySeverity::Security) >= 1);
}

TEST_CASE("Anomalies: Clean message reports no critical anomalies", "[mime][anomalies]") {
    libglot::Arena arena;
    std::string_view source =
        "Content-Type: text/plain; charset=utf-8\n"
        "Subject: all good\n"
        "\n"
        "Body\n";

    auto result = parse_message(arena, source);

    REQUIRE(result.message != nullptr);
    REQUIRE(!result.report.has_critical_anomalies());
    REQUIRE(!result.rejected);
    REQUIRE(!result.has_anomaly(AnomalyKind::DuplicateContentType));
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

    auto result = parse_message(arena, source);

    REQUIRE(result.message != nullptr);
    REQUIRE(result.message->parts.size() == 2);
    REQUIRE(result.has_anomaly(AnomalyKind::MissingFinalBoundary));
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

    auto result = parse_message(arena, source);

    REQUIRE(result.message != nullptr);
    REQUIRE(result.message->parts.size() == 1);
    REQUIRE(!result.has_anomaly(AnomalyKind::MissingFinalBoundary));
}

TEST_CASE("Anomalies: Missing boundary parameter is reported", "[mime][anomalies]") {
    libglot::Arena arena;
    std::string_view source =
        "Content-Type: multipart/mixed\n"
        "\n"
        "Body without any boundary\n";

    auto result = parse_message(arena, source);

    REQUIRE(result.message != nullptr);
    REQUIRE(result.has_anomaly(AnomalyKind::MissingBoundaryParameter));
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
    ParseOptions options;
    options.limits.max_nesting_depth = 5;

    auto result = parse_message(arena, content, options);

    REQUIRE(result.message != nullptr);
    REQUIRE(result.has_anomaly(AnomalyKind::ExcessiveNestingDepth));

    // DoS severity counts as critical
    REQUIRE(result.report.has_critical_anomalies());
}

TEST_CASE("Anomalies: Invalid RFC 2231 percent-encoding is reported by the pipeline", "[mime][anomalies][rfc2231]") {
    libglot::Arena arena;
    std::string_view source =
        "Content-Type: application/pdf; filename*0*=\"utf-8''bad%ZZname.pdf\"\n"
        "\n"
        "Body\n";

    ParseResult result;
    REQUIRE_NOTHROW(result = parse_message(arena, source));

    REQUIRE(result.message != nullptr);
    REQUIRE(result.has_anomaly(AnomalyKind::InvalidParameterSyntax));
}

TEST_CASE("Anomalies: Severity lookup is exposed via AnomalyConfig", "[mime][anomalies]") {
    // Regression check for the previous compile error: get_severity is a
    // static member of AnomalyConfig and must be called qualified.
    REQUIRE(AnomalyConfig::get_severity(AnomalyKind::DuplicateContentType) == AnomalySeverity::Security);
    REQUIRE(AnomalyConfig::get_severity(AnomalyKind::ExcessiveNestingDepth) == AnomalySeverity::DoS);
    REQUIRE(AnomalyConfig::get_severity(AnomalyKind::MissingFinalBoundary) == AnomalySeverity::Structural);
}
