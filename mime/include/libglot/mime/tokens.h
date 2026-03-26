#pragma once

#include <string_view>
#include <vector>
#include <cctype>

namespace libglot::mime {

/// ============================================================================
/// MIME Token Types
/// ============================================================================
///
/// Minimal token set for parsing MIME headers:
///   Content-Type: text/plain
///   Subject: Hello World
///
///   <body>
/// ============================================================================

enum class MimeTokenType {
    // Structure
    IDENTIFIER,     ///< Header field name (Content-Type, Subject, etc.)
    COLON,          ///< ':'
    STRING,         ///< Header value (may contain spaces)
    NEWLINE,        ///< '\n' or '\r\n'

    // Special
    EOF_TOKEN,      ///< End of input
    INVALID         ///< Invalid token (error recovery)
};

/// ============================================================================
/// MIME Token Structure
/// ============================================================================

struct MimeToken {
    MimeTokenType type;
    std::string_view text;
    size_t start;
    size_t end;
    size_t line;
    size_t col;
};

/// ============================================================================
/// Simple MIME Tokenizer
/// ============================================================================
///
/// Tokenizes MIME headers. Does not tokenize body content.
/// Stops at first blank line (end of headers).
/// ============================================================================

class MimeTokenizer {
public:
    explicit MimeTokenizer(std::string_view source)
        : source_(source)
        , pos_(0)
        , line_(1)
        , col_(1)
        , after_colon_(false)
    {}

    std::vector<MimeToken> tokenize_all() {
        std::vector<MimeToken> tokens;

        while (true) {
            auto tok = next_token();
            tokens.push_back(tok);

            if (tok.type == MimeTokenType::EOF_TOKEN) {
                break;
            }

            // Check for blank line (NEWLINE followed by another NEWLINE)
            // This indicates end of headers
            if (tok.type == MimeTokenType::NEWLINE && peek() == '\n') {
                // Consume the blank line NEWLINE and position after it
                advance();  // Skip the '\n' of the blank line
                tokens.push_back(make_token(MimeTokenType::NEWLINE, pos_ - 1, pos_));
                // Now produce EOF (body extraction will use current position)
                tokens.push_back(make_token(MimeTokenType::EOF_TOKEN, pos_, pos_));
                break;
            }
        }

        return tokens;
    }

private:
    MimeToken next_token() {
        skip_whitespace_except_newline();

        if (is_eof()) {
            return make_token(MimeTokenType::EOF_TOKEN, pos_, pos_);
        }

        const char c = peek();
        const size_t start = pos_;

        // Newline
        if (c == '\n') {
            advance();
            line_++;
            col_ = 1;
            after_colon_ = false;  // Reset state after newline
            return make_token(MimeTokenType::NEWLINE, start, pos_);
        }

        if (c == '\r' && peek_next() == '\n') {
            advance();
            advance();
            line_++;
            col_ = 1;
            after_colon_ = false;  // Reset state after newline
            return make_token(MimeTokenType::NEWLINE, start, pos_);
        }

        // Colon
        if (c == ':') {
            advance();
            after_colon_ = true;  // Next token should be a STRING (header value)
            return make_token(MimeTokenType::COLON, start, pos_);
        }

        // Identifier (header field name) - alphanumeric + '-'
        // Only tokenize identifiers at start of line, not after colon
        if (!after_colon_ && (std::isalpha(c) || c == '-')) {
            while (!is_eof() && (std::isalnum(peek()) || peek() == '-')) {
                advance();
            }
            return make_token(MimeTokenType::IDENTIFIER, start, pos_);
        }

        // String (header value) - everything until newline after colon
        // This is simplified; real MIME allows folded headers
        if (std::isprint(c) && c != ':' && c != '\n' && c != '\r') {
            while (!is_eof() && peek() != '\n' && peek() != '\r') {
                advance();
            }
            // Trim trailing whitespace
            size_t end = pos_;
            while (end > start && std::isspace(source_[end - 1]) && source_[end - 1] != '\n' && source_[end - 1] != '\r') {
                end--;
            }
            return make_token(MimeTokenType::STRING, start, end);
        }

        // Invalid
        advance();
        return make_token(MimeTokenType::INVALID, start, pos_);
    }

    void skip_whitespace_except_newline() {
        while (!is_eof() && std::isspace(peek()) && peek() != '\n' && peek() != '\r') {
            advance();
        }
    }

    [[nodiscard]] bool is_eof() const noexcept {
        return pos_ >= source_.size();
    }

    [[nodiscard]] char peek() const noexcept {
        return is_eof() ? '\0' : source_[pos_];
    }

    [[nodiscard]] char peek_next() const noexcept {
        return (pos_ + 1 >= source_.size()) ? '\0' : source_[pos_ + 1];
    }

    void advance() noexcept {
        if (!is_eof()) {
            pos_++;
            col_++;
        }
    }

    MimeToken make_token(MimeTokenType type, size_t start, size_t end) const {
        return MimeToken{
            type,
            source_.substr(start, end - start),
            start,
            end,
            line_,
            col_
        };
    }

    std::string_view source_;
    size_t pos_;
    size_t line_;
    size_t col_;
    bool after_colon_;  ///< Track if we just saw a colon (next token should be STRING)
};

/// ============================================================================
/// Token Type Name (for debugging/errors)
/// ============================================================================

inline std::string_view mime_token_type_name(MimeTokenType type) {
    switch (type) {
        case MimeTokenType::IDENTIFIER: return "IDENTIFIER";
        case MimeTokenType::COLON: return "COLON";
        case MimeTokenType::STRING: return "STRING";
        case MimeTokenType::NEWLINE: return "NEWLINE";
        case MimeTokenType::EOF_TOKEN: return "EOF";
        case MimeTokenType::INVALID: return "INVALID";
    }
    return "UNKNOWN";
}

} // namespace libglot::mime
