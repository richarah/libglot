#pragma once

#include "spec.h"
#include "../util/intern.h"
#include <string_view>
#include <vector>
#include <optional>
#include <cstdint>
#include <concepts>

namespace libglot {

/// ============================================================================
/// Generic Tokenizer - Template-based lexical analysis with CRTP
/// ============================================================================
///
/// Zero-cost abstraction for domain-specific tokenization (SQL, MIME, logs, etc.)
///
/// PERFORMANCE CRITICAL:
/// - All polymorphism resolved at compile-time via CRTP
/// - No virtual function calls on hot path
/// - Branchless optimizations where possible
/// - Cache-friendly sequential scanning
/// - Perfect hash keyword lookup
///
/// DESIGN:
/// - Template parameter: Spec (TokenSpec concept)
/// - CRTP parameter: Derived (domain-specific tokenizer)
/// - Core tokenization logic in base class
/// - Domain-specific extensions in derived class
///
/// USAGE:
///   See example at bottom of file for SQL tokenizer implementation.
///
/// CUSTOMIZATION POINTS (override in derived class):
///   - tokenize_operator() - Domain-specific operators
///   - tokenize_special_string() - Domain-specific string literals
///   - tokenize_special_token() - Domain-specific tokens (e.g., parameters, pragmas)
/// ============================================================================

template<TokenSpec Spec, typename Derived>
class TokenizerBase {
public:
    // ========================================================================
    // Type Aliases
    // ========================================================================

    using TokenKind = typename Spec::TokenKind;
    using TokenType = Token<TokenKind>;
    using KeywordTable = typename Spec::KeywordTable;

    // ========================================================================
    // Construction
    // ========================================================================

    explicit TokenizerBase(std::string_view source, LocalStringPool* pool = nullptr)
        : source_(source)
        , pos_(0)
        , line_(1)
        , col_(1)
        , pool_(pool)
        , default_pool_()
    {
        if (!pool_) {
            pool_ = &default_pool_;
        }
    }

    // ========================================================================
    // Public API
    // ========================================================================

    /// Tokenize entire source into vector of tokens
    std::vector<TokenType> tokenize_all() {
        std::vector<TokenType> tokens;
        tokens.reserve(source_.size() / 8);  // Estimate: ~8 chars per token

        while (true) {
            auto tok = next_token();
            tokens.push_back(tok);
            if (tok.type == error_token()) {
                break;  // Stop on error
            }
            if (is_eof_token(tok.type)) {
                break;  // Stop on EOF
            }
        }

        return tokens;
    }

