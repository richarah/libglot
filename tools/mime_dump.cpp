// mime_dump: parse one message (path given as argv[1]) through the full
// libglot::mime pipeline and print a canonical JSON description of its
// STRUCTURE to stdout. This is the libglot half of the differential-testing
// harness in scripts/mime_diff.py: that script builds the identical
// canonical structure from Python's stdlib `email` package and diffs the
// two, turning every corpus message into a field-by-field assertion instead
// of a "did it crash" smoke test.
//
// Only stdout carries the JSON document; nothing else is printed there.
//
// ============================================================================
// Output schema (keep this comment and the code in sync)
// ============================================================================
//
//   {
//     "parse_error": bool,     // libglot::ParseError was thrown (malformed
//                              // header section, e.g. a line with no ':').
//                              // When true there is no "root" key: the
//                              // message could not be parsed at all.
//     "rejected": bool,        // a Reject-policy Security/DoS anomaly fired;
//                              // "root" is present but may be a partial tree
//                              // per libglot::mime::ParseResult::rejected.
//     "root": <Node>           // present unless parse_error is true
//   }
//
// <Node> (recursive; describes one Message/part):
//   {
//     "content_type": "text/plain",
//       // lowercased "type/subtype" only, no parameters. Defaulted to
//       // "text/plain" per RFC 2045 5.2 when the Content-Type header is
//       // absent (that section's full default is "text/plain;
//       // charset=us-ascii" -- see the "content-type" header entry below).
//
//     "headers": { "lowercased-field-name": ["canonical value", ...], ... },
//       // Only fields with at least one occurrence on this node are
//       // present. Selected fields, each canonicalized as described below:
//       //
//       //   from / to / cc:
//       //     The header value is RFC 2047 decoded as a whole (encoded
//       //     words only legally appear in the display-name/phrase
//       //     portion, never inside addr-spec, so whole-value decoding is
//       //     safe) then split into individual mailboxes on commas that
//       //     are not inside a quoted-string or angle-address bracket
//       //     pair. Each mailbox becomes either "addr-spec" (no display
//       //     name) or "Display Name <addr-spec>". This is a diff key, not
//       //     a re-serialization of RFC 5322 syntax: it is never re-parsed,
//       //     so no quoting/escaping of the display name is performed --
//       //     scripts/mime_diff.py builds the Python side with the exact
//       //     same "Name <addr>" / "addr" convention from
//       //     email.headerregistry.Address so the two sides line up
//       //     without reimplementing full mailbox grammar twice. A raw
//       //     header line contributing more than one mailbox contributes
//       //     that many entries, in written order; a field repeated across
//       //     several physical header lines (a duplicate-header anomaly)
//       //     appends across lines in header-appearance order.
//       //
//       //   subject:
//       //     RFC 2047 decoded, then runs of ASCII whitespace (left behind
//       //     by unfolding a multi-line header) are collapsed to a single
//       //     ' ' and the result is trimmed.
//       //
//       //   date:
//       //     If libglot's DateTimeParser considers the header valid, an
//       //     ISO-8601 string "YYYY-MM-DDTHH:MM:SS+HH:MM" built from the
//       //     parsed fields. When the header's timezone is unknown (a
//       //     literal "-0000", or an obsolete single-letter/military zone
//       //     per RFC 5322 4.3), the offset is omitted, matching Python's
//       //     own treatment of "-0000" as a naive datetime -- NOTE this is
//       //     a deliberate point of documented divergence for zones like
//       //     "Z"/"UTC" that Python's table resolves to a concrete offset
//       //     but RFC 5322 4.3 says must be treated as unreliable; see
//       //     scripts/mime_diff.py and the stage-4 report for the
//       //     classification. When the header is present but does not
//       //     parse, the raw (RFC-2047-decoded, whitespace-collapsed)
//       //     header text is used verbatim, so a parse-success/failure
//       //     mismatch between libglot and Python surfaces as an ISO
//       //     string on one side and raw text on the other -- exactly the
//       //     kind of thing this tool exists to catch.
//       //
//       //   message-id:
//       //     Angle brackets and surrounding whitespace stripped.
//       //
//       //   content-type:
//       //     "type/subtype" plus "; charset=<lowercased charset>" when a
//       //     charset parameter is present (or implied by the RFC 2045
//       //     absent-header default). Other parameters (boundary, name,
//       //     ...) are intentionally omitted here: boundary differences
//       //     already surface via the "parts" shape, and "name" is
//       //     reported via the dedicated "filename" field below. Charset
//       //     names are lowercased and a small alias table (utf8ee->utf-8,
//       //     ascii->us-ascii, latin1->iso-8859-1, latin9/iso8859-15-style
//       //     spellings->iso-8859-15) is applied on both sides so spelling
//       //     variants of the same charset do not read as a disagreement.
//       //
//       //   content-transfer-encoding:
//       //     The declared token, lowercased and trimmed; defaulted to
//       //     "7bit" (RFC 2045 6.1) when the header is absent.
//       //
//       //   content-disposition:
//       //     The disposition-type token only ("attachment"/"inline"/...),
//       //     lowercased. Absent when the header is absent (RFC 2183 has
//       //     no default). The filename parameter is reported separately.
//
//     "filename": "data.bin",
//       // Resolved attachment/part display filename: Content-Disposition's
//       // "filename" parameter if present, else Content-Type's "name"
//       // parameter (RFC 2231 continuations/percent-encoding/charset
//       // already resolved by the library into a single value). Omitted
//       // when neither is present.
//
//     "body_len": 1234,
//       // Byte length of the body AFTER Content-Transfer-Encoding decoding
//       // (base64/quoted-printable undone; 7bit/8bit/binary pass through
//       // unchanged), matching Python's len(msg.get_payload(decode=True)).
//       // Present for every leaf node; absent for multipart/rfc822
//       // container nodes (which have "parts" instead of a body).
//
//     "body_text": "decoded text",
//       // Present only for a text/* leaf whose transfer decoding AND
//       // charset conversion (libglot::mime::decoded_body_utf8) both
//       // succeeded. May contain bytes that are not valid UTF-8 in the one
//       // case the library itself does not validate (no charset declared:
//       // it passes the transfer-decoded bytes through verbatim) -- see
//       // the ESCAPING note below for how such bytes are represented
//       // losslessly in this JSON.
//
//     "body_decode_error": "bad_transfer_encoding" | "unsupported_charset:<name>",
//       // Present exactly when "body_text" is absent for a text/* leaf, or
//       // when transfer decoding itself failed for ANY leaf (in which case
//       // "body_len"/"body_digest" fall back to describing the raw,
//       // undecoded body). "unsupported_charset:<name>" covers both a
//       // genuinely unknown charset and one outside libglot's supported
//       // set (UTF-8, US-ASCII, ISO-8859-1, Windows-1252, UTF-16[BE|LE]);
//       // scripts/mime_diff.py treats that specific case as an accepted,
//       // documented gap (see docs/FEATURE_MATRIX.md / ROADMAP non-goals)
//       // rather than counting it against the agreement rate.
//
//     "body_digest": "fnv1a64:0123456789abcdef",
//       // Present for every leaf whose body is not reported as body_text:
//       // binary parts, and text parts hitting body_decode_error. FNV-1a
//       // 64-bit over the relevant bytes (transfer-decoded bytes normally;
//       // the raw, undecoded body when transfer decoding itself failed).
//       // FNV-1a was chosen over hand-rolling SHA-256: it is a handful of
//       // lines, trivial to get bit-for-bit identical in both C++ and
//       // Python (no test vectors needed to trust it), and this digest
//       // only has to distinguish "same bytes" from "different bytes" for
//       // a diff report -- it carries no security expectation, so a real
//       // cryptographic digest buys nothing here.
//
//     "parts": [ <Node>, ... ]
//       // Always present. Multipart children, in order. A message/rfc822
//       // part's single encapsulated message is represented the same way:
//       // a one-element "parts" list (this mirrors Python's own
//       // email.message.EmailMessage, whose get_payload() also returns a
//       // one-element list for message/rfc822 -- see scripts/mime_diff.py).
//       // Empty for a leaf.
//   }
//
// ============================================================================
// ESCAPING (documented here because it is the one place this tool departs
// from being "just JSON")
// ============================================================================
//
// String content (header values, filenames, body_text) may contain bytes
// that are not valid UTF-8 -- libglot passes a text/* body through
// unvalidated when no charset is declared (see decoded_body_utf8 in
// mime.h), and this tool must represent that losslessly rather than
// silently repairing or rejecting it.
//
// The writer scans each string for maximal valid UTF-8 sequences and emits
// those literally (as raw UTF-8 bytes in the output, same as any ordinary
// JSON string). A byte that cannot begin, or continue, a valid UTF-8
// sequence at its position is instead escaped as "\uE0XX", where XX is that
// byte's value in hex -- i.e. it is placed at codepoint U+E000+byte, in the
// Unicode Private Use Area. This is unambiguous: this writer never emits a
// \u escape for a byte that was part of a valid decoded sequence (those are
// always written literally), so - appearing in this output
// always means "one raw undecodable byte", never a real character -- even
// in the vanishingly unlikely case the original text legitimately contained
// a Private Use Area character, since THAT would decode validly and would
// therefore be emitted literally (as its real, valid multi-byte UTF-8
// encoding), not via this escape. Because \uE0XX is standard JSON
// \uXXXX syntax, no custom parser is needed on the reading side --
// scripts/mime_diff.py uses the stdlib json module directly and recovers
// the original byte with `ord(ch) - 0xE000` whenever it needs to.
//
// Control characters (0x00-0x1F) use the standard JSON escapes (\n \t \r
// \b \f, or \u00XX); '"' and '\\' are escaped as usual; everything else in
// 0x20-0x7E is written literally.

