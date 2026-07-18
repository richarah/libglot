/// ============================================================================
/// RFC 2045/2046/5322 Conformance Suite (vendored from Apache James Mime4j)
/// ============================================================================
///
/// docs/ROADMAP.md's remaining work called for importing an established RFC
/// conformance test suite, independent of the SpamAssassin/Enron corpora
/// used elsewhere, to exercise edge cases those corpora don't happen to
/// contain. The 32 fixtures in mime/tests/data/mime4j/ are Apache James
/// Mime4j's own hand-crafted conformance messages (Apache License 2.0; see
/// that directory's NOTICE file) -- not real mail, but deliberately
/// constructed boundary/header edge cases.
///
/// Each fixture is read from disk (so its exact bytes -- CRLFs, long
/// boundary strings -- are never retyped by hand) and parsed through
/// libglot's real pipeline. Assertions check libglot's own verified,
/// currently-correct behavior; where that behavior differs from mime4j's
/// own expected-output XML (also vendored upstream, not copied here), the
/// TEST_CASE says why. Two real bugs were found and fixed by this import
/// (see git history for parser_extended.h around this date):
///
///   1. `finish_message` checked a Reject-severity anomaly *before* calling
///      parse_date_header/parse_threading_headers, so an unrelated header
///      problem anywhere in a message silently suppressed Date and
///      Message-ID parsing too (found via the differential-residual
///      classification pass, not this suite, but the same root cause).
///   2. message/rfc822 parts never transfer-decoded their body before
///      recursing (found HERE, via mime4j's
///      base64encoded-rfc822message*.msg fixtures): a base64-encoded
///      message/rfc822 body -- which real senders do even though RFC 2046
///      §5.2.1 permits only 7bit/8bit/binary there -- was parsed as
///      headers+body directly against the still-base64 bytes, silently
///      yielding an empty nested message instead of the real, recoverable
///      content.
///
/// This suite also found that libglot's Message had no preamble/epilogue
/// concept at all (RFC 2046 §5.1.1 defines both); a multipart whose
/// *only* boundary occurrence is the close delimiter (zero body-parts,
/// which RFC 2046 permits) looked "undivided" only because that content
/// was never surfaced, not because the 0-part result itself was wrong --
/// mime4j reports 0 parts for the same fixtures too. Fixed as a follow-up
/// pass (Message::preamble/epilogue, parser_extended.h's
/// parse_multipart_body, mime_dump.cpp's JSON schema): see
/// multipartnopart.msg and missing-inner-start-boundary.msg below, and
/// docs/ROADMAP.md.
///
/// A fourth issue looked, at first, like the biggest of all: libglot threw
/// on RFC 5322 §4's obsolete header grammar entirely (WSP before ':',
/// blank lines mid-fold) rather than tolerating it. Tracing *why*
/// obsolete.msg failed showed the obs-* grammar was already tokenizing
/// correctly -- HeaderFolding::unfold_headers already joins a
/// whitespace-only continuation line into its parent header, and the
/// tokenizer already treats WSP before/after ':' as ordinary skippable
/// whitespace. The actual, sole blocker was one line whose field name
/// contains a non-ASCII byte turning into a stray INVALID token, which
/// aborted parsing of the *entire* message rather than just that one
/// line. Fixed by making `parse_header_with_parameters` recover from a
/// malformed header line (skip to the next NEWLINE, record
/// AnomalyKind::ObsoleteHeaderSyntax, keep going) instead of throwing --
/// the same recovery `parse_part`'s line scanner already had for a
/// colon-less line inside a multipart part, now applied consistently at
/// the top level too (obsolete.msg and the malformedHeader*/
/// malformedHeaderStartsBody*.msg fixtures below).
/// ============================================================================

#include "../../core/include/libglot/util/arena.h"
#include "../include/libglot/mime/mime.h"
#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <sstream>
#include <string>

using namespace libglot::mime;

