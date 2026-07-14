/// ============================================================================
/// MIME Pipeline End-to-End Tests
/// ============================================================================
///
/// Exercises the single entry point parse_message() (mime.h) across the
/// whole pipeline: header unfolding, RFC 5322 comment stripping, address
/// groups, RFC 2231 continued parameters, multipart splitting, Content-Type
/// validation, limits, anomaly policies (Ignore/Repair/Reject), and
/// decoded-to-UTF-8 body retrieval.
/// ============================================================================

#include <catch2/catch_test_macros.hpp>
#include "../include/libglot/mime/mime.h"
#include "../../core/include/libglot/util/arena.h"

#include <algorithm>
#include <string>

using namespace libglot::mime;

namespace {

/// Depth of the parsed multipart tree (0 = no parts)
int multipart_depth(const Message* msg) {
    int deepest = 0;
    for (const auto* part : msg->parts) {
        deepest = std::max(deepest, multipart_depth(part));
    }
    return msg->parts.empty() ? 0 : deepest + 1;
}

const AnomalyRecord* find_record(const AnomalyReport& report, AnomalyKind kind) {
    for (const auto& rec : report.records) {
        if (rec.kind == kind) {
            return &rec;
        }
    }
    return nullptr;
}

std::string_view parameter(const Header* header, std::string_view name) {
    for (const auto& param : header->parameters) {
        if (param.first == name) {
            return param.second;
        }
    }
    return "<missing>";
}

} // namespace

TEST_CASE("Pipeline: realistic multipart email through the one entry point", "[mime][pipeline]") {
    libglot::Arena arena;
    std::string_view source =
        "MIME-Version: 1.0\n"
        "From: Alice Example (Founder) <alice@example.com>\n"
        "To: Team: bob@example.com, carol@example.com;\n"
        "Subject: Quarterly\n"
        " report attached\n"
        "Content-Type: multipart/mixed; boundary=\"mix\"\n"
        "\n"
        "This preamble is discarded.\n"
        "--mix\n"
        "Content-Type: text/plain; charset=ISO-8859-1\n"
        "Content-Transfer-Encoding: quoted-printable\n"
        "\n"
        "Caf=E9 r=E9sum=E9\n"
        "--mix\n"
        "Content-Type: application/octet-stream\n"
        "Content-Disposition: attachment;\n"
        " filename*0*=\"utf-8''very%20long%20\";\n"
        " filename*1=\"report file.pdf\"\n"
        "Content-Transfer-Encoding: base64\n"
        "\n"
        "SGVsbG8gV29ybGQ=\n"
        "--mix--\n"
        "Epilogue is discarded.\n";

    auto result = parse_message(arena, source);
    Message* msg = result.message;

    REQUIRE(msg != nullptr);
    REQUIRE(!result.rejected);
    // A clean, well-formed message records no anomalies at all
    REQUIRE(result.report.empty());

    // ---- Top-level headers ----
    REQUIRE(msg->headers.size() == 5);

    // Folded Subject header is unfolded
    const Header* subject = find_header(*msg, "Subject");
    REQUIRE(subject != nullptr);
    REQUIRE(subject->value == "Quarterly report attached");

    // RFC 5322 comment "(Founder)" is stripped from the structured From field
    const Header* from = find_header(*msg, "From");
    REQUIRE(from != nullptr);
    REQUIRE(from->value == "Alice Example  <alice@example.com>");

    // RFC 5322 address group syntax is parsed on the To header
    const Header* to = find_header(*msg, "To");
    REQUIRE(to != nullptr);
    REQUIRE(to->address_groups != nullptr);
    REQUIRE(to->address_groups->size() == 1);
    REQUIRE((*to->address_groups)[0].group_name == "Team");
    REQUIRE((*to->address_groups)[0].addresses.size() == 2);
    REQUIRE((*to->address_groups)[0].addresses[0] == "bob@example.com");
    REQUIRE((*to->address_groups)[0].addresses[1] == "carol@example.com");

    const Header* content_type = find_header(*msg, "Content-Type");
    REQUIRE(content_type != nullptr);
    REQUIRE(content_type->value == "multipart/mixed; boundary=\"mix\"");
    REQUIRE(parameter(content_type, "boundary") == "mix");

    // ---- Multipart structure ----
    REQUIRE(msg->parts.size() == 2);

    // Part 1: ISO-8859-1 quoted-printable text, decoded to UTF-8
    Message* text_part = msg->parts[0];
    const Header* text_ct = find_header(*text_part, "Content-Type");
    REQUIRE(text_ct != nullptr);
    REQUIRE(text_ct->value == "text/plain; charset=ISO-8859-1");
    REQUIRE(text_part->body == "Caf=E9 r=E9sum=E9");

    auto text_utf8 = decoded_body_utf8(*text_part);
    REQUIRE(text_utf8.has_value());
    REQUIRE(*text_utf8 == "Caf\xC3\xA9 r\xC3\xA9sum\xC3\xA9");  // "Café résumé"

    // Part 2: base64 attachment with an RFC 2231 continued filename
    Message* attachment = msg->parts[1];
    const Header* disposition = find_header(*attachment, "Content-Disposition");
    REQUIRE(disposition != nullptr);
    // The continuation fragments are reassembled and percent-decoded into a
    // single "filename" parameter
    REQUIRE(parameter(disposition, "filename") == "very long report file.pdf");

    auto attachment_bytes = decoded_body(*attachment);
    REQUIRE(attachment_bytes.has_value());
    REQUIRE(*attachment_bytes == "Hello World");
}

