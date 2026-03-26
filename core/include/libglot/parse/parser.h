#pragma once

#include "grammar.h"
#include "../lex/tokenizer.h"
#include "../ast/node.h"
#include "../util/arena.h"
#include "error_recovery.h"
#include <vector>
#include <string_view>
#include <optional>
#include <stdexcept>
#include <functional>
#include <concepts>

namespace libglot {

/// ============================================================================
/// Parse Error Exception with Location Tracking
/// ============================================================================

class ParseError : public std::runtime_error {
public:
    uint16_t line;
    uint16_t column;
    std::string context;

    explicit ParseError(
        const std::string& msg,
        uint16_t l = 0,
        uint16_t c = 0,
        const std::string& ctx = ""
    )
        : std::runtime_error(format_message(msg, l, c, ctx))
        , line(l)
        , column(c)
        , context(ctx)
    {}

private:
    static std::string format_message(
        const std::string& msg,
        uint16_t line,
        uint16_t col,
        const std::string& ctx
    ) {
        std::string formatted;
        if (line > 0) {
            formatted += "Line " + std::to_string(line);
            if (col > 0) {
                formatted += ", column " + std::to_string(col);
            }
            formatted += ": ";
        }
        formatted += msg;
        if (!ctx.empty()) {
            formatted += " (found: '" + ctx + "')";
        }
        return formatted;
    }
};

/// ============================================================================
/// Generic Parser Base - Infrastructure for domain-specific parsers
/// ============================================================================
///
/// Zero-cost abstraction for domain-specific parsing (SQL, MIME, logs, etc.)
///
/// PERFORMANCE CRITICAL:
/// - All polymorphism resolved at compile-time via CRTP
/// - No virtual function calls on hot path
/// - Operator precedence climbing for binary expressions
/// - Recursive descent with tail-call optimization hints
///
/// DESIGN:
/// - Template parameter: Spec (GrammarSpec concept)
/// - CRTP parameter: Derived (domain-specific parser)
/// - Core parsing infrastructure in base class
/// - Domain-specific grammar rules in derived class
///
/// USAGE:
///   See example at bottom of file for SQL parser implementation.
///
/// CUSTOMIZATION POINTS (must implement in derived class):
///   - parse_primary() - Parse atomic expressions (literals, identifiers, etc.)
///   - parse_postfix() - Parse postfix operators (function calls, array access, etc.)
///   - Domain-specific statement parsing (parse_select, parse_insert, etc.)
/// ============================================================================

template<GrammarSpec Spec, typename Derived>
class ParserBase {
public:
    // ========================================================================
    // Type Aliases
    // ========================================================================

    using TokenKind = typename Spec::TokenKind;
    using TokenType = Token<TokenKind>;
    using AstNodeType = typename Spec::AstNodeType;
    using NodeKind = typename Spec::NodeKind;

    // ========================================================================
    // Configuration
    // ========================================================================

    static constexpr size_t kMaxRecursionDepth = 256;

    // ========================================================================
    // Construction
    // ========================================================================

    ParserBase(Arena& arena, std::vector<TokenType>&& tokens)
        : arena_(arena)
        , tokens_(std::move(tokens))
        , pos_(0)
        , recursion_depth_(0)
        , error_recovery_()
    {}

    // ========================================================================
    // Public API (convenience wrappers - call derived implementations)
    // ========================================================================

    /// Parse entire token stream
    /// Derived class must implement this to define top-level grammar rule
    AstNodeType* parse() {
        return derived().parse_top_level();
    }

    // ========================================================================
    // Protected Helpers (for derived classes)
    // ========================================================================

protected:
    /// CRTP: Get reference to derived class
    [[nodiscard]] Derived& derived() noexcept {
        return static_cast<Derived&>(*this);
    }

    [[nodiscard]] const Derived& derived() const noexcept {
        return static_cast<const Derived&>(*this);
    }

    // ========================================================================
    // Token Stream Management (HOT PATH)
    // ========================================================================

    /// Check if at end of token stream
    [[nodiscard]] bool is_eof() const noexcept {
        return pos_ >= tokens_.size() || tokens_[pos_].type == eof_token();
    }

    /// Get current token (HOT PATH - called very frequently)
    [[nodiscard]] const TokenType& current() const noexcept {
        if (pos_ >= tokens_.size()) {
            static const TokenType eof{eof_token(), 0, 0, 0, 0, ""};
            return eof;
        }
        return tokens_[pos_];
    }

    /// Peek ahead at token (offset = 0 means current, offset = 1 means next, etc.)
    [[nodiscard]] const TokenType& peek(size_t offset = 0) const noexcept {
        const size_t idx = pos_ + offset;
        if (idx >= tokens_.size()) {
            static const TokenType eof{eof_token(), 0, 0, 0, 0, ""};
            return eof;
        }
        return tokens_[idx];
    }

