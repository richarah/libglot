// Direct tests for the vendored SQL tokenizer (libglot::sql::lex::Tokenizer).
//
// Covers keyword vs identifier classification, quoted identifiers, string
// literals (doubled '' and backslash escapes), number formats, dollar-quoted
// strings, comments, multi-character operators, parameter syntax, and the
// per-dialect TokenizerConfig variants (sqlserver / postgresql / snowflake /
// default).
//
// Unterminated literals and embedded NUL bytes are lexical errors (ERROR
// tokens): both let the generator re-emit SQL that would not re-lex.

#include <catch2/catch_test_macros.hpp>
#include <libglot/sql/lex/tokenizer.h>

#include <string>
#include <vector>

using namespace libglot::sql::lex;

namespace {

// Interned token text lives in the pool, so the pool must outlive the
// returned tokens. A function-local static pool keeps every token text
// valid for the whole test run.
std::vector<Token> lex(std::string_view src, TokenizerConfig cfg = {}) {
    static LocalStringPool pool;
    Tokenizer tokenizer(src, &pool, cfg);
    return tokenizer.tokenize_all();
}

std::string text_of(const Token& tok) {
    return tok.text ? std::string(tok.text) : std::string();
}

} // namespace

// ============================================================================
// Keywords vs identifiers
// ============================================================================

TEST_CASE("Tokenizer - keywords are recognized case-insensitively", "[tokenizer][keywords]") {
    auto toks = lex("select From WHERE");

    REQUIRE(toks.size() == 4); // 3 tokens + EOF
    REQUIRE(toks[0].type == TokenType::SELECT);
    REQUIRE(text_of(toks[0]) == "select"); // original spelling preserved
    REQUIRE(toks[1].type == TokenType::FROM);
    REQUIRE(text_of(toks[1]) == "From");
    REQUIRE(toks[2].type == TokenType::WHERE);
    REQUIRE(text_of(toks[2]) == "WHERE");
    REQUIRE(toks[3].type == TokenType::EOF_TOKEN);
}

TEST_CASE("Tokenizer - near-keywords and plain names are identifiers", "[tokenizer][keywords]") {
    auto toks = lex("selects _id abc123 a$b");

    REQUIRE(toks.size() == 5);
    REQUIRE(toks[0].type == TokenType::IDENTIFIER);
    REQUIRE(text_of(toks[0]) == "selects");
    REQUIRE(toks[1].type == TokenType::IDENTIFIER);
    REQUIRE(text_of(toks[1]) == "_id");
    REQUIRE(toks[2].type == TokenType::IDENTIFIER);
    REQUIRE(text_of(toks[2]) == "abc123");
    // '$' is a valid identifier-continue character
    REQUIRE(toks[3].type == TokenType::IDENTIFIER);
    REQUIRE(text_of(toks[3]) == "a$b");
}

// ============================================================================
// Quoted identifiers
// ============================================================================

TEST_CASE("Tokenizer - quoted identifiers strip their quotes", "[tokenizer][identifiers]") {
    auto toks = lex("\"my col\" `tick` [brack]");

    REQUIRE(toks.size() == 4);
    REQUIRE(toks[0].type == TokenType::IDENTIFIER);
    REQUIRE(text_of(toks[0]) == "my col");
    REQUIRE(toks[1].type == TokenType::IDENTIFIER);
    REQUIRE(text_of(toks[1]) == "tick");
    REQUIRE(toks[2].type == TokenType::IDENTIFIER);
    REQUIRE(text_of(toks[2]) == "brack");
}

TEST_CASE("Tokenizer - quoted identifier can contain keywords and symbols",
          "[tokenizer][identifiers]") {
    auto toks = lex("\"select * from\"");

    REQUIRE(toks.size() == 2);
    REQUIRE(toks[0].type == TokenType::IDENTIFIER);
    REQUIRE(text_of(toks[0]) == "select * from");
}

