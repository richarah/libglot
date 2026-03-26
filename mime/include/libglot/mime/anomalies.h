#pragma once

#include "../../../../core/include/libglot/ast/location.h"
#include <string_view>
#include <vector>
#include <cstdint>

namespace libglot::mime {

/// ============================================================================
/// MIME Anomaly Handling - Production-Grade
/// ============================================================================
///
/// Comprehensive anomaly catalog based on:
/// - RFC 7103 (Advice for Safe Handling of Malformed Messages)
/// - MIMEminer (CCS'24 paper on MIME parser differentials)
/// - Real-world corpus analysis (Enron, SpamAssassin, phishing databases)
///
/// Severity levels allow for configurable tolerance:
/// - Cosmetic: Whitespace, formatting quirks
/// - Degraded: Violates spec but intent is clear
/// - Structural: Missing required fields, broken MIME structure
/// - Security: Potential for parser confusion or exploitation
/// - DoS: Resource exhaustion attacks
///
/// ============================================================================

/// Anomaly severity classification
enum class AnomalySeverity : uint8_t {
    /// Cosmetic issues: minor formatting quirks, no semantic impact
    /// Example: extra whitespace before boundary, missing trailing CRLF
    Cosmetic = 0,

    /// Degraded: violates spec but intent is clear, widely accepted
    /// Example: 8-bit characters in headers without encoding
    Degraded = 1,

    /// Structural: broken MIME structure, missing required fields
    /// Example: missing Content-Type, missing MIME-Version
    Structural = 2,

    /// Security: potential for parser confusion, interpretation mismatches
    /// Example: duplicate Content-Type, null bytes in headers
    Security = 3,

    /// DoS: resource exhaustion, algorithmic complexity attacks
    /// Example: excessive nesting depth, huge headers
    DoS = 4
};

/// Anomaly handling policy
enum class AnomalyPolicy : uint8_t {
    /// Ignore anomaly - parse as-is (dangerous, use only for testing)
    Ignore = 0,

    /// Repair anomaly - apply heuristic fix and continue parsing
    /// Example: strip null bytes, use first Content-Type
    Repair = 1,

    /// Reject anomaly - emit error and fail parsing
    /// Example: reject security-critical anomalies in strict mode
    Reject = 2
};

/// Comprehensive anomaly kind enumeration
enum class AnomalyKind : uint16_t {
    // ========================================================================
    // Line Ending Anomalies (RFC 7103 §6)
    // ========================================================================

    /// Bare CR (\\r) without LF
    NakedCR,

    /// Bare LF (\\n) without CR
    NakedLF,

    /// Mixed line endings (CRLF + LF + CR in same message)
    MixedLineEndings,

    // ========================================================================
    // Header Syntax Anomalies (RFC 7103 §7)
    // ========================================================================

    /// Obsolete header syntax (deprecated RFC 822 constructs)
    ObsoleteHeaderSyntax,

    /// Duplicate angle brackets in Message-ID: <<id>>
    DuplicateAngleBrackets,

    /// Missing angle brackets in Message-ID: id
    MissingAngleBrackets,

    /// Folded header line contains only whitespace
    WhitespaceOnlyFoldLine,

    /// 8-bit characters in header without encoding
    EightBitUnencoded,

    /// Missing charset parameter in Content-Type
    MissingCharsetInfo,

    /// Incorrect charset info (e.g., declared ISO-8859-1 but contains UTF-8)
    IncorrectCharsetInfo,

    /// Non-ASCII characters in unstructured header field
    NonAsciiInUnstructuredHeader,

    /// Extra colon in header field name: "X-Custom::value"
    ExtraColonInHeaderName,

    /// Space before colon: "Subject : value"
    SpaceBeforeColon,

    /// Missing space after colon: "Subject:value"
    MissingSpaceAfterColon,

    // ========================================================================
    // MIME Structural Anomalies (RFC 7103 §8, MIMEminer Cat 2)
    // ========================================================================

    /// Missing MIME-Version header
    MissingMIMEVersion,

    /// Duplicate Content-Type headers
    DuplicateContentType,

    /// Multiple values in single Content-Type header
    MultipleContentTypeValues,

    /// Boundary parameter defined multiple times
    DuplicateBoundaryDefinition,

    /// Empty boundary parameter: boundary=""
    EmptyBoundary,

