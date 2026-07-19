/// ============================================================================
/// RFC 6532 Internationalized (Raw UTF-8) Header Tests
/// ============================================================================
///
/// Exercises the UTF-8 validation now applied to every header value in
/// MimeParserExtended::enhance_header (parser_extended.h): RFC 6532 permits
/// raw UTF-8 bytes directly in header values (not just RFC 2047
/// encoded-words). Bytes >= 0x80 are legal and pass through completely
/// unmodified either way; only bytes that fail CharsetConverter::is_valid_utf8
/// are flagged, via AnomalyKind::InvalidUtf8Header, and even then the raw
/// bytes are preserved verbatim -- never corrupted. Valid raw UTF-8 in a
/// field RFC 5322 treats as unstructured free text (Subject, or any field
/// with no defined grammar) is separately, informationally flagged via
/// AnomalyKind::NonAsciiInUnstructuredHeader -- the two anomalies are
/// mutually exclusive per header value.
/// ============================================================================

#include "../../core/include/libglot/util/arena.h"
#include "../include/libglot/mime/mime.h"
#include <catch2/catch_test_macros.hpp>

using namespace libglot::mime;

TEST_CASE("UTF-8 headers: valid raw UTF-8 subject survives intact, no anomaly",
          "[mime][utf8]") {
    libglot::Arena arena;
    // "Caf\xC3\xA9 R\xC3\xA9sum\xC3\xA9" == "Café Résumé" as raw UTF-8 bytes.
    std::string_view source = "Subject: Caf\xC3\xA9 R\xC3\xA9sum\xC3\xA9\n\nbody\n";

    auto result = parse_message(arena, source);
    REQUIRE(result.message != nullptr);
    REQUIRE(!result.rejected);
    REQUIRE(!result.has_anomaly(AnomalyKind::InvalidUtf8Header));

    const Header* subject = find_header(*result.message, "Subject");
    REQUIRE(subject != nullptr);
    REQUIRE(subject->value == "Caf\xC3\xA9 R\xC3\xA9sum\xC3\xA9");
}

TEST_CASE("UTF-8 headers: valid raw UTF-8 display name in From, no anomaly",
          "[mime][utf8]") {
    libglot::Arena arena;
    // "Jos\xC3\xA9 Garc\xC3\xAD" "a" == "José García" as raw UTF-8 bytes.
    std::string_view source =
        "From: Jos\xC3\xA9 Garc\xC3\xAD"
        "a <jose@example.com>\n\nbody\n";

    auto result = parse_message(arena, source);
    REQUIRE(result.message != nullptr);
    REQUIRE(!result.has_anomaly(AnomalyKind::InvalidUtf8Header));

    const Header* from = find_header(*result.message, "From");
    REQUIRE(from != nullptr);
    REQUIRE(from->value == "Jos\xC3\xA9 Garc\xC3\xAD"
                           "a <jose@example.com>");
}

TEST_CASE("UTF-8 headers: ASCII-only headers are unaffected", "[mime][utf8]") {
    libglot::Arena arena;
    std::string_view source = "Subject: Plain ASCII subject\n\nbody\n";

    auto result = parse_message(arena, source);
    REQUIRE(result.message != nullptr);
    REQUIRE(!result.has_anomaly(AnomalyKind::InvalidUtf8Header));
}

TEST_CASE("UTF-8 headers: invalid UTF-8 bytes are flagged, not corrupted",
          "[mime][utf8]") {
    libglot::Arena arena;
    // 0xC3 is a valid 2-byte lead but 0x20 (space) is not a valid
    // continuation byte (must be 0x80-0xBF): this is ill-formed UTF-8.
    std::string_view source = "Subject: Bad \xC3 sequence\n\nbody\n";

    auto result = parse_message(arena, source);
    REQUIRE(result.message != nullptr);
    REQUIRE(result.has_anomaly(AnomalyKind::InvalidUtf8Header));

    // The raw bytes are preserved unchanged -- never corrupted or replaced.
    const Header* subject = find_header(*result.message, "Subject");
    REQUIRE(subject != nullptr);
    REQUIRE(subject->value == "Bad \xC3 sequence");
}

TEST_CASE("UTF-8 headers: stray continuation byte is flagged", "[mime][utf8]") {
    libglot::Arena arena;
    // 0x80 alone, with no preceding lead byte, is a stray continuation byte.
    std::string_view source = "Subject: stray \x80 byte\n\nbody\n";

    auto result = parse_message(arena, source);
    REQUIRE(result.message != nullptr);
    REQUIRE(result.has_anomaly(AnomalyKind::InvalidUtf8Header));
}