TEST_CASE("Tokenizer - unterminated quoted identifier is a lexical error",
          "[tokenizer][identifiers]") {
    // Previously this yielded an IDENTIFIER token, which let the generator
    // re-emit malformed SQL that would not re-lex (fuzz_sql_roundtrip).
    auto toks = lex("\"unterminated");

    REQUIRE(toks.size() == 2);
    REQUIRE(toks[0].type == TokenType::ERROR);
    REQUIRE(toks[1].type == TokenType::EOF_TOKEN);
}

// ============================================================================
// String literals
// ============================================================================

TEST_CASE("Tokenizer - string literals keep quotes and doubled-quote escapes",
          "[tokenizer][strings]") {
    auto toks = lex("'hello' 'it''s'");

    REQUIRE(toks.size() == 3);
    REQUIRE(toks[0].type == TokenType::STRING);
    REQUIRE(text_of(toks[0]) == "'hello'");
    // Doubled quote stays inside a single token
    REQUIRE(toks[1].type == TokenType::STRING);
    REQUIRE(text_of(toks[1]) == "'it''s'");
}

TEST_CASE("Tokenizer - backslash escape does not end a string", "[tokenizer][strings]") {
    auto toks = lex("'back\\'slash'");

    REQUIRE(toks.size() == 2);
    REQUIRE(toks[0].type == TokenType::STRING);
    REQUIRE(text_of(toks[0]) == "'back\\'slash'");
}

TEST_CASE("Tokenizer - unterminated string is a lexical error", "[tokenizer][strings]") {
    // Previously this yielded a STRING token spanning to EOF; the generator
    // then re-emitted the unbalanced literal and the result did not re-lex
    // (fuzz_sql_roundtrip). An unterminated literal is now an ERROR token.
    auto toks = lex("'unterminated");

    REQUIRE(toks.size() == 2);
    REQUIRE(toks[0].type == TokenType::ERROR);
    REQUIRE(toks[1].type == TokenType::EOF_TOKEN);
}

TEST_CASE("Tokenizer - embedded NUL is a lexical error", "[tokenizer][strings]") {
    // A NUL byte aliases the out-of-bounds sentinel and truncates interned
    // token text, so it cannot be carried through a literal safely.
    using namespace std::string_view_literals;
    auto toks = lex("'has\0nul'"sv);

    REQUIRE(toks.size() == 2);
    REQUIRE(toks[0].type == TokenType::ERROR);
    REQUIRE(toks[1].type == TokenType::EOF_TOKEN);
}

// ============================================================================
// Numbers
// ============================================================================

TEST_CASE("Tokenizer - number formats", "[tokenizer][numbers]") {
    auto toks = lex("123 45.67 0x1F 0b1010 1.5e10 2E-3");

    REQUIRE(toks.size() == 7);
    for (size_t i = 0; i < 6; ++i) {
        REQUIRE(toks[i].type == TokenType::NUMBER);
    }
    REQUIRE(text_of(toks[0]) == "123");
    REQUIRE(text_of(toks[1]) == "45.67");
    REQUIRE(text_of(toks[2]) == "0x1F");
    REQUIRE(text_of(toks[3]) == "0b1010");
    REQUIRE(text_of(toks[4]) == "1.5e10");
    REQUIRE(text_of(toks[5]) == "2E-3");
}

TEST_CASE("Tokenizer - range 1..5 lexes as NUMBER DOUBLE_DOT NUMBER", "[tokenizer][numbers]") {
    auto toks = lex("1..5");

    REQUIRE(toks.size() == 4);
    REQUIRE(toks[0].type == TokenType::NUMBER);
    REQUIRE(text_of(toks[0]) == "1");
    REQUIRE(toks[1].type == TokenType::DOUBLE_DOT);
    REQUIRE(toks[2].type == TokenType::NUMBER);
    REQUIRE(text_of(toks[2]) == "5");
}

// ============================================================================
// Dollar-quoted strings (PostgreSQL style)
// ============================================================================

