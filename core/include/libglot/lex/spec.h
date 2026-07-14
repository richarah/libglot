#pragma once

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <type_traits>

namespace libglot {

/// ============================================================================
/// TokenSpec Concept - Defines contract for domain-specific tokenization
/// ============================================================================
///
/// Every domain (SQL, MIME, log, etc.) must implement a TokenSpec that:
/// 1. Defines its token types (enum class TokenKind)
/// 2. Provides keyword lookup (KeywordTable)
/// 3. Implements character classification (is_identifier_start, is_digit, etc.)
/// 4. Defines comment syntax and string delimiters
///
/// This enables zero-cost abstraction via compile-time polymorphism.
/// ============================================================================

template<typename T>
concept TokenSpec = requires(char c, std::string_view sv) {
    // ========================================================================
    // Required Types
    // ========================================================================

    /// Token type enumeration (e.g., enum class TokenKind { IDENTIFIER, NUMBER, ... })
    typename T::TokenKind;

    /// Keyword lookup table type.
    /// Must provide: static TokenKind lookup(std::string_view) noexcept
    typename T::KeywordTable;

    /// Keyword lookup function (must be static and noexcept for hot path)
    { T::KeywordTable::lookup(sv) } -> std::convertible_to<typename T::TokenKind>;

    // ========================================================================
    // Character Classification (CRITICAL HOT PATH - must be constexpr/inline)
    // ========================================================================

    /// Check if character can start an identifier (a-z, A-Z, _)
    { T::is_identifier_start(c) } -> std::same_as<bool>;

    /// Check if character can continue an identifier (a-z, A-Z, 0-9, _, $)
    { T::is_identifier_continue(c) } -> std::same_as<bool>;

    /// Check if character is a digit (0-9)
    { T::is_digit(c) } -> std::same_as<bool>;

    /// Check if character is hex digit (0-9, a-f, A-F)
    { T::is_hex_digit(c) } -> std::same_as<bool>;

    /// Check if character is whitespace (space, tab, newline, etc.)
    { T::is_whitespace(c) } -> std::same_as<bool>;

    // ========================================================================
    // Comment Detection
    // ========================================================================

    /// Check if string view starts with a comment
    /// Returns length of comment start sequence if match, nullopt otherwise
    /// Examples:
    ///   SQL:  "--"  returns 2, "/*" returns 2, "#" returns 1
    ///   MIME: "("   returns 1 (structured field comments)
    { T::comment_start(sv) } -> std::same_as<std::optional<size_t>>;

    /// Check if string view is end of block comment
    /// Returns length of comment end sequence if match
    /// Examples: "*/" returns 2, ")" returns 1 (MIME)
    { T::comment_end(sv) } -> std::same_as<std::optional<size_t>>;

    // ========================================================================
    // String Literal Delimiters
    // ========================================================================

    /// Get the primary string quote character
    { T::string_quote_char() } -> std::same_as<char>;

    /// Check if character can quote identifiers
    /// SQL: '"', MySQL: '`', SQL Server: '['
    /// Returns closing quote character if c is opening quote
    { T::identifier_quote_char(c) } -> std::same_as<std::optional<char>>;
};

// ============================================================================
// Validation: Ensure TokenKind is an enum
// ============================================================================

template<typename T>
concept ValidTokenKind = requires {
    requires std::is_enum_v<T>;
};

// ============================================================================
// Helper: Token struct template
// ============================================================================

template<ValidTokenKind Kind>
struct Token {
    Kind type;
    uint32_t start;           ///< Byte offset in source (0-indexed)
    uint32_t end;             ///< Byte offset (exclusive)
    uint32_t line;            ///< Line number (1-indexed)
    uint32_t col;             ///< Column number (1-indexed)
    std::string_view text;    ///< Token text (preserves length information)

    [[nodiscard]] constexpr size_t length() const noexcept {
        return end >= start ? end - start : 0;
    }

    [[nodiscard]] constexpr std::string_view view(std::string_view source) const noexcept {
        if (start >= source.size()) return "";
        size_t len = std::min<size_t>(length(), source.size() - start);
        return source.substr(start, len);
    }
};

} // namespace libglot
