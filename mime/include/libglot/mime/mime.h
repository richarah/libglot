#pragma once

#include "charset.h"
#include "encoding.h"
#include "parser_extended.h"
#include <optional>
#include <string>
#include <string_view>

namespace libglot::mime {

/// ============================================================================
/// libglot MIME - Single Entry Point
/// ============================================================================
///
/// parse_message() is THE way to parse a MIME message. The pipeline:
///
///   raw bytes
///     -> header unfolding (RFC 5322 §2.2.3, header_folding.h)
///     -> header tokenization + field/value parsing (parser.h)
///     -> header enhancement (parser_extended.h):
///          * RFC 5322 comment stripping in structured fields
///          * parameter parsing, RFC 2231 continuations with percent- and
///            charset-decoding (complete_features.h)
///          * Content-Type syntax validation (mime_type_validator.h)
///          * RFC 5322 address-group parsing on address headers
///     -> body extraction, RFC 2046 multipart splitting (boundary.h) with
///        recursive part parsing, message/external-body references
///     -> limits enforcement (limits.h) + structural anomaly detection,
///        recorded against the AnomalyConfig policies (anomalies.h)
///
/// Anomaly policies are honored as follows: Ignore -> the anomaly is
/// dropped; Repair -> recorded, parsing continues; Reject -> recorded, and
/// for Security/DoS severity the parse stops descending and the result is
/// marked rejected.
///
/// Transfer decoding (base64 / quoted-printable, encoding.h) and charset
/// conversion to UTF-8 (charset.h) are exposed via decoded_body() and
/// decoded_body_utf8() below; RFC 2047 encoded words in header values are
/// decoded via EncodedWordDecoder (encoding.h).
///
/// Note: syntactically broken header sections (e.g. a line without ':')
/// throw libglot::ParseError, exactly like the underlying parser.
/// ============================================================================

/// Configuration for parse_message()
struct ParseOptions {
    ParserLimits limits = ParserLimits::standard();
    AnomalyConfig anomalies = AnomalyConfig::standard();
};

/// Result of parse_message()
struct ParseResult {
    /// Parsed message tree (never nullptr on return; partial when rejected)
    Message* message = nullptr;

    /// All anomalies recorded during the parse, per the configured policies
    AnomalyReport report;

    /// True when a Reject-policy anomaly of Security/DoS severity was hit;
    /// the message tree is then partial and should not be trusted.
    bool rejected = false;

    /// Convenience: was this specific anomaly recorded?
    [[nodiscard]] bool has_anomaly(AnomalyKind kind) const {
        for (const auto& rec : report.records) {
            if (rec.kind == kind) {
                return true;
            }
        }
        return false;
    }
};

/// Parse a MIME message through the full pipeline. The returned Message and
/// everything it points to live in (or alongside) `arena`.
inline ParseResult parse_message(libglot::Arena& arena, std::string_view raw,
                                 const ParseOptions& options = {}) {
    MimeParserExtended parser(arena, raw, options.limits, options.anomalies);
    ParseResult result;
    result.message = parser.parse_message_multipart();
    result.report = parser.anomalies();
    result.rejected = parser.rejected();
    return result;
}

/// ============================================================================
/// Decoded Body Retrieval
/// ============================================================================

/// Find a header by (case-insensitive) field name; nullptr when absent.
inline const Header* find_header(const Message& msg, std::string_view field) {
    for (const auto* header : msg.headers) {
        if (header && detail::ascii_ieq(header->field, field)) {
            return header;
        }
    }
    return nullptr;
}

/// Transfer-decode a part's body per its Content-Transfer-Encoding header
/// (base64 / quoted-printable; 7bit/8bit/binary pass through). Returns
/// std::nullopt when the declared base64 payload is invalid.
inline std::optional<std::string> decoded_body(const Message& part) {
    auto encoding = TransferEncoding::Encoding::SevenBit;
    if (const Header* cte = find_header(part, "Content-Transfer-Encoding")) {
        encoding = TransferEncoding::detect_encoding(cte->value);
    }
    if (encoding == TransferEncoding::Encoding::Base64) {
        return TransferEncoding::decode_base64_strict(part.body);
    }
    return TransferEncoding::decode_body(part.body, encoding);
}

/// Decode a text/* part's body to UTF-8: transfer-decode, then convert from
/// the Content-Type charset parameter (default: treated as UTF-8/US-ASCII
/// passthrough). Returns std::nullopt when the transfer encoding is broken
/// or the charset is unknown / not convertible.
inline std::optional<std::string> decoded_body_utf8(const Message& part) {
    auto decoded = decoded_body(part);
    if (!decoded) {
        return std::nullopt;
    }

    std::string_view charset_name;
    if (const Header* ct = find_header(part, "Content-Type")) {
        for (const auto& param : ct->parameters) {
            if (detail::ascii_ieq(param.first, "charset")) {
                charset_name = param.second;
                break;
            }
        }
    }
    if (charset_name.empty()) {
        // No declared charset: pass through (UTF-8 / US-ASCII assumption)
        return decoded;
    }

    auto charset = CharsetConverter::detect_charset(detail::ascii_lower(charset_name));
    switch (charset) {
    case CharsetConverter::Charset::UTF8:
    case CharsetConverter::Charset::USASCII:
        return decoded;
    case CharsetConverter::Charset::ISO88591:
        return CharsetConverter::iso88591_to_utf8(*decoded);
    case CharsetConverter::Charset::ISO885915:
        return CharsetConverter::iso885915_to_utf8(*decoded);
    case CharsetConverter::Charset::WINDOWS1252:
        return CharsetConverter::windows1252_to_utf8(*decoded);
    case CharsetConverter::Charset::UTF16:
    case CharsetConverter::Charset::UTF16BE:
    case CharsetConverter::Charset::UTF16LE:
        return CharsetConverter::to_utf8(*decoded, charset);
    default:
        // Unknown or unconvertible charset
        return std::nullopt;
    }
}

} // namespace libglot::mime