TEST_CASE("Tokenizer - dollar-quoted strings", "[tokenizer][dollar]") {
    auto toks = lex("$$body$$ $tag$x$tag$");

    REQUIRE(toks.size() == 3);
    REQUIRE(toks[0].type == TokenType::STRING);
    REQUIRE(text_of(toks[0]) == "$$body$$");
    REQUIRE(toks[1].type == TokenType::STRING);
    REQUIRE(text_of(toks[1]) == "$tag$x$tag$");
}

TEST_CASE("Tokenizer - dollar quote with embedded quotes and newlines", "[tokenizer][dollar]") {
    auto toks = lex("$fn$it's a 'quote'\nline2$fn$");

    REQUIRE(toks.size() == 2);
    REQUIRE(toks[0].type == TokenType::STRING);
    REQUIRE(text_of(toks[0]) == "$fn$it's a 'quote'\nline2$fn$");
}

TEST_CASE("Tokenizer - unterminated dollar quote consumes to EOF as STRING",
          "[tokenizer][dollar]") {
    auto toks = lex("$tag$unterminated");

    REQUIRE(toks.size() == 2);
    REQUIRE(toks[0].type == TokenType::STRING);
    REQUIRE(text_of(toks[0]) == "$tag$unterminated");
    REQUIRE(toks[1].type == TokenType::EOF_TOKEN);
}

// ============================================================================
// Comments
// ============================================================================

TEST_CASE("Tokenizer - line and block comments are skipped", "[tokenizer][comments]") {
    auto line = lex("-- a comment\nSELECT");
    REQUIRE(line.size() == 2);
    REQUIRE(line[0].type == TokenType::SELECT);

    auto block = lex("/* block\ncomment */ 1");
    REQUIRE(block.size() == 2);
    REQUIRE(block[0].type == TokenType::NUMBER);
    REQUIRE(text_of(block[0]) == "1");

    auto only = lex("/* nothing else */");
    REQUIRE(only.size() == 1);
    REQUIRE(only[0].type == TokenType::EOF_TOKEN);
}

TEST_CASE("Tokenizer - default config treats hash as a line comment",
          "[tokenizer][comments][config]") {
    auto toks = lex("# comment line\n5");

    REQUIRE(toks.size() == 2);
    REQUIRE(toks[0].type == TokenType::NUMBER);
    REQUIRE(text_of(toks[0]) == "5");
}

// ============================================================================
// Operators
// ============================================================================

TEST_CASE("Tokenizer - multi-character operators", "[tokenizer][operators]") {
    // Note: '#'-operators need the postgresql config; with the default
    // config '#' would start a line comment (covered separately below).
    auto toks = lex("<= <> != >= || :: := -> ->> @> <@ <=> ..");

    REQUIRE(toks.size() == 14);
    REQUIRE(toks[0].type == TokenType::LTE);
    REQUIRE(toks[1].type == TokenType::NEQ); // <>
    REQUIRE(toks[2].type == TokenType::NEQ); // != maps to the same NEQ
    REQUIRE(toks[3].type == TokenType::GTE);
    REQUIRE(toks[4].type == TokenType::CONCAT);
    REQUIRE(toks[5].type == TokenType::DOUBLE_COLON);
    REQUIRE(toks[6].type == TokenType::COLON_EQUALS);
    REQUIRE(toks[7].type == TokenType::ARROW);
    REQUIRE(toks[8].type == TokenType::LONG_ARROW);
    REQUIRE(toks[9].type == TokenType::AT_GT);
    REQUIRE(toks[10].type == TokenType::LT_AT);
    REQUIRE(toks[11].type == TokenType::NULL_SAFE_EQ); // <=>
    REQUIRE(toks[12].type == TokenType::DOUBLE_DOT);
    REQUIRE(toks[13].type == TokenType::EOF_TOKEN);
}