    /// Missing boundary parameter in multipart Content-Type
    MissingBoundaryParameter,

    /// Missing final boundary (--boundary--)
    MissingFinalBoundary,

    /// Extra whitespace before boundary delimiter
    ExtraWhitespaceBeforeBoundary,

    /// Boundary delimiter found within quoted string
    BoundaryWithinQuotedString,

    /// MIME part with zero-length body
    ZeroLengthPart,

    /// MIME part missing Content-Type header
    MissingContentType,

    /// Typo in multipart subtype: "multipart/mixd" vs "multipart/mixed"
    MultipartTypo,

    // ========================================================================
    // Header Field Ambiguities (MIMEminer Cat 1)
    // ========================================================================

    /// Conflicting Content-Transfer-Encoding values
    ConflictingTransferEncoding,

    /// Null byte (\\x00) in header field value
    NullByteInHeader,

    /// Encoded-word (=?charset?encoding?text?=) in boundary parameter
    EncodedWordInBoundary,

    /// Encoded-word in parameter value (e.g., filename)
    EncodedWordInParameterValue,

    /// Truncated encoded-word: =?UTF-8?B?dGV (missing closing ?=)
    TruncatedEncodedWord,

    /// Malformed encoded-word delimiters: =?charset?
    MalformedEncodedWordDelimiters,

    /// Nested encoding ambiguity (multiple layers of encoding)
    NestedEncodingAmbiguity,

    // ========================================================================
    // Decoding Anomalies (MIMEminer Cat 3, RFC 7103 §8.2)
    // ========================================================================

    /// Invalid Base64 characters (not A-Z, a-z, 0-9, +, /, =)
    InvalidBase64Chars,

    /// Truncated Base64 data (length not multiple of 4)
    TruncatedBase64,

    /// Null byte in Base64-decoded data
    NullInBase64,

    /// Broken quoted-printable soft break (= at end of line without CRLF)
    BrokenQuotedPrintableSoftBreak,

    /// Invalid quoted-printable escape sequence (not =XX where X is hex)
    InvalidQuotedPrintableSequence,

    /// Quoted-printable non-ASCII passthrough (8-bit chars not encoded)
    QuotedPrintableNonAsciiPassthrough,

    /// Faulty Content-Transfer-Encoding (e.g., "7bit" but contains 8-bit)
    FaultyContentTransferEncoding,

    // ========================================================================
    // Body Anomalies (RFC 7103 §9)
    // ========================================================================

    /// Line exceeds 998 characters (RFC 5322 limit)
    OversizedLine,

    /// Binary data in text/* part (contains null bytes or control chars)
    BinaryInTextPart,

    // ========================================================================
    // DoS Prevention
    // ========================================================================

    /// Excessive MIME nesting depth (e.g., 100+ levels)
    ExcessiveNestingDepth,

    /// Excessive part count (e.g., 10,000+ parts)
    ExcessivePartCount,

    /// Excessive header size (e.g., 2MB+ headers)
    ExcessiveHeaderSize,

    /// Excessive line length (e.g., 1MB+ line)
    ExcessiveLineLength,

    /// Excessive message size
    ExcessiveMessageSize,

    // ========================================================================
    // Legacy Encoding
    // ========================================================================

    /// Uuencoded content (begin/end markers)
    UuencodedContent,

    /// BinHex content (HQX format)
    BinhexContent,

    /// yEnc content (yEnc encoding)
    YencContent,

    // ========================================================================
    // Charset Anomalies
    // ========================================================================

    /// Charset mismatch (declared vs actual)
    CharsetMismatch,

    /// UTF-8 BOM (EF BB BF) in body
    UTF8BOMInBody,

    /// UTF-8 BOM in header field value
    UTF8BOMInHeader,

    /// Unknown/unsupported charset
    UnknownCharset,

    /// Windows-1252 declared as ISO-8859-1 (common Microsoft error)
    Windows1252AsISO8859_1,

    // ========================================================================
    // Additional Real-World Anomalies
    // ========================================================================

    /// Invalid parameter syntax: name=value=extra
    InvalidParameterSyntax,

    /// Unquoted special characters in parameter value
    UnquotedSpecialChars,

    /// Missing closing quote in parameter value
    MissingClosingQuote,

    /// Invalid media type format
    InvalidMediaType,

