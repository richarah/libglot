#pragma once

#include "anomalies.h"
#include "boundary.h"
#include "header_folding.h"
#include <algorithm>
#include <cctype>
#include <charconv>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace libglot::mime {

/// ============================================================================
/// MIME Utility Classes - RFC Corner Cases
/// ============================================================================
///
/// Standalone helpers for the trickier corners of the MIME RFCs. They are
/// wired into the main pipeline (see mime.h / parser_extended.h) so a normal
/// parse benefits from them automatically, and remain directly usable:
///
/// 1. RFC 2231 parameter continuations (RFC2231Parser)
/// 2. Comment parsing in headers, RFC 5322 (HeaderCommentParser)
/// 3. Address group syntax (AddressGroupParser)
/// 4. Boundary error recovery (BoundaryRecovery)
/// 5. message/external-body support (ExternalBodyParser)
/// ============================================================================

/// ============================================================================
/// RFC 2231 Parameter Continuations
/// ============================================================================

class RFC2231Parser {
public:
    struct ContinuedParameter {
        std::string name;
        std::string value;
        std::string charset;
        std::string language;
        bool encoded;
    };

    /// Parse continued parameters: name*0=value0; name*1=value1; name*2=value2
    /// If `report` is non-null, invalid RFC 2231 percent-encoding is recorded
    /// there instead of aborting the parse.
    static std::unordered_map<std::string, ContinuedParameter> parse_continued_parameters(
        const std::vector<std::pair<std::string_view, std::string_view>>& params,
        AnomalyReport* report = nullptr) {
        std::unordered_map<std::string, std::vector<std::pair<int, std::string>>> fragments;
        std::unordered_map<std::string, bool> encoded_flags;
        std::unordered_map<std::string, std::string> charsets;
        std::unordered_map<std::string, std::string> languages;

        for (const auto& [key, value] : params) {
            std::string key_str(key);

            // Check for parameter continuation: name*N or name*N*
            size_t star_pos = key_str.find('*');
            if (star_pos == std::string::npos)
                continue;

            std::string base_name = key_str.substr(0, star_pos);
            std::string suffix = key_str.substr(star_pos + 1);

            // Check if it's encoded (ends with *)
            bool is_encoded = suffix.ends_with('*');
            if (is_encoded) {
                suffix = suffix.substr(0, suffix.length() - 1);
            }

            // Extract sequence number (attacker-controlled: parse defensively,
            // treating malformed or out-of-range values as section 0)
            int seq = 0;
            if (!suffix.empty() && std::isdigit(static_cast<unsigned char>(suffix[0]))) {
                auto [ptr, ec] = std::from_chars(suffix.data(), suffix.data() + suffix.size(), seq);
                if (ec != std::errc()) {
                    seq = 0;
                }
            }

            // First fragment (seq=0) may contain charset and language
            std::string value_str(value);
            if (seq == 0 && is_encoded) {
                // Format: charset'language'value
                size_t first_quote = value_str.find('\'');
                if (first_quote != std::string::npos) {
                    charsets[base_name] = value_str.substr(0, first_quote);
                    size_t second_quote = value_str.find('\'', first_quote + 1);
                    if (second_quote != std::string::npos) {
                        languages[base_name] =
                            value_str.substr(first_quote + 1, second_quote - first_quote - 1);
                        value_str = value_str.substr(second_quote + 1);
                    }
                }
            }

            fragments[base_name].push_back({seq, value_str});
            encoded_flags[base_name] = encoded_flags[base_name] || is_encoded;
        }

        // Reassemble fragments
        std::unordered_map<std::string, ContinuedParameter> result;
        for (auto& [name, frags] : fragments) {
            // Sort by sequence number
            std::sort(frags.begin(), frags.end(),
                      [](const auto& a, const auto& b) { return a.first < b.first; });

            ContinuedParameter param;
            param.name = name;
            param.encoded = encoded_flags[name];
            param.charset = charsets[name];
            param.language = languages[name];

            // Concatenate all fragments
            for (const auto& [seq, val] : frags) {
                param.value += val;
            }

            // Decode if encoded
            if (param.encoded) {
                bool invalid_encoding = false;
                param.value = percent_decode(param.value, &invalid_encoding);
                if (invalid_encoding && report) {
                    report->add(AnomalyKind::InvalidParameterSyntax,
                                AnomalyConfig::get_severity(AnomalyKind::InvalidParameterSyntax),
                                AnomalyPolicy::Repair, SourceLocation{}, "",
                                "invalid RFC 2231 percent-encoding in parameter value");
                }
            }

            result[name] = param;
        }

        return result;
    }

private:
    /// Decode %XX percent-encoding. The input is attacker-controlled, so both
    /// hex digits are validated by hand (no std::stoi, which throws on
    /// malformed input). Invalid sequences such as "%ZZ" or a truncated "%X"
    /// are kept literally and flagged via `invalid` when provided.
    static std::string percent_decode(const std::string& encoded, bool* invalid = nullptr) {
        std::string result;
        result.reserve(encoded.length());

        for (size_t i = 0; i < encoded.length(); i++) {
            if (encoded[i] == '%') {
                if (i + 2 < encoded.length()) {
                    int hi = hex_digit_value(encoded[i + 1]);
                    int lo = hex_digit_value(encoded[i + 2]);
                    if (hi >= 0 && lo >= 0) {
                        result += static_cast<char>((hi << 4) | lo);
                        i += 2;
                        continue;
                    }
                }
                // Invalid or truncated %XX sequence: keep literally
                if (invalid) {
                    *invalid = true;
                }
                result += encoded[i];
            } else {
                result += encoded[i];
            }
        }
        return result;
    }