    /// Get next token (HOT PATH)
    TokenType next_token() {
        skip_whitespace_and_comments();

        if (is_eof()) {
            return make_token(eof_token());
        }

        const uint32_t start_pos = pos_;
        const uint16_t start_line = line_;
        const uint16_t start_col = col_;

        const char c = peek();

        // ====================================================================
        // Identifier or Keyword (including quoted identifiers)
        // ====================================================================

        if (Spec::is_identifier_start(c)) {
            return tokenize_identifier();
        }

        // Quoted identifiers (domain-specific quote characters)
        if (auto close_quote = Spec::identifier_quote_char(c); close_quote.has_value()) {
            return tokenize_quoted_identifier(*close_quote);
        }

        // ====================================================================
        // Number Literals
        // ====================================================================

        if (Spec::is_digit(c)) {
            return tokenize_number();
        }

        // ====================================================================
        // String Literals
        // ====================================================================

        if (c == Spec::string_quote_char()) {
            return tokenize_string();
        }

        // ====================================================================
        // Domain-Specific Special Tokens (CRTP customization point)
        // ====================================================================

        if (auto special = derived().try_tokenize_special(start_pos, start_line, start_col); special.has_value()) {
            return *special;
        }

        // ====================================================================
        // Operators and Delimiters (CRTP customization point)
        // ====================================================================

        return derived().tokenize_operator();
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
    // Position Tracking
    // ========================================================================

    [[nodiscard]] bool is_eof() const noexcept {
        return pos_ >= source_.size();
    }

    [[nodiscard]] char peek(size_t offset = 0) const noexcept {
        // Guard against integer overflow
        if (offset > source_.size() || pos_ > source_.size() - offset) {
            return '\0';
        }
        const size_t p = pos_ + offset;
        if (p >= source_.size()) {
            return '\0';
        }
        return source_[p];
    }

    char advance() noexcept {
        if (is_eof()) return '\0';

        const char c = source_[pos_++];
        if (c == '\n') {
            line_++;
            col_ = 1;
        } else {
            col_++;
        }
        return c;
    }

    void backtrack(uint32_t new_pos, uint16_t new_line, uint16_t new_col) noexcept {
        pos_ = new_pos;
        line_ = new_line;
        col_ = new_col;
    }

    // ========================================================================
    // Token Construction
    // ========================================================================

    [[nodiscard]] TokenType make_token(
        TokenKind type,
        uint32_t start_pos,
        uint32_t end_pos,
        uint16_t start_line,
        uint16_t start_col,
        const char* text = nullptr
    ) const noexcept {
        return TokenType{type, start_pos, end_pos, start_line, start_col, text};
    }

    [[nodiscard]] TokenType make_token(TokenKind type, const char* text = nullptr) const noexcept {
        return TokenType{type, pos_, pos_, line_, col_, text};
    }

    // ========================================================================
    // Whitespace and Comments (HOT PATH)
    // ========================================================================

    void skip_whitespace_and_comments() noexcept {
        while (!is_eof()) {
            const char c = peek();

            // Whitespace (use domain-specific classification)
            if (Spec::is_whitespace(c)) {
                advance();
                continue;
            }

            // Comments (domain-specific)
            std::string_view remaining(source_.data() + pos_, source_.size() - pos_);

            // Check for comment start
            if (auto comment_len = Spec::comment_start(remaining); comment_len.has_value()) {
                skip_comment(*comment_len);
                continue;
            }

            break;
        }
    }

    void skip_comment(size_t start_len) noexcept {
        // Advance past comment start sequence
        for (size_t i = 0; i < start_len; ++i) {
            advance();
        }

        // Check if this is a line comment (single-line) or block comment
        std::string_view remaining(source_.data() + pos_, source_.size() - pos_);

        // Try to find block comment end
        while (!is_eof()) {
            remaining = std::string_view(source_.data() + pos_, source_.size() - pos_);

            // Check for end of block comment
            if (auto end_len = Spec::comment_end(remaining); end_len.has_value()) {
                // Advance past comment end sequence
                for (size_t i = 0; i < *end_len; ++i) {
                    advance();
                }
                return;
            }

            // For line comments, stop at newline
            if (peek() == '\n') {
                // Don't consume the newline (whitespace skipper will do it)
                return;
            }

            advance();
        }
    }

    // ========================================================================
    // Identifier Tokenization
    // ========================================================================

    [[nodiscard]] TokenType tokenize_identifier() {
        const uint32_t start_pos = pos_;
        const uint16_t start_line = line_;
        const uint16_t start_col = col_;

        // Scan identifier characters
        while (!is_eof() && Spec::is_identifier_continue(peek())) {
            advance();
        }

        std::string_view text = source_.substr(start_pos, pos_ - start_pos);
        const char* interned = pool_->intern(text);

        // Check if it's a keyword (O(1) perfect hash lookup)
        TokenKind type = KeywordTable::lookup(text);

        return make_token(type, start_pos, pos_, start_line, start_col, interned);
    }

    [[nodiscard]] TokenType tokenize_quoted_identifier(char close_quote) {
        const uint32_t start_pos = pos_;
        const uint16_t start_line = line_;
        const uint16_t start_col = col_;

        advance();  // Skip opening quote

        const uint32_t content_start = pos_;

        // Scan until closing quote
        while (!is_eof() && peek() != close_quote) {
            advance();
        }

        const uint32_t content_end = pos_;

        if (!is_eof()) {
            advance();  // Skip closing quote
        }

        // Store identifier WITHOUT quotes
        std::string_view text = source_.substr(content_start, content_end - content_start);
        const char* interned = pool_->intern(text);

        return make_token(identifier_token(), start_pos, pos_, start_line, start_col, interned);
    }

    // ========================================================================
    // Number Tokenization
    // ========================================================================

    [[nodiscard]] TokenType tokenize_number() {
        const uint32_t start_pos = pos_;
        const uint16_t start_line = line_;
        const uint16_t start_col = col_;

        // Hex: 0x...
        if (peek() == '0' && (peek(1) == 'x' || peek(1) == 'X')) {
            advance(); advance();
            while (!is_eof() && Spec::is_hex_digit(peek())) {
                advance();
            }
            std::string_view text = source_.substr(start_pos, pos_ - start_pos);
            return make_token(number_token(), start_pos, pos_, start_line, start_col, pool_->intern(text));
        }

        // Binary: 0b...
        if (peek() == '0' && (peek(1) == 'b' || peek(1) == 'B')) {
            advance(); advance();
            while (!is_eof() && (peek() == '0' || peek() == '1')) {
                advance();
            }
            std::string_view text = source_.substr(start_pos, pos_ - start_pos);
            return make_token(number_token(), start_pos, pos_, start_line, start_col, pool_->intern(text));
        }

        // Decimal number
        while (!is_eof() && Spec::is_digit(peek())) {
            advance();
        }

        // Decimal point
        if (peek() == '.' && Spec::is_digit(peek(1))) {
            advance();  // .
            while (!is_eof() && Spec::is_digit(peek())) {
                advance();
            }
        }

        // Exponent
        if (peek() == 'e' || peek() == 'E') {
            advance();
            if (peek() == '+' || peek() == '-') {
                advance();
            }
            while (!is_eof() && Spec::is_digit(peek())) {
                advance();
            }
        }

        std::string_view text = source_.substr(start_pos, pos_ - start_pos);
        return make_token(number_token(), start_pos, pos_, start_line, start_col, pool_->intern(text));
    }

    // ========================================================================
    // String Tokenization
    // ========================================================================

    [[nodiscard]] TokenType tokenize_string() {
        const uint32_t start_pos = pos_;
        const uint16_t start_line = line_;
        const uint16_t start_col = col_;

        const char quote = advance();  // Opening quote

        while (!is_eof()) {
            const char c = peek();

            if (c == quote) {
                // Check for escaped quote (doubled)
                if (peek(1) == quote) {
                    advance(); advance();
                    continue;
                }
                advance();  // Closing quote
                break;
            }

            if (c == '\\') {
                advance();  // Backslash
                if (!is_eof()) {
                    advance();  // Escaped char
                }
                continue;
            }

            advance();
        }

        std::string_view text = source_.substr(start_pos, pos_ - start_pos);
        return make_token(string_token(), start_pos, pos_, start_line, start_col, pool_->intern(text));
    }

    // ========================================================================
    // Token Type Helpers (must be implemented by Derived)
    // ========================================================================

    /// Get the IDENTIFIER token type for this domain
    [[nodiscard]] static constexpr TokenKind identifier_token() noexcept {
        return TokenKind::IDENTIFIER;
    }

    /// Get the NUMBER token type for this domain
    /// Override in derived class if domain has different number types
    [[nodiscard]] virtual TokenKind number_token() const noexcept {
        static_assert(requires { TokenKind::NUMBER; }, "TokenKind must have a NUMBER variant");
        return TokenKind::NUMBER;
    }

    /// Get the STRING token type for this domain
    /// Override in derived class if domain has different string types
    [[nodiscard]] virtual TokenKind string_token() const noexcept {
        static_assert(requires { TokenKind::STRING; }, "TokenKind must have a STRING variant");
        return TokenKind::STRING;
    }

    /// Get the EOF token type for this domain
    [[nodiscard]] static constexpr TokenKind eof_token() noexcept {
        static_assert(requires { TokenKind::EOF_TOKEN; }, "TokenKind must have an EOF_TOKEN variant");
        return TokenKind::EOF_TOKEN;
    }

    /// Get the ERROR token type for this domain
    [[nodiscard]] static constexpr TokenKind error_token() noexcept {
        static_assert(requires { TokenKind::ERROR; }, "TokenKind must have an ERROR variant");
        return TokenKind::ERROR;
    }

    /// Check if token type is EOF
    [[nodiscard]] static constexpr bool is_eof_token(TokenKind type) noexcept {
        return type == eof_token();
    }

    // ========================================================================
    // String Pool Access
    // ========================================================================

    [[nodiscard]] LocalStringPool* pool() noexcept {
        return pool_;
    }

    [[nodiscard]] const LocalStringPool* pool() const noexcept {
        return pool_;
    }

    [[nodiscard]] std::string_view source() const noexcept {
        return source_;
    }

    [[nodiscard]] uint32_t position() const noexcept {
        return pos_;
    }

    [[nodiscard]] uint16_t line() const noexcept {
        return line_;
    }

    [[nodiscard]] uint16_t column() const noexcept {
        return col_;
    }

    // ========================================================================
    // Member Variables
    // ========================================================================

    std::string_view source_;
    uint32_t pos_;
    uint16_t line_;
    uint16_t col_;
    LocalStringPool* pool_;
    LocalStringPool default_pool_;
};

// ============================================================================
/// Minimal Tokenizer (for simple domains with no special tokens/operators)
/// ============================================================================

template<TokenSpec Spec>
class SimpleTokenizer : public TokenizerBase<Spec, SimpleTokenizer<Spec>> {
public:
    using Base = TokenizerBase<Spec, SimpleTokenizer<Spec>>;
    using typename Base::TokenKind;
    using typename Base::TokenType;

