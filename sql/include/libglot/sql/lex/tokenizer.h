#pragma once

#include "fwd.h"
#include "tokens.h"
#include "intern.h"
#include "keywords.h"
#include <string_view>
#include <vector>
#include <optional>
#include <cstdint>

namespace libglot::sql::lex {

/// Per-dialect lexing configuration.
///
/// Only genuinely lexical differences belong here; syntactic dialect
/// differences are handled by the parser and generator.
struct TokenizerConfig {
    /// '#' starts a line comment (MySQL-style). Off for SQL Server, where
    /// '#' introduces temp-table names, and PostgreSQL, where '#>' / '#>>'
    /// are JSON path operators.
    bool hash_line_comment = true;
    /// '#' may start an identifier (SQL Server temp tables: #t, ##t).
    bool hash_identifier_start = false;
    /// ':' followed by a name lexes as a host parameter (:name). Off for
    /// Snowflake, where ':' is the JSON path access operator (col:field).
    bool colon_parameters = true;
    /// '?' lexes as the QUESTION operator (PostgreSQL jsonb key-exists)
    /// instead of a positional parameter placeholder.
    bool question_is_operator = false;
    /// '[' starts a bracket-quoted identifier ([name], SQL Server / Access
    /// style). Off for Snowflake, where '[' is array subscripting
    /// (col:field[0]) and identifiers are quoted with double quotes.
    bool bracket_identifiers = true;

    static constexpr TokenizerConfig default_config() noexcept { return {}; }
    static constexpr TokenizerConfig mysql() noexcept { return {}; }
    static constexpr TokenizerConfig postgresql() noexcept {
        return {.hash_line_comment = false, .hash_identifier_start = false, .colon_parameters = true,
                .question_is_operator = true, .bracket_identifiers = true};
    }
    static constexpr TokenizerConfig sqlserver() noexcept {
        return {.hash_line_comment = false, .hash_identifier_start = true, .colon_parameters = true};
    }
    static constexpr TokenizerConfig snowflake() noexcept {
        return {.hash_line_comment = true, .hash_identifier_start = false, .colon_parameters = false,
                .question_is_operator = false, .bracket_identifiers = false};
    }
    /// BigQuery quotes identifiers with backticks, never `[ident]` brackets -
    /// bracket_identifiers must be off so `arr[OFFSET(0)]` lexes as
    /// array-subscript brackets rather than a single bracket-quoted
    /// identifier token.
    static constexpr TokenizerConfig bigquery() noexcept {
        return {.hash_line_comment = true, .hash_identifier_start = false, .colon_parameters = true,
                .question_is_operator = false, .bracket_identifiers = false};
    }
};

/// Tokenizer - converts SQL source text into tokens
/// Fast scalar implementation with perfect hash keyword lookup
/// Thread-safe (stateless), uses LocalStringPool for interning
class Tokenizer {
public:
    explicit Tokenizer(std::string_view source, LocalStringPool* pool = nullptr,
                       TokenizerConfig config = {})
        : source_(source)
        , pos_(0)
        , line_(1)
        , col_(1)
        , pool_(pool)
        , default_pool_()
        , config_(config)
    {
        if (!pool_) {
            pool_ = &default_pool_;
        }
    }

    /// Tokenize entire source into vector of tokens
    std::vector<Token> tokenize_all() {
        std::vector<Token> tokens;
        tokens.reserve(source_.size() / 8); // Estimate: ~8 chars per token

        while (true) {
            auto tok = next_token();
            tokens.push_back(tok);
            if (tok.type == TokenType::EOF_TOKEN) break;
        }

        return tokens;
    }

