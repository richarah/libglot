#pragma once

#include "../hash/perfect_hash.h"
#include <concepts>
#include <string_view>
#include <optional>
#include <cstddef>
#include <cstdint>

namespace libglot {

/// ============================================================================
/// TokenSpec Concept - Defines contract for domain-specific tokenization
/// ============================================================================
///
/// Every domain (SQL, MIME, log, etc.) must implement a TokenSpec that:
/// 1. Defines its token types (enum class TokenKind)
/// 2. Provides perfect hash keyword lookup (KeywordTable)
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

    /// Perfect hash table type for keyword lookup (see hash/perfect_hash.h)
    /// Must provide: static TokenKind lookup(std::string_view) noexcept
    typename T::KeywordTable;

    /// Keyword lookup function (must be static and noexcept for hot path)
    { T::KeywordTable::lookup(sv) } -> std::convertible_to<typename T::TokenKind>;

    // ========================================================================
    // Character Classification (CRITICAL HOT PATH - must be constexpr/inline)
    // ========================================================================

    /// Check if character can start an identifier (a-z, A-Z, _)
    /// PERFORMANCE: Called once per character, must be branchless
    { T::is_identifier_start(c) } -> std::same_as<bool>;

    /// Check if character can continue an identifier (a-z, A-Z, 0-9, _, $)
    /// PERFORMANCE: Called once per character in identifier
    { T::is_identifier_continue(c) } -> std::same_as<bool>;

    /// Check if character is a digit (0-9)
    /// PERFORMANCE: Called once per character, must be branchless
    { T::is_digit(c) } -> std::same_as<bool>;

    /// Check if character is hex digit (0-9, a-f, A-F)
    { T::is_hex_digit(c) } -> std::same_as<bool>;

    /// Check if character is whitespace (space, tab, newline, etc.)
    /// PERFORMANCE: Called once per character
    { T::is_whitespace(c) } -> std::same_as<bool>;

    // ========================================================================
    // Comment Detection (HOT PATH)
    // ========================================================================

    /// Check if string view starts with a comment
    /// Returns length of comment start sequence if match, nullopt otherwise
    /// Examples:
    ///   SQL:  "--"  returns 2, "/*" returns 2, "#" returns 1
    ///   MIME: "("   returns 1 (structured field comments)
    ///   C++:  "//"  returns 2, "/*" returns 2
    { T::comment_start(sv) } -> std::same_as<std::optional<size_t>>;

    /// Check if string view is end of block comment
    /// Returns length of comment end sequence if match
    /// Examples: "*/" returns 2, ")" returns 1 (MIME)
    { T::comment_end(sv) } -> std::same_as<std::optional<size_t>>;

    // ========================================================================
    // String Literal Delimiters
    // ========================================================================

    /// Get the primary string quote character
    /// Examples: SQL/MIME use '\'', some dialects use '"'
    { T::string_quote_char() } -> std::same_as<char>;

    /// Check if character can quote identifiers
    /// SQL: '"', MySQL: '`', SQL Server: '['
    /// Returns closing quote character if c is opening quote
    { T::identifier_quote_char(c) } -> std::same_as<std::optional<char>>;

    // ========================================================================
    // Optional: Domain-Specific Extensions
    // ========================================================================

    /// Check if string view starts with a special string literal (e.g., $$tag$$ in PostgreSQL)
    /// Not required, but allows domains to extend tokenization
    // { T::special_string_start(sv) } -> std::same_as<std::optional<size_t>>;
};

// ============================================================================
// Validation: Ensure TokenKind is an enum
// ============================================================================

template<typename T>
concept ValidTokenKind = requires {
    requires std::is_enum_v<T>;
};

template<typename T>
concept StrictTokenSpec = TokenSpec<T> && requires {
    requires ValidTokenKind<typename T::TokenKind>;
};

// ============================================================================
// Helper: Token struct template
// ============================================================================

template<ValidTokenKind Kind>
struct Token {
    Kind type;
    uint32_t start;           ///< Byte offset in source (0-indexed)
    uint32_t end;             ///< Byte offset (exclusive)
    uint16_t line;            ///< Line number (1-indexed)
    uint16_t col;             ///< Column number (1-indexed)
    std::string_view text;    ///< Token text (preserves length information)

    [[nodiscard]] constexpr size_t length() const noexcept {
        return end - start;
    }

    [[nodiscard]] constexpr std::string_view view(std::string_view source) const noexcept {
        if (start >= source.size()) return "";
        size_t len = std::min<size_t>(length(), source.size() - start);
        return source.substr(start, len);
    }
};

// ============================================================================
// Example TokenSpec Implementation (for documentation)
// ============================================================================

#if 0  // Example only, not compiled

struct ExampleTokenSpec {
    enum class TokenKind : uint16_t {
        ERROR, EOF_TOKEN,
        IDENTIFIER, NUMBER, STRING,
        PLUS, MINUS, STAR, SLASH,
        // ... domain-specific tokens
    };

    // Option 1: Use PerfectHashTable directly
    using KeywordTable = libglot::PerfectHashTable<TokenKind>;

    // Option 2: Custom wrapper (for compatibility)
    struct KeywordTableWrapper {
        static constexpr libglot::PerfectHashTable<TokenKind> table = /* ... */;

        static TokenKind lookup(std::string_view text) noexcept {
            return table.lookup(text);
        }
    };

    // For this example, we'll use a simple wrapper
    struct KeywordTable {
        static TokenKind lookup(std::string_view text) noexcept {
            // Perfect hash implementation (see hash/perfect_hash.h)
            return TokenKind::IDENTIFIER;
        }
    };

    static constexpr bool is_identifier_start(char c) noexcept {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
    }

    static constexpr bool is_identifier_continue(char c) noexcept {
        return is_identifier_start(c) || is_digit(c);
    }

    static constexpr bool is_digit(char c) noexcept {
        return c >= '0' && c <= '9';
    }

    static constexpr bool is_hex_digit(char c) noexcept {
        return is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    }

    static constexpr bool is_whitespace(char c) noexcept {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r';
    }

    static constexpr std::optional<size_t> comment_start(std::string_view text) noexcept {
        if (text.starts_with("//")) return 2;
        if (text.starts_with("/*")) return 2;
        return std::nullopt;
    }

    static constexpr std::optional<size_t> comment_end(std::string_view text) noexcept {
        if (text.starts_with("*/")) return 2;
        return std::nullopt;
    }

    static constexpr char string_quote_char() noexcept {
        return '\'';
    }

    static constexpr std::optional<char> identifier_quote_char(char c) noexcept {
        if (c == '"') return '"';
        if (c == '`') return '`';
        if (c == '[') return ']';
        return std::nullopt;
    }
};

static_assert(TokenSpec<ExampleTokenSpec>, "ExampleTokenSpec must satisfy TokenSpec concept");

#endif  // Example

} // namespace libglot