#include <libglot/mime/mime.h>
#include <libglot/util/arena.h>

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace mime = libglot::mime;

namespace {

// ============================================================================
// Small string utilities local to this tool (not library changes)
// ============================================================================

std::string ascii_lower_copy(std::string_view s) {
    std::string out(s);
    for (char& c : out) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return out;
}

std::string trim_copy(std::string_view s) {
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) {
        ++start;
    }
    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) {
        --end;
    }
    return std::string(s.substr(start, end - start));
}

/// Collapse runs of ASCII whitespace (left behind by header unfolding) into
/// a single ' ', then trim the ends.
std::string collapse_whitespace(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    bool in_ws = false;
    for (char c : s) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            in_ws = true;
        } else {
            if (in_ws && !out.empty()) {
                out.push_back(' ');
            }
            in_ws = false;
            out.push_back(c);
        }
    }
    return out;
}

/// Charset name aliases so spelling variants of the same charset do not
/// read as a content-type disagreement. Mirrors the charset families
/// libglot::mime::CharsetConverter::detect_charset recognizes (charset.h).
std::string canonical_charset_name(std::string_view raw) {
    std::string lower = ascii_lower_copy(raw);
    if (lower == "utf8") {
        return "utf-8";
    }
    if (lower == "ascii" || lower == "us_ascii") {
        return "us-ascii";
    }
    if (lower == "latin1" || lower == "latin-1" || lower == "iso8859-1" || lower == "8859_1") {
        return "iso-8859-1";
    }
    if (lower == "windows1252" || lower == "cp1252") {
        return "windows-1252";
    }
    if (lower == "iso8859-15" || lower == "iso8859_15" || lower == "iso_8859-15" ||
        lower == "latin9" || lower == "latin-9") {
        return "iso-8859-15";
    }
    return lower;
}

