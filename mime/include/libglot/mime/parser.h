#pragma once

#include "ast_nodes.h"
#include "grammar.h"
#include "header_folding.h"
#include "tokens.h"
#include <libglot/parse/parser.h>

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
        : MimeParser(arena, tokenize_and_copy(arena, source)) {}

    // ========================================================================
    // Top-Level Parsing Entry Point (Required by Base)
    // ========================================================================

    Message* parse_top_level() { return parse_message(); }

    // ========================================================================
    // CRTP Customization Points (Required by ParserBase)
    // ========================================================================

    /// Parse prefix expression (not used for MIME)
    [[nodiscard]] MimeNode* parse_prefix() {
        this->error("Unexpected token in MIME header");
        return nullptr;
    }

    /// Parse postfix expression (not used for MIME)
    [[nodiscard]] MimeNode* parse_postfix(MimeNode* base) { return base; }

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

    /// Shadow token_name for better error messages (CRTP customization point)
    [[nodiscard]] std::string token_name(TK type) const {
        return std::string(mime_token_type_name(type));
    }

protected:
    // ========================================================================
    // Lifetime-Safe Tokenization Helper
    // ========================================================================

    struct TokenizeResult {
        std::vector<TokenType> tokens;
        std::string_view source;
        // See MimeParser::pending_whitespace_only_fold_: set during
        // unfolding, consumed once by MimeParserExtended's constructor
        // body (the earliest point where record_anomaly is callable --
        // this base class constructs before any derived-class anomaly
        // machinery exists).
        bool whitespace_only_fold_seen = false;
    };

    /// Delegating constructor that receives pre-tokenized result.
    /// (Base is listed first to match actual initialization order; moving
    /// the token vector does not touch result.source.)
    MimeParser(libglot::Arena& arena, TokenizeResult&& result)
        : Base(arena, std::move(result.tokens)),
          pending_whitespace_only_fold_(result.whitespace_only_fold_seen),
          source_(result.source) {}

    /// Copy source into arena and tokenize the arena-owned copy
    /// This ensures all token string_views point to arena memory.
    /// Folded (continuation) header lines are unfolded first (RFC 5322
    /// §2.2.3) so each header occupies exactly one line; the body bytes
    /// are left untouched.
    static TokenizeResult tokenize_and_copy(libglot::Arena& arena, std::string_view source) {
        bool whitespace_only_fold_seen = false;
        auto arena_source =
            arena.copy_source(HeaderFolding::unfold_headers(source, &whitespace_only_fold_seen));
        auto tokens = tokenize(arena_source);
        return {std::move(tokens), arena_source, whitespace_only_fold_seen};
    }

    /// Set by tokenize_and_copy when unfolding the top-level message finds
    /// a whitespace-only fold continuation line; consumed exactly once by
    /// MimeParserExtended's constructor body via record_anomaly (see that
    /// class). Not touched for multipart parts -- parse_part calls
    /// unfold_headers directly and records the anomaly immediately, since
    /// record_anomaly is already available there.
    bool pending_whitespace_only_fold_ = false;

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
                tok.type, static_cast<uint32_t>(tok.start), static_cast<uint32_t>(tok.end),
                static_cast<uint16_t>(tok.line), static_cast<uint16_t>(tok.col), tok.text});
        }

        return result;
    }

    std::string_view source_;
};

} // namespace libglot::mime