    /// Missing media subtype (e.g., "text" instead of "text/plain")
    MissingMediaSubtype,

    /// Obsolete media type (e.g., "application/x-msdownload")
    ObsoleteMediaType,

    /// Invalid folding (whitespace-only line not at continuation point)
    InvalidFolding,

    /// Header field exceeds reasonable size
    ExcessiveHeaderFieldSize,

    /// Invalid date format in Date header
    InvalidDateFormat,

    /// Future date in Date header (more than 1 day ahead)
    FutureDateValue,

    /// Very old date in Date header (before 1970 or before MIME invention)
    AncientDateValue,

    // ========================================================================
    // Content-Disposition Anomalies
    // ========================================================================

    /// Invalid Content-Disposition value
    InvalidContentDisposition,

    /// Duplicate filename parameters
    DuplicateFilenameParameter,

    /// Invalid filename characters (null bytes, path separators)
    InvalidFilenameChars,

    /// Excessively long filename
    ExcessiveFilenameLength,

    // ========================================================================
    // Multipart Boundary Anomalies
    // ========================================================================

    /// Boundary appears in preamble (before first boundary delimiter)
    BoundaryInPreamble,

    /// Boundary appears in epilogue (after final boundary delimiter)
    BoundaryInEpilogue,

    /// Boundary delimiter without leading dashes (missing "--" prefix)
    MissingBoundaryPrefix,

    /// Malformed boundary delimiter (extra characters after boundary)
    MalformedBoundaryDelimiter,

    /// ========================================================================
    /// Sentinel (for iteration)
    /// ========================================================================
    ANOMALY_KIND_COUNT
};

/// Anomaly record - full context for reporting
struct AnomalyRecord {
    AnomalyKind kind;
    AnomalySeverity severity;
    AnomalyPolicy applied_policy;
    SourceLocation location;
    std::string_view context;    // Up to 80 chars of surrounding raw data
    std::string_view detail;     // Human-readable description
};

/// Anomaly report - attached to parsed messages
struct AnomalyReport {
    std::vector<AnomalyRecord> records;

    /// Add anomaly to report
    void add(AnomalyKind kind, AnomalySeverity severity, AnomalyPolicy policy,
             SourceLocation loc, std::string_view context, std::string_view detail) {
        records.push_back({kind, severity, policy, loc, context, detail});
    }

    /// Get count of anomalies at or above severity level
    [[nodiscard]] size_t count_at_severity(AnomalySeverity min_severity) const {
        size_t count = 0;
        for (const auto& rec : records) {
            if (rec.severity >= min_severity) {
                ++count;
            }
        }
        return count;
    }

    /// Check if report contains any security or DoS anomalies
    [[nodiscard]] bool has_critical_anomalies() const {
        return count_at_severity(AnomalySeverity::Security) > 0;
    }

    [[nodiscard]] bool empty() const { return records.empty(); }
    [[nodiscard]] size_t size() const { return records.size(); }
};

/// Anomaly configuration - defines policy per anomaly kind
struct AnomalyConfig {
    /// Per-anomaly policy overrides
    AnomalyPolicy policies[static_cast<size_t>(AnomalyKind::ANOMALY_KIND_COUNT)];

    /// Default policy for anomalies not explicitly configured
    AnomalyPolicy default_policy = AnomalyPolicy::Repair;

    // ========================================================================
    // Preset Configurations
    // ========================================================================

    /// Permissive: Repair everything
    static AnomalyConfig permissive() {
        AnomalyConfig cfg;
        cfg.default_policy = AnomalyPolicy::Repair;
        for (size_t i = 0; i < static_cast<size_t>(AnomalyKind::ANOMALY_KIND_COUNT); ++i) {
            cfg.policies[i] = AnomalyPolicy::Repair;
        }
        return cfg;
    }

    /// Standard: Repair cosmetic/degraded/structural, Reject security/DoS
    static AnomalyConfig standard() {
        AnomalyConfig cfg;
        cfg.default_policy = AnomalyPolicy::Repair;

        // Set policies based on severity
        for (size_t i = 0; i < static_cast<size_t>(AnomalyKind::ANOMALY_KIND_COUNT); ++i) {
            auto kind = static_cast<AnomalyKind>(i);
            auto severity = get_severity(kind);

            if (severity >= AnomalySeverity::Security) {
                cfg.policies[i] = AnomalyPolicy::Reject;
            } else {
                cfg.policies[i] = AnomalyPolicy::Repair;
            }
        }

        return cfg;
    }