    using Base::Base;  // Inherit constructors

    // ========================================================================
    // CRTP Customization Points (required by base class)
    // ========================================================================

    /// Try to tokenize domain-specific special token
    /// Returns nullopt if no special token recognized
    [[nodiscard]] std::optional<TokenType> try_tokenize_special(
        uint32_t /*start_pos*/,
        uint16_t /*start_line*/,
        uint16_t /*start_col*/
    ) noexcept {
        return std::nullopt;  // No special tokens in simple tokenizer
    }

    /// Tokenize operators and delimiters
    /// For simple domains, we just return ERROR for unknown characters
    [[nodiscard]] TokenType tokenize_operator() {
        const uint32_t start_pos = this->pos_;
        const uint16_t start_line = this->line_;
        const uint16_t start_col = this->col_;

        this->advance();  // Consume unknown character

        return this->make_token(Base::error_token(), start_pos, this->pos_, start_line, start_col);
    }
};

// ============================================================================
/// Example: SQL Tokenizer (for documentation)
/// ============================================================================

#if 0  // Example only, not compiled

// See sql/tokenizer.h for full SQL tokenizer implementation
class SQLTokenizer : public TokenizerBase<SQLTokenSpec, SQLTokenizer> {
public:
    using Base = TokenizerBase<SQLTokenSpec, SQLTokenizer>;
    using Base::Base;

    /// SQL-specific: Try to tokenize parameters (@name, :name, $1, ?)
    std::optional<TokenType> try_tokenize_special(uint32_t start_pos, uint16_t start_line, uint16_t start_col) {
        char c = peek();

        // Dollar-quoted strings (PostgreSQL)
        if (c == '$' && (peek(1) == '$' || is_identifier_start(peek(1)))) {
            return tokenize_dollar_string();
        }

        // Parameters
        if (c == '@' || c == ':' || c == '$' || c == '?') {
            return tokenize_parameter();
        }

        return std::nullopt;
    }

    /// SQL-specific: Operators (+, -, *, /, ||, <=>, etc.)
    TokenType tokenize_operator() {
        // ... SQL operator tokenization logic
    }

private:
    TokenType tokenize_dollar_string() { /* ... */ }
    TokenType tokenize_parameter() { /* ... */ }
};

#endif  // Example

} // namespace libglot