// ============================================================================
// FNV-1a 64-bit -- see the "body_digest" schema note above for why this
// (rather than a real cryptographic hash) was chosen.
// ============================================================================

std::string fnv1a64_hex(std::string_view data) {
    uint64_t h = 0xcbf29ce484222325ULL;
    for (unsigned char c : data) {
        h ^= c;
        h *= 0x100000001b3ULL;
    }
    char buf[32];
    std::snprintf(buf, sizeof(buf), "fnv1a64:%016llx", static_cast<unsigned long long>(h));
    return std::string(buf);
}

// ============================================================================
// UTF-8 aware JSON string escaping -- see the ESCAPING doc comment above.
// ============================================================================

/// Length (1-4) of the valid UTF-8 sequence starting at data[i], or 0 if
/// data[i] cannot begin/continue a valid sequence there (overlong forms,
/// surrogate halves, out-of-range lead bytes, and truncated tails at the
/// end of the buffer are all rejected).
size_t utf8_seq_len(const unsigned char* data, size_t size, size_t i) {
    unsigned char b0 = data[i];
    if (b0 < 0x80) {
        return 1;
    }
    size_t len = 0;
    if ((b0 & 0xE0) == 0xC0) {
        if (b0 < 0xC2) {
            return 0; // overlong 2-byte form
        }
        len = 2;
    } else if ((b0 & 0xF0) == 0xE0) {
        len = 3;
    } else if ((b0 & 0xF8) == 0xF0) {
        if (b0 > 0xF4) {
            return 0; // beyond U+10FFFF
        }
        len = 4;
    } else {
        return 0;
    }
    if (i + len > size) {
        return 0; // truncated
    }
    unsigned char b1 = data[i + 1];
    if (len == 3) {
        if (b0 == 0xE0 && b1 < 0xA0) {
            return 0; // overlong 3-byte form
        }
        if (b0 == 0xED && b1 > 0x9F) {
            return 0; // UTF-16 surrogate half D800-DFFF
        }
    } else if (len == 4) {
        if (b0 == 0xF0 && b1 < 0x90) {
            return 0; // overlong 4-byte form
        }
        if (b0 == 0xF4 && b1 > 0x8F) {
            return 0; // beyond U+10FFFF
        }
    }
    if (b1 < 0x80 || b1 > 0xBF) {
        return 0;
    }
    for (size_t k = 2; k < len; ++k) {
        unsigned char bk = data[i + k];
        if (bk < 0x80 || bk > 0xBF) {
            return 0;
        }
    }
    return len;
}