    static int hex_digit_value(char c) {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'A' && c <= 'F')
            return c - 'A' + 10;
        if (c >= 'a' && c <= 'f')
            return c - 'a' + 10;
        return -1;
    }
};

/// ============================================================================
/// Comment Parsing in Headers (RFC 5322)
/// ============================================================================

class HeaderCommentParser {
public:
    /// Parse header with comments: From: John Doe (CEO) <john@example.com>
    static std::string remove_comments(std::string_view header) {
        std::string result;
        int depth = 0;
        bool in_quote = false;
        bool escaped = false;

        for (char c : header) {
            if (escaped) {
                if (depth == 0)
                    result += c;
                escaped = false;
                continue;
            }

            if (c == '\\') {
                escaped = true;
                if (depth == 0)
                    result += c;
                continue;
            }

            if (c == '"') {
                in_quote = !in_quote;
                if (depth == 0)
                    result += c;
                continue;
            }

            if (!in_quote) {
                if (c == '(') {
                    depth++;
                    continue;
                } else if (c == ')') {
                    if (depth > 0)
                        depth--;
                    continue;
                }
            }

            if (depth == 0) {
                result += c;
            }
        }

        return result;
    }

    /// Extract comments from header
    static std::vector<std::string> extract_comments(std::string_view header) {
        std::vector<std::string> comments;
        std::string current_comment;
        int depth = 0;
        bool in_quote = false;
        bool escaped = false;

        for (char c : header) {
            if (escaped) {
                if (depth > 0)
                    current_comment += c;
                escaped = false;
                continue;
            }

            if (c == '\\') {
                escaped = true;
                if (depth > 0)
                    current_comment += c;
                continue;
            }

            if (c == '"') {
                in_quote = !in_quote;
                continue;
            }

            if (!in_quote) {
                if (c == '(') {
                    if (depth == 0)
                        current_comment.clear();
                    depth++;
                    continue;
                } else if (c == ')') {
                    depth--;
                    if (depth == 0) {
                        comments.push_back(current_comment);
                    }
                    continue;
                }
            }

            if (depth > 0) {
                current_comment += c;
            }
        }

        return comments;
    }
};

/// ============================================================================
/// Address Group Syntax (RFC 5322)
/// ============================================================================

struct AddressGroup {
    std::string group_name;
    std::vector<std::string> addresses;
};

