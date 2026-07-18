/// ============================================================================
/// Security / Adversarial-Input Corpus (docs/ROADMAP.md remaining work)
/// ============================================================================
///
/// Distinct from fuzz/fuzz_mime_parser.cpp's randomized mutation fuzzing:
/// these are hand-crafted, understood attack shapes (boundary confusion,
/// null-byte smuggling, header injection via decode, path traversal via
/// filename) with a specific expected defensive outcome each, run
/// deterministically in CI like every other Catch2 test rather than a
/// time-boxed background job.
///
/// Building this corpus found that three AnomalyKind values existed in
/// anomalies.h -- NullByteInHeader, NullInBase64, InvalidFilenameChars --
/// complete with Security-severity classification and display names, but
/// were never actually raised anywhere in the parser: real dead code,
/// presumably intended when the anomaly enum was designed and never
/// finished. All three are now implemented (parser_extended.h:
/// enhance_header, check_null_in_base64, and the filename-parameter check
/// alongside RFC 2231 reassembly) and verified to introduce zero false
/// positives over the full 517,401-message Enron corpus and the raw
/// SpamAssassin corpus (which does trip InvalidFilenameChars once, on a
/// genuine MHT-style attachment named with an embedded relative path --
/// see docs/ROADMAP.md). Two more are still dead
/// (`DuplicateFilenameParameter`, and `ParserLimits::max_filename_length`
/// is defined per config tier but never checked against anything) --
/// documented as follow-up, not fixed here.
/// ============================================================================

#include "../../core/include/libglot/util/arena.h"
#include "../include/libglot/mime/mime.h"
#include <catch2/catch_test_macros.hpp>

#include <string>

using namespace libglot::mime;

// ============================================================================
// NullByteInHeader (previously defined, never raised)
// ============================================================================

TEST_CASE("Security: a NUL byte in a header value is flagged", "[mime][security]") {
    // A `const char*` literal containing "\0" truncates there when handed
    // to std::string's constructor -- build the source by concatenation
    // so the embedded NUL survives into the actual test input.
    libglot::Arena arena;
    std::string source = std::string("Subject: hello") + '\0' + "world\n\nbody\n";
    auto result = parse_message(arena, std::string_view(source.data(), source.size()));
    REQUIRE(result.message != nullptr);
    REQUIRE(result.has_anomaly(AnomalyKind::NullByteInHeader));
    // Security severity under the standard config: Reject.
    REQUIRE(result.rejected);
}

TEST_CASE("Security: a clean header carries no NullByteInHeader anomaly", "[mime][security]") {
    libglot::Arena arena;
    auto result = parse_message(arena, "Subject: hello world\n\nbody\n");
    REQUIRE(result.message != nullptr);
    REQUIRE(!result.has_anomaly(AnomalyKind::NullByteInHeader));
    REQUIRE(!result.rejected);
}

// ============================================================================
// NullInBase64 (previously defined, never raised)
// ============================================================================

TEST_CASE("Security: a NUL byte in a base64-declared body is flagged", "[mime][security]") {
    // Valid base64 text never contains a literal NUL (the alphabet is
    // A-Za-z0-9+/=); one present is either corruption or smuggling past a
    // downstream C-string-based consumer.
    libglot::Arena arena;
    std::string source = std::string("Content-Type: application/octet-stream\n"
                                     "Content-Transfer-Encoding: base64\n"
                                     "\n"
                                     "SGVs") +
                         '\0' + "bG8=\n";
    auto result = parse_message(arena, std::string_view(source.data(), source.size()));
    REQUIRE(result.message != nullptr);
    REQUIRE(result.has_anomaly(AnomalyKind::NullInBase64));
    REQUIRE(result.rejected);
}

TEST_CASE("Security: a NUL byte in a non-base64 body is NOT flagged as NullInBase64",
          "[mime][security]") {
    // The check is scoped to bodies actually declared base64; NUL bytes are
    // ordinary (if unusual) content in a binary/8bit body.
    libglot::Arena arena;
    std::string source = "Content-Type: application/octet-stream\n"
                         "Content-Transfer-Encoding: binary\n"
                         "\n"
                         "raw\0bytes\n";
    auto result = parse_message(arena, std::string_view(source.data(), source.size()));
    REQUIRE(result.message != nullptr);
    REQUIRE(!result.has_anomaly(AnomalyKind::NullInBase64));
}