void append_json_string(std::string& out, std::string_view raw) {
    out.push_back('"');
    const auto* data = reinterpret_cast<const unsigned char*>(raw.data());
    const size_t size = raw.size();
    size_t i = 0;
    char buf[8];
    while (i < size) {
        unsigned char c = data[i];
        if (c == '"') {
            out += "\\\"";
            ++i;
        } else if (c == '\\') {
            out += "\\\\";
            ++i;
        } else if (c == '\n') {
            out += "\\n";
            ++i;
        } else if (c == '\r') {
            out += "\\r";
            ++i;
        } else if (c == '\t') {
            out += "\\t";
            ++i;
        } else if (c == '\b') {
            out += "\\b";
            ++i;
        } else if (c == '\f') {
            out += "\\f";
            ++i;
        } else if (c < 0x20) {
            std::snprintf(buf, sizeof(buf), "\\u%04x", c);
            out += buf;
            ++i;
        } else if (c < 0x80) {
            out.push_back(static_cast<char>(c));
            ++i;
        } else {
            size_t len = utf8_seq_len(data, size, i);
            if (len > 0) {
                out.append(raw.substr(i, len));
                i += len;
            } else {
                std::snprintf(buf, sizeof(buf), "\\uE0%02x", c);
                out += buf;
                ++i;
            }
        }
    }
    out.push_back('"');
}

// ============================================================================
// Canonical node tree (built from the parsed Message, then serialized)
// ============================================================================

struct NodeData {
    std::string content_type;
    std::vector<std::pair<std::string, std::vector<std::string>>> headers;
    std::optional<std::string> filename;
    std::optional<size_t> body_len;
    std::optional<std::string> body_text;
    std::optional<std::string> body_decode_error;
    std::optional<std::string> body_digest;
    std::vector<NodeData> parts;
};

const mime::Header* find_ci(const mime::Message& msg, std::string_view field) {
    return mime::find_header(msg, field);
}

std::vector<const mime::Header*> find_all_ci(const mime::Message& msg, std::string_view field) {
    std::vector<const mime::Header*> out;
    for (const auto* h : msg.headers) {
        if (h != nullptr && mime::detail::ascii_ieq(h->field, field)) {
            out.push_back(h);
        }
    }
    return out;
}