class AddressGroupParser {
public:
    /// Parse address groups: Executives: john@example.com, jane@example.com;
    static std::vector<AddressGroup> parse(std::string_view header_value) {
        std::vector<AddressGroup> groups;

        size_t pos = 0;
        while (pos < header_value.length()) {
            // Look for group syntax: group_name: addr1, addr2;
            size_t colon = header_value.find(':', pos);
            if (colon == std::string::npos)
                break;

            AddressGroup group;
            group.group_name = std::string(trim(header_value.substr(pos, colon - pos)));

            // Find the semicolon that ends the group
            size_t semi = header_value.find(';', colon);
            if (semi == std::string::npos)
                semi = header_value.length();

            // Parse addresses in the group
            std::string_view addrs = header_value.substr(colon + 1, semi - colon - 1);
            size_t addr_pos = 0;
            while (addr_pos < addrs.length()) {
                size_t comma = addrs.find(',', addr_pos);
                if (comma == std::string::npos)
                    comma = addrs.length();

                std::string addr(trim(addrs.substr(addr_pos, comma - addr_pos)));
                if (!addr.empty()) {
                    group.addresses.push_back(addr);
                }

                addr_pos = comma + 1;
            }

            groups.push_back(group);
            pos = semi + 1;
        }

        return groups;
    }

private:
    static std::string_view trim(std::string_view str) {
        size_t start = 0;
        while (start < str.length() && std::isspace(static_cast<unsigned char>(str[start])))
            start++;
        size_t end = str.length();
        while (end > start && std::isspace(static_cast<unsigned char>(str[end - 1])))
            end--;
        return str.substr(start, end - start);
    }
};

/// ============================================================================
/// Boundary Error Recovery
/// ============================================================================

class BoundaryRecovery {
public:
    /// Auto-detect boundary when Content-Type is missing or incorrect
    static std::string detect_boundary(std::string_view body) {
        // Look for lines starting with "--": both part delimiters
        // ("--boundary") and close delimiters ("--boundary--") count as
        // occurrences of the same boundary candidate.
        size_t pos = 0;
        std::unordered_map<std::string, int> boundary_candidates;

        while ((pos = body.find("--", pos)) != std::string_view::npos) {
            // Boundary delimiters only occur at the start of a line
            if (pos != 0 && body[pos - 1] != '\n' && body[pos - 1] != '\r') {
                pos += 1;
                continue;
            }

            size_t end = body.find_first_of("\r\n", pos);
            if (end == std::string_view::npos)
                end = body.length();

            std::string_view candidate = body.substr(pos + 2, end - pos - 2);

            // Strip transport padding and a trailing "--" (close delimiter)
            while (!candidate.empty() && (candidate.back() == ' ' || candidate.back() == '\t')) {
                candidate.remove_suffix(1);
            }
            if (candidate.size() >= 2 && candidate.substr(candidate.size() - 2) == "--") {
                candidate.remove_suffix(2);
            }
            while (!candidate.empty() && (candidate.back() == ' ' || candidate.back() == '\t')) {
                candidate.remove_suffix(1);
            }

            if (!candidate.empty()) {
                boundary_candidates[std::string(candidate)]++;
            }

            pos = end;
        }

        // Return most common boundary
        std::string best_boundary;
        int max_count = 0;
        for (const auto& [boundary, count] : boundary_candidates) {
            if (count > max_count && count > 1) { // Must appear at least twice
                max_count = count;
                best_boundary = boundary;
            }
        }

        return best_boundary;
    }

    /// Split a multipart body on RFC 2046 boundary delimiter lines, with
    /// recovery when the final close delimiter is missing (the remainder of
    /// the body becomes the last part). Preamble (before the first delimiter)
    /// and epilogue (after the close delimiter) are discarded; boundary text
    /// appearing mid-line inside part content does not split.
    static std::vector<std::string_view> split_with_recovery(std::string_view body,
                                                             std::string_view boundary) {
        std::vector<std::string_view> parts;

        auto delim = find_boundary_delimiter(body, boundary, 0);
        if (!delim.found)
            return parts;

        bool closed = delim.is_close;
        size_t part_start = delim.next_pos;

        while (!closed) {
            auto next = find_boundary_delimiter(body, boundary, part_start);

            if (!next.found) {
                // Missing final close delimiter: recover by taking the rest
                parts.push_back(body.substr(part_start));
                break;
            }

            size_t content_end = std::max(next.content_end, part_start);
            parts.push_back(body.substr(part_start, content_end - part_start));

            closed = next.is_close;
            part_start = next.next_pos;
        }

        return parts;
    }
};

/// ============================================================================
/// message/external-body Support (RFC 2046 Section 5.2.3)
/// ============================================================================

struct ExternalBodyRef {
    std::string access_type; // ftp, http, local-file, mail-server
    std::string name;        // Filename
    std::string site;        // FTP/HTTP server
    std::string directory;   // Directory path
    std::string server;      // Mail server
    std::string subject;     // Mail subject
    size_t size;             // File size in octets
    std::string expiration;  // Expiration date
};

