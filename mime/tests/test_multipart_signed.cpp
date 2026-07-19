/// ============================================================================
/// multipart/signed / multipart/encrypted (RFC 1847) Tests
/// ============================================================================
///
/// RFC 1847 requires the "protocol" parameter on both multipart/signed and
/// multipart/encrypted (checked in MimeParserExtended::finish_message,
/// recording AnomalyKind::MissingProtocolParameter when absent).
///
/// CRITICAL SUBTLETY (per the task): signature verification needs the
/// signed part's bytes EXACTLY as transmitted -- not re-unfolded,
/// re-encoded, or otherwise normalized. Message::raw_source (populated in
/// MimeParserExtended::parse_part) gives byte-exact access to a part's
/// headers+body exactly as they appeared between boundary delimiters. These
/// tests prove that raw_source preserves a folded header line and trailing
/// whitespace verbatim, even though the parsed Header::value for that same
/// header has been unfolded/normalized as usual.
///
/// libglot does NOT verify signatures (no crypto dependency -- explicitly
/// out of scope, see docs/FEATURE_MATRIX.md); it only guarantees the bytes
/// a verifier would need are never corrupted.
/// ============================================================================

#include "../../core/include/libglot/util/arena.h"
#include "../include/libglot/mime/mime.h"
#include <catch2/catch_test_macros.hpp>

#include <string>

using namespace libglot::mime;

TEST_CASE("multipart/signed: protocol parameter present records no anomaly",
          "[mime][signed]") {
    libglot::Arena arena;
    std::string_view source =
        "Content-Type: multipart/signed; protocol=\"application/pgp-signature\"; "
        "micalg=pgp-sha256; boundary=\"sig\"\n"
        "\n"
        "--sig\n"
        "Content-Type: text/plain\n"
        "\n"
        "signed content\n"
        "--sig\n"
        "Content-Type: application/pgp-signature\n"
        "\n"
        "fake-signature\n"
        "--sig--\n";

    auto result = parse_message(arena, source);
    REQUIRE(result.message != nullptr);
    REQUIRE(!result.has_anomaly(AnomalyKind::MissingProtocolParameter));
    REQUIRE(result.message->parts.size() == 2);
}

TEST_CASE("multipart/signed: first part's raw_source is byte-exact, "
          "even though the parsed header value is normalized",
          "[mime][signed][byte-exact]") {
    libglot::Arena arena;

    // The signed part's Content-Type is deliberately folded across two
    // lines, and its body has trailing whitespace before the boundary --
    // both must survive in raw_source exactly as transmitted.
    std::string source =
        "Content-Type: multipart/signed; protocol=\"application/pgp-signature\"; "
        "boundary=\"sig-boundary\"\n"
        "\n"
        "--sig-boundary\n"
        "Content-Type: text/plain;\n"
        " charset=utf-8\n"
        "\n"
        "This   is  the exact   signed content.  \n"
        "--sig-boundary\n"
        "Content-Type: application/pgp-signature\n"
        "\n"
        "-----BEGIN PGP SIGNATURE-----\n"
        "fake-signature-data\n"
        "-----END PGP SIGNATURE-----\n"
        "--sig-boundary--\n";

    auto result = parse_message(arena, source);
    Message* msg = result.message;

    REQUIRE(msg != nullptr);
    REQUIRE(!result.rejected);
    REQUIRE(msg->parts.size() == 2);

    Message* signed_part = msg->parts[0];

    // The parsed Content-Type header value IS unfolded/normalized, as usual.
    const Header* ct = find_header(*signed_part, "Content-Type");
    REQUIRE(ct != nullptr);
    REQUIRE(ct->value == "text/plain; charset=utf-8");

    // But raw_source retains the exact transmitted bytes: the fold is still
    // a literal line break + space, and the trailing whitespace before the
    // boundary is preserved. This is the byte-exact view a signature would
    // have been computed over.
    std::string expected_raw = "Content-Type: text/plain;\n"
                               " charset=utf-8\n"
                               "\n"
                               "This   is  the exact   signed content.  ";
    REQUIRE(signed_part->raw_source == expected_raw);

    // The second part is the detached signature itself.
    Message* signature_part = msg->parts[1];
    const Header* sig_ct = find_header(*signature_part, "Content-Type");
    REQUIRE(sig_ct != nullptr);
    REQUIRE(sig_ct->value == "application/pgp-signature");
}

TEST_CASE("multipart/signed: missing protocol parameter is flagged",
          "[mime][signed]") {
    libglot::Arena arena;
    std::string_view source = "Content-Type: multipart/signed; boundary=\"sig\"\n"
                              "\n"
                              "--sig\n"
                              "Content-Type: text/plain\n"
                              "\n"
                              "content\n"
                              "--sig\n"
                              "Content-Type: application/pgp-signature\n"
                              "\n"
                              "sig\n"
                              "--sig--\n";

    auto result = parse_message(arena, source);
    REQUIRE(result.message != nullptr);
    REQUIRE(result.has_anomaly(AnomalyKind::MissingProtocolParameter));
}

TEST_CASE("multipart/encrypted: missing protocol parameter is flagged",
          "[mime][signed]") {
    libglot::Arena arena;
    std::string_view source = "Content-Type: multipart/encrypted; boundary=\"enc\"\n"
                              "\n"
                              "--enc\n"
                              "Content-Type: application/pgp-encrypted\n"
                              "\n"
                              "Version: 1\n"
                              "--enc\n"
                              "Content-Type: application/octet-stream\n"
                              "\n"
                              "ciphertext\n"
                              "--enc--\n";

    auto result = parse_message(arena, source);
    REQUIRE(result.message != nullptr);
    REQUIRE(result.has_anomaly(AnomalyKind::MissingProtocolParameter));
}

TEST_CASE("multipart/encrypted: protocol parameter present records no anomaly",
          "[mime][signed]") {
    libglot::Arena arena;
    std::string_view source =
        "Content-Type: multipart/encrypted; protocol=\"application/pgp-encrypted\"; "
        "boundary=\"enc\"\n"
        "\n"
        "--enc\n"
        "Content-Type: application/pgp-encrypted\n"
        "\n"
        "Version: 1\n"
        "--enc\n"
        "Content-Type: application/octet-stream\n"
        "\n"
        "ciphertext\n"
        "--enc--\n";

    auto result = parse_message(arena, source);
    REQUIRE(result.message != nullptr);
    REQUIRE(!result.has_anomaly(AnomalyKind::MissingProtocolParameter));
}

TEST_CASE("raw_source is empty for the top-level message, populated for parts",
          "[mime][signed][byte-exact]") {
    libglot::Arena arena;
    std::string_view source = "Content-Type: multipart/mixed; boundary=\"b\"\n"
                              "\n"
                              "--b\n"
                              "Content-Type: text/plain\n"
                              "\n"
                              "hello\n"
                              "--b--\n";

    auto result = parse_message(arena, source);
    REQUIRE(result.message != nullptr);
    REQUIRE(result.message->raw_source.empty());
    REQUIRE(result.message->parts.size() == 1);
    REQUIRE(!result.message->parts[0]->raw_source.empty());
}
