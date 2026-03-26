#pragma once

#include "../../../../core/include/libglot/lex/spec.h"
#include "../../../../core/include/libglot/hash/perfect_hash.h"
#include "../../../../libsqlglot/include/libsqlglot/tokens.h"
#include "../../../../libsqlglot/include/libsqlglot/keywords.h"
#include <optional>
#include <string_view>

namespace libglot::sql {

/// ============================================================================
/// SQL Token Specification - Satisfies TokenSpec Concept
/// ============================================================================
///
/// Thin shim over libsqlglot's existing TokenType enum and keyword data.
/// This adapts the existing SQL tokenization logic to the libglot-core
/// TokenSpec concept without rewriting the keyword list or character tables.
///
/// For Phase C1, this implements the minimum subset needed to parse:
///   SELECT col AS alias FROM table WHERE col = 1 ORDER BY col LIMIT 10
/// ============================================================================

struct SQLTokenSpec {
    // ========================================================================
    // Required Types (TokenSpec Concept)
    // ========================================================================

    /// Reuse libsqlglot's existing token type enum
    using TokenKind = libsqlglot::TokenType;

    /// Keyword lookup table (perfect hash over SQL keywords)
    struct KeywordTable {
        static TokenKind lookup(std::string_view text) noexcept {
            // Delegate to libsqlglot's existing perfect hash implementation
            return libsqlglot::KeywordLookup::lookup(text);
        }
    };

    // ========================================================================
    // Character Classification (TokenSpec Concept Requirements)
    // ========================================================================

    /// Check if character can start an identifier (a-z, A-Z, _)
    static constexpr bool is_identifier_start(char c) noexcept {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
    }

    /// Check if character can continue an identifier (a-z, A-Z, 0-9, _, $)
    static constexpr bool is_identifier_continue(char c) noexcept {
        return is_identifier_start(c) || is_digit(c) || c == '$';
    }

    /// Check if character is a digit (0-9)
    static constexpr bool is_digit(char c) noexcept {
        return c >= '0' && c <= '9';
    }

    /// Check if character is hex digit (0-9, a-f, A-F)
    static constexpr bool is_hex_digit(char c) noexcept {
        return is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    }

    /// Check if character is whitespace (space, tab, newline, etc.)
    static constexpr bool is_whitespace(char c) noexcept {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r';
    }

    // ========================================================================
    // Comment Detection (TokenSpec Concept Requirements)
    // ========================================================================

    /// Check if string view starts with a comment
    /// SQL supports: -- (line comment), # (MySQL line comment), /* (block comment)
    static constexpr std::optional<size_t> comment_start(std::string_view text) noexcept {
        if (text.size() >= 2) {
            if (text[0] == '-' && text[1] == '-') return 2;  // -- comment
            if (text[0] == '/' && text[1] == '*') return 2;  // /* comment
        }
        if (text.size() >= 1 && text[0] == '#') {
            return 1;  // # comment (MySQL)
        }
        return std::nullopt;
    }

    /// Check if string view is end of block comment (*/)
    static constexpr std::optional<size_t> comment_end(std::string_view text) noexcept {
        if (text.size() >= 2 && text[0] == '*' && text[1] == '/') {
            return 2;
        }
        return std::nullopt;
    }

    // ========================================================================
    // String Literal Delimiters (TokenSpec Concept Requirements)
    // ========================================================================

    /// Get the primary string quote character (SQL uses single quotes)
    static constexpr char string_quote_char() noexcept {
        return '\'';
    }

    /// Check if character can quote identifiers
    /// SQL: " (standard), ` (MySQL), [ (SQL Server)
    /// Returns closing quote if c is opening quote
    static constexpr std::optional<char> identifier_quote_char(char c) noexcept {
        if (c == '"') return '"';   // Standard SQL
        if (c == '`') return '`';   // MySQL backtick
        if (c == '[') return ']';   // SQL Server bracket
        return std::nullopt;
    }
};

// ============================================================================
/// Verify concept satisfaction at compile time
/// ============================================================================

static_assert(libglot::TokenSpec<SQLTokenSpec>,
    "SQLTokenSpec must satisfy libglot::TokenSpec concept");

} // namespace libglot::sql