class ExternalBodyParser {
public:
    static ExternalBodyRef
    parse(const std::vector<std::pair<std::string_view, std::string_view>>& params) {
        ExternalBodyRef ref;
        ref.size = 0;

        for (const auto& [key, value] : params) {
            std::string key_lower(key);
            std::transform(key_lower.begin(), key_lower.end(), key_lower.begin(), ::tolower);

            if (key_lower == "access-type") {
                ref.access_type = value;
            } else if (key_lower == "name") {
                ref.name = value;
            } else if (key_lower == "site") {
                ref.site = value;
            } else if (key_lower == "directory") {
                ref.directory = value;
            } else if (key_lower == "server") {
                ref.server = value;
            } else if (key_lower == "subject") {
                ref.subject = value;
            } else if (key_lower == "size") {
                // Attacker-controlled numeric parameter: parse with
                // std::from_chars (no exceptions). Malformed or out-of-range
                // values are ignored and size stays 0.
                size_t parsed = 0;
                auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), parsed);
                if (ec == std::errc() && ptr == value.data() + value.size()) {
                    ref.size = parsed;
                }
            } else if (key_lower == "expiration") {
                ref.expiration = value;
            }
        }

        return ref;
    }
};

/// ============================================================================
/// message/partial Support (RFC 2046 Section 5.2.2)
/// ============================================================================
///
/// A large message split across several message/partial fragments carries
/// id/number/total parameters on Content-Type identifying the fragment.
/// Reassembly (collecting fragments sharing `id`, ordering by `number` up to
/// `total`, and concatenating their bodies) is out of scope here: this only
/// detects the reference and exposes its parameters so a caller can perform
/// (or refuse) reassembly.
/// ============================================================================

struct MessagePartialRef {
    std::string id; // Shared identifier across all fragments of one message
    int number = 0; // This fragment's 1-based sequence number (0 = absent/invalid)
    int total = 0;  // Total fragment count (0 = absent/invalid)
};

class MessagePartialParser {
public:
    /// Parse id/number/total parameters off a `Content-Type: message/partial`
    /// header. `number` and `total` are attacker-controlled: parsed with
    /// std::from_chars (never throws); a malformed, negative, or
    /// out-of-range value leaves the field at 0 rather than propagating
    /// garbage.
    static MessagePartialRef
    parse(const std::vector<std::pair<std::string_view, std::string_view>>& params) {
        MessagePartialRef ref;

        for (const auto& [key, value] : params) {
            std::string key_lower(key);
            std::transform(key_lower.begin(), key_lower.end(), key_lower.begin(), ::tolower);

            if (key_lower == "id") {
                ref.id = value;
            } else if (key_lower == "number") {
                ref.number = parse_positive_int(value);
            } else if (key_lower == "total") {
                ref.total = parse_positive_int(value);
            }
        }

        return ref;
    }

private:
    static int parse_positive_int(std::string_view value) {
        int parsed = 0;
        auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), parsed);
        if (ec == std::errc() && ptr == value.data() + value.size() && parsed > 0) {
            return parsed;
        }
        return 0;
    }
};

/// ============================================================================
/// Date: Parsing (RFC 5322 Section 3.3, date-time)
/// ============================================================================
///
/// Parses the RFC 5322 date-time grammar:
///
///   date-time = [ day-of-week "," ] date time [CFWS]
///   date      = day month year
///   time      = time-of-day zone
///
/// including the obsolete forms permitted by RFC 5322 Section 4.3:
/// - obs-year: 2 or 3 digit years. Per Section 4.3, a 2-digit year 00-49 is
///   interpreted as 20xx, 50-99 (and any 3-digit year) as 19xx.
/// - obs-zone: the named zones UT/GMT (+0000) and the North American zones
///   EST/EDT/CST/CDT/MST/MDT/PST/PDT (fixed offsets), plus the single-letter
///   "military" zones. RFC 5322 Section 4.3 explicitly says the military
///   zones were mis-defined by RFC 822 and are unpredictable, so "they
///   SHOULD all be considered equivalent to '-0000'" -- i.e. offset unknown.
/// - "-0000" itself (Section 3.3): a legal zero offset, but one that
///   indicates the offset is not reliably known (as opposed to "+0000").
///
/// Leap seconds (second == 60) are accepted, matching the `second = 2DIGIT`
/// grammar (0-60, RFC 5322 does not exclude the leap-second value).
///
/// Folded header values are already unfolded upstream (HeaderFolding, RFC
/// 5322 Section 2.2.3) and RFC 5322 comments in the Date field are already
/// stripped upstream (Date is in MimeParserExtended::is_structured_field),
/// so this parser only has to deal with plain whitespace.
///
/// Never throws: a value that does not match the grammar, or that has an
/// out-of-range field (month 13, day 32, Feb 30, hour 24, ...) simply
/// yields `valid == false` so the caller can record an anomaly instead.
/// ============================================================================