    /// Get next token
    Token next_token() {
        skip_whitespace_and_comments();

        if (is_eof()) {
            return make_token(TokenType::EOF_TOKEN);
        }

        char c = peek();

        // Identifiers and keywords (including quoted identifiers)
        if (is_identifier_start(c) || c == '"' || c == '`' ||
            (c == '[' && config_.bracket_identifiers) ||
            (c == '#' && config_.hash_identifier_start)) {
            return tokenize_identifier();
        }

        // Numbers
        if (is_digit(c)) {
            return tokenize_number();
        }

        // Strings (single quotes only - double quotes are for identifiers in SQL standard)
        if (c == '\'') {
            return tokenize_string(c);
        }

        // Dollar-quoted strings (PostgreSQL): $$...$$ or $tag$...$tag$
        if (c == '$' && (peek(1) == '$' || is_identifier_start(peek(1)))) {
            // Check if this looks like a dollar quote
            size_t lookahead = 1;
            if (peek(1) == '$') {
                // $$ - definitely a dollar quote
                return tokenize_dollar_string();
            } else {
                // Might be $tag$ - look for closing $
                // Note: dollar is NOT part of the tag name, only letters/digits/_
                while (lookahead < 64 && is_identifier_start(peek(lookahead))) {
                    lookahead++;
                }
                // Continue with digits (but not $)
                while (lookahead < 64 && is_digit(peek(lookahead))) {
                    lookahead++;
                }
                if (peek(lookahead) == '$') {
                    // Found $tag$ pattern - this is a dollar quote
                    return tokenize_dollar_string();
                }
                // Not a dollar quote pattern, fall through to parameter handling
            }
        }

        // Parameters: @name (T-SQL), :name (Oracle), $1 (Postgres), ?
        // When ':' is a path operator (Snowflake), it lexes as COLON instead.
        // When '?' is an operator (PostgreSQL jsonb), it lexes as QUESTION.
        if (c == '@' || (c == ':' && config_.colon_parameters) || c == '$' ||
            (c == '?' && !config_.question_is_operator)) {
            return tokenize_parameter();
        }

        // Operators and delimiters
        return tokenize_operator();
    }

private:
    bool is_eof() const { return pos_ >= source_.size(); }

    char peek(size_t offset = 0) const {
        // Guard against integer overflow: check offset is reasonable before adding
        if (offset > source_.size() || pos_ > source_.size() - offset) {
            return '\0';  // Out of bounds
        }
        size_t p = pos_ + offset;
        if (p >= source_.size()) {
            return '\0';  // Out of bounds
        }
        return source_[p];
    }

    char advance() {
        if (is_eof()) return '\0';
        char c = source_[pos_++];
        if (c == '\n') {
            line_++;
            col_ = 1;
        } else {
            col_++;
        }
        return c;
    }

    Token make_token(TokenType type, uint32_t start_pos, uint32_t end_pos,
                     uint32_t start_line, uint32_t start_col, const char* text = nullptr) {
        return Token{type, static_cast<uint32_t>(start_pos), static_cast<uint32_t>(end_pos),
                     start_line, start_col, text};
    }

    Token make_token(TokenType type, const char* text = nullptr) {
        return Token{type, static_cast<uint32_t>(pos_), static_cast<uint32_t>(pos_), line_, col_, text};
    }

    void skip_whitespace_and_comments() {
        while (!is_eof()) {
            char c = peek();

            // Whitespace
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                advance();
                continue;
            }

            // Line comment: -- or # (where the dialect allows # comments)
            if ((c == '-' && peek(1) == '-') || (c == '#' && config_.hash_line_comment)) {
                while (!is_eof() && peek() != '\n') {
                    advance();
                }
                continue;
            }

            // Block comment: /* */
            if (c == '/' && peek(1) == '*') {
                advance(); advance(); // Skip /*
                while (!is_eof()) {
                    if (peek() == '*' && peek(1) == '/') {
                        advance(); advance(); // Skip */
                        break;
                    }
                    advance();
                }
                continue;
            }

            break;
        }
    }

    static bool is_identifier_start(char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
    }

    static bool is_identifier_continue(char c) {
        return is_identifier_start(c) || is_digit(c) || c == '$';
    }

    static bool is_digit(char c) {
        return c >= '0' && c <= '9';
    }

    static bool is_hex_digit(char c) {
        return is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    }

