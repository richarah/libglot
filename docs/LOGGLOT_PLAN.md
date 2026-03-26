# logglot: Log Format Parser - Viability Assessment

**Date**: 2026-03-23
**Status**: Planning Phase
**Goal**: Assess feasibility of applying libglot-core framework to structured log parsing

## Executive Summary

**Recommendation**: **Viable with caveats**. Log format parsing can benefit from libglot-core's zero-cost abstractions, but requires architectural adaptations to handle:
1. **Line-oriented processing** (vs. token-stream parsing)
2. **Field position ambiguity** (whitespace-delimited fields)
3. **Format polymorphism** (same file, multiple formats)
4. **Timestamp hell** (100+ datetime formats across systems)

**Best fit**: Structured formats (JSON lines, logfmt, syslog RFC 5424)
**Worst fit**: Free-text logs with regex-only parsing (Nginx custom, ad-hoc formats)

---

## Log Format Catalog (20 Formats Assessed)

### Category 1: Structured / Well-Specified (★★★★ - Excellent Fit)

| Format | Spec | Grammar? | Viability | Notes |
|--------|------|----------|-----------|-------|
| **Syslog RFC 5424** | RFC 5424 | ABNF | ★★★★★ | Perfect fit. ABNF → TokenSpec. Structured data elements map to AST. |
| **JSON Lines (JSONL)** | json.org | JSON spec | ★★★★★ | Delegate to JSON parser (libglot-json). Trivial AST wrapping. |
| **logfmt** | Heroku spec | Informal | ★★★★☆ | Simple key=value pairs. Easy TokenSpec (KEY, EQ, VALUE, WS). |
| **Common Event Format (CEF)** | ArcSight spec | Pipe-delimited | ★★★★☆ | Fixed pipe positions. Easy TokenSpec. Vendor extensions require schema. |
| **Log Event Extended Format (LEEF)** | IBM QRadar spec | Tab-delimited | ★★★★☆ | Similar to CEF. Fixed tabs + key=value extensions. |
| **Graylog Extended Log Format (GELF)** | GELF spec | JSON | ★★★★★ | JSON-based. Delegate to libglot-json. |

**Architecture notes**:
- ABNF → TokenSpec via code generation (already proven in MIME)
- JSON formats delegate to `libglot::json::parse()` and wrap result
- Key=value formats use simple TokenSpec: `{KEY, EQ, VALUE, WHITESPACE, NEWLINE}`

### Category 2: Semi-Structured / Line-Oriented (★★★☆ - Good Fit)