    /// Strict: Reject everything except Cosmetic
    static AnomalyConfig strict() {
        AnomalyConfig cfg;
        cfg.default_policy = AnomalyPolicy::Reject;

        for (size_t i = 0; i < static_cast<size_t>(AnomalyKind::ANOMALY_KIND_COUNT); ++i) {
            auto kind = static_cast<AnomalyKind>(i);
            auto severity = get_severity(kind);

            if (severity == AnomalySeverity::Cosmetic) {
                cfg.policies[i] = AnomalyPolicy::Repair;
            } else {
                cfg.policies[i] = AnomalyPolicy::Reject;
            }
        }

        return cfg;
    }

    /// Paranoid: Reject everything
    static AnomalyConfig paranoid() {
        AnomalyConfig cfg;
        cfg.default_policy = AnomalyPolicy::Reject;
        for (size_t i = 0; i < static_cast<size_t>(AnomalyKind::ANOMALY_KIND_COUNT); ++i) {
            cfg.policies[i] = AnomalyPolicy::Reject;
        }
        return cfg;
    }

    /// Get policy for a specific anomaly kind
    [[nodiscard]] AnomalyPolicy get_policy(AnomalyKind kind) const {
        size_t index = static_cast<size_t>(kind);
        if (index >= static_cast<size_t>(AnomalyKind::ANOMALY_KIND_COUNT)) {
            return default_policy;
        }
        return policies[index];
    }

    /// Set policy for a specific anomaly kind
    void set_policy(AnomalyKind kind, AnomalyPolicy policy) {
        size_t index = static_cast<size_t>(kind);
        if (index < static_cast<size_t>(AnomalyKind::ANOMALY_KIND_COUNT)) {
            policies[index] = policy;
        }
    }

    // ========================================================================
    // Severity Mapping (Static)
    // ========================================================================

