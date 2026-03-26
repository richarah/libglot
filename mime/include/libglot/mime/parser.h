#pragma once

#include "../../../../core/include/libglot/parse/parser.h"
#include "grammar.h"
#include "ast_nodes.h"
#include "tokens.h"

namespace libglot::mime {

/// ============================================================================
/// MIME Parser - CRTP Instantiation of ParserBase
/// ============================================================================
///
/// Parses MIME headers:
///   Content-Type: text/plain
///   Subject: Hello World
///
///   <body>
/// ============================================================================

class MimeParser : public libglot::ParserBase<MimeGrammarSpec, MimeParser> {
public:
    using Base = libglot::ParserBase<MimeGrammarSpec, MimeParser>;
    using TokenType = Base::TokenType;
    using TK = MimeTokenType;

    // ========================================================================
    // Construction
    // ========================================================================

    explicit MimeParser(libglot::Arena& arena, std::string_view source)
        : MimeParser(arena, tokenize_and_copy(arena, source))
    {}

    // ========================================================================
    // Top-Level Parsing Entry Point (Required by Base)
    // ========================================================================

    Message* parse_top_level() {
        return parse_message();
    }

    // ========================================================================
    // CRTP Customization Points (Required by ParserBase)
    // ========================================================================

    /// Parse prefix expression (not used for MIME)
    [[nodiscard]] MimeNode* parse_prefix() {
        this->error("Unexpected token in MIME header");
        return nullptr;
    }

    /// Parse postfix expression (not used for MIME)
    [[nodiscard]] MimeNode* parse_postfix(MimeNode* base) {
        return base;
    }

    /// Create binary operator node (not used for MIME)
    [[nodiscard]] MimeNode* make_binary_operator(TK, MimeNode*, MimeNode*) {
        this->error("Binary operators not supported in MIME");
        return nullptr;
    }

    // ========================================================================
    // MIME-Specific Parsing Rules
    // ========================================================================

    /// Parse MIME message (headers + optional body)
    Message* parse_message() {
        std::vector<Header*> headers;

        // Parse headers until blank line or EOF
        while (!this->check(TK::EOF_TOKEN)) {
            // Check for blank line (NEWLINE followed by NEWLINE or EOF)
            if (this->check(TK::NEWLINE)) {
                this->advance();
                break;
            }

            headers.push_back(parse_header());
        }

        // Body is everything after blank line
        // The EOF token's start position points to where the body begins
        std::string_view body = "";
        if (this->check(TK::EOF_TOKEN)) {
            size_t body_start = this->current().start;
            if (body_start < source_.size()) {
                body = source_.substr(body_start);
            }
        }

        return this->template create_node<Message>(headers, body);
    }

    /// Parse single header (field: value)
    Header* parse_header() {
        // Field name
        if (!this->check(TK::IDENTIFIER)) {
            this->error("Expected header field name");
        }
        auto field_tok = this->advance();

        // Colon
        if (!this->match(TK::COLON)) {
            this->error("Expected ':' after header field name");
        }

        // Value (may be empty)
        std::string_view value = "";
        if (this->check(TK::STRING)) {
            value = this->advance().text;
        }

        // Newline
        if (!this->match(TK::NEWLINE)) {
            this->error("Expected newline after header value");
        }

        return this->template create_node<Header>(field_tok.text, value);
    }

    /// Override token_name for better error messages
    [[nodiscard]] std::string token_name(TK type) const override {
        return std::string(mime_token_type_name(type));
    }

protected:
    // ========================================================================
    // Lifetime-Safe Tokenization Helper
    // ========================================================================

    struct TokenizeResult {
        std::vector<TokenType> tokens;
        std::string_view source;
    };

    /// Delegating constructor that receives pre-tokenized result
    MimeParser(libglot::Arena& arena, TokenizeResult&& result)
        : source_(result.source)
        , Base(arena, std::move(result.tokens))
    {}

    /// Copy source into arena and tokenize the arena-owned copy
    /// This ensures all token string_views point to arena memory
    static TokenizeResult tokenize_and_copy(libglot::Arena& arena, std::string_view source) {
        auto arena_source = arena.copy_source(source);
        auto tokens = tokenize(arena_source);
        return {std::move(tokens), arena_source};
    }

    // ========================================================================
    // Tokenization
    // ========================================================================

    static std::vector<TokenType> tokenize(std::string_view source) {
        MimeTokenizer tokenizer(source);
        auto mime_tokens = tokenizer.tokenize_all();

        // Convert MimeToken to libglot::Token<TokenKind>
        std::vector<TokenType> result;
        result.reserve(mime_tokens.size());

        for (const auto& tok : mime_tokens) {
            result.push_back(TokenType{
                tok.type,
                static_cast<uint32_t>(tok.start),
                static_cast<uint32_t>(tok.end),
                static_cast<uint16_t>(tok.line),
                static_cast<uint16_t>(tok.col),
                tok.text
            });
        }

        return result;
    }

    std::string_view source_;
};

} // namespace libglot::mime