struct ParsedDateTime {
    std::string_view raw; // Original header value, verbatim
    int year = 0;
    int month = 0;             // 1-12
    int day = 0;               // 1-31
    int hour = 0;               // 0-23
    int minute = 0;             // 0-59
    int second = 0;             // 0-60 (60 = leap second)
    int tz_offset_minutes = 0; // Minutes east of UTC
    bool tz_unknown = false;   // "-0000" or an obsolete military zone: the
                               // offset is a placeholder, not reliable info
                               // (RFC 5322 §3.3, §4.3)
    bool valid = false;
};

class DateTimeParser {
public:
    static ParsedDateTime parse(std::string_view value) {
        ParsedDateTime out;
        out.raw = value;

        std::string_view s = trim(value);

        // Optional day-of-week ("Mon, ...") -- RFC 5322 day-of-week /
        // obs-day-of-week. Not validated against the actual weekday (many
        // real-world messages get it wrong); only the syntax is consumed.
        size_t comma = s.find(',');
        if (comma != std::string_view::npos) {
            std::string_view before = trim(s.substr(0, comma));
            if (!before.empty() && before.size() <= 3 &&
                std::all_of(before.begin(), before.end(),
                            [](char c) { return std::isalpha(static_cast<unsigned char>(c)); })) {
                s = trim(s.substr(comma + 1));
            }
        }

        size_t pos = 0;
        int day = 0;
        if (!parse_digits(s, pos, 1, 2, day)) {
            return out;
        }
        if (!skip_required_ws(s, pos)) {
            return out;
        }

        int month = parse_month_name(s, pos);
        if (month == 0) {
            return out;
        }
        if (!skip_required_ws(s, pos)) {
            return out;
        }

        size_t year_start = pos;
        while (pos < s.size() && std::isdigit(static_cast<unsigned char>(s[pos]))) {
            ++pos;
        }
        size_t year_digits = pos - year_start;
        if (year_digits < 2) {
            return out;
        }
        int year_val = 0;
        auto [yptr, yec] =
            std::from_chars(s.data() + year_start, s.data() + pos, year_val);
        if (yec != std::errc()) {
            return out;
        }
        int year;
        if (year_digits == 2) {
            // RFC 5322 §4.3 obs-year rule
            year = (year_val <= 49) ? 2000 + year_val : 1900 + year_val;
        } else if (year_digits == 3) {
            year = 1900 + year_val;
        } else {
            year = year_val;
        }
        if (!skip_required_ws(s, pos)) {
            return out;
        }

        int hour = 0;
        int minute = 0;
        int second = 0;
        if (!parse_digits(s, pos, 1, 2, hour)) {
            return out;
        }
        if (pos >= s.size() || s[pos] != ':') {
            return out;
        }
        ++pos;
        if (!parse_digits(s, pos, 1, 2, minute)) {
            return out;
        }
        if (pos < s.size() && s[pos] == ':') {
            ++pos;
            if (!parse_digits(s, pos, 1, 2, second)) {
                return out;
            }
        }
        if (!skip_required_ws(s, pos)) {
            return out;
        }
        if (pos >= s.size()) {
            return out; // No zone: not a valid date-time
        }

        int tz_offset = 0;
        bool tz_unknown = false;
        char zc = s[pos];
        if (zc == '+' || zc == '-') {
            ++pos;
            int zval = 0;
            if (!parse_digits(s, pos, 4, 4, zval)) {
                return out;
            }
            int zh = zval / 100;
            int zm = zval % 100;
            if (zm > 59) {
                return out;
            }
            tz_offset = zh * 60 + zm;
            if (zc == '-') {
                if (zval == 0) {
                    tz_unknown = true; // RFC 5322 §3.3: "-0000"
                }
                tz_offset = -tz_offset;
            }
        } else if (std::isalpha(static_cast<unsigned char>(zc))) {
            size_t zone_start = pos;
            while (pos < s.size() && std::isalpha(static_cast<unsigned char>(s[pos]))) {
                ++pos;
            }
            std::string_view zone = s.substr(zone_start, pos - zone_start);
            if (!resolve_obs_zone(zone, tz_offset, tz_unknown)) {
                return out;
            }
        } else {
            return out;
        }

        if (month < 1 || month > 12) {
            return out;
        }
        if (day < 1 || day > days_in_month(year, month)) {
            return out;
        }
        if (hour > 23 || minute > 59 || second > 60) {
            return out;
        }

        out.year = year;
        out.month = month;
        out.day = day;
        out.hour = hour;
        out.minute = minute;
        out.second = second;
        out.tz_offset_minutes = tz_offset;
        out.tz_unknown = tz_unknown;
        out.valid = true;
        return out;
    }

private:
    static std::string_view trim(std::string_view str) {
        size_t start = 0;
        while (start < str.size() && std::isspace(static_cast<unsigned char>(str[start]))) {
            ++start;
        }
        size_t end = str.size();
        while (end > start && std::isspace(static_cast<unsigned char>(str[end - 1]))) {
            --end;
        }
        return str.substr(start, end - start);
    }