    /// Get severity level for an anomaly kind
    [[nodiscard]] static constexpr AnomalySeverity get_severity(AnomalyKind kind) {
        switch (kind) {
            // Cosmetic
            case AnomalyKind::ExtraWhitespaceBeforeBoundary:
            case AnomalyKind::MissingSpaceAfterColon:
            case AnomalyKind::SpaceBeforeColon:
                return AnomalySeverity::Cosmetic;

            // Degraded
            case AnomalyKind::NakedCR:
            case AnomalyKind::NakedLF:
            case AnomalyKind::MixedLineEndings:
            case AnomalyKind::EightBitUnencoded:
            case AnomalyKind::NonAsciiInUnstructuredHeader:
            case AnomalyKind::ObsoleteHeaderSyntax:
            case AnomalyKind::WhitespaceOnlyFoldLine:
            case AnomalyKind::InvalidDateFormat:
                return AnomalySeverity::Degraded;

            // Structural
            case AnomalyKind::MissingMIMEVersion:
            case AnomalyKind::MissingContentType:
            case AnomalyKind::MissingBoundaryParameter:
            case AnomalyKind::MissingFinalBoundary:
            case AnomalyKind::EmptyBoundary:
            case AnomalyKind::ZeroLengthPart:
            case AnomalyKind::MissingCharsetInfo:
            case AnomalyKind::TruncatedBase64:
            case AnomalyKind::InvalidQuotedPrintableSequence:
            case AnomalyKind::MultipartTypo:
            case AnomalyKind::InvalidMediaType:
            case AnomalyKind::MissingMediaSubtype:
                return AnomalySeverity::Structural;

            // Security
            case AnomalyKind::DuplicateContentType:
            case AnomalyKind::NullByteInHeader:
            case AnomalyKind::DuplicateBoundaryDefinition:
            case AnomalyKind::EncodedWordInBoundary:
            case AnomalyKind::ConflictingTransferEncoding:
            case AnomalyKind::NestedEncodingAmbiguity:
            case AnomalyKind::NullInBase64:
            case AnomalyKind::BinaryInTextPart:
            case AnomalyKind::CharsetMismatch:
            case AnomalyKind::Windows1252AsISO8859_1:
            case AnomalyKind::InvalidFilenameChars:
            case AnomalyKind::BoundaryWithinQuotedString:
            case AnomalyKind::FaultyContentTransferEncoding:
                return AnomalySeverity::Security;

            // DoS
            case AnomalyKind::ExcessiveNestingDepth:
            case AnomalyKind::ExcessivePartCount:
            case AnomalyKind::ExcessiveHeaderSize:
            case AnomalyKind::ExcessiveLineLength:
            case AnomalyKind::ExcessiveMessageSize:
            case AnomalyKind::OversizedLine:
            case AnomalyKind::ExcessiveHeaderFieldSize:
            case AnomalyKind::ExcessiveFilenameLength:
                return AnomalySeverity::DoS;

            // Default to Structural for anything not explicitly categorized
            default:
                return AnomalySeverity::Structural;
        }
    }
};

/// Get human-readable name for anomaly kind
[[nodiscard]] constexpr std::string_view anomaly_kind_name(AnomalyKind kind) {
    switch (kind) {
        case AnomalyKind::NakedCR: return "NakedCR";
        case AnomalyKind::NakedLF: return "NakedLF";
        case AnomalyKind::MixedLineEndings: return "MixedLineEndings";
        case AnomalyKind::ObsoleteHeaderSyntax: return "ObsoleteHeaderSyntax";
        case AnomalyKind::DuplicateAngleBrackets: return "DuplicateAngleBrackets";
        case AnomalyKind::MissingAngleBrackets: return "MissingAngleBrackets";
        case AnomalyKind::WhitespaceOnlyFoldLine: return "WhitespaceOnlyFoldLine";
        case AnomalyKind::EightBitUnencoded: return "EightBitUnencoded";
        case AnomalyKind::MissingCharsetInfo: return "MissingCharsetInfo";
        case AnomalyKind::IncorrectCharsetInfo: return "IncorrectCharsetInfo";
        case AnomalyKind::NonAsciiInUnstructuredHeader: return "NonAsciiInUnstructuredHeader";
        case AnomalyKind::ExtraColonInHeaderName: return "ExtraColonInHeaderName";
        case AnomalyKind::SpaceBeforeColon: return "SpaceBeforeColon";
        case AnomalyKind::MissingSpaceAfterColon: return "MissingSpaceAfterColon";
        case AnomalyKind::MissingMIMEVersion: return "MissingMIMEVersion";
        case AnomalyKind::DuplicateContentType: return "DuplicateContentType";
        case AnomalyKind::MultipleContentTypeValues: return "MultipleContentTypeValues";
        case AnomalyKind::DuplicateBoundaryDefinition: return "DuplicateBoundaryDefinition";
        case AnomalyKind::EmptyBoundary: return "EmptyBoundary";
        case AnomalyKind::MissingBoundaryParameter: return "MissingBoundaryParameter";
        case AnomalyKind::MissingFinalBoundary: return "MissingFinalBoundary";
        case AnomalyKind::ExtraWhitespaceBeforeBoundary: return "ExtraWhitespaceBeforeBoundary";
        case AnomalyKind::BoundaryWithinQuotedString: return "BoundaryWithinQuotedString";
        case AnomalyKind::ZeroLengthPart: return "ZeroLengthPart";
        case AnomalyKind::MissingContentType: return "MissingContentType";
        case AnomalyKind::MultipartTypo: return "MultipartTypo";
        case AnomalyKind::ConflictingTransferEncoding: return "ConflictingTransferEncoding";
        case AnomalyKind::NullByteInHeader: return "NullByteInHeader";
        case AnomalyKind::EncodedWordInBoundary: return "EncodedWordInBoundary";
        case AnomalyKind::EncodedWordInParameterValue: return "EncodedWordInParameterValue";
        case AnomalyKind::TruncatedEncodedWord: return "TruncatedEncodedWord";
        case AnomalyKind::MalformedEncodedWordDelimiters: return "MalformedEncodedWordDelimiters";
        case AnomalyKind::NestedEncodingAmbiguity: return "NestedEncodingAmbiguity";
        case AnomalyKind::InvalidBase64Chars: return "InvalidBase64Chars";
        case AnomalyKind::TruncatedBase64: return "TruncatedBase64";
        case AnomalyKind::NullInBase64: return "NullInBase64";
        case AnomalyKind::BrokenQuotedPrintableSoftBreak: return "BrokenQuotedPrintableSoftBreak";
        case AnomalyKind::InvalidQuotedPrintableSequence: return "InvalidQuotedPrintableSequence";
        case AnomalyKind::QuotedPrintableNonAsciiPassthrough: return "QuotedPrintableNonAsciiPassthrough";
        case AnomalyKind::FaultyContentTransferEncoding: return "FaultyContentTransferEncoding";
        case AnomalyKind::OversizedLine: return "OversizedLine";
        case AnomalyKind::BinaryInTextPart: return "BinaryInTextPart";
        case AnomalyKind::ExcessiveNestingDepth: return "ExcessiveNestingDepth";
        case AnomalyKind::ExcessivePartCount: return "ExcessivePartCount";
        case AnomalyKind::ExcessiveHeaderSize: return "ExcessiveHeaderSize";
        case AnomalyKind::ExcessiveLineLength: return "ExcessiveLineLength";
        case AnomalyKind::ExcessiveMessageSize: return "ExcessiveMessageSize";
        case AnomalyKind::UuencodedContent: return "UuencodedContent";
        case AnomalyKind::BinhexContent: return "BinhexContent";
        case AnomalyKind::YencContent: return "YencContent";
        case AnomalyKind::CharsetMismatch: return "CharsetMismatch";
        case AnomalyKind::UTF8BOMInBody: return "UTF8BOMInBody";
        case AnomalyKind::UTF8BOMInHeader: return "UTF8BOMInHeader";
        case AnomalyKind::UnknownCharset: return "UnknownCharset";
        case AnomalyKind::Windows1252AsISO8859_1: return "Windows1252AsISO8859_1";
        case AnomalyKind::InvalidParameterSyntax: return "InvalidParameterSyntax";
        case AnomalyKind::UnquotedSpecialChars: return "UnquotedSpecialChars";
        case AnomalyKind::MissingClosingQuote: return "MissingClosingQuote";
        case AnomalyKind::InvalidMediaType: return "InvalidMediaType";
        case AnomalyKind::MissingMediaSubtype: return "MissingMediaSubtype";
        case AnomalyKind::ObsoleteMediaType: return "ObsoleteMediaType";
        case AnomalyKind::InvalidFolding: return "InvalidFolding";
        case AnomalyKind::ExcessiveHeaderFieldSize: return "ExcessiveHeaderFieldSize";
        case AnomalyKind::InvalidDateFormat: return "InvalidDateFormat";
        case AnomalyKind::FutureDateValue: return "FutureDateValue";
        case AnomalyKind::AncientDateValue: return "AncientDateValue";
        case AnomalyKind::InvalidContentDisposition: return "InvalidContentDisposition";
        case AnomalyKind::DuplicateFilenameParameter: return "DuplicateFilenameParameter";
        case AnomalyKind::InvalidFilenameChars: return "InvalidFilenameChars";
        case AnomalyKind::ExcessiveFilenameLength: return "ExcessiveFilenameLength";
        case AnomalyKind::BoundaryInPreamble: return "BoundaryInPreamble";
        case AnomalyKind::BoundaryInEpilogue: return "BoundaryInEpilogue";
        case AnomalyKind::MissingBoundaryPrefix: return "MissingBoundaryPrefix";
        case AnomalyKind::MalformedBoundaryDelimiter: return "MalformedBoundaryDelimiter";
        default: return "Unknown";
    }
}

/// Get human-readable name for severity level
[[nodiscard]] constexpr std::string_view anomaly_severity_name(AnomalySeverity severity) {
    switch (severity) {
        case AnomalySeverity::Cosmetic: return "Cosmetic";
        case AnomalySeverity::Degraded: return "Degraded";
        case AnomalySeverity::Structural: return "Structural";
        case AnomalySeverity::Security: return "Security";
        case AnomalySeverity::DoS: return "DoS";
        default: return "Unknown";
    }
}

/// Get human-readable name for policy
[[nodiscard]] constexpr std::string_view anomaly_policy_name(AnomalyPolicy policy) {
    switch (policy) {
        case AnomalyPolicy::Ignore: return "Ignore";
        case AnomalyPolicy::Repair: return "Repair";
        case AnomalyPolicy::Reject: return "Reject";
        default: return "Unknown";
    }
}

} // namespace libglot::mime
