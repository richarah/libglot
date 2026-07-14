#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace libglot::mime {

/// ============================================================================
/// RFC 2046 Boundary Delimiter Matching
/// ============================================================================
///
/// A boundary delimiter line is:
///   CRLF "--" boundary [ "--" ] *WSP CRLF
/// - It must start at the beginning of a line (position 0 or right after a
///   line break); boundary text appearing mid-line is part content.
/// - The line break immediately preceding the delimiter belongs to the
///   delimiter, not to the previous part's content.
/// - "--boundary--" is the close delimiter; content after it is the
///   epilogue, content before the first delimiter is the preamble.
/// ============================================================================

struct BoundaryDelimiter {
    bool found = false;
    bool is_close = false;    ///< Close delimiter ("--boundary--")
    size_t line_start = 0;    ///< Position of the "--" that starts the line
    size_t content_end = 0;   ///< End of preceding part content (excludes the
                              ///< line break owned by the delimiter)
    size_t next_pos = 0;      ///< Position just past the delimiter line
};

/// Find the next RFC 2046 boundary delimiter line at or after `from`.
inline BoundaryDelimiter find_boundary_delimiter(std::string_view body,
                                                 std::string_view boundary,
                                                 size_t from) {
    BoundaryDelimiter result;
    if (boundary.empty()) {
        return result;
    }

    const std::string marker = "--" + std::string(boundary);

    size_t pos = from;
    while (pos < body.size()) {
        size_t p = body.find(marker, pos);
        if (p == std::string_view::npos) {
            return result;
        }

        // Must be at the start of a line (CRLF, LF, or bare CR before it)
        if (p != 0 && body[p - 1] != '\n' && body[p - 1] != '\r') {
            pos = p + 1;
            continue;
        }

        size_t q = p + marker.size();
        bool is_close = false;
        if (body.substr(q, 2) == "--") {
            is_close = true;
            q += 2;
        }

        // Optional transport padding (whitespace) after the marker
        while (q < body.size() && (body[q] == ' ' || body[q] == '\t')) {
            ++q;
        }

        // The rest of the line must be empty (line break or end of body);
        // otherwise the boundary text merely appears as a prefix of some
        // longer token and this is NOT a delimiter line.
        if (q != body.size() && body[q] != '\n' && body[q] != '\r') {
            pos = p + 1;
            continue;
        }

        result.found = true;
        result.is_close = is_close;
        result.line_start = p;

        // The line break before the delimiter belongs to the delimiter
        size_t content_end = p;
        if (content_end > 0 && body[content_end - 1] == '\n') {
            --content_end;
            if (content_end > 0 && body[content_end - 1] == '\r') {
                --content_end;
            }
        } else if (content_end > 0 && body[content_end - 1] == '\r') {
            --content_end;
        }
        result.content_end = content_end;

        // Skip past the delimiter line's own break
        size_t next = q;
        if (next < body.size()) {
            if (body[next] == '\r') {
                ++next;
                if (next < body.size() && body[next] == '\n') {
                    ++next;
                }
            } else if (body[next] == '\n') {
                ++next;
            }
        }
        result.next_pos = next;
        return result;
    }

    return result;
}

} // namespace libglot::mime