    /// Consume 1+ whitespace characters (FWS, already unfolded); required
    /// because date fields must be separated by at least one space.
    static bool skip_required_ws(std::string_view s, size_t& pos) {
        size_t start = pos;
        while (pos < s.size() && std::isspace(static_cast<unsigned char>(s[pos]))) {
            ++pos;
        }
        return pos > start;
    }

    /// Parse between `min_digits` and `max_digits` decimal digits.
    static bool parse_digits(std::string_view s, size_t& pos, size_t min_digits,
                              size_t max_digits, int& out_value) {
        size_t start = pos;
        while (pos < s.size() && (pos - start) < max_digits &&
               std::isdigit(static_cast<unsigned char>(s[pos]))) {
            ++pos;
        }
        size_t count = pos - start;
        if (count < min_digits) {
            return false;
        }
        auto [ptr, ec] = std::from_chars(s.data() + start, s.data() + pos, out_value);
        return ec == std::errc();
    }

    static int parse_month_name(std::string_view s, size_t& pos) {
        static constexpr std::string_view kMonths[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                                        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
        if (pos + 3 > s.size()) {
            return 0;
        }
        std::string_view candidate = s.substr(pos, 3);
        for (size_t i = 0; i < 12; ++i) {
            if (ieq3(candidate, kMonths[i])) {
                pos += 3;
                return static_cast<int>(i) + 1;
            }
        }
        return 0;
    }

    static bool ieq3(std::string_view a, std::string_view b) {
        if (a.size() != 3 || b.size() != 3) {
            return false;
        }
        for (int i = 0; i < 3; ++i) {
            if (std::tolower(static_cast<unsigned char>(a[static_cast<size_t>(i)])) !=
                std::tolower(static_cast<unsigned char>(b[static_cast<size_t>(i)]))) {
                return false;
            }
        }
        return true;
    }

    static bool ieq(std::string_view a, std::string_view b) {
        if (a.size() != b.size()) {
            return false;
        }
        for (size_t i = 0; i < a.size(); ++i) {
            if (std::tolower(static_cast<unsigned char>(a[i])) !=
                std::tolower(static_cast<unsigned char>(b[i]))) {
                return false;
            }
        }
        return true;
    }

    /// Resolve an RFC 5322 obs-zone name to an offset. Returns false when
    /// the token is not alphabetic zone syntax at all (grammar mismatch);
    /// unrecognized-but-alphabetic tokens (arbitrary obs-zone letters,
    /// single military-style letters) fall back to "-0000" per §4.3.
    static bool resolve_obs_zone(std::string_view zone, int& offset_minutes, bool& unknown) {
        if (zone.empty()) {
            return false;
        }
        struct NamedZone {
            std::string_view name;
            int offset;
        };
        static constexpr NamedZone kNamed[] = {
            {"UT", 0},      {"GMT", 0},     {"EST", -5 * 60}, {"EDT", -4 * 60},
            {"CST", -6 * 60}, {"CDT", -5 * 60}, {"MST", -7 * 60}, {"MDT", -6 * 60},
            {"PST", -8 * 60}, {"PDT", -7 * 60},
        };
        for (const auto& nz : kNamed) {
            if (ieq(zone, nz.name)) {
                offset_minutes = nz.offset;
                unknown = false;
                return true;
            }
        }
        // Any other alphabetic zone (single military letters "A".."Z" minus
        // "J", or any other obs-zone letters) is unreliable per RFC 5322
        // §4.3 and is treated as equivalent to "-0000".
        for (char c : zone) {
            if (!std::isalpha(static_cast<unsigned char>(c))) {
                return false;
            }
        }
        offset_minutes = 0;
        unknown = true;
        return true;
    }

    static bool is_leap_year(int year) {
        return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
    }

    static int days_in_month(int year, int month) {
        static constexpr int kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        if (month == 2 && is_leap_year(year)) {
            return 29;
        }
        return kDays[month - 1];
    }
};

/// ============================================================================
/// Message-ID / In-Reply-To / References (RFC 5322 Section 3.6.4)
/// ============================================================================
///
///   msg-id  = [CFWS] "<" id-left "@" id-right ">" [CFWS]
///
/// CFWS around/inside the id is already handled upstream: Message-ID,
/// In-Reply-To and References are in
/// MimeParserExtended::is_structured_field, so RFC 5322 comments are
/// stripped before this parser ever sees the value, and folding is undone
/// by HeaderFolding before header values are even split out.
///
/// A malformed candidate (no angle brackets, no '@', empty local-part or
/// domain, disallowed characters) is returned with valid == false rather
/// than thrown or silently dropped, so the caller can record an anomaly.
/// ============================================================================

struct MessageId {
    std::string value; // "local@domain" (angle brackets stripped) -- the
                        // raw candidate text when invalid
    bool valid = false;
};

class MessageIdParser {
public:
    /// Parse a header value that may carry one or more msg-id tokens
    /// (In-Reply-To / References allow 1*msg-id; Message-ID has exactly
    /// one, but callers just take the first entry for that case).
    static std::vector<MessageId> parse_list(std::string_view value) {
        std::vector<MessageId> ids;
        size_t pos = 0;
        while (pos < value.size()) {
            size_t lt = value.find('<', pos);
            if (lt == std::string_view::npos) {
                break;
            }
            size_t gt = value.find('>', lt);
            if (gt == std::string_view::npos) {
                ids.push_back(parse_one(value.substr(lt)));
                break;
            }
            ids.push_back(parse_one(value.substr(lt, gt - lt + 1)));
            pos = gt + 1;
        }
        if (ids.empty()) {
            // No "<...>" candidate found at all (RFC 5322 §3.6.4 requires
            // angle brackets): still surface the raw text as one malformed
            // candidate so the caller can flag AnomalyKind::InvalidMessageIdSyntax
            // instead of silently treating the header as absent.
            std::string_view trimmed = trim(value);
            if (!trimmed.empty()) {
                MessageId id;
                id.value = std::string(trimmed);
                ids.push_back(id);
            }
        }
        return ids;
    }