/// Split an RFC-2047-decoded address-list header value into individual
/// mailbox strings, on commas that are not inside a quoted-string or an
/// angle-address bracket pair. See the "from / to / cc" schema note above.
std::vector<std::string> split_address_list(std::string_view s) {
    std::vector<std::string> out;
    size_t start = 0;
    bool in_quotes = false;
    int angle_depth = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (c == '"' && (i == 0 || s[i - 1] != '\\')) {
            in_quotes = !in_quotes;
        } else if (!in_quotes && c == '<') {
            ++angle_depth;
        } else if (!in_quotes && c == '>' && angle_depth > 0) {
            --angle_depth;
        } else if (!in_quotes && angle_depth == 0 && c == ',') {
            out.push_back(trim_copy(s.substr(start, i - start)));
            start = i + 1;
        }
    }
    if (start < s.size() || !out.empty()) {
        auto tail = trim_copy(s.substr(start));
        if (!tail.empty() || out.empty()) {
            out.push_back(tail);
        }
    }
    return out;
}

/// One mailbox ("Name <addr>" or bare "addr") -> the canonical
/// "Display Name <addr>" / "addr" diff-key form described in the schema
/// comment. Not a re-serialization of RFC 5322 syntax; see that comment.
std::string canonical_mailbox(std::string_view mailbox) {
    size_t lt = mailbox.find('<');
    size_t gt = mailbox.rfind('>');
    if (lt != std::string_view::npos && gt != std::string_view::npos && gt > lt) {
        std::string addr = trim_copy(mailbox.substr(lt + 1, gt - lt - 1));
        std::string disp = trim_copy(mailbox.substr(0, lt));
        if (disp.size() >= 2 && disp.front() == '"' && disp.back() == '"') {
            disp = disp.substr(1, disp.size() - 2);
            // Unescape the two backslash-escapes a quoted-string permits.
            std::string unescaped;
            unescaped.reserve(disp.size());
            for (size_t i = 0; i < disp.size(); ++i) {
                if (disp[i] == '\\' && i + 1 < disp.size()) {
                    ++i;
                }
                unescaped.push_back(disp[i]);
            }
            disp = unescaped;
        }
        if (disp.empty()) {
            return addr;
        }
        return disp + " <" + addr + ">";
    }
    return trim_copy(mailbox);
}

std::vector<std::string> canonical_address_values(const mime::Message& msg,
                                                    std::string_view field) {
    std::vector<std::string> out;
    for (const auto* h : find_all_ci(msg, field)) {
        std::string decoded = mime::EncodedWordDecoder::decode(h->value);
        for (const auto& mailbox : split_address_list(decoded)) {
            if (!mailbox.empty()) {
                out.push_back(canonical_mailbox(mailbox));
            }
        }
    }
    return out;
}

std::vector<std::string> canonical_text_values(const mime::Message& msg, std::string_view field) {
    std::vector<std::string> out;
    for (const auto* h : find_all_ci(msg, field)) {
        out.push_back(collapse_whitespace(mime::EncodedWordDecoder::decode(h->value)));
    }
    return out;
}

/// "message-id" / "in-reply-to" style values: angle brackets stripped.
std::string strip_angle_brackets(std::string_view v) {
    std::string s = trim_copy(v);
    if (s.size() >= 2 && s.front() == '<' && s.back() == '>') {
        s = s.substr(1, s.size() - 2);
    }
    return s;
}

std::optional<std::string> canonical_date(const mime::Message& msg) {
    const auto* hdr = find_ci(msg, "Date");
    if (hdr == nullptr) {
        return std::nullopt;
    }
    if (msg.date != nullptr && msg.date->valid) {
        const auto& d = *msg.date;
        char buf[48];
        if (d.tz_unknown) {
            std::snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d", d.year, d.month,
                          d.day, d.hour, d.minute, d.second);
        } else {
            int off = d.tz_offset_minutes;
            char sign = off < 0 ? '-' : '+';
            int abs_off = off < 0 ? -off : off;
            std::snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d%c%02d:%02d", d.year,
                          d.month, d.day, d.hour, d.minute, d.second, sign, abs_off / 60,
                          abs_off % 60);
        }
        return std::string(buf);
    }
    // Header present but unparseable: raw fallback, same convention Python
    // falls back to when it cannot build a datetime either.
    return collapse_whitespace(mime::EncodedWordDecoder::decode(hdr->value));
}