    Token tokenize_identifier() {
        uint32_t start_pos = pos_;
        uint32_t start_line = line_;
        uint32_t start_col = col_;

        // Handle quoted identifiers
        if (peek() == '"' || peek() == '`' || peek() == '[') {
            char quote = advance();
            char end_quote = (quote == '[') ? ']' : quote;
            uint32_t content_start = pos_;  // Start of actual identifier (after opening quote)

            // A doubled closing quote inside the identifier is an escaped
            // literal quote character ("emb""edded" -> emb"edded). When one
            // is present, build the unescaped text in a scratch buffer and
            // intern that; otherwise intern the raw content span directly.
            bool has_escape = false;
            std::string unescaped;
            while (!is_eof()) {
                char c = peek();
                if (c == end_quote) {
                    if (peek(1) == end_quote) {
                        if (!has_escape) {
                            unescaped.assign(source_.substr(content_start, pos_ - content_start));
                            has_escape = true;
                        }
                        unescaped.push_back(end_quote);
                        advance();
                        advance();
                        continue;
                    }
                    break;  // Genuine closing quote
                }
                if (has_escape) {
                    unescaped.push_back(c);
                }
                advance();
            }
            uint32_t content_end = pos_;  // End of actual identifier (before closing quote)
            if (!is_eof()) advance(); // Skip closing quote

            // Store identifier WITHOUT quotes (and with escapes collapsed)
            const char* interned = has_escape
                ? pool_->intern(unescaped)
                : pool_->intern(source_.substr(content_start, content_end - content_start));
            return make_token(TokenType::IDENTIFIER, start_pos, pos_, start_line, start_col, interned);
        }

        // Temp-table prefix (SQL Server): #local or ##global
        if (config_.hash_identifier_start) {
            while (peek() == '#') {
                advance();
            }
        }

        // Regular identifier
        while (!is_eof() && is_identifier_continue(peek())) {
            advance();
        }

        std::string_view text = source_.substr(start_pos, pos_ - start_pos);
        const char* interned = pool_->intern(text);

        // Check if it's a keyword
        TokenType type = keyword_type(text);

        return make_token(type, start_pos, pos_, start_line, start_col, interned);
    }

    Token tokenize_number() {
        uint32_t start_pos = pos_;
        uint32_t start_line = line_;
        uint32_t start_col = col_;

        // Hex: 0x...
        if (peek() == '0' && (peek(1) == 'x' || peek(1) == 'X')) {
            advance(); advance();
            while (!is_eof() && is_hex_digit(peek())) {
                advance();
            }
            std::string_view text = source_.substr(start_pos, pos_ - start_pos);
            return make_token(TokenType::NUMBER, start_pos, pos_, start_line, start_col, pool_->intern(text));
        }

        // Binary: 0b...
        if (peek() == '0' && (peek(1) == 'b' || peek(1) == 'B')) {
            advance(); advance();
            while (!is_eof() && (peek() == '0' || peek() == '1')) {
                advance();
            }
            std::string_view text = source_.substr(start_pos, pos_ - start_pos);
            return make_token(TokenType::NUMBER, start_pos, pos_, start_line, start_col, pool_->intern(text));
        }

        // Decimal number
        while (!is_eof() && is_digit(peek())) {
            advance();
        }

        // Decimal point
        if (peek() == '.' && is_digit(peek(1))) {
            advance(); // .
            while (!is_eof() && is_digit(peek())) {
                advance();
            }
        }

        // Exponent
        if (peek() == 'e' || peek() == 'E') {
            advance();
            if (peek() == '+' || peek() == '-') advance();
            while (!is_eof() && is_digit(peek())) {
                advance();
            }
        }

        std::string_view text = source_.substr(start_pos, pos_ - start_pos);
        return make_token(TokenType::NUMBER, start_pos, pos_, start_line, start_col, pool_->intern(text));
    }

    Token tokenize_string(char quote) {
        uint32_t start_pos = pos_;
        uint32_t start_line = line_;
        uint32_t start_col = col_;

        advance(); // Opening quote

        while (!is_eof()) {
            char c = peek();

            if (c == quote) {
                // Check for escaped quote (doubled)
                if (peek(1) == quote) {
                    advance(); advance();
                    continue;
                }
                advance(); // Closing quote
                break;
            }

            if (c == '\\') {
                advance(); // Backslash
                if (!is_eof()) advance(); // Escaped char
                continue;
            }

            advance();
        }

        std::string_view text = source_.substr(start_pos, pos_ - start_pos);
        return make_token(TokenType::STRING, start_pos, pos_, start_line, start_col, pool_->intern(text));
    }