    /// Parse a single token that should be a "<local@domain>" msg-id.
    static MessageId parse_one(std::string_view token) {
        MessageId id;
        std::string_view t = token;

        if (t.empty() || t.front() != '<') {
            id.value = std::string(t);
            return id;
        }

        // A matching closing '>' is required; an unterminated "<foo@bar"
        // (no closing bracket at all) is malformed, not merely stripped.
        bool has_closing = t.size() > 1 && t.back() == '>';
        if (has_closing) {
            t.remove_suffix(1);
        }
        t.remove_prefix(1);

        if (t.empty()) {
            id.value = std::string(t);
            return id;
        }

        size_t at = t.find('@');
        if (at == std::string_view::npos || at == 0 || at + 1 == t.size()) {
            id.value = std::string(t);
            return id; // missing '@', or empty local-part/domain
        }

        // dot-atom-text / no-fold-literal both exclude CTLs, whitespace,
        // and the angle-bracket delimiters themselves; a leftover '<'/'>'
        // here means nested/duplicated angle brackets.
        for (char c : t) {
            if (c == '<' || c == '>' || static_cast<unsigned char>(c) < 33) {
                id.value = std::string(t);
                return id;
            }
        }

        id.value = std::string(t);
        id.valid = has_closing;
        return id;
    }

private:
    static std::string_view trim(std::string_view str) {
        size_t start = 0;
        while (start < str.size() && std::isspace(static_cast<unsigned char>(str[start]))) {
            ++start;
        }
        size_t end = str.size();
        while (end > start && std::isspace(static_cast<unsigned char>(str[end - 1]))) {
            --end;
        }
        return str.substr(start, end - start);
    }
};

/// ============================================================================
/// message/delivery-status (RFC 3464, transported per RFC 6522 inside a
/// multipart/report with report-type=delivery-status)
/// ============================================================================
///
/// The body of a message/delivery-status part is NOT a nested MIME entity;
/// RFC 3464 §2.1 defines it as one or more groups of RFC 5322-style
/// "field: value" lines separated by blank lines: exactly one per-message
/// group (Reporting-MTA, Arrival-Date, ...) followed by one per-recipient
/// group per recipient (Final-Recipient, Action, Status, ...). This only
/// extracts the field groups; it does not interpret DSN semantics (action
/// values, status codes, ...).
/// ============================================================================

struct DeliveryStatusRef {
    std::vector<std::pair<std::string, std::string>> message_fields;
    std::vector<std::vector<std::pair<std::string, std::string>>> recipient_fields;
};

class DeliveryStatusParser {
public:
    static DeliveryStatusRef parse(std::string_view body) {
        DeliveryStatusRef ref;
        bool first_group = true;
        for (auto block : split_blocks(body)) {
            auto fields = parse_fields(block);
            if (fields.empty()) {
                continue;
            }
            if (first_group) {
                ref.message_fields = std::move(fields);
                first_group = false;
            } else {
                ref.recipient_fields.push_back(std::move(fields));
            }
        }
        return ref;
    }

private:
    /// Split on blank lines (CRLF/LF/bare-CR tolerant), like the top-level
    /// header/body split, but repeated for every group boundary in the body.
    static std::vector<std::string_view> split_blocks(std::string_view body) {
        std::vector<std::string_view> blocks;
        size_t block_start = 0;
        size_t pos = 0;
        while (pos < body.size()) {
            size_t eol = body.find_first_of("\r\n", pos);
            size_t line_end = (eol == std::string_view::npos) ? body.size() : eol;
            size_t next = line_end;
            if (eol != std::string_view::npos) {
                next = eol + 1;
                if (body[eol] == '\r' && next < body.size() && body[next] == '\n') {
                    ++next;
                }
            }
            if (line_end == pos) {
                // Blank line: close the current block
                if (block_start < pos) {
                    blocks.push_back(body.substr(block_start, pos - block_start));
                }
                block_start = next;
            }
            if (eol == std::string_view::npos) {
                break;
            }
            pos = next;
        }
        if (block_start < body.size()) {
            blocks.push_back(body.substr(block_start));
        }
        return blocks;
    }