TEST_CASE("Tokenizer - single-character operators and delimiters", "[tokenizer][operators]") {
    auto toks = lex("+ - * / % = < > ( ) , ; .");

    REQUIRE(toks.size() == 14);
    REQUIRE(toks[0].type == TokenType::PLUS);
    REQUIRE(toks[1].type == TokenType::MINUS);
    REQUIRE(toks[2].type == TokenType::STAR);
    REQUIRE(toks[3].type == TokenType::SLASH);
    REQUIRE(toks[4].type == TokenType::PERCENT);
    REQUIRE(toks[5].type == TokenType::EQ);
    REQUIRE(toks[6].type == TokenType::LT);
    REQUIRE(toks[7].type == TokenType::GT);
    REQUIRE(toks[8].type == TokenType::LPAREN);
    REQUIRE(toks[9].type == TokenType::RPAREN);
    REQUIRE(toks[10].type == TokenType::COMMA);
    REQUIRE(toks[11].type == TokenType::SEMICOLON);
    REQUIRE(toks[12].type == TokenType::DOT);
}

// ============================================================================
// Parameters
// ============================================================================

TEST_CASE("Tokenizer - parameter syntaxes", "[tokenizer][parameters]") {
    auto toks = lex("@name :name $1 ?");

    REQUIRE(toks.size() == 5);
    REQUIRE(toks[0].type == TokenType::PARAMETER);
    REQUIRE(text_of(toks[0]) == "@name");
    REQUIRE(toks[1].type == TokenType::PARAMETER);
    REQUIRE(text_of(toks[1]) == ":name");
    REQUIRE(toks[2].type == TokenType::PARAMETER);
    REQUIRE(text_of(toks[2]) == "$1");
    REQUIRE(toks[3].type == TokenType::PARAMETER);
    REQUIRE(text_of(toks[3]) == "?");
}

TEST_CASE("Tokenizer - colon-equals and double-colon are operators not parameters",
          "[tokenizer][parameters]") {
    auto toks = lex("x := 1 :: y");

    REQUIRE(toks.size() == 6);
    REQUIRE(toks[0].type == TokenType::IDENTIFIER);
    REQUIRE(toks[1].type == TokenType::COLON_EQUALS);
    REQUIRE(toks[2].type == TokenType::NUMBER);
    REQUIRE(toks[3].type == TokenType::DOUBLE_COLON);
    REQUIRE(toks[4].type == TokenType::IDENTIFIER);
}

// ============================================================================
// TokenizerConfig: sqlserver()
// ============================================================================

TEST_CASE("Tokenizer - sqlserver config lexes temp table names as identifiers",
          "[tokenizer][config][sqlserver]") {
    auto toks = lex("#temp ##global", TokenizerConfig::sqlserver());

    REQUIRE(toks.size() == 3);
    REQUIRE(toks[0].type == TokenType::IDENTIFIER);
    REQUIRE(text_of(toks[0]) == "#temp");
    REQUIRE(toks[1].type == TokenType::IDENTIFIER);
    REQUIRE(text_of(toks[1]) == "##global");
}

TEST_CASE("Tokenizer - sqlserver config does not treat hash as a comment",
          "[tokenizer][config][sqlserver]") {
    // With the default config everything after '#' would be skipped.
    auto def = lex("#t 5");
    REQUIRE(def.size() == 1);
    REQUIRE(def[0].type == TokenType::EOF_TOKEN);

    auto mssql = lex("#t 5", TokenizerConfig::sqlserver());
    REQUIRE(mssql.size() == 3);
    REQUIRE(mssql[0].type == TokenType::IDENTIFIER);
    REQUIRE(text_of(mssql[0]) == "#t");
    REQUIRE(mssql[1].type == TokenType::NUMBER);
    REQUIRE(text_of(mssql[1]) == "5");
}

// ============================================================================
// TokenizerConfig: postgresql()
// ============================================================================

TEST_CASE("Tokenizer - postgresql config lexes hash arrows and hash as operators",
          "[tokenizer][config][postgresql]") {
    auto toks = lex("#> #>> #", TokenizerConfig::postgresql());

    REQUIRE(toks.size() == 4);
    REQUIRE(toks[0].type == TokenType::HASH_ARROW);
    REQUIRE(toks[1].type == TokenType::HASH_LONG_ARROW);
    REQUIRE(toks[2].type == TokenType::HASH);
    REQUIRE(toks[3].type == TokenType::EOF_TOKEN);
}