TEST_CASE("UTF-8 headers: severity is Security under the standard config",
          "[mime][utf8]") {
    REQUIRE(AnomalyConfig::get_severity(AnomalyKind::InvalidUtf8Header) ==
            AnomalySeverity::Security);
}

// ============================================================================
// NonAsciiInUnstructuredHeader: valid raw UTF-8 (legal under RFC 6532) in a
// field RFC 5322 itself treats as unstructured free text -- informational
// only, distinct from InvalidUtf8Header (Security severity, genuinely
// invalid bytes).
// ============================================================================

TEST_CASE("UTF-8 headers: valid raw UTF-8 in Subject (unstructured) is flagged "
          "informationally, not as invalid",
          "[mime][utf8]") {
    libglot::Arena arena;
    std::string_view source = "Subject: Caf\xC3\xA9 R\xC3\xA9sum\xC3\xA9\n\nbody\n";

    auto result = parse_message(arena, source);
    REQUIRE(result.message != nullptr);
    REQUIRE(!result.rejected);
    REQUIRE(!result.has_anomaly(AnomalyKind::InvalidUtf8Header));
    REQUIRE(result.has_anomaly(AnomalyKind::NonAsciiInUnstructuredHeader));
}

TEST_CASE("UTF-8 headers: valid raw UTF-8 in From (structured) is NOT flagged "
          "as NonAsciiInUnstructuredHeader",
          "[mime][utf8]") {
    // From has RFC 5322 grammar (mailbox / addr-spec); it is not the
    // "unstructured" field type this anomaly is scoped to, even though the
    // display-name portion is itself free text carrying the UTF-8 bytes.
    libglot::Arena arena;
    std::string_view source =
        "From: Jos\xC3\xA9 Garc\xC3\xAD"
        "a <jose@example.com>\n\nbody\n";

    auto result = parse_message(arena, source);
    REQUIRE(result.message != nullptr);
    REQUIRE(!result.has_anomaly(AnomalyKind::InvalidUtf8Header));
    REQUIRE(!result.has_anomaly(AnomalyKind::NonAsciiInUnstructuredHeader));
}

TEST_CASE("UTF-8 headers: valid raw UTF-8 in a custom/unrecognized field is "
          "flagged too (no defined grammar is the same 'unstructured' situation)",
          "[mime][utf8]") {
    libglot::Arena arena;
    std::string_view source = "X-Custom-Note: caf\xC3\xA9\n\nbody\n";

    auto result = parse_message(arena, source);
    REQUIRE(result.message != nullptr);
    REQUIRE(result.has_anomaly(AnomalyKind::NonAsciiInUnstructuredHeader));
}

TEST_CASE("UTF-8 headers: ASCII-only Subject carries neither UTF-8 anomaly",
          "[mime][utf8]") {
    libglot::Arena arena;
    std::string_view source = "Subject: Plain ASCII subject\n\nbody\n";

    auto result = parse_message(arena, source);
    REQUIRE(result.message != nullptr);
    REQUIRE(!result.has_anomaly(AnomalyKind::InvalidUtf8Header));
    REQUIRE(!result.has_anomaly(AnomalyKind::NonAsciiInUnstructuredHeader));
}

TEST_CASE("UTF-8 headers: genuinely invalid UTF-8 in Subject is flagged as "
          "InvalidUtf8Header, not NonAsciiInUnstructuredHeader",
          "[mime][utf8]") {
    // The two anomalies are mutually exclusive per header value: invalid
    // bytes are a Security concern regardless of which field carries them.
    libglot::Arena arena;
    std::string_view source = "Subject: Bad \xC3 sequence\n\nbody\n";

    auto result = parse_message(arena, source);
    REQUIRE(result.message != nullptr);
    REQUIRE(result.has_anomaly(AnomalyKind::InvalidUtf8Header));
    REQUIRE(!result.has_anomaly(AnomalyKind::NonAsciiInUnstructuredHeader));
}

TEST_CASE("UTF-8 headers: NonAsciiInUnstructuredHeader severity is Degraded "
          "under the standard config",
          "[mime][utf8]") {
    REQUIRE(AnomalyConfig::get_severity(AnomalyKind::NonAsciiInUnstructuredHeader) ==
            AnomalySeverity::Degraded);
}