TEST_CASE("Security: clean base64 content carries no NullInBase64 anomaly", "[mime][security]") {
    libglot::Arena arena;
    auto result = parse_message(arena, "Content-Type: application/octet-stream\n"
                                       "Content-Transfer-Encoding: base64\n"
                                       "\n"
                                       "SGVsbG8=\n");
    REQUIRE(result.message != nullptr);
    REQUIRE(!result.has_anomaly(AnomalyKind::NullInBase64));
    REQUIRE(!result.rejected);
}

// ============================================================================
// InvalidFilenameChars (previously defined, never raised)
// ============================================================================

TEST_CASE("Security: a path-traversal filename in Content-Disposition is flagged",
          "[mime][security]") {
    libglot::Arena arena;
    auto result = parse_message(arena,
                                "Content-Type: application/octet-stream\n"
                                "Content-Disposition: attachment; filename=\"../../etc/passwd\"\n"
                                "\n"
                                "data\n");
    REQUIRE(result.message != nullptr);
    REQUIRE(result.has_anomaly(AnomalyKind::InvalidFilenameChars));
    REQUIRE(result.rejected);
}

TEST_CASE("Security: a path-traversal name in Content-Type is also flagged", "[mime][security]") {
    // The real-world case this was found against: an MHT-style export that
    // uses the original local relative path as Content-Type's legacy
    // "name" parameter (docs/ROADMAP.md's SpamAssassin corpus finding).
    libglot::Arena arena;
    auto result =
        parse_message(arena, "Content-Type: image/jpeg; name=\"./MassMail_files/image002.jpg\"\n"
                             "Content-Transfer-Encoding: base64\n"
                             "\n"
                             "/9j/4AAQSkZJRg==\n");
    REQUIRE(result.message != nullptr);
    REQUIRE(result.has_anomaly(AnomalyKind::InvalidFilenameChars));
}

TEST_CASE("Security: a NUL byte in a filename is flagged (truncation past the real "
          "extension check)",
          "[mime][security]") {
    libglot::Arena arena;
    std::string source = std::string("Content-Type: application/octet-stream\n"
                                     "Content-Disposition: attachment; filename=\"safe.pdf") +
                         '\0' + ".exe\"\n\ndata\n";
    auto result = parse_message(arena, std::string_view(source.data(), source.size()));
    REQUIRE(result.message != nullptr);
    REQUIRE(result.has_anomaly(AnomalyKind::InvalidFilenameChars));
}

TEST_CASE("Security: an RFC 2047-encoded filename whose base64 payload merely "
          "contains '/' as an encoding artifact is NOT flagged",
          "[mime][security]") {
    // Regression for a real false positive hit while building this corpus:
    // the encoded-word's base64 text can legitimately contain '/' as part
    // of its alphabet even though the *decoded* filename has none. The
    // check must decode first (mime4j's russian-headers.msg fixture,
    // test_rfc_conformance_mime4j.cpp, exercises the same header).
    libglot::Arena arena;
    auto result =
        parse_message(arena, "Content-Type: text/plain\n"
                             "Content-Disposition: attachment; "
                             "filename==?koi8-r?B?89DJ08/LLmRvYw==?=\n"
                             "\n"
                             "body\n");
    REQUIRE(result.message != nullptr);
    REQUIRE(!result.has_anomaly(AnomalyKind::InvalidFilenameChars));
}

TEST_CASE("Security: an RFC 2047-encoded filename whose *decoded* content contains "
          "a path separator is still flagged (decoding first closes an evasion)",
          "[mime][security]") {
    // "=?UTF-8?B?Li4vLi4vZXZpbA==?=" base64-decodes to "../../evil": an
    // attacker cannot hide a traversal filename from this check by simply
    // RFC 2047-encoding it.
    libglot::Arena arena;
    auto result =
        parse_message(arena, "Content-Type: application/octet-stream\n"
                             "Content-Disposition: attachment; "
                             "filename==?UTF-8?B?Li4vLi4vZXZpbA==?=\n"
                             "\n"
                             "data\n");
    REQUIRE(result.message != nullptr);
    REQUIRE(result.has_anomaly(AnomalyKind::InvalidFilenameChars));
}