/// media type ("type/subtype", lowercase, no params) plus the charset
/// parameter of a Content-Type header, if any.
struct ContentTypeInfo {
    std::string media_type;
    std::string charset; // empty when absent
};

ContentTypeInfo content_type_info(const mime::Message& msg) {
    ContentTypeInfo info;
    const auto* ct = find_ci(msg, "Content-Type");
    if (ct == nullptr) {
        // RFC 2045 5.2 default: "text/plain; charset=us-ascii"
        info.media_type = "text/plain";
        info.charset = "us-ascii";
        return info;
    }
    info.media_type = ascii_lower_copy(mime::detail::media_type_of(ct->value));
    for (const auto& param : ct->parameters) {
        if (mime::detail::ascii_ieq(param.first, "charset")) {
            info.charset = canonical_charset_name(param.second);
            break;
        }
    }
    return info;
}

std::string content_transfer_encoding_value(const mime::Message& msg) {
    const auto* cte = find_ci(msg, "Content-Transfer-Encoding");
    if (cte == nullptr) {
        return "7bit"; // RFC 2045 6.1 default
    }
    return ascii_lower_copy(trim_copy(cte->value));
}

std::optional<std::string> content_disposition_type(const mime::Message& msg) {
    const auto* cd = find_ci(msg, "Content-Disposition");
    if (cd == nullptr) {
        return std::nullopt;
    }
    return ascii_lower_copy(mime::detail::media_type_of(cd->value));
}

std::optional<std::string> resolve_filename(const mime::Message& msg) {
    if (const auto* cd = find_ci(msg, "Content-Disposition")) {
        for (const auto& param : cd->parameters) {
            if (mime::detail::ascii_ieq(param.first, "filename")) {
                return std::string(param.second);
            }
        }
    }
    if (const auto* ct = find_ci(msg, "Content-Type")) {
        for (const auto& param : ct->parameters) {
            if (mime::detail::ascii_ieq(param.first, "name")) {
                return std::string(param.second);
            }
        }
    }
    return std::nullopt;
}

NodeData build_node(const mime::Message& msg) {
    NodeData node;
    ContentTypeInfo ct_info = content_type_info(msg);
    node.content_type = ct_info.media_type;

    auto add_header = [&](std::string key, std::vector<std::string> values) {
        if (!values.empty()) {
            node.headers.emplace_back(std::move(key), std::move(values));
        }
    };
    add_header("from", canonical_address_values(msg, "From"));
    add_header("to", canonical_address_values(msg, "To"));
    add_header("cc", canonical_address_values(msg, "Cc"));
    add_header("subject", canonical_text_values(msg, "Subject"));
    if (auto date = canonical_date(msg)) {
        add_header("date", {*date});
    }
    if (const auto* mid = find_ci(msg, "Message-ID")) {
        add_header("message-id", {strip_angle_brackets(msg.message_id != nullptr
                                                             ? msg.message_id->value
                                                             : mid->value)});
    }
    {
        std::string ct_value = ct_info.charset.empty()
                                    ? ct_info.media_type
                                    : (ct_info.media_type + "; charset=" + ct_info.charset);
        add_header("content-type", {ct_value});
    }
    add_header("content-transfer-encoding", {content_transfer_encoding_value(msg)});
    if (auto disp = content_disposition_type(msg)) {
        add_header("content-disposition", {*disp});
    }

    if (auto fname = resolve_filename(msg)) {
        node.filename = *fname;
    }

    const bool is_container = !msg.parts.empty() || msg.encapsulated != nullptr;
    if (is_container) {
        if (!msg.parts.empty()) {
            for (const auto* part : msg.parts) {
                if (part != nullptr) {
                    node.parts.push_back(build_node(*part));
                }
            }
        } else {
            node.parts.push_back(build_node(*msg.encapsulated));
        }
        return node;
    }

    auto decoded = mime::decoded_body(msg);
    if (!decoded) {
        node.body_decode_error = "bad_transfer_encoding";
        node.body_len = msg.body.size();
        node.body_digest = fnv1a64_hex(msg.body);
        return node;
    }
    node.body_len = decoded->size();
    const bool is_text = ct_info.media_type.rfind("text/", 0) == 0;
    if (is_text) {
        if (auto utf8 = mime::decoded_body_utf8(msg)) {
            node.body_text = *utf8;
            return node;
        }
        node.body_decode_error = "unsupported_charset:" +
                                  (ct_info.charset.empty() ? std::string("(none)") : ct_info.charset);
    }
    node.body_digest = fnv1a64_hex(*decoded);
    return node;
}

