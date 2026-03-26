#pragma once

#include "parser_extended.h"
#include "anomalies.h"
#include <vector>

namespace libglot::mime {

/// ============================================================================
/// Anomaly Report - Tracks detected anomalies during parsing
/// ============================================================================

struct AnomalyReport {
    std::vector<AnomalyKind> anomalies;

    void add(AnomalyKind kind) {
        anomalies.push_back(kind);
    }

    bool has_anomalies() const {
        return !anomalies.empty();
    }

    bool has_critical_anomalies() const {
        for (auto kind : anomalies) {
            if (get_severity(kind) >= AnomalySeverity::Security) {
                return true;
            }
        }
        return false;
    }

    size_t count_by_severity(AnomalySeverity severity) const {
        size_t count = 0;
        for (auto kind : anomalies) {
            if (get_severity(kind) == severity) {
                ++count;
            }
        }
        return count;
    }
};

/// ============================================================================
/// MIME Parser with Anomaly Detection
/// ============================================================================

class MimeParserWithAnomalies : public MimeParserExtended {
public:
    MimeParserWithAnomalies(libglot::Arena& arena, std::string_view source, AnomalyConfig config = AnomalyConfig::standard())
        : MimeParserExtended(arena, source)
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

    /// Get the anomaly report
    const AnomalyReport& anomaly_report() const {
        return report_;
    }

private:
    AnomalyConfig config_;
    AnomalyReport report_;

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
            report_.add(AnomalyKind::MissingMIMEVersion);
        }

        if (!has_content_type && !msg->parts.empty()) {
            report_.add(AnomalyKind::MissingContentType);
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
                        report_.add(AnomalyKind::DuplicateContentType);
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
                    report_.add(AnomalyKind::MissingMediaSubtype);
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
                        report_.add(AnomalyKind::MissingCharsetInfo);
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
                        report_.add(AnomalyKind::MissingBoundaryParameter);
                    } else if (boundary_empty) {
                        report_.add(AnomalyKind::EmptyBoundary);
                    }
                }
            }
        }
    }
};

} // namespace libglot::mime