    /// Parse "Field: value" lines in one block, joining folded continuation
    /// lines (leading SP/HTAB) onto the previous field's value.
    static std::vector<std::pair<std::string, std::string>> parse_fields(std::string_view block) {
        std::vector<std::pair<std::string, std::string>> fields;
        size_t pos = 0;
        while (pos < block.size()) {
            size_t eol = block.find_first_of("\r\n", pos);
            size_t line_end = (eol == std::string_view::npos) ? block.size() : eol;
            std::string_view line = block.substr(pos, line_end - pos);
            size_t next = line_end;
            if (eol != std::string_view::npos) {
                next = eol + 1;
                if (block[eol] == '\r' && next < block.size() && block[next] == '\n') {
                    ++next;
                }
            }

            if (!line.empty() && (line.front() == ' ' || line.front() == '\t') && !fields.empty()) {
                std::string_view cont = line;
                while (!cont.empty() && (cont.front() == ' ' || cont.front() == '\t')) {
                    cont.remove_prefix(1);
                }
                fields.back().second += ' ';
                fields.back().second += std::string(cont);
            } else {
                size_t colon = line.find(':');
                if (colon != std::string_view::npos) {
                    std::string_view field = line.substr(0, colon);
                    std::string_view value = line.substr(colon + 1);
                    while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) {
                        value.remove_prefix(1);
                    }
                    fields.emplace_back(std::string(field), std::string(value));
                }
            }

            pos = next;
        }
        return fields;
    }
};

} // namespace libglot::mime
