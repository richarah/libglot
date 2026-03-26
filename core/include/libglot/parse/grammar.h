#pragma once

#include "../lex/spec.h"
#include "../ast/node.h"
#include <concepts>
#include <span>

namespace libglot {

/// ============================================================================
/// GrammarSpec Concept - Defines contract for domain-specific grammar
/// ============================================================================
///
/// Every domain must define a grammar specification that combines:
/// 1. TokenSpec (lexical structure)
/// 2. AstNode hierarchy (syntactic structure)
/// 3. Operator precedence table
/// 4. Parsing rules (via derived parser class)
///
/// This enables generic parser infrastructure while allowing domain-specific rules.
/// ============================================================================

// ============================================================================
/// Operator precedence and associativity (compile-time configuration)
/// ============================================================================

enum class Associativity : uint8_t {
    LEFT,
    RIGHT,
    NONE
};

template<ValidTokenKind Kind>
struct OperatorInfo {
    Kind op;
    uint8_t precedence;       ///< Higher number = higher precedence
    Associativity associativity;

    constexpr OperatorInfo(Kind o, uint8_t prec, Associativity assoc) noexcept
        : op(o), precedence(prec), associativity(assoc) {}
};

// ============================================================================
/// GrammarSpec Concept
/// ============================================================================

template<typename T>
concept GrammarSpec = requires {
    // ========================================================================
    // Required Types
    // ========================================================================

    /// Lexical specification
    typename T::TokenSpecType;
    requires TokenSpec<typename T::TokenSpecType>;

    /// AST node type
    typename T::AstNodeType;
    requires AstNode<typename T::AstNodeType>;

    /// Token kind (derived from TokenSpec)
    typename T::TokenKind;
    requires std::same_as<typename T::TokenKind, typename T::TokenSpecType::TokenKind>;

    /// Node kind (derived from AstNode)
    typename T::NodeKind;
    requires std::same_as<typename T::NodeKind, typename T::AstNodeType::NodeKind>;

    // ========================================================================
    // Operator Precedence Table (compile-time constant)
    // ========================================================================

    /// Span of operator precedence entries
    { T::operator_precedence() } -> std::convertible_to<std::span<const OperatorInfo<typename T::TokenKind>>>;
};

// ============================================================================
/// Helper: Get precedence for an operator
/// ============================================================================

template<GrammarSpec Spec>
[[nodiscard]] constexpr int get_precedence(typename Spec::TokenKind op) noexcept {
    for (const auto& entry : Spec::operator_precedence()) {
        if (entry.op == op) {
            return entry.precedence;
        }
    }
    return -1;  // Not an operator
}

template<GrammarSpec Spec>
[[nodiscard]] constexpr Associativity get_associativity(typename Spec::TokenKind op) noexcept {
    for (const auto& entry : Spec::operator_precedence()) {
        if (entry.op == op) {
            return entry.associativity;
        }
    }
    return Associativity::NONE;
}

// ============================================================================
/// Example GrammarSpec Implementation (for documentation)
/// ============================================================================

#if 0  // Example only, not compiled

// Assume we have ExampleTokenSpec and ExampleNode from previous examples

struct ExampleGrammar {
    using TokenSpecType = ExampleTokenSpec;
    using AstNodeType = ExampleNode;
    using TokenKind = ExampleTokenSpec::TokenKind;
    using NodeKind = ExampleNode::NodeKind;

    static constexpr std::span<const OperatorInfo<TokenKind>> operator_precedence() noexcept {
        static constexpr OperatorInfo<TokenKind> table[] = {
            {TokenKind::STAR, 12, Associativity::LEFT},
            {TokenKind::SLASH, 12, Associativity::LEFT},
            {TokenKind::PLUS, 11, Associativity::LEFT},
            {TokenKind::MINUS, 11, Associativity::LEFT},
            // ... more operators
        };
        return std::span{table};
    }
};

static_assert(GrammarSpec<ExampleGrammar>, "ExampleGrammar must satisfy GrammarSpec");

#endif  // Example

} // namespace libglot