TEST_CASE("Pipeline: hostile message hits limits and is rejected", "[mime][pipeline][limits]") {
    // Deep nesting + invalid Content-Type + missing final boundary
    std::string nested = "Content-Type: text/plain\n\nleaf";
    for (int level = 30; level >= 1; --level) {
        std::string b = "n" + std::to_string(level);
        nested = "Content-Type: multipart/mixed; boundary=" + b + "\n\n"
                 "--" + b + "\n" + nested + "\n--" + b + "--\n";
    }

    std::string source =
        "MIME-Version: 1.0\n"
        "Content-Type: multipart/mixed; boundary=outer\n"
        "\n"
        "--outer\n"
        "Content-Type: br[oken/type\n"
        "\n"
        "part with syntactically invalid content type\n"
        "--outer\n"
        + nested;  // no "--outer--" close delimiter

    libglot::Arena arena;
    ParseOptions options;
    options.limits.max_nesting_depth = 5;

    auto result = parse_message(arena, source, options);
    Message* msg = result.message;

    REQUIRE(msg != nullptr);
    REQUIRE(msg->parts.size() == 2);

    // Nesting stopped at the limit, and the tree reflects that
    REQUIRE(result.has_anomaly(AnomalyKind::ExcessiveNestingDepth));
    REQUIRE(multipart_depth(msg) == 5);

    // Invalid Content-Type syntax and the missing close delimiter are both
    // recorded (Structural severity: parse repaired and continued)
    REQUIRE(result.has_anomaly(AnomalyKind::InvalidMediaType));
    REQUIRE(result.has_anomaly(AnomalyKind::MissingFinalBoundary));

    // The DoS-severity limit anomaly carries Reject policy under the
    // standard config and marks the whole parse rejected
    const AnomalyRecord* depth_record =
        find_record(result.report, AnomalyKind::ExcessiveNestingDepth);
    REQUIRE(depth_record != nullptr);
    REQUIRE(depth_record->severity == AnomalySeverity::DoS);
    REQUIRE(depth_record->applied_policy == AnomalyPolicy::Reject);
    REQUIRE(result.rejected);
}

TEST_CASE("Pipeline: anomaly policies Ignore/Repair/Reject are honored", "[mime][pipeline][anomalies]") {
    std::string_view source =
        "Content-Type: text/plain; charset=utf-8\n"
        "Content-Type: text/html; charset=utf-8\n"
        "\n"
        "Body\n";

    libglot::Arena arena;

    SECTION("strict config: Reject policy actually rejects") {
        ParseOptions options;
        options.anomalies = AnomalyConfig::strict();

        auto result = parse_message(arena, source, options);

        REQUIRE(result.message != nullptr);
        const AnomalyRecord* rec = find_record(result.report, AnomalyKind::DuplicateContentType);
        REQUIRE(rec != nullptr);
        REQUIRE(rec->applied_policy == AnomalyPolicy::Reject);
        REQUIRE(result.rejected);
    }

    SECTION("permissive config: Repair policy records but does not reject") {
        ParseOptions options;
        options.anomalies = AnomalyConfig::permissive();

        auto result = parse_message(arena, source, options);

        REQUIRE(result.message != nullptr);
        const AnomalyRecord* rec = find_record(result.report, AnomalyKind::DuplicateContentType);
        REQUIRE(rec != nullptr);
        REQUIRE(rec->applied_policy == AnomalyPolicy::Repair);
        REQUIRE(!result.rejected);
    }

    SECTION("Ignore policy drops the anomaly entirely") {
        ParseOptions options;
        options.anomalies = AnomalyConfig::standard();
        options.anomalies.set_policy(AnomalyKind::DuplicateContentType, AnomalyPolicy::Ignore);

        auto result = parse_message(arena, source, options);

        REQUIRE(result.message != nullptr);
        REQUIRE(!result.has_anomaly(AnomalyKind::DuplicateContentType));
        REQUIRE(!result.rejected);
    }
}

TEST_CASE("Pipeline: message/external-body reference is parsed", "[mime][pipeline][external-body]") {
    libglot::Arena arena;
    std::string_view source =
        "Content-Type: message/external-body; access-type=ftp; "
        "name=\"data.bin\"; site=ftp.example.com; size=1024\n"
        "\n"
        "phantom body\n";

    auto result = parse_message(arena, source);
    Message* msg = result.message;

    REQUIRE(msg != nullptr);
    REQUIRE(msg->external_body != nullptr);
    REQUIRE(msg->external_body->access_type == "ftp");
    REQUIRE(msg->external_body->name == "data.bin");
    REQUIRE(msg->external_body->site == "ftp.example.com");
    REQUIRE(msg->external_body->size == 1024);
}

TEST_CASE("Pipeline: decoded body helpers flag undecodable content", "[mime][pipeline][charset]") {
    libglot::Arena arena;

    SECTION("unknown charset yields no UTF-8 text") {
        std::string_view source =
            "Content-Type: text/plain; charset=KOI8-R\n"
            "\n"
            "some bytes\n";

        auto result = parse_message(arena, source);
        REQUIRE(result.message != nullptr);
        REQUIRE(!decoded_body_utf8(*result.message).has_value());
    }

    SECTION("invalid base64 payload yields no bytes") {
        std::string_view source =
            "Content-Type: application/octet-stream\n"
            "Content-Transfer-Encoding: base64\n"
            "\n"
            "!!!not-base64!!!\n";

        auto result = parse_message(arena, source);
        REQUIRE(result.message != nullptr);
        REQUIRE(!decoded_body(*result.message).has_value());
    }
}