namespace {

std::string read_fixture(const char* name) {
    std::string path = std::string(MIME4J_FIXTURES_DIR) + "/" + name;
    std::ifstream f(path, std::ios::binary);
    REQUIRE(f.is_open());
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

} // namespace

// ============================================================================
// Straightforward positive-path messages
// ============================================================================

TEST_CASE("mime4j basic-plain: simple single-part message parses cleanly",
          "[mime][rfc-conformance][mime4j]") {
    std::string raw = read_fixture("basic-plain.msg");
    libglot::Arena arena;
    auto result = parse_message(arena, raw);
    REQUIRE(result.message != nullptr);
    REQUIRE(!result.rejected);
    REQUIRE(find_header(*result.message, "Subject")->value == "Simple Subject");
    REQUIRE(find_header(*result.message, "From")->value == "foo@example.com");
    REQUIRE(result.message->body ==
            "This is a very simple message with a simple body and no weird things at \r\n"
            "all.\r\n");
}

TEST_CASE("mime4j qp-body: quoted-printable ISO-8859-15 body decodes the euro sign",
          "[mime][rfc-conformance][mime4j]") {
    std::string raw = read_fixture("qp-body.msg");
    libglot::Arena arena;
    auto result = parse_message(arena, raw);
    REQUIRE(result.message != nullptr);
    REQUIRE(!result.rejected);
    auto decoded = decoded_body_utf8(*result.message);
    REQUIRE(decoded.has_value());
    REQUIRE(*decoded == "7bit content with euro \xE2\x82\xAC symbol \r\n");
}

TEST_CASE("mime4j russian-headers: RFC 2047 encoded-word in a filename parameter "
          "is preserved verbatim, matching mime4j's own (non-decoding) expectation",
          "[mime][rfc-conformance][mime4j]") {
    // Encoded-words are defined for RFC 822 "phrase"/unstructured-text
    // contexts (RFC 2047 §5), not parameter values -- mime4j's own
    // expected XML for this fixture doesn't decode it either, it just
    // preserves the header text as-is. Decoding non-standard encoded-words
    // inside parameter values is out of scope on both sides.
    std::string raw = read_fixture("russian-headers.msg");
    libglot::Arena arena;
    auto result = parse_message(arena, raw);
    REQUIRE(result.message != nullptr);
    REQUIRE(!result.rejected);
    const Header* disp = find_header(*result.message, "Content-Disposition");
    REQUIRE(disp != nullptr);
    REQUIRE(disp->value == "attachment; filename==?koi8-r?B?89DJ08/LLmRvYw==?=");
    REQUIRE(result.message->body == "A simple body.\r\n");
}

TEST_CASE("mime4j basic-plain-very-long-lines: a long single-part body is not truncated",
          "[mime][rfc-conformance][mime4j]") {
    std::string raw = read_fixture("basic-plain-very-long-lines.msg");
    libglot::Arena arena;
    auto result = parse_message(arena, raw);
    REQUIRE(result.message != nullptr);
    REQUIRE(!result.rejected);
    REQUIRE(result.message->body.size() == 7171);
}

// ============================================================================
// Multipart: normal splitting, nesting, attachments
// ============================================================================

TEST_CASE("mime4j simple-attachment: two parts, one with a filename", "[mime][rfc-conformance][mime4j]") {
    std::string raw = read_fixture("simple-attachment.msg");
    libglot::Arena arena;
    auto result = parse_message(arena, raw);
    REQUIRE(result.message != nullptr);
    REQUIRE(!result.rejected);
    REQUIRE(result.message->parts.size() == 2);
    REQUIRE(result.message->parts[0]->body == "Body.\r\n");
    auto attachment = decoded_body(*result.message->parts[1]); // base64 -> raw bytes
    REQUIRE(attachment.has_value());
    REQUIRE(attachment->size() == 1024);
    const Header* disp = find_header(*result.message->parts[1], "Content-Disposition");
    REQUIRE(disp != nullptr);
    REQUIRE(disp->value.find("data.bin") != std::string_view::npos);
}

TEST_CASE("mime4j example: a real 4-part multipart/mixed with two identical-name attachments",
          "[mime][rfc-conformance][mime4j]") {
    std::string raw = read_fixture("example.msg");
    libglot::Arena arena;
    auto result = parse_message(arena, raw);
    REQUIRE(result.message != nullptr);
    REQUIRE(!result.rejected);
    REQUIRE(result.message->parts.size() == 4);
    REQUIRE(result.message->parts[0]->body.size() == 772); // 7bit: raw == decoded
    for (size_t i : {1, 2, 3}) { // base64 (x2) and quoted-printable: decode first
        auto decoded = decoded_body(*result.message->parts[i]);
        REQUIRE(decoded.has_value());
        if (i == 3) {
            REQUIRE(decoded->size() == 3073);
        } else {
            REQUIRE(decoded->size() == 355);
        }
    }
}

TEST_CASE("mime4j boundary-name-clash: an inner boundary that is a prefix of the outer one",
          "[mime][rfc-conformance][mime4j]") {
    // "--boundary.X" (outer) vs "--boundary.X-1" (inner): the outer marker
    // is a strict prefix of the inner one, so matching must not let the
    // shorter marker's search accidentally consume the longer one's lines.
    std::string raw = read_fixture("boundary-name-clash.msg");
    libglot::Arena arena;
    auto result = parse_message(arena, raw);
    REQUIRE(result.message != nullptr);
    REQUIRE(!result.rejected);
    REQUIRE(result.message->parts.size() == 2);
    const Message* alternative = result.message->parts[0];
    REQUIRE(alternative->parts.size() == 2);
    REQUIRE(alternative->parts[0]->body ==
            "Please see attachment for report Daily_Stats-2022-05-12-0700");
    REQUIRE(alternative->parts[1]->body.size() == 271);
    const Header* disp = find_header(*result.message->parts[1], "Content-Disposition");
    REQUIRE(disp != nullptr);
    REQUIRE(disp->value.find("Daily_Stats-2022-05-12-0700.pdf") != std::string_view::npos);
}

TEST_CASE("mime4j intermediate-boundaries: transport-padded and near-miss boundary lines",
          "[mime][rfc-conformance][mime4j]") {
    // A boundary line may carry trailing whitespace (RFC 2046 transport
    // padding, must still match); a boundary-looking line indented by even
    // one space is not at the start of a line and must NOT match, so it
    // stays part of the body content.
    std::string raw = read_fixture("intermediate-boundaries.msg");
    libglot::Arena arena;
    auto result = parse_message(arena, raw);
    REQUIRE(result.message != nullptr);
    REQUIRE(!result.rejected);
    REQUIRE(result.message->parts.size() == 2);
    REQUIRE(result.message->parts[0]->body == "first part\r\n");
    REQUIRE(result.message->parts[1]->body.find(" --boundary\r\n") != std::string_view::npos);
    REQUIRE(result.message->parts[1]->body.find("... that should be ignored") !=
            std::string_view::npos);
}

TEST_CASE("mime4j misplaced-boundary: boundary text mid-line is body content, not a delimiter",
          "[mime][rfc-conformance][mime4j]") {
    std::string raw = read_fixture("misplaced-boundary.msg");
    libglot::Arena arena;
    auto result = parse_message(arena, raw);
    REQUIRE(result.message != nullptr);
    REQUIRE(!result.rejected);
    REQUIRE(result.message->parts.size() == 1);
    REQUIRE(result.message->parts[0]->body ==
            "This should be a text including the --boundary\r\n"
            "string and should not be parsed as multiple bodies\r\n");
}

TEST_CASE("mime4j weird-boundary: a boundary value using every RFC 2046 bchars character",
          "[mime][rfc-conformance][mime4j]") {
    std::string raw = read_fixture("weird-boundary.msg");
    libglot::Arena arena;
    auto result = parse_message(arena, raw);
    REQUIRE(result.message != nullptr);
    REQUIRE(!result.rejected);
    REQUIRE(result.message->parts.size() == 1);
    REQUIRE(result.message->parts[0]->body.size() == 186);
    REQUIRE(result.message->parts[0]->body.find("Text body") == 0);
    // A near-match missing the boundary's trailing space stays in the body.
    REQUIRE(result.message->parts[0]->body.find("miss a final space") != std::string_view::npos);
}

TEST_CASE("mime4j very-long-boundary and very-very-long-boundary: matching doesn't "
          "degrade or truncate as the boundary string grows",
          "[mime][rfc-conformance][mime4j]") {
    libglot::Arena arena1;
    auto r1 = parse_message(arena1, read_fixture("very-long-boundary.msg"));
    REQUIRE(r1.message != nullptr);
    REQUIRE(!r1.rejected);
    REQUIRE(r1.message->parts.size() == 1);
    REQUIRE(r1.message->parts[0]->body.size() == 816);

    libglot::Arena arena2;
    auto r2 = parse_message(arena2, read_fixture("very-very-long-boundary.msg"));
    REQUIRE(r2.message != nullptr);
    REQUIRE(!r2.rejected);
    REQUIRE(r2.message->parts.size() == 1);
    REQUIRE(r2.message->parts[0]->body.size() == 7116);
}

TEST_CASE("mime4j bad-newlines-multiple-parts: bare-LF (not CRLF) multipart still "
          "splits, with preamble and epilogue captured (RFC 2046 §5.1.1)",
          "[mime][rfc-conformance][mime4j]") {
    std::string raw = read_fixture("bad-newlines-multiple-parts.msg");
    libglot::Arena arena;
    auto result = parse_message(arena, raw);
    REQUIRE(result.message != nullptr);
    REQUIRE(!result.rejected);
    REQUIRE(result.message->parts.size() == 1);
    REQUIRE(result.message->parts[0]->body == "Text body\n");
    REQUIRE(result.message->preamble == "This is a multi-part message in MIME format.\n");
    REQUIRE(result.message->epilogue == "That was a multi-part message in MIME format.\n");
}

TEST_CASE("mime4j multipartdigestnestedemptyparts: multipart/digest with a "
          "message/rfc822 default part media type",
          "[mime][rfc-conformance][mime4j]") {
    // multipart/digest's default part Content-Type is message/rfc822 (RFC
    // 2046 §5.1.5), not text/plain; here the (single, absent-header) part's
    // body is itself a full nested MIME message, decoded as raw text since
    // this suite doesn't check message/rfc822-by-digest-default recursion.
    std::string raw = read_fixture("multipartdigestnestedemptyparts.msg");
    libglot::Arena arena;
    auto result = parse_message(arena, raw);
    REQUIRE(result.message != nullptr);
    REQUIRE(!result.rejected);
    REQUIRE(find_header(*result.message, "Content-Type")->value.find("multipart/digest") == 0);
    REQUIRE(result.message->parts.size() == 1);
    REQUIRE(result.message->parts[0]->body.size() == 356);
}

// ============================================================================
// message/rfc822 recursion, including transfer-encoded bodies (bug fixed by
// this import: see the file header comment)
// ============================================================================

TEST_CASE("mime4j base64encoded-rfc822message: a base64-encoded message/rfc822 body "
          "is transfer-decoded before being parsed as the nested message",
          "[mime][rfc-conformance][mime4j][regression]") {
    std::string raw = read_fixture("base64encoded-rfc822message.msg");
    libglot::Arena arena;
    auto result = parse_message(arena, raw);
    REQUIRE(result.message != nullptr);
    REQUIRE(!result.rejected);
    REQUIRE(result.message->encapsulated != nullptr);
    const Message* nested = result.message->encapsulated;
    REQUIRE(find_header(*nested, "Content-Type")->value == "text/plain; charset=us-ascii");
    REQUIRE(nested->body == "Text body\n\r\n");
}

TEST_CASE("mime4j base64encoded-rfc822message-nested: two layers of base64-encoded "
          "message/rfc822 both decode",
          "[mime][rfc-conformance][mime4j][regression]") {
    std::string raw = read_fixture("base64encoded-rfc822message-nested.msg");
    libglot::Arena arena;
    auto result = parse_message(arena, raw);
    REQUIRE(result.message != nullptr);
    REQUIRE(!result.rejected);
    REQUIRE(result.message->encapsulated != nullptr);
    const Message* level1 = result.message->encapsulated;
    REQUIRE(level1->encapsulated != nullptr);
    const Message* level2 = level1->encapsulated;
    REQUIRE(level2->body == "Text body\n\r\n");
}

TEST_CASE("mime4j bad-newlines-multiple-parts-base64: a base64-encoded message/rfc822 "
          "whose decoded content is itself multipart still splits into parts",
          "[mime][rfc-conformance][mime4j][regression]") {
    std::string raw = read_fixture("bad-newlines-multiple-parts-base64.msg");
    libglot::Arena arena;
    auto result = parse_message(arena, raw);
    REQUIRE(result.message != nullptr);
    REQUIRE(!result.rejected);
    REQUIRE(result.message->encapsulated != nullptr);
    const Message* nested = result.message->encapsulated;
    REQUIRE(nested->parts.size() == 1);
    REQUIRE(nested->parts[0]->body == "Text body\n");
}

TEST_CASE("mime4j base64-encoded-text: a text/plain part whose base64 payload merely "
          "looks like a MIME message is decoded as literal text, not recursed into",
          "[mime][rfc-conformance][mime4j]") {
    std::string raw = read_fixture("base64-encoded-text.msg");
    libglot::Arena arena;
    auto result = parse_message(arena, raw);
    REQUIRE(result.message != nullptr);
    REQUIRE(!result.rejected);
    REQUIRE(result.message->encapsulated == nullptr);
    auto decoded = decoded_body(*result.message);
    REQUIRE(decoded.has_value());
    REQUIRE(decoded->find("Content-Type: multipart/mixed") == 0);
    REQUIRE(decoded->find("Text body") != std::string::npos);
}

TEST_CASE("mime4j multipartnestedemptyparts: multipart -> message/rfc822 -> "
          "multipart -> one empty part",
          "[mime][rfc-conformance][mime4j]") {
    std::string raw = read_fixture("multipartnestedemptyparts.msg");
    libglot::Arena arena;
    auto result = parse_message(arena, raw);
    REQUIRE(result.message != nullptr);
    REQUIRE(!result.rejected);
    REQUIRE(result.message->parts.size() == 1);
    const Message* rfc822_part = result.message->parts[0];
    REQUIRE(rfc822_part->encapsulated != nullptr);
    const Message* nested = rfc822_part->encapsulated;
    REQUIRE(nested->parts.size() == 1);
    REQUIRE(nested->parts[0]->body.empty());
}

TEST_CASE("mime4j multipartnestedemptypartsnorfc822: multipart -> multipart -> one empty part",
          "[mime][rfc-conformance][mime4j]") {
    std::string raw = read_fixture("multipartnestedemptypartsnorfc822.msg");
    libglot::Arena arena;
    auto result = parse_message(arena, raw);
    REQUIRE(result.message != nullptr);
    REQUIRE(!result.rejected);
    REQUIRE(result.message->parts.size() == 1);
    REQUIRE(result.message->parts[0]->parts.size() == 1);
    REQUIRE(result.message->parts[0]->parts[0]->body.empty());
}

TEST_CASE("mime4j multipartemptypart: a legitimately empty part between two boundaries",
          "[mime][rfc-conformance][mime4j]") {
    // Nothing at all between the opening boundary and the closing one: no
    // Content-Type header of its own (RFC 2045's text/plain;charset=us-ascii
    // default is a caller-facing convention, e.g. tools/mime_dump.cpp's
    // presentation layer -- the parsed AST simply has no such header, and
    // decoded_body_utf8's own absent-charset handling (UTF-8/US-ASCII
    // passthrough) still resolves the empty body without error).
    std::string raw = read_fixture("multipartemptypart.msg");
    libglot::Arena arena;
    auto result = parse_message(arena, raw);
    REQUIRE(result.message != nullptr);
    REQUIRE(!result.rejected);
    REQUIRE(result.message->parts.size() == 1);
    REQUIRE(result.message->parts[0]->body.empty());
    REQUIRE(find_header(*result.message->parts[0], "Content-Type") == nullptr);
    auto decoded = decoded_body_utf8(*result.message->parts[0]);
    REQUIRE(decoded.has_value());
    REQUIRE(decoded->empty());
}

// ============================================================================
// Deliberate libglot strictness: RFC-literal boundary/header parsing where
// mime4j is more lenient than the grammar strictly requires. Not bugs --
// the same "decline rather than guess wrong" philosophy already documented
// throughout docs/ROADMAP.md's differential-residual classification.
// ============================================================================

TEST_CASE("mime4j ending-boundaries: trailing non-whitespace text on a boundary line "
          "means it is not a valid delimiter (RFC 2046 permits only LWSP there)",
          "[mime][rfc-conformance][mime4j][strictness]") {
    // mime4j treats "--boundary <arbitrary text>" as a delimiter anyway
    // (ignoring anything after the marker, not just whitespace); libglot
    // follows the grammar literally -- transport-padding is "*LWSP-char",
    // not arbitrary text -- so the message's only two "--boundary..."
    // occurrences with trailing garbage are NOT recognized as delimiters.
    // There is a third, clean "--boundary--" further down with nothing
    // after it on the line, which IS recognized: 0 parts (nothing opened
    // before it), with everything before it captured as preamble
    // (including the two garbage "delimiter-shaped" lines, which are just
    // ordinary text from libglot's point of view) and everything after
    // as epilogue.
    std::string raw = read_fixture("ending-boundaries.msg");
    libglot::Arena arena;
    auto result = parse_message(arena, raw);
    REQUIRE(result.message != nullptr);
    REQUIRE(!result.rejected);
    REQUIRE(result.message->parts.empty());
    REQUIRE(result.message->body.size() == 702);
    REQUIRE(result.message->preamble.find("--boundary This should be ignored") == 0);
    REQUIRE(result.message->preamble.find("first part") != std::string_view::npos);
    REQUIRE(result.message->epilogue ==
            "\r\nThe above boundary should be part of the epilogue, too.");
}

TEST_CASE("mime4j multipartnopart: a multipart whose only boundary occurrence is the "
          "close delimiter reports 0 parts plus preamble/epilogue, matching mime4j",
          "[mime][rfc-conformance][mime4j]") {
    // RFC 2046 permits a multipart with zero body-parts; mime4j reports 0
    // parts plus preamble/epilogue text for this exact fixture, which is
    // what libglot now reports too (Message::preamble/epilogue, added
    // alongside this conformance suite -- see docs/ROADMAP.md).
    std::string raw = read_fixture("multipartnopart.msg");
    libglot::Arena arena;
    auto result = parse_message(arena, raw);
    REQUIRE(result.message != nullptr);
    REQUIRE(!result.rejected);
    REQUIRE(result.message->parts.empty());
    REQUIRE(result.message->body.size() == 115);
    REQUIRE(result.message->preamble ==
            "This is a multi-part message in MIME format with no parts.\r\n");
    REQUIRE(result.message->epilogue == "\r\nEpilogue\r\n");
}

TEST_CASE("mime4j missing-boundary: a multipart with no boundary occurrence at all "
          "reports its content undivided",
          "[mime][rfc-conformance][mime4j]") {
    std::string raw = read_fixture("missing-boundary.msg");
    libglot::Arena arena;
    auto result = parse_message(arena, raw);
    REQUIRE(result.message != nullptr);
    REQUIRE(!result.rejected);
    REQUIRE(result.message->parts.empty());
    REQUIRE(result.message->body == "AAA\r\n\r\n");
}

TEST_CASE("mime4j missing-inner-boundary: an inner multipart with an opening boundary "
          "but no closing one falls back to undivided content",
          "[mime][rfc-conformance][mime4j]") {
    std::string raw = read_fixture("missing-inner-boundary.msg");
    libglot::Arena arena;
    auto result = parse_message(arena, raw);
    REQUIRE(result.message != nullptr);
    REQUIRE(!result.rejected);
    REQUIRE(result.message->parts.size() == 2);
    REQUIRE(result.message->parts[0]->body == "Foo\r\n");
    REQUIRE(result.message->parts[1]->parts.empty());
    REQUIRE(result.message->parts[1]->body == "AAA\r\n");
}

TEST_CASE("mime4j missing-inner-start-boundary: an inner multipart whose only boundary "
          "occurrence is its own close delimiter reports 0 parts plus a preamble, "
          "same shape as multipartnopart",
          "[mime][rfc-conformance][mime4j]") {
    std::string raw = read_fixture("missing-inner-start-boundary.msg");
    libglot::Arena arena;
    auto result = parse_message(arena, raw);
    REQUIRE(result.message != nullptr);
    REQUIRE(!result.rejected);
    REQUIRE(result.message->preamble == "Outer preamble\r\n");
    REQUIRE(result.message->epilogue == "Outer epilouge\r\n");
    REQUIRE(result.message->parts.size() == 2);
    REQUIRE(result.message->parts[0]->body == "Foo\r\n");
    REQUIRE(result.message->parts[1]->parts.empty());
    REQUIRE(result.message->parts[1]->body.size() == 25);
    REQUIRE(result.message->parts[1]->preamble == "AAA\r\n");
    REQUIRE(result.message->parts[1]->epilogue.empty());
}

// ============================================================================
// Malformed header sections: a header line that doesn't parse as
// field-name + ':' + value is now skipped (AnomalyKind::ObsoleteHeaderSyntax
// recorded), not a fatal ParseError -- matching how a colon-less line was
// already handled inside a multipart part (parse_part's own line scanner),
// and matching mime4j's own tolerant behavior for every fixture below.
// This closed the last of the four remaining Stage 5 follow-up gaps (see
// docs/ROADMAP.md): tracing through *why* obsolete.msg failed showed the
// obs-* grammar (WSP before ':', blank lines mid-fold) was already
// tokenizing correctly -- the actual, sole blocker was one line whose
// field name contains a non-ASCII byte, which aborted the whole parse
// rather than just that one line.
// ============================================================================

TEST_CASE("mime4j basic-plain-with-bad-header-separator: a header/body separator "
          "line with a stray space is not a valid blank line, so everything "
          "after it is read as (malformed, skipped) headers until real body text",
          "[mime][rfc-conformance][mime4j]") {
    std::string raw = read_fixture("basic-plain-with-bad-header-separator.msg");
    libglot::Arena arena;
    auto result = parse_message(arena, raw);
    REQUIRE(result.message != nullptr);
    REQUIRE(!result.rejected);
    REQUIRE(find_header(*result.message, "Subject")->value == "Simple Subject");
    REQUIRE(result.message->body == "This results in a bogus header.\r\n");
}

TEST_CASE("mime4j malformedHeader-nocrlfcrlf: a body-shaped line with no colon "
          "in the header section is skipped, not fatal",
          "[mime][rfc-conformance][mime4j]") {
    std::string raw = read_fixture("malformedHeader-nocrlfcrlf.msg");
    libglot::Arena arena;
    auto result = parse_message(arena, raw);
    REQUIRE(result.message != nullptr);
    REQUIRE(!result.rejected);
    REQUIRE(result.has_anomaly(AnomalyKind::ObsoleteHeaderSyntax));
    REQUIRE(find_header(*result.message, "Subject")->value == "this is a subject");
    REQUIRE(find_header(*result.message, "AnotherHeader") != nullptr);
    REQUIRE(result.message->body == "Body text\r\n");
}

TEST_CASE("mime4j malformedHeader-noheader: no header section at all reads as an "
          "all-skipped header block with an empty body, matching mime4j exactly",
          "[mime][rfc-conformance][mime4j]") {
    std::string raw = read_fixture("malformedHeader-noheader.msg");
    libglot::Arena arena;
    auto result = parse_message(arena, raw);
    REQUIRE(result.message != nullptr);
    REQUIRE(!result.rejected);
    REQUIRE(result.message->body.empty());
}

TEST_CASE("mime4j malformedHeaderStartsBody-nocrlfcrlf: variant with the bogus "
          "colon-less line placed differently is likewise skipped, not fatal",
          "[mime][rfc-conformance][mime4j]") {
    std::string raw = read_fixture("malformedHeaderStartsBody-nocrlfcrlf.msg");
    libglot::Arena arena;
    auto result = parse_message(arena, raw);
    REQUIRE(result.message != nullptr);
    REQUIRE(!result.rejected);
    REQUIRE(find_header(*result.message, "Subject")->value == "this is a subject");
    REQUIRE(result.message->body == "Body text\r\n");
}

TEST_CASE("mime4j malformedHeaderStartsBody-noheader: variant with no header "
          "section, different body shape, also an empty body",
          "[mime][rfc-conformance][mime4j]") {
    std::string raw = read_fixture("malformedHeaderStartsBody-noheader.msg");
    libglot::Arena arena;
    auto result = parse_message(arena, raw);
    REQUIRE(result.message != nullptr);
    REQUIRE(!result.rejected);
    REQUIRE(result.message->body.empty());
}

TEST_CASE("mime4j obsolete: RFC 5322 obs-* header grammar (WSP before ':', blank "
          "lines mid-fold, WSP around ':') all parse correctly; the one line "
          "with a non-ASCII field name is skipped rather than aborting everything",
          "[mime][rfc-conformance][mime4j]") {
    std::string raw = read_fixture("obsolete.msg");
    libglot::Arena arena;
    auto result = parse_message(arena, raw);
    REQUIRE(result.message != nullptr);
    REQUIRE(!result.rejected);
    REQUIRE(result.has_anomaly(AnomalyKind::ObsoleteHeaderSyntax));
    // "Subject :The obsolete syntax...folding." -- WSP before ':', and the
    // blank-looking continuation line, both already unfold correctly.
    REQUIRE(find_header(*result.message, "Subject")->value ==
            "The obsolete syntax allow spaces before the colon        "
            "and also empty lines in folding.");
    // "Date    :     Malformed Date." -- WSP around ':'; the value itself
    // is deliberately not a valid RFC 5322 date-time (that's the fixture's
    // point), so it's kept as a header but fails date-parsing separately.
    REQUIRE(find_header(*result.message, "Date")->value == "Malformed Date.");
    REQUIRE(result.message->date == nullptr);
    REQUIRE(result.has_anomaly(AnomalyKind::InvalidDateFormat));
    // "HeaderWithWSP \t  \t:\t\tvalue." -- WSP/tabs on both sides of ':'.
    REQUIRE(find_header(*result.message, "HeaderWithWSP")->value == "value.");
    // "Inval<0xED>d-Header: this is not valid." -- a field name with a
    // non-ASCII byte is not valid RFC 5322 ftext under either grammar;
    // skipped rather than kept or fatal. Only the three well-formed
    // headers above survive.
    REQUIRE(result.message->headers.size() == 3);
    REQUIRE(result.message->body == "body\r\n");
}
