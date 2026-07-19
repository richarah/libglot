# Security Policy

## Reporting a vulnerability

Please report suspected vulnerabilities privately via GitHub's
["Report a vulnerability"](https://github.com/richarah/libglot/security/advisories/new)
flow (Security → Advisories → Report a vulnerability). Do not open a public
issue for anything you believe is exploitable. You can expect an
acknowledgement within a week.

## Threat model

libglot parses **untrusted input** by design. The security contract is:

- **No memory unsafety on any input.** Both parsers are fuzzed (libFuzzer,
  `fuzz/`) under ASan/UBSan, and the test suites run under ASan in CI.
  Arena allocation with registered destructors avoids manual lifetime
  management; every `string_view` stored in tokens or AST nodes points into
  arena-owned memory (`core/include/libglot/LIFETIME.md`).
- **No unbounded resource use.**
  - SQL: recursion depth is capped (`kMaxRecursionDepth`, ParseError past
    the limit); pathological inputs (1000-deep nesting, 1000-item IN lists)
    are exercised in `sql/tests/test_mad_queries.cpp`.
  - MIME: `ParserLimits` bounds multipart nesting depth and total part
    count; hostile inputs are exercised in `mime/tests/test_pipeline.cpp`.
- **Malformed input fails cleanly.** SQL throws `libglot::ParseError` with
  location info; trailing unparsed input is an error, never silently
  dropped (an injected payload after a statement cannot ride along).
  MIME records anomalies per configured policy and can reject messages on
  Security/DoS-severity findings (`AnomalyConfig`).
- **Output is escape-correct.** The SQL generator quote-escapes identifiers
  and doubles quotes in string literals (`sql/tests/test_security.cpp`);
  unhandled constructs throw `std::logic_error` rather than emitting
  wrong or truncated SQL.

Out of scope: libglot does not execute SQL and is not itself an injection
filter — generating SQL from untrusted ASTs still requires the usual
parameterization discipline in the consuming application.

## Supported versions

The `master` branch and the most recent tagged release receive fixes.
