#pragma once

#include "../../../../core/include/libglot/parse/grammar.h"
#include "token_spec.h"
#include "ast_nodes.h"
#include <span>

namespace libglot::sql {

/// ============================================================================
/// SQL Grammar Specification - Satisfies GrammarSpec Concept
/// ============================================================================
///
/// Combines:
/// - SQLTokenSpec (lexical structure)
/// - SQLNode (AST hierarchy)
/// - Operator precedence table
///
/// This enables the generic ParserBase to parse SQL using precedence climbing.
/// ============================================================================

struct SQLGrammarSpec {
    // ========================================================================
    // Required Types (GrammarSpec Concept)
    // ========================================================================

    using TokenSpecType = SQLTokenSpec;
    using AstNodeType = SQLNode;
    using TokenKind = SQLTokenSpec::TokenKind;
    using NodeKind = SQLNodeKind;

    // ========================================================================
    // Operator Precedence Table (SQL Standard + Common Extensions)
    // ========================================================================
    ///
    /// Precedence levels (higher number = higher precedence):
    /// 15: Unary +, -, NOT
    /// 14: *, /, %
    /// 13: +, -, || (concat), JSON operators (->, ->>, #>, #>>)
    /// 12: =, <>, <, <=, >, >=, LIKE, ILIKE, IN, BETWEEN, @>, <@, ?
    /// 11: IS NULL, IS NOT NULL
    /// 10: NOT (boolean)
    /// 9:  AND
    /// 8:  OR
    ///
    /// This follows PostgreSQL precedence with JSON operator extensions.
    /// ========================================================================

    static constexpr std::span<const libglot::OperatorInfo<TokenKind>> operator_precedence() noexcept {
        using libglot::OperatorInfo;
        using libglot::Associativity;
        using TK = libsqlglot::TokenType;

        static constexpr OperatorInfo<TokenKind> table[] = {
            // Arithmetic (precedence 13-14)
            {TK::STAR, 14, Associativity::LEFT},       // *
            {TK::SLASH, 14, Associativity::LEFT},      // /
            {TK::PERCENT, 14, Associativity::LEFT},    // %
            {TK::PLUS, 13, Associativity::LEFT},       // +
            {TK::MINUS, 13, Associativity::LEFT},      // -
            {TK::CONCAT, 13, Associativity::LEFT},     // ||

            // JSON access operators (precedence 13 - same as concat)
            {TK::ARROW, 13, Associativity::LEFT},           // -> (JSON field access)
            {TK::LONG_ARROW, 13, Associativity::LEFT},      // ->> (JSON field as text)
            {TK::HASH_ARROW, 13, Associativity::LEFT},      // #> (JSON path)
            {TK::HASH_LONG_ARROW, 13, Associativity::LEFT}, // #>> (JSON path as text)

            // Comparison (precedence 12)
            {TK::EQ, 12, Associativity::LEFT},         // =
            {TK::NEQ, 12, Associativity::LEFT},        // <>, !=
            {TK::LT, 12, Associativity::LEFT},         // <
            {TK::LTE, 12, Associativity::LEFT},        // <=
            {TK::GT, 12, Associativity::LEFT},         // >
            {TK::GTE, 12, Associativity::LEFT},        // >=
            {TK::LIKE, 12, Associativity::LEFT},       // LIKE
            {TK::ILIKE, 12, Associativity::LEFT},      // ILIKE
            // NOTE: IN is handled in parse_postfix(), not as binary operator
            {TK::BETWEEN, 12, Associativity::LEFT},    // BETWEEN

            // JSON containment operators (precedence 12 - same as comparison)
            {TK::AT_GT, 12, Associativity::LEFT},      // @> (contains)
            {TK::LT_AT, 12, Associativity::LEFT},      // <@ (contained by)
            {TK::QUESTION, 12, Associativity::LEFT},   // ? (key exists)

            // IS NULL / IS NOT NULL (precedence 11)
            {TK::IS, 11, Associativity::LEFT},         // IS

            // Boolean (precedence 8-10)
            {TK::NOT, 10, Associativity::RIGHT},       // NOT
            {TK::AND, 9, Associativity::LEFT},         // AND
            {TK::OR, 8, Associativity::LEFT},          // OR
        };

        return std::span{table};
    }
};

// ============================================================================
/// Verify Concept Satisfaction
/// ============================================================================

static_assert(libglot::GrammarSpec<SQLGrammarSpec>,
    "SQLGrammarSpec must satisfy libglot::GrammarSpec concept");

} // namespace libglot::sql