    /// Advance to next token and return previous token
    [[nodiscard]] const TokenType& advance() noexcept {
        if (pos_ < tokens_.size()) {
            return tokens_[pos_++];
        }
        static const TokenType eof{eof_token(), 0, 0, 0, 0, ""};
        return eof;
    }

    /// Check if current token matches type (without consuming)
    [[nodiscard]] bool check(TokenKind type) const noexcept {
        // Special case: when checking for EOF, return true if we're at EOF
        if (type == eof_token()) {
            return is_eof() || current().type == type;
        }
        return !is_eof() && current().type == type;
    }

    /// Check if current token matches any of the given types
    template<typename... Types>
    [[nodiscard]] bool check_any(Types... types) const noexcept {
        return (check(types) || ...);
    }

    /// Consume token if it matches type (returns true if matched)
    [[nodiscard]] bool match(TokenKind type) noexcept {
        if (check(type)) {
            advance();
            return true;
        }
        return false;
    }

    /// Consume token if it matches any of the given types
    template<typename... Types>
    [[nodiscard]] bool match_any(Types... types) noexcept {
        return (match(types) || ...);
    }

    /// Expect token of given type, error if not found
    void expect(TokenKind type) {
        if (!match(type)) {
            error("Expected " + token_name(type));
        }
    }

    /// Expect token and return it
    [[nodiscard]] const TokenType& expect_and_get(TokenKind type) {
        const TokenType& tok = current();
        expect(type);
        return tok;
    }

    // ========================================================================
    // Expression Parsing (Precedence Climbing)
    // ========================================================================

    /// Parse expression with precedence climbing algorithm
    ///
    /// This is the core of operator precedence parsing.
    /// Handles left-associative, right-associative, and non-associative operators.
    ///
    /// @param min_precedence Minimum precedence level to parse
    /// @return Parsed expression AST node
    [[nodiscard]] AstNodeType* parse_expression(int min_precedence = 0) {
        RecursionGuard guard(*this);

        // Parse primary expression (atomic term)
        AstNodeType* left = derived().parse_prefix();

        // Parse binary operators using precedence climbing
        while (!is_eof()) {
            const TokenKind op = current().type;
            const int prec = get_precedence<Spec>(op);

            // Not an operator, or precedence too low
            if (prec < min_precedence) {
                break;
            }

            const Associativity assoc = get_associativity<Spec>(op);
            advance();  // Consume operator

            // For right-associative operators, don't increment precedence
            // For left-associative, increment to ensure left-to-right parsing
            const int next_min_prec = (assoc == Associativity::RIGHT) ? prec : (prec + 1);

            // Parse right side with appropriate precedence
            AstNodeType* right = parse_expression(next_min_prec);

            // Create binary operator node
            left = derived().make_binary_operator(op, left, right);
        }

        // Parse postfix operators (function calls, array access, etc.)
        left = derived().parse_postfix(left);

        return left;
    }

    // ========================================================================
    // List Parsing (common pattern: comma-separated lists)
    // ========================================================================

    /// Parse comma-separated list of items
    ///
    /// @param parse_item Function to parse a single item
    /// @param terminator Token that ends the list (e.g., RPAREN, RBRACKET)
    /// @return Vector of parsed items
    template<typename ParseFunc>
    [[nodiscard]] std::vector<AstNodeType*> parse_list(
        ParseFunc parse_item,
        TokenKind terminator
    ) {
        std::vector<AstNodeType*> items;

        // Empty list
        if (check(terminator)) {
            return items;
        }

        // Parse first item
        items.push_back(parse_item());

        // Parse remaining items (comma-separated)
        while (match(TokenKind::COMMA)) {
            // Allow trailing comma
            if (check(terminator)) {
                break;
            }
            items.push_back(parse_item());
        }

        return items;
    }

    /// Parse comma-separated list with optional terminator check
    template<typename ParseFunc>
    [[nodiscard]] std::vector<AstNodeType*> parse_list_until(
        ParseFunc parse_item,
        std::function<bool()> should_continue
    ) {
        std::vector<AstNodeType*> items;

        while (should_continue()) {
            items.push_back(parse_item());

            if (!match(TokenKind::COMMA)) {
                break;
            }

            // Allow trailing comma
            if (!should_continue()) {
                break;
            }
        }

        return items;
    }

    // ========================================================================
    // Error Handling and Recovery
    // ========================================================================

    /// Report parse error and throw exception
    [[noreturn]] void error(const std::string& msg) {
        const auto& tok = current();
        const std::string context = std::string(tok.text);

        // Record error for recovery
        error_recovery_.add_error(msg, tok.start, tok.line, tok.col, context);

        throw ParseError(msg, tok.line, tok.col, context);
    }