| Format | Spec | Grammar? | Viability | Notes |
|--------|------|----------|-----------|-------|
| **Syslog BSD (RFC 3164)** | RFC 3164 | Informal | ★★★☆☆ | Ambiguous format. Timestamp parsing is brittle. Hostname/program name heuristics. |
| **Apache Common Log Format** | Apache docs | Regex | ★★★★☆ | Fixed field positions. Space-delimited with quoted strings. Easy TokenSpec. |
| **Apache Combined Log Format** | Apache docs | Regex | ★★★★☆ | Extension of Common. Add Referer + User-Agent fields. |
| **W3C Extended Log Format** | W3C spec | Custom | ★★★☆☆ | Header-defined schema (#Fields: ...). Dynamic field parsing. |
| **AWS CloudWatch Logs** | AWS docs | None | ★★☆☆☆ | Timestamp + message blob. No structure beyond timestamp. |
| **systemd journal export** | systemd docs | Key=value | ★★★★☆ | Key=value pairs with length-prefixed binary values. |

**Architecture notes**:
- **Regex formats**: TokenSpec defines field boundaries (spaces, quotes). Parser uses fixed field order.
- **Header-driven formats**: Requires two-pass parsing (parse header schema, then apply schema to lines).
- **Timestamp parsing**: Centralize in `libglot::log::parse_timestamp(std::string_view)` (see below).

### Category 3: Dialect Chaos / Vendor-Specific (★★☆☆ - Poor Fit)

| Format | Spec | Grammar? | Viability | Notes |
|--------|------|----------|-----------|-------|
| **Nginx default log format** | Nginx docs | Configurable | ★★☆☆☆ | User-defined format string (`log_format`). Requires format parser + code gen. |
| **Nginx custom log formats** | User-defined | None | ★☆☆☆☆ | Arbitrary format strings. Not worth generic parser - use regex. |
| **Cisco IOS syslog** | Cisco docs | None | ★★☆☆☆ | Vendor-specific message IDs. Free-text messages. Requires message catalog. |
| **Palo Alto firewall logs** | Palo Alto docs | CSV-like | ★★★☆☆ | Comma-delimited with 30+ fields. Field order varies by log type. |
| **PostgreSQL csvlog** | PostgreSQL docs | CSV | ★★★★☆ | Standard CSV. Delegate to CSV parser (libglot-csv if it exists). |
| **PostgreSQL stderr format** | PostgreSQL docs | Free-text | ★★☆☆☆ | Timestamp + severity + free-text message. Minimal structure. |
| **Windows Event Log (EVTX XML)** | Microsoft | XML | ★★★★☆ | XML-based. Delegate to libglot-xml (if it exists) or tinyxml2. |
| **Kubernetes container logs** | Docker JSON-file | JSON Lines | ★★★★★ | JSON Lines with `log`, `stream`, `time` fields. Easy. |

**Architecture notes**:
- **Configurable formats** (Nginx): Requires **format string parser** → generates TokenSpec. High complexity.
- **Vendor-specific**: Requires **message catalogs** (thousands of message IDs). Not worth generic parser.
- **CSV/XML/JSON**: Delegate to specialized parsers. libglot-log wraps them.

---

## Timestamp Hell: The Hard Problem

**Problem**: Log timestamps have **100+ format variations** across systems. No single parser handles them all.

**Examples**:

```
2024-01-15T10:30:45Z                        # ISO 8601 (UTC)
2024-01-15T10:30:45+00:00                   # ISO 8601 with offset
2024-01-15 10:30:45                         # PostgreSQL
Jan 15 10:30:45                             # Syslog BSD (no year!)
15/Jan/2024:10:30:45 +0000                  # Apache
1705318245                                  # Unix epoch (seconds)
1705318245000                               # Unix epoch (milliseconds)
Mon Jan 15 10:30:45 2024                    # asctime()
2024-01-15T10:30:45.123456Z                 # ISO 8601 with microseconds
20240115103045                              # Compact (no delimiters)
```

**Solution**: Centralized timestamp parser with format detection:

```cpp
namespace libglot::log {

struct Timestamp {
    std::chrono::system_clock::time_point time;
    std::string_view original_text;
    std::string_view detected_format;
};

/// Parse timestamp with format auto-detection
/// Tries known formats in order of likelihood (configurable)
[[nodiscard]] std::optional<Timestamp> parse_timestamp(std::string_view text);

/// Parse timestamp with explicit format hint
[[nodiscard]] std::optional<Timestamp> parse_timestamp(std::string_view text, std::string_view format_hint);

} // namespace libglot::log
```

**Format detection heuristics**:
1. Starts with digit? Likely Unix epoch or ISO 8601.
2. Starts with letter? Likely asctime() or syslog BSD.
3. Contains 'T'? Likely ISO 8601.
4. Contains '/'? Likely Apache or European date format.
5. Contains '['? Likely bracketed timestamp.

**Implementation note**: Use Howard Hinnant's `date` library (C++20 `<chrono>` backport) for robust parsing.

---

## Canonical Log AST Sketch

```cpp
namespace libglot::log {

enum class LogNodeKind {
    LOG_ENTRY,        // Root node
    TIMESTAMP,
    SEVERITY,
    SOURCE,           // Hostname, process, PID, thread
    MESSAGE,
    STRUCTURED_FIELD, // Key-value pair
    EXCEPTION,        // Stack trace
    TRACE_ID,         // Distributed tracing ID
};

struct LogNode : libglot::AstNodeBase<LogNode, LogNodeKind> {
    using Base = libglot::AstNodeBase<LogNode, LogNodeKind>;
    using Base::Base;

    SourceLocation loc{};
};

struct LogEntry : LogNode {
    Timestamp* timestamp;
    Severity* severity;
    Source* source;
    Message* message;
    std::vector<StructuredField*> structured_fields;

    LogEntry() : LogNode(LogNodeKind::LOG_ENTRY), timestamp(nullptr), severity(nullptr),
                 source(nullptr), message(nullptr) {}
};

enum class SeverityLevel {
    EMERGENCY, ALERT, CRITICAL, ERROR, WARNING, NOTICE, INFO, DEBUG
};

struct Severity : LogNode {
    SeverityLevel level;
    std::string_view original_text;  // "ERROR", "ERR", "E", "3", etc.

    Severity(SeverityLevel lvl, std::string_view text)
        : LogNode(LogNodeKind::SEVERITY), level(lvl), original_text(text) {}
};

struct Source : LogNode {
    std::string_view hostname;
    std::string_view process_name;
    std::string_view pid;
    std::string_view thread_id;
    std::string_view filename;  // __FILE__ in structured logging
    int line_number = 0;        // __LINE__ in structured logging

    Source() : LogNode(LogNodeKind::SOURCE) {}
};

struct Message : LogNode {
    std::string_view text;

    explicit Message(std::string_view msg)
        : LogNode(LogNodeKind::MESSAGE), text(msg) {}
};

struct StructuredField : LogNode {
    std::string_view key;
    std::string_view value;

    StructuredField(std::string_view k, std::string_view v)
        : LogNode(LogNodeKind::STRUCTURED_FIELD), key(k), value(v) {}
};

} // namespace libglot::log
```

---

## TokenSpec and GrammarSpec Sketches

### Challenge: Line-Oriented vs. Token-Stream

**Problem**: Most log formats are **line-oriented** (one entry per line), but libglot-core assumes **token-stream parsing** (tokens consumed sequentially across line boundaries).

**Solutions**:

#### Option 1: Line-Level Tokenization

```cpp
struct LogTokenSpec {
    using TokenKind = LogTokenType;

    enum class LogTokenType {
        NEWLINE,          // End of log entry
        TIMESTAMP,        // Entire timestamp (pre-parsed)
        SEVERITY_KEYWORD, // INFO, ERROR, etc.
        BRACKETED_TEXT,   // [process_name]
        QUOTED_STRING,    // "message text"
        KEY_VALUE_PAIR,   // key=value (logfmt)
        JSON_OBJECT,      // {...} (structured logging)
        PIPE,             // | (CEF delimiter)
        TAB,              // \t (LEEF delimiter)
        WHITESPACE,
        TEXT,             // Free-text message
        EOF_TOKEN
    };

    // Line-oriented: tokenize one line at a time
    static std::vector<Token<TokenKind>> tokenize_line(std::string_view line);
};
```

**Pros**: Natural for log parsing (one line = one entry).
**Cons**: Breaks libglot-core's token-stream abstraction (no cross-line parsing).

#### Option 2: Field-Level Tokenization

```cpp
struct ApacheCommonLogTokenSpec {
    using TokenKind = ApacheTokenType;

    enum class ApacheTokenType {
        IP_ADDRESS,       // 192.168.1.1
        HYPHEN,           // - (missing field)
        TIMESTAMP,        // [15/Jan/2024:10:30:45 +0000]
        QUOTED_STRING,    // "GET /index.html HTTP/1.1"
        NUMBER,           // 200 (status code), 1234 (bytes)
        NEWLINE,
        EOF_TOKEN
    };

    // Field-oriented: tokenize specific log format
    static std::vector<Token<TokenKind>> tokenize(std::string_view text);
};
```

**Pros**: Fits libglot-core's token-stream model.
**Cons**: Requires **one TokenSpec per log format** (high maintenance).

**Recommendation**: **Option 1 (line-level)** with adapters for libglot-core.

---

## Viability Assessment by Format Category

### ★★★★★ Excellent Fit (Structured, Well-Specified)

**Formats**: syslog RFC 5424, JSON Lines, GELF, CEF, LEEF, logfmt, PostgreSQL csvlog, Kubernetes container logs

**Why it works**:
- ABNF or formal spec → TokenSpec code generation
- Unambiguous field boundaries
- Fixed or header-defined schemas
- libglot-core's perfect hash + LUT dispatch is optimal for keyword matching (severity levels, structured data element names)

**Implementation effort**: **Low** (2-4 weeks per format)

### ★★★☆☆ Good Fit (Semi-Structured)

**Formats**: syslog BSD, Apache Common/Combined, W3C Extended, systemd journal export

**Why it works**:
- Fixed field positions (space-delimited or regex-based)
- Timestamp parsing is the hard part (centralize it)
- Moderate ambiguity (syslog BSD hostname/program heuristics)

**Implementation effort**: **Medium** (4-8 weeks per format)

### ★★☆☆☆ Poor Fit (Vendor-Specific, Configurable)

**Formats**: Nginx custom formats, Cisco IOS, Palo Alto, AWS CloudWatch, PostgreSQL stderr

**Why it's hard**:
- **Configurable formats** (Nginx `log_format`): Requires format string parser + code generation. High complexity, low ROI.
- **Vendor message catalogs** (Cisco IOS): Thousands of message IDs, no generic structure. Better served by regex + lookup tables.
- **Free-text messages** (CloudWatch, PostgreSQL stderr): No structure beyond timestamp + message. Not worth generic parser.

**Implementation effort**: **High** (8-16 weeks per format, low value)

**Recommendation**: **Skip these**. Provide regex-based fallback parser:

```cpp
namespace libglot::log {

/// Fallback parser for unstructured logs
/// Extracts timestamp + message using regex
struct RegexLogParser {
    std::regex timestamp_pattern;
    std::regex severity_pattern;

    [[nodiscard]] std::optional<LogEntry> parse(std::string_view line);
};

} // namespace libglot::log
```

---

## Architecture Recommendations

### 1. Multi-Parser Architecture

```
libglot-log/
├── include/libglot/log/
│   ├── common/
│   │   ├── timestamp.h          // Centralized timestamp parsing
│   │   ├── severity.h           // Severity level mapping
│   │   └── ast_nodes.h          // Canonical log AST
│   ├── formats/
│   │   ├── syslog_rfc5424.h     // syslog RFC 5424 parser
│   │   ├── json_lines.h         // JSON Lines parser (wraps libglot-json)
│   │   ├── logfmt.h             // logfmt parser
│   │   ├── apache_common.h      // Apache Common Log Format parser
│   │   └── cef.h                // Common Event Format parser
│   └── regex_fallback.h         // Regex-based fallback parser
└── src/
    └── timestamp.cpp             // Timestamp format detection + parsing
```

### 2. Format Detection

**Problem**: Given a log file, which parser should be used?

**Solution**: Format auto-detection via heuristics:

```cpp
namespace libglot::log {

enum class LogFormat {
    UNKNOWN,
    SYSLOG_RFC5424,
    SYSLOG_BSD,
    JSON_LINES,
    LOGFMT,
    APACHE_COMMON,
    APACHE_COMBINED,
    CEF,
    LEEF,
    // ...
};

/// Detect log format from first N lines (default: 10)
[[nodiscard]] LogFormat detect_format(std::string_view log_data, size_t sample_lines = 10);

/// Detect log format from single line
[[nodiscard]] LogFormat detect_format_line(std::string_view line);

} // namespace libglot::log
```

**Heuristics**:
- Starts with `<` + digit + `>`? → syslog RFC 5424
- Starts with `{`? → JSON Lines
- Contains `=` but no `:`? → logfmt
- Contains `|` separators? → CEF or LEEF
- Matches IP address pattern at start? → Apache logs
- All else → UNKNOWN (use regex fallback)

### 3. Performance Optimization

**Key insight**: Log parsing is **batch-oriented** (millions of lines). Optimize for throughput:

1. **SIMD line splitting**: Use AVX2 to find newlines (20GB/s throughput)
2. **Parallel parsing**: Parse each line independently (perfect parallelism with `std::execution::par_unseq`)
3. **Arena allocation**: Reuse arena across lines (amortize allocation cost)
4. **Perfect hash for severity keywords**: O(1) lookup for INFO/ERROR/WARN/etc.

**Expected performance**:
- **Structured formats** (syslog RFC 5424, CEF): **10-50 million lines/sec** (single-threaded)
- **Semi-structured** (Apache logs): **5-20 million lines/sec**
- **JSON Lines**: Limited by JSON parser (~1-5 million lines/sec)

### 4. Streaming API

**Problem**: Log files are often **multi-GB**. Cannot load entire file into memory.

**Solution**: Streaming line-by-line parser:

```cpp
namespace libglot::log {

struct LogParser {
    LogFormat format;
    libglot::Arena arena;

    explicit LogParser(LogFormat fmt) : format(fmt) {}

    /// Parse single line (arena-allocated AST)
    [[nodiscard]] std::optional<LogEntry*> parse_line(std::string_view line);

    /// Parse all lines from stream (callback per line)
    void parse_stream(std::istream& input, std::function<void(LogEntry*)> callback);

    /// Clear arena (reuse memory for next batch)
    void reset_arena() { arena.clear(); }
};

} // namespace libglot::log
```

---

## Implementation Roadmap

### Phase 1: Proof of Concept (4-6 weeks)

**Goal**: Validate libglot-core fit for structured log parsing.

1. Implement `libglot::log::timestamp` (centralized timestamp parser)
2. Implement `libglot::log::LogEntry` AST (canonical log representation)
3. Implement 3 parsers:
   - syslog RFC 5424 (ABNF-based)
   - logfmt (key=value pairs)
   - JSON Lines (delegate to libglot-json)
4. Benchmark against existing parsers:
   - syslog: compare to `rsyslog` (C)
   - logfmt: compare to Go `logfmt` library
   - JSON Lines: compare to `jq` (C)

**Success criteria**:
- 10+ million lines/sec for structured formats (single-threaded)
- Zero-cost abstraction validated (no virtual dispatch overhead)
- Canonical AST generalizes across formats

### Phase 2: Production Hardening (8-12 weeks)

**Goal**: Production-ready log parser library.

1. Implement 10+ format parsers (cover 80% of real-world logs)
2. Format auto-detection
3. Streaming API for multi-GB files
4. SIMD line splitting
5. Parallel parsing (std::execution)
6. Comprehensive test suite (1M+ real-world log lines)
7. Fuzzing (AFL++ or libFuzzer)
8. Python bindings (nanobind)

### Phase 3: Advanced Features (12-16 weeks)

**Goal**: Log analysis and transformation.

1. Query language (SQL-like filtering: `SELECT * FROM logs WHERE severity = 'ERROR'`)
2. Aggregation (count, group by, time bucketing)
3. Log normalization (convert any format → canonical AST → output format)
4. Real-time streaming (tail -f integration)
5. Cloud integrations (S3, CloudWatch, Splunk, Elasticsearch)

---

## Risks and Mitigations

| Risk | Impact | Mitigation |
|------|--------|-----------|
| **Timestamp parsing complexity** | High | Centralize in `timestamp.h`, use Howard Hinnant's `date` library |
| **Format ambiguity** (e.g., syslog BSD) | Medium | Accept imperfect heuristics, document limitations |
| **Configurable formats** (Nginx `log_format`) | High | Skip for v1.0, add in v2.0 if demand exists |
| **Performance regression** vs. regex | High | Benchmark against `grep`, `awk`, `rsyslog` early |
| **Token-stream mismatch** (line-oriented logs) | Medium | Adapt libglot-core with line-level tokenization |

---

## Conclusion

**logglot is viable** for structured and semi-structured log formats (covering ~80% of real-world logs). The libglot-core framework's strengths (zero-cost abstraction, perfect hash dispatch, arena allocation) map well to log parsing.

**Key challenges**:
1. Timestamp format hell (100+ variations)
2. Line-oriented processing (vs. token-stream model)
3. Format auto-detection
4. Performance parity with regex-based tools

**Recommendation**: **Proceed with Phase 1 (Proof of Concept)** to validate performance and architecture. Focus on structured formats (syslog RFC 5424, logfmt, CEF, JSON Lines) first. Defer vendor-specific and configurable formats to Phase 2+.

**Expected outcome**: A **10-50× faster log parser** than Python/regex-based tools for structured formats, with a canonical AST enabling log normalization and analysis.

---

## References

### Log Format Specifications

- **Syslog**: RFC 3164 (BSD), RFC 5424 (Structured)
- **Apache**: https://httpd.apache.org/docs/current/logs.html
- **W3C Extended**: https://www.w3.org/TR/WD-logfile.html
- **logfmt**: https://brandur.org/logfmt
- **CEF**: https://www.microfocus.com/documentation/arcsight/arcsight-smartconnectors/pdfdoc/common-event-format-v25/common-event-format-v25.pdf
- **LEEF**: https://www.ibm.com/docs/en/dsm?topic=leef-overview
- **GELF**: https://go2docs.graylog.org/5-0/getting_in_log_data/gelf.html
- **systemd journal**: https://www.freedesktop.org/software/systemd/man/systemd.journal-fields.html

### Existing Log Parsers

- **rsyslog** (C): https://www.rsyslog.com/
- **logstash** (Java): https://www.elastic.co/logstash
- **fluentd** (Ruby): https://www.fluentd.org/
- **Vector** (Rust): https://vector.dev/

### Timestamp Libraries

- **Howard Hinnant's date library**: https://github.com/HowardHinnant/date
- **C++20 `<chrono>`**: https://en.cppreference.com/w/cpp/chrono

### Performance References

- **SIMD line splitting**: https://github.com/lemire/simdjson
- **Parallel parsing**: https://en.cppreference.com/w/cpp/algorithm/execution_policy_tag_t