// ============================================================================
// Serialization
// ============================================================================

void indent_to(std::string& out, int indent) {
    out.append(static_cast<size_t>(indent) * 2, ' ');
}

void serialize_string_array(std::string& out, const std::vector<std::string>& values, int indent) {
    out += "[";
    for (size_t i = 0; i < values.size(); ++i) {
        if (i > 0) {
            out += ", ";
        }
        append_json_string(out, values[i]);
    }
    out += "]";
    (void)indent;
}

void serialize_node(std::string& out, const NodeData& node, int indent) {
    out += "{\n";
    indent_to(out, indent + 1);
    out += "\"content_type\": ";
    append_json_string(out, node.content_type);
    out += ",\n";

    indent_to(out, indent + 1);
    out += "\"headers\": {";
    if (node.headers.empty()) {
        out += "}";
    } else {
        out += "\n";
        for (size_t i = 0; i < node.headers.size(); ++i) {
            indent_to(out, indent + 2);
            append_json_string(out, node.headers[i].first);
            out += ": ";
            serialize_string_array(out, node.headers[i].second, indent + 2);
            out += (i + 1 < node.headers.size()) ? ",\n" : "\n";
        }
        indent_to(out, indent + 1);
        out += "}";
    }
    out += ",\n";

    if (node.filename) {
        indent_to(out, indent + 1);
        out += "\"filename\": ";
        append_json_string(out, *node.filename);
        out += ",\n";
    }
    if (node.body_len) {
        indent_to(out, indent + 1);
        out += "\"body_len\": " + std::to_string(*node.body_len);
        out += ",\n";
    }
    if (node.body_text) {
        indent_to(out, indent + 1);
        out += "\"body_text\": ";
        append_json_string(out, *node.body_text);
        out += ",\n";
    }
    if (node.body_decode_error) {
        indent_to(out, indent + 1);
        out += "\"body_decode_error\": ";
        append_json_string(out, *node.body_decode_error);
        out += ",\n";
    }
    if (node.body_digest) {
        indent_to(out, indent + 1);
        out += "\"body_digest\": ";
        append_json_string(out, *node.body_digest);
        out += ",\n";
    }

    indent_to(out, indent + 1);
    out += "\"parts\": [";
    if (node.parts.empty()) {
        out += "]";
    } else {
        out += "\n";
        for (size_t i = 0; i < node.parts.size(); ++i) {
            indent_to(out, indent + 2);
            serialize_node(out, node.parts[i], indent + 2);
            out += (i + 1 < node.parts.size()) ? ",\n" : "\n";
        }
        indent_to(out, indent + 1);
        out += "]";
    }
    out += "\n";
    indent_to(out, indent);
    out += "}";
}

std::string read_file(const std::string& path, bool& ok) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        ok = false;
        return {};
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    ok = true;
    return ss.str();
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: mime_dump MESSAGE_FILE\n");
        return 2;
    }

    bool ok = false;
    const std::string raw = read_file(argv[1], ok);
    if (!ok) {
        std::fprintf(stderr, "error: cannot read %s\n", argv[1]);
        return 2;
    }

    libglot::Arena arena;
    std::string out;
    out += "{\n";
    try {
        const mime::ParseResult result = mime::parse_message(arena, raw);
        out += "  \"parse_error\": false,\n";
        out += std::string("  \"rejected\": ") + (result.rejected ? "true" : "false") + ",\n";
        out += "  \"root\": ";
        serialize_node(out, build_node(*result.message), 1);
        out += "\n";
    } catch (const libglot::ParseError&) {
        out += "  \"parse_error\": true,\n";
        out += "  \"rejected\": false\n";
    }
    out += "}\n";

    std::fwrite(out.data(), 1, out.size(), stdout);
    return 0;
}