    Token tokenize_dollar_string() {
        uint32_t start_pos = pos_;
        uint32_t start_line = line_;
        uint32_t start_col = col_;

        // Parse opening delimiter: $$ or $tag$
        advance(); // First $

        std::string delimiter = "$";
        if (is_identifier_start(peek())) {
            // Tagged delimiter: $tag$
            // Note: tag can only contain letters, digits, and underscores (NOT $)
            size_t tag_start = pos_;
            while (!is_eof() && (is_identifier_start(peek()) || is_digit(peek()))) {
                advance();
            }
            delimiter += std::string(source_.substr(tag_start, pos_ - tag_start));
        }

        if (peek() != '$') {
            // Malformed delimiter - treat as error
            return make_token(TokenType::ERROR, start_pos, pos_, start_line, start_col);
        }

        advance(); // Closing $ of delimiter
        delimiter += "$";

        // Now find the matching closing delimiter
        // We need to search for the exact delimiter sequence
        bool found_end = false;

        while (!is_eof()) {
            // Check if we're at the start of the closing delimiter
            if (peek() == '$') {
                size_t check_pos = pos_;
                bool matches = true;

                // Try to match the full delimiter
                for (size_t i = 0; i < delimiter.size() && check_pos + i < source_.size(); ++i) {
                    if (source_[check_pos + i] != delimiter[i]) {
                        matches = false;
                        break;
                    }
                }

                if (matches && check_pos + delimiter.size() <= source_.size()) {
                    // Found matching delimiter
                    for (size_t i = 0; i < delimiter.size(); ++i) {
                        advance();
                    }
                    found_end = true;
                    break;
                }
            }

            advance();
        }

        if (!found_end) {
            // Unterminated dollar-quoted string - still return STRING token
            // Error handling will be done at parse time if needed
        }

        // Return the entire dollar-quoted string including delimiters
        std::string_view text = source_.substr(start_pos, pos_ - start_pos);
        return make_token(TokenType::STRING, start_pos, pos_, start_line, start_col, pool_->intern(text));
    }

    Token tokenize_parameter() {
        uint32_t start_pos = pos_;
        uint32_t start_line = line_;
        uint32_t start_col = col_;

        char prefix = advance(); // @ or : or $ or ?

        // For standalone ? parameter, return immediately
        if (prefix == '?') {
            std::string_view text = source_.substr(start_pos, pos_ - start_pos);
            return make_token(TokenType::PARAMETER, start_pos, pos_, start_line, start_col, pool_->intern(text));
        }

        // For :=, don't treat as parameter (it's assignment operator)
        if (prefix == ':' && peek() == '=') {
            // Backtrack - this is COLON_EQUALS, not a parameter
            pos_ = start_pos;
            col_ = start_col;
            return tokenize_operator();
        }

        // For :: (double colon cast), don't treat as parameter
        if (prefix == ':' && peek() == ':') {
            // Backtrack - this is DOUBLE_COLON, not a parameter
            pos_ = start_pos;
            col_ = start_col;
            return tokenize_operator();
        }

        // For $1, $2, etc. (Postgres positional parameters)
        if (prefix == '$' && is_digit(peek())) {
            while (!is_eof() && is_digit(peek())) {
                advance();
            }
            std::string_view text = source_.substr(start_pos, pos_ - start_pos);
            return make_token(TokenType::PARAMETER, start_pos, pos_, start_line, start_col, pool_->intern(text));
        }

        // For @name or :name (must be followed by identifier)
        if (is_identifier_start(peek())) {
            while (!is_eof() && is_identifier_continue(peek())) {
                advance();
            }
            std::string_view text = source_.substr(start_pos, pos_ - start_pos);
            return make_token(TokenType::PARAMETER, start_pos, pos_, start_line, start_col, pool_->intern(text));
        }

        // If not followed by identifier/digit, backtrack and treat as operator
        // (e.g., @ alone, : alone, $ alone)
        pos_ = start_pos;
        col_ = start_col;
        return tokenize_operator();
    }