TEST_CASE("Tokenizer - postgresql config lexes question mark as QUESTION operator",
          "[tokenizer][config][postgresql]") {
    auto pg = lex("?", TokenizerConfig::postgresql());
    REQUIRE(pg.size() == 2);
    REQUIRE(pg[0].type == TokenType::QUESTION);

    // Default config: '?' is a positional parameter instead
    auto def = lex("?");
    REQUIRE(def.size() == 2);
    REQUIRE(def[0].type == TokenType::PARAMETER);
}

// ============================================================================
// TokenizerConfig: snowflake()
// ============================================================================

TEST_CASE("Tokenizer - snowflake config lexes colon as COLON path operator",
          "[tokenizer][config][snowflake]") {
    auto toks = lex("col:field", TokenizerConfig::snowflake());

    REQUIRE(toks.size() == 4);
    REQUIRE(toks[0].type == TokenType::IDENTIFIER);
    REQUIRE(text_of(toks[0]) == "col");
    REQUIRE(toks[1].type == TokenType::COLON);
    REQUIRE(toks[2].type == TokenType::IDENTIFIER);
    REQUIRE(text_of(toks[2]) == "field");

    // Default config lexes the same input as a ':field' host parameter
    auto def = lex("col:field");
    REQUIRE(def.size() == 3);
    REQUIRE(def[0].type == TokenType::IDENTIFIER);
    REQUIRE(def[1].type == TokenType::PARAMETER);
    REQUIRE(text_of(def[1]) == ":field");
}

TEST_CASE("Tokenizer - snowflake config lexes bracket as LBRACKET subscript",
          "[tokenizer][config][snowflake]") {
    auto snow = lex("[0]", TokenizerConfig::snowflake());
    REQUIRE(snow.size() == 4);
    REQUIRE(snow[0].type == TokenType::LBRACKET);
    REQUIRE(snow[1].type == TokenType::NUMBER);
    REQUIRE(text_of(snow[1]) == "0");
    REQUIRE(snow[2].type == TokenType::RBRACKET);

    // Default config: '[0]' is a bracket-quoted identifier
    auto def = lex("[0]");
    REQUIRE(def.size() == 2);
    REQUIRE(def[0].type == TokenType::IDENTIFIER);
    REQUIRE(text_of(def[0]) == "0");
}

// ============================================================================
// Token positions
// ============================================================================

TEST_CASE("Tokenizer - line and column tracking", "[tokenizer][positions]") {
    auto toks = lex("SELECT\n  id");

    REQUIRE(toks.size() == 3);
    REQUIRE(toks[0].line == 1);
    REQUIRE(toks[0].col == 1);
    REQUIRE(toks[1].type == TokenType::IDENTIFIER);
    REQUIRE(toks[1].line == 2);
    REQUIRE(toks[1].col == 3);
}

TEST_CASE("Tokenizer - start/end offsets slice the source exactly", "[tokenizer][positions]") {
    std::string src = "SELECT abc";
    auto toks = lex(src);

    REQUIRE(toks.size() == 3);
    REQUIRE(toks[0].view(src) == "SELECT");
    REQUIRE(toks[1].view(src) == "abc");
    REQUIRE(toks[1].start == 7);
    REQUIRE(toks[1].end == 10);
}

TEST_CASE("Tokenizer - quoted identifier with doubled quotes unescapes",
          "[tokenizer][identifiers]") {
    auto toks = lex("\"emb\"\"edded\"");

    REQUIRE(toks.size() == 2);
    REQUIRE(toks[0].type == TokenType::IDENTIFIER);
    REQUIRE(text_of(toks[0]) == "emb\"edded");
    REQUIRE(toks[1].type == TokenType::EOF_TOKEN);
}