TEST_CASE("Security: a clean filename carries no InvalidFilenameChars anomaly",
          "[mime][security]") {
    libglot::Arena arena;
    auto result =
        parse_message(arena, "Content-Type: application/octet-stream\n"
                             "Content-Disposition: attachment; filename=\"report.pdf\"\n"
                             "\n"
                             "data\n");
    REQUIRE(result.message != nullptr);
    REQUIRE(!result.has_anomaly(AnomalyKind::InvalidFilenameChars));
    REQUIRE(!result.rejected);
}

// ============================================================================
// Boundary confusion: an inner boundary chosen so a naive parser could
// disagree with libglot on where a part ends (real-world MIME smuggling
// class; mime4j's boundary-name-clash.msg in test_rfc_conformance_mime4j.cpp
// covers the "inner is outer + suffix" shape, this covers the reverse)
// ============================================================================

TEST_CASE("Security: an inner boundary that is a PREFIX of the outer one does not "
          "let the outer boundary prematurely close the inner part",
          "[mime][security][boundary]") {
    // Inner boundary "b" is a strict prefix of outer boundary "boundary";
    // a delimiter search must match the FULL declared marker at each
    // candidate line, never a shorter marker that happens to be a prefix
    // of what is actually there.
    libglot::Arena arena;
    std::string_view source =
        "Content-Type: multipart/mixed; boundary=\"boundary\"\n"
        "\n"
        "--boundary\n"
        "Content-Type: multipart/alternative; boundary=\"b\"\n"
        "\n"
        "--b\n"
        "Content-Type: text/plain\n"
        "\n"
        "inner part one\n"
        "--b\n"
        "Content-Type: text/plain\n"
        "\n"
        "inner part two\n"
        "--b--\n"
        "--boundary--\n";
    auto result = parse_message(arena, source);
    REQUIRE(result.message != nullptr);
    REQUIRE(!result.rejected);
    REQUIRE(result.message->parts.size() == 1);
    const Message* inner = result.message->parts[0];
    REQUIRE(inner->parts.size() == 2);
    // The line break immediately before each delimiter belongs to the
    // delimiter, not the preceding part's content (boundary.h).
    REQUIRE(inner->parts[0]->body == "inner part one");
    REQUIRE(inner->parts[1]->body == "inner part two");
}

// ============================================================================
// message/rfc822 + Content-Transfer-Encoding: the transfer-decode-then-
// recurse path added this session (docs/ROADMAP.md) is new code with its
// own recursion; confirm the existing DoS nesting-depth limit still
// applies to it exactly as it does to the plain (non-transfer-encoded)
// chain already covered by test_message_rfc822.cpp.
// ============================================================================

TEST_CASE("Security: nesting-depth limit still applies through a base64-encoded "
          "message/rfc822 chain",
          "[mime][security][limits]") {
    std::string chain = "Subject: leaf\n\nleaf body\n";
    constexpr int kChainDepth = 6;
    for (int i = 0; i < kChainDepth; ++i) {
        std::string encoded = TransferEncoding::encode_base64(chain);
        chain = "Content-Type: message/rfc822\n"
                "Content-Transfer-Encoding: base64\n\n" +
                encoded;
    }

    libglot::Arena arena;
    ParseOptions options;
    options.limits.max_nesting_depth = 3;

    auto result = parse_message(arena, chain, options);
    REQUIRE(result.message != nullptr);
    REQUIRE(result.has_anomaly(AnomalyKind::ExcessiveNestingDepth));
    REQUIRE(result.rejected);

    Message* cur = result.message;
    int depth = 0;
    while (cur->encapsulated != nullptr) {
        cur = cur->encapsulated;
        ++depth;
    }
    REQUIRE(depth < kChainDepth);
    REQUIRE(depth == 3);
}
