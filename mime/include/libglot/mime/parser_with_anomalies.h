#pragma once

#include "parser_extended.h"
#include "anomalies.h"

namespace libglot::mime {

/// ============================================================================
/// MIME Parser with Anomaly Detection
/// ============================================================================
///
/// Extends MimeParserExtended with post-parse structural anomaly detection
/// (duplicate Content-Type, missing boundary parameter, ...). Anomalies
/// detected during parsing itself (missing final boundary, nesting depth
/// exceeded, ...) are recorded by the base class into the same report,
/// which uses the AnomalyReport/AnomalyRecord types from anomalies.h.
/// ============================================================================

class MimeParserWithAnomalies : public MimeParserExtended {
public:
    MimeParserWithAnomalies(libglot::Arena& arena, std::string_view source,
                            AnomalyConfig config = AnomalyConfig::standard(),
                            ParserLimits limits = ParserLimits::standard())
        : MimeParserExtended(arena, source, limits)
        , config_(config)
    {}

    /// Parse message and detect anomalies
    Message* parse_with_anomaly_detection() {
        auto* msg = parse_message_multipart();

        // Detect structural anomalies
        detect_missing_headers(msg);
        detect_duplicate_headers(msg);
        detect_content_type_issues(msg);
        detect_boundary_issues(msg);

        return msg;
    }

    /// Get the anomaly report (parse-time + structural anomalies)
    [[nodiscard]] const AnomalyReport& anomaly_report() const noexcept {
        return report_;
    }

private:
    AnomalyConfig config_;

    void detect_missing_headers(Message* msg) {
        bool has_mime_version = false;
        bool has_content_type = false;

        for (auto* header : msg->headers) {
            if (header->field == "MIME-Version" || header->field == "mime-version") {
                has_mime_version = true;
            }
            if (header->field == "Content-Type" || header->field == "content-type") {
                has_content_type = true;
            }
        }

        if (!has_mime_version && !msg->parts.empty()) {
            record_anomaly(AnomalyKind::MissingMIMEVersion,
                           "multipart message lacks a MIME-Version header");
        }

        if (!has_content_type && !msg->parts.empty()) {
            record_anomaly(AnomalyKind::MissingContentType,
                           "multipart message lacks a Content-Type header");
        }
    }

    void detect_duplicate_headers(Message* msg) {
        std::vector<std::string_view> seen_headers;

        for (auto* header : msg->headers) {
            std::string_view field = header->field;

            // Check for duplicates of critical headers
            if (field == "Content-Type" || field == "content-type") {
                for (auto seen : seen_headers) {
                    if (seen == field) {
                        record_anomaly(AnomalyKind::DuplicateContentType,
                                       "message contains multiple Content-Type headers");
                        break;
                    }
                }
            }

            seen_headers.push_back(field);
        }
    }

    void detect_content_type_issues(Message* msg) {
        for (auto* header : msg->headers) {
            if (header->field == "Content-Type" || header->field == "content-type") {
                std::string_view value = header->value;

                // Check for missing subtype (e.g., "text" instead of "text/plain")
                if (value.find('/') == std::string_view::npos) {
                    record_anomaly(AnomalyKind::MissingMediaSubtype,
                                   "Content-Type lacks a media subtype");
                }

                // Check for missing charset in text/* types
                if (value.find("text/") == 0) {
                    bool has_charset = false;
                    for (const auto& param : header->parameters) {
                        if (param.first == "charset") {
                            has_charset = true;
                            break;
                        }
                    }
                    if (!has_charset) {
                        record_anomaly(AnomalyKind::MissingCharsetInfo,
                                       "text/* Content-Type lacks a charset parameter");
                    }
                }
            }
        }
    }

    void detect_boundary_issues(Message* msg) {
        for (auto* header : msg->headers) {
            if (header->field == "Content-Type" || header->field == "content-type") {
                std::string_view value = header->value;

                // Check if multipart but missing boundary
                if (value.find("multipart/") == 0) {
                    bool has_boundary = false;
                    bool boundary_empty = false;

                    for (const auto& param : header->parameters) {
                        if (param.first == "boundary") {
                            has_boundary = true;
                            if (param.second.empty()) {
                                boundary_empty = true;
                            }
                            break;
                        }
                    }

                    if (!has_boundary) {
                        record_anomaly(AnomalyKind::MissingBoundaryParameter,
                                       "multipart Content-Type lacks a boundary parameter");
                    } else if (boundary_empty) {
                        record_anomaly(AnomalyKind::EmptyBoundary,
                                       "multipart Content-Type has an empty boundary parameter");
                    }
                }
            }
        }
    }
};

} // namespace libglot::mime
