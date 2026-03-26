#pragma once

#include "tokens.h"
#include "ast_nodes.h"
#include "../../../../core/include/libglot/lex/spec.h"
#include "../../../../core/include/libglot/parse/grammar.h"
#include <optional>
#include <span>

namespace libglot::mime {

/// ============================================================================
/// MIME Token Spec
/// ============================================================================

struct MimeTokenSpec {
    using TokenKind = MimeTokenType;

    // Keyword table (MIME doesn't have keywords, so this is trivial)
    struct KeywordTable {
        static constexpr TokenKind lookup(std::string_view) noexcept {
            return MimeTokenType::IDENTIFIER;
        }
    };

    // Character classification
    static constexpr bool is_identifier_start(char c) noexcept {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '-';
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
        return c == ' ' || c == '\t';
    }

    // Comments (MIME doesn't have comments in headers)
    static constexpr std::optional<size_t> comment_start(std::string_view) noexcept {
        return std::nullopt;
    }

    static constexpr std::optional<size_t> comment_end(std::string_view) noexcept {
        return std::nullopt;
    }

    // String literals (MIME doesn't use quotes in simple headers)
    static constexpr char string_quote_char() noexcept {
        return '\'';
    }

    static constexpr std::optional<char> identifier_quote_char(char) noexcept {
        return std::nullopt;
    }

    static constexpr TokenKind eof_token() noexcept {
        return MimeTokenType::EOF_TOKEN;
    }

    static constexpr TokenKind invalid_token() noexcept {
        return MimeTokenType::INVALID;
    }

    static std::string token_name(TokenKind kind) {
        return std::string(mime_token_type_name(kind));
    }
};

// Verify that MimeTokenSpec satisfies the TokenSpec concept
static_assert(libglot::TokenSpec<MimeTokenSpec>,
              "MimeTokenSpec must satisfy TokenSpec concept");

/// ============================================================================
/// MIME Grammar Spec
/// ============================================================================
///
/// Provides all required types for ParserBase<Spec, Derived> instantiation.
/// ============================================================================

struct MimeGrammarSpec {
    using TokenSpecType = MimeTokenSpec;
    using TokenKind = MimeTokenType;
    using AstNodeType = MimeNode;
    using NodeKind = MimeNodeKind;

    // Operator precedence (MIME has no operators, return empty table)
    static constexpr std::span<const libglot::OperatorInfo<TokenKind>> operator_precedence() noexcept {
        return std::span<const libglot::OperatorInfo<TokenKind>>{};
    }
};

// Verify that MimeGrammarSpec satisfies the GrammarSpec concept
static_assert(libglot::GrammarSpec<MimeGrammarSpec>,
              "MimeGrammarSpec must satisfy GrammarSpec concept");

} // namespace libglot::mime