    /// Report parse error at specific token
    [[noreturn]] void error_at(const TokenType& tok, const std::string& msg) {
        const std::string context = std::string(tok.text);
        error_recovery_.add_error(msg, tok.start, tok.line, tok.col, context);
        throw ParseError(msg, tok.line, tok.col, context);
    }

    /// Synchronize parser state after error (panic mode recovery)
    ///
    /// Advances until a synchronization point (e.g., semicolon, keyword)
    /// to enable parsing to continue after an error.
    void synchronize(const std::vector<TokenKind>& sync_tokens) {
        while (!is_eof()) {
            for (TokenKind sync : sync_tokens) {
                if (check(sync)) {
                    return;
                }
            }
            advance();
        }
    }

    // ========================================================================
    // Recursion Depth Tracking (stack overflow protection)
    // ========================================================================

    /// RAII guard for recursion depth tracking
    struct RecursionGuard {
        ParserBase& parser;

        explicit RecursionGuard(ParserBase& p) : parser(p) {
            if (++parser.recursion_depth_ > kMaxRecursionDepth) {
                parser.error("Maximum recursion depth exceeded (possible infinite loop)");
            }
        }

        ~RecursionGuard() {
            --parser.recursion_depth_;
        }

        // Non-copyable, non-movable
        RecursionGuard(const RecursionGuard&) = delete;
        RecursionGuard& operator=(const RecursionGuard&) = delete;
    };

    // ========================================================================
    // Arena Allocation
    // ========================================================================

    /// Allocate AST node in arena
    template<typename NodeType, typename... Args>
    [[nodiscard]] NodeType* create_node(Args&&... args) {
        return arena_.create<NodeType>(std::forward<Args>(args)...);
    }

    [[nodiscard]] Arena& arena() noexcept {
        return arena_;
    }

    // ========================================================================
    // Token Type Helpers
    // ========================================================================

    /// Get EOF token type for this domain
    [[nodiscard]] static constexpr TokenKind eof_token() noexcept {
        static_assert(requires { TokenKind::EOF_TOKEN; }, "TokenKind must have EOF_TOKEN");
        return TokenKind::EOF_TOKEN;
    }

    /// Get human-readable token name (for error messages)
    /// Override in derived class for domain-specific names
    [[nodiscard]] virtual std::string token_name(TokenKind type) const {
        // Default: use enum value
        return std::to_string(static_cast<int>(type));
    }

    // ========================================================================
    // Member Variables
    // ========================================================================

    Arena& arena_;
    std::vector<TokenType> tokens_;
    size_t pos_;
    size_t recursion_depth_;
    ErrorCollector error_recovery_;
};

// ============================================================================
/// Example: Expression Parser (for documentation)
/// ============================================================================

#if 0  // Example only, not compiled

// Example grammar spec
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
        };
        return std::span{table};
    }
};

// Example parser implementation
class ExampleParser : public ParserBase<ExampleGrammar, ExampleParser> {
public:
    using Base = ParserBase<ExampleGrammar, ExampleParser>;
    using Base::Base;

    // Top-level parsing entry point
    AstNodeType* parse_top_level() {
        return parse_expression();
    }

    // Parse prefix expression (atomic terms and unary operators)
    AstNodeType* parse_prefix() {
        if (check(TokenKind::NUMBER)) {
            auto tok = advance();
            return create_node<LiteralNode>(NodeKind::LITERAL, tok.text);
        }

        if (check(TokenKind::IDENTIFIER)) {
            auto tok = advance();
            return create_node<IdentifierNode>(NodeKind::IDENTIFIER, tok.text);
        }

        if (match(TokenKind::LPAREN)) {
            auto expr = parse_expression();
            expect(TokenKind::RPAREN);
            return expr;
        }

        error("Expected expression");
    }

    // Parse postfix expression (function calls, array access, etc.)
    AstNodeType* parse_postfix(AstNodeType* base) {
        // Function call: foo(args)
        if (match(TokenKind::LPAREN)) {
            auto args = parse_list([this]() { return parse_expression(); }, TokenKind::RPAREN);
            expect(TokenKind::RPAREN);
            return create_node<CallNode>(NodeKind::CALL, base, std::move(args));
        }

        return base;  // No postfix operator
    }

    // Create binary operator node (required by base class)
    AstNodeType* make_binary_operator(TokenKind op, AstNodeType* left, AstNodeType* right) {
        NodeKind kind;
        switch (op) {
            case TokenKind::PLUS: kind = NodeKind::ADD; break;
            case TokenKind::MINUS: kind = NodeKind::SUB; break;
            case TokenKind::STAR: kind = NodeKind::MUL; break;
            case TokenKind::SLASH: kind = NodeKind::DIV; break;
            default: error("Unknown binary operator");
        }
        return create_node<BinaryOpNode>(kind, left, right);
    }
};

#endif  // Example

} // namespace libglot