    Token tokenize_operator() {
        uint32_t start_pos = pos_;
        uint32_t start_line = line_;
        uint32_t start_col = col_;

        char c = advance();
        char next = peek();

        // Three-character operators
        if (c == '<' && next == '=' && peek(1) == '>') {
            advance(); advance(); // <=
            return make_token(TokenType::NULL_SAFE_EQ, start_pos, pos_, start_line, start_col);
        }

        // Two-character operators
        if (c == '|' && next == '|') { advance(); return make_token(TokenType::CONCAT, start_pos, pos_, start_line, start_col); }
        if (c == '<' && next == '>') { advance(); return make_token(TokenType::NEQ, start_pos, pos_, start_line, start_col); }
        if (c == '@' && next == '>') { advance(); return make_token(TokenType::AT_GT, start_pos, pos_, start_line, start_col); }
        if (c == '<' && next == '@') { advance(); return make_token(TokenType::LT_AT, start_pos, pos_, start_line, start_col); }
        if (c == '<' && next == '=') { advance(); return make_token(TokenType::LTE, start_pos, pos_, start_line, start_col); }
        if (c == '>' && next == '=') { advance(); return make_token(TokenType::GTE, start_pos, pos_, start_line, start_col); }
        if (c == '!' && next == '=') { advance(); return make_token(TokenType::NEQ, start_pos, pos_, start_line, start_col); }
        if (c == '=' && next == '>') { advance(); return make_token(TokenType::FAT_ARROW, start_pos, pos_, start_line, start_col); }
        if (c == ':' && next == '=') { advance(); return make_token(TokenType::COLON_EQUALS, start_pos, pos_, start_line, start_col); }
        if (c == ':' && next == ':') { advance(); return make_token(TokenType::DOUBLE_COLON, start_pos, pos_, start_line, start_col); }
        if (c == '.' && next == '.') { advance(); return make_token(TokenType::DOUBLE_DOT, start_pos, pos_, start_line, start_col); }
        if (c == '-' && next == '>') {
            advance();
            if (peek() == '>') { advance(); return make_token(TokenType::LONG_ARROW, start_pos, pos_, start_line, start_col); }
            return make_token(TokenType::ARROW, start_pos, pos_, start_line, start_col);
        }
        if (c == '#' && next == '>') {
            advance();
            if (peek() == '>') { advance(); return make_token(TokenType::HASH_LONG_ARROW, start_pos, pos_, start_line, start_col); }
            return make_token(TokenType::HASH_ARROW, start_pos, pos_, start_line, start_col);
        }

        // Single-character operators
        switch (c) {
            case '+': return make_token(TokenType::PLUS, start_pos, pos_, start_line, start_col);
            case '-': return make_token(TokenType::MINUS, start_pos, pos_, start_line, start_col);
            case '*': return make_token(TokenType::STAR, start_pos, pos_, start_line, start_col);
            case '/': return make_token(TokenType::SLASH, start_pos, pos_, start_line, start_col);
            case '%': return make_token(TokenType::PERCENT, start_pos, pos_, start_line, start_col);
            case '^': return make_token(TokenType::CARET, start_pos, pos_, start_line, start_col);
            case '&': return make_token(TokenType::AMPERSAND, start_pos, pos_, start_line, start_col);
            case '|': return make_token(TokenType::PIPE, start_pos, pos_, start_line, start_col);
            case '~': return make_token(TokenType::TILDE, start_pos, pos_, start_line, start_col);
            case '=': return make_token(TokenType::EQ, start_pos, pos_, start_line, start_col);
            case '<': return make_token(TokenType::LT, start_pos, pos_, start_line, start_col);
            case '>': return make_token(TokenType::GT, start_pos, pos_, start_line, start_col);
            case '(': return make_token(TokenType::LPAREN, start_pos, pos_, start_line, start_col);
            case ')': return make_token(TokenType::RPAREN, start_pos, pos_, start_line, start_col);
            case '[': return make_token(TokenType::LBRACKET, start_pos, pos_, start_line, start_col);
            case ']': return make_token(TokenType::RBRACKET, start_pos, pos_, start_line, start_col);
            case '{': return make_token(TokenType::LBRACE, start_pos, pos_, start_line, start_col);
            case '}': return make_token(TokenType::RBRACE, start_pos, pos_, start_line, start_col);
            case ',': return make_token(TokenType::COMMA, start_pos, pos_, start_line, start_col);
            case ';': return make_token(TokenType::SEMICOLON, start_pos, pos_, start_line, start_col);
            case '.': return make_token(TokenType::DOT, start_pos, pos_, start_line, start_col);
            case ':': return make_token(TokenType::COLON, start_pos, pos_, start_line, start_col);
            case '#': return make_token(TokenType::HASH, start_pos, pos_, start_line, start_col);
            case '?': return make_token(TokenType::QUESTION, start_pos, pos_, start_line, start_col);
            default: return make_token(TokenType::ERROR, start_pos, pos_, start_line, start_col);
        }
    }

    TokenType keyword_type(std::string_view text) {
        // O(1) perfect hash lookup - 5-10x faster than linear scan
        return KeywordLookup::lookup(text);
    }

    std::string_view source_;
    size_t pos_;
    uint32_t line_;
    uint32_t col_;
    LocalStringPool* pool_;
    LocalStringPool default_pool_;
    TokenizerConfig config_;
};

} // namespace libglot::sql::lex
