#pragma once

#include "ast_nodes.h"
#include "dialect_traits.h"
#include "grammar.h"
#include "lex/tokenizer.h"
#include <algorithm>
#include <iostream>
#include <libglot/parse/parser.h>
#include <memory>

namespace libglot::sql {

/// ============================================================================
/// SQL Parser - CRTP Instantiation of ParserBase
/// ============================================================================
///
/// Implements SQL-specific parsing rules over the generic ParserBase template.
/// For Phase C1, supports parsing:
///   SELECT col AS alias FROM table WHERE col = 1 ORDER BY col LIMIT 10
///
/// Uses libsqlglot's existing tokenizer for the lexical analysis phase.
/// ============================================================================

class SQLParser : public libglot::ParserBase<SQLGrammarSpec, SQLParser> {
public:
    using Base = libglot::ParserBase<SQLGrammarSpec, SQLParser>;
    using TokenType = Base::TokenType;
    using TK = libglot::sql::lex::TokenType;

    // Precedence anchors (must stay in sync with the table in grammar.h):
    // boolean NOT sits between AND (9) and IS (11); arithmetic unary +/-
    // binds above the highest binary level (15, CARET); BETWEEN/IN bounds
    // parse above the comparison level (12) so AND/comparisons are not
    // consumed.
    static constexpr int kNotPrecedence = 10;
    static constexpr int kUnaryArithmeticPrecedence = 16;
    static constexpr int kComparisonOperandPrecedence = 13;

    // ========================================================================
    // Construction
    // ========================================================================

    explicit SQLParser(libglot::Arena& arena, std::string_view source,
                       SQLDialect dialect = SQLDialect::PostgreSQL)
        : SQLParser(arena, tokenize_and_copy(arena, source, dialect), dialect) {}

    // ========================================================================
    // Top-Level Parsing Entry Point (Required by Base)
    // ========================================================================

    SQLNode* parse_top_level() {
        SQLNode* stmt = parse_statement();

        // A statement may be terminated by (possibly repeated) semicolons.
        while (match(TK::SEMICOLON)) {
        }

        // Anything else left over would previously be dropped on the floor
        // (SELECT 2 ^ 3 silently became SELECT 2; SELECT ... FOR UPDATE lost
        // its locking clause). Turn silent drops into a clean parse error.
        if (!is_eof()) {
            error("Unexpected trailing input after statement");
        }

        return stmt;
    }

    /// Parse a single statement without end-of-input enforcement.
    /// Used recursively for statement bodies (BEGIN..END, IF, loops, ...)
    /// and callable repeatedly to consume a multi-statement script.
    SQLNode* parse_statement() {
        // Skip statement separators left over from a previous statement
        while (match(TK::SEMICOLON)) {
        }

        // Dispatch to appropriate statement parser
        if (check(TK::WITH) || check(TK::SELECT)) {
            return parse_select();
        } else if (check(TK::INSERT)) {
            return parse_insert();
        } else if (check(TK::UPDATE)) {
            return parse_update();
        } else if (check(TK::DELETE)) {
            return parse_delete();
        } else if (check(TK::MERGE)) {
            return parse_merge();
        } else if (check(TK::CREATE)) {
            return parse_create_statement();
        } else if (check(TK::DROP)) {
            return parse_drop_statement();
        } else if (check(TK::ALTER)) {
            return parse_alter_statement();
        } else if (check(TK::TRUNCATE)) {
            return parse_truncate();
        } else if (check(TK::BEGIN)) {
            return parse_begin();
        } else if (check(TK::COMMIT)) {
            return parse_commit();
        } else if (check(TK::ROLLBACK)) {
            return parse_rollback();
        } else if (check(TK::SAVEPOINT)) {
            return parse_savepoint();
        } else if (check(TK::SET)) {
            return parse_set();
        } else if (check(TK::SHOW)) {
            return parse_show();
        } else if (check(TK::DESCRIBE) || check(TK::DESC)) {
            return parse_describe();
        } else if (check(TK::EXPLAIN)) {
            return parse_explain();
        } else if (check(TK::ANALYZE)) {
            return parse_analyze();
        } else if (check(TK::VACUUM)) {
            return parse_vacuum();
        } else if (check(TK::GRANT)) {
            return parse_grant();
        } else if (check(TK::REVOKE)) {
            return parse_revoke();
        } else if (check(TK::CALL)) {
            return parse_call();
        } else if (check(TK::DECLARE)) {
            return parse_declare();
        } else if (check(TK::IF_KW)) {
            return parse_if();
        } else if (check(TK::WHILE)) {
            return parse_while_loop();
        } else if (check(TK::FOR)) {
            return parse_for_loop();
        } else if (check(TK::LOOP)) {
            return parse_loop();
        } else if (check(TK::RAISE)) {
            return parse_raise();
        } else if (check(TK::SIGNAL)) {
            return parse_raise();
        } else if (check(TK::IDENTIFIER) &&
                   (current().text == "RAISERROR" || current().text == "raiserror") &&
                   peek(1).type == TK::LPAREN) {
            return parse_raiserror();
        } else if (check(TK::OPEN)) {
            return parse_open_cursor();
        } else if (check(TK::FETCH)) {
            return parse_fetch_cursor();
        } else if (check(TK::CLOSE)) {
            return parse_close_cursor();
        } else if (check(TK::RETURN_KW)) {
            return parse_return();
        } else if (check(TK::BREAK)) {
            return parse_break();
        } else if (check(TK::EXIT)) {
            return parse_exit();
        } else if (check(TK::CONTINUE)) {
            return parse_continue();
        } else if (check(TK::IDENTIFIER) && peek(1).type == TK::COLON_EQUALS) {
            return parse_assignment();
        } else if (check(TK::DELIMITER_KW)) {
            return parse_delimiter();
        } else if (check(TK::DO)) {
            return parse_do();
        } else if (check(TK::UPSERT)) {
            return parse_upsert();
        } else if (check(TK::TAIL)) {
            return parse_tail();
        } else if (check(TK::OPTIMIZE)) {
            return parse_optimize();
        } else if (check(TK::COMPUTE)) {
            return parse_compute_stats();
        } else if (check(TK::IDENTIFIER) &&
                   (current().text == "CACHE" || current().text == "cache")) {
            return parse_cache_table();
        }

        error("Expected SQL statement (SELECT, INSERT, UPDATE, DELETE, CREATE, DROP, ALTER, etc.)");
        return nullptr;
    }

    // ========================================================================
    // CRTP Customization Points (Required by ParserBase)
    // ========================================================================

    /// Parse prefix expression (atomic terms and unary operators)
    [[nodiscard]] SQLNode* parse_prefix() {
        // CASE expression
        if (match(TK::CASE)) {
            return parse_case_expression();
        }

        // EXISTS (subquery)
        if (match(TK::EXISTS)) {
            expect(TK::LPAREN);
            auto subquery = parse_select();
            expect(TK::RPAREN);
            return this->template create_node<ExistsExpr>(subquery);
        }

        // ANY (subquery or expression)
        // Note: ANY typically appears in binary comparisons like "x = ANY(subquery)"
        // This parses standalone ANY() which is less common but valid
        if (match(TK::ANY)) {
            expect(TK::LPAREN);
            SQLNode* subquery = nullptr;
            if (check(TK::SELECT)) {
                subquery = parse_select();
            } else if (!check(TK::RPAREN)) {
                subquery = parse_expression();
            }
            expect(TK::RPAREN);

            // Create AnyExpr with null left (will be filled by binary expression parser)
            // For standalone ANY, use FunctionCall fallback
            return this->template create_node<FunctionCall>("ANY", std::vector<SQLNode*>{subquery});
        }

        // ALL (subquery or expression)
        if (match(TK::ALL)) {
            expect(TK::LPAREN);
            std::vector<SQLNode*> args;
            if (check(TK::SELECT)) {
                args.push_back(parse_select());
            } else if (!check(TK::RPAREN)) {
                args.push_back(parse_expression());
            }
            expect(TK::RPAREN);
            return this->template create_node<FunctionCall>("ALL", args);
        }

        // Array literal: ARRAY[1, 2, 3]
        // Note: The tokenizer may lex [elements] as a single quoted identifier token in SQL Server
        // mode
        if (match(TK::ARRAY)) {
            // Check if we have a bracket-quoted identifier (SQL Server style) vs separate bracket
            // tokens. Token text is quote-stripped, so inspect the raw source at the token start.
            if (check(TK::IDENTIFIER) && current().start < source_.size() &&
                source_[current().start] == '[') {
                // Tokenizer lexed [node_id] as a single identifier - need to parse the interior
                // This is a limitation of the generic tokenizer
                // For now, create a simple array with the unquoted identifier
                std::string_view interior = current().text; // Already stripped of brackets
                (void)advance();
                // Parse the interior as a simple identifier
                auto elem = this->template create_node<Column>(interior);
                return this->template create_node<ArrayLiteral>(std::vector<SQLNode*>{elem});
            }

            // Normal array literal with separate bracket tokens
            expect(TK::LBRACKET);
            std::vector<SQLNode*> elements;
            if (!check(TK::RBRACKET)) {
                do {
                    elements.push_back(parse_expression());
                } while (match(TK::COMMA));
            }
            expect(TK::RBRACKET);
            return this->template create_node<ArrayLiteral>(elements);
        }

        // Bracket array literal: [1, 2, 3] (BigQuery, DuckDB)
        if (match(TK::LBRACKET)) {
            std::vector<SQLNode*> elements;
            if (!check(TK::RBRACKET)) {
                do {
                    elements.push_back(parse_expression());
                } while (match(TK::COMMA));
            }
            expect(TK::RBRACKET);
            return this->template create_node<ArrayLiteral>(elements);
        }

        // Parenthesized expression or subquery
        if (match(TK::LPAREN)) {
            // Check if it's a subquery
            if (check(TK::SELECT) || check(TK::WITH)) {
                auto subquery = parse_select();
                expect(TK::RPAREN);
                return this->template create_node<SubqueryExpr>(subquery);
            }
            auto expr = parse_expression();
            expect(TK::RPAREN);
            return expr;
        }

        // NULL, TRUE, FALSE
        if (match(TK::NULL_KW)) {
            return this->template create_node<Literal>("NULL");
        }

        if (match(TK::TRUE)) {
            return this->template create_node<Literal>("TRUE");
        }

        if (match(TK::FALSE)) {
            return this->template create_node<Literal>("FALSE");
        }

        // Current datetime functions (no parens)
        if (match(TK::CURRENT_TIMESTAMP)) {
            return this->template create_node<Literal>("CURRENT_TIMESTAMP");
        }

        if (match(TK::CURRENT_DATE)) {
            return this->template create_node<Literal>("CURRENT_DATE");
        }

        if (match(TK::CURRENT_TIME)) {
            return this->template create_node<Literal>("CURRENT_TIME");
        }

        // Literals
        if (check(TK::NUMBER)) {
            auto tok = advance();
            return this->template create_node<Literal>(tok.text);
        }

        if (check(TK::STRING)) {
            auto tok = advance();
            return this->template create_node<Literal>(tok.text);
        }

        // Parameter (@name, :name, $1, ?)
        if (check(TK::PARAMETER)) {
            auto tok = advance();
            return this->template create_node<Parameter>(tok.text);
        }

        // Special keyword functions
        if (match(TK::COALESCE)) {
            expect(TK::LPAREN);
            std::vector<SQLNode*> args;
            do {
                args.push_back(parse_expression());
            } while (match(TK::COMMA));
            expect(TK::RPAREN);
            return this->template create_node<CoalesceExpr>(args);
        }

        if (match(TK::NULLIF)) {
            expect(TK::LPAREN);
            auto arg1 = parse_expression();
            expect(TK::COMMA);
            auto arg2 = parse_expression();
            expect(TK::RPAREN);
            return this->template create_node<NullifExpr>(arg1, arg2);
        }

        // Typed literals: DATE 'string', TIMESTAMP 'string', TIME 'string'
        // Check if it's a typed literal pattern before treating as function
        if (match(TK::DATE)) {
            if (check(TK::STRING)) {
                auto lit = advance();
                auto lit_node = this->template create_node<Literal>(lit.text);
                return this->template create_node<CastExpr>(lit_node, "DATE");
            }
            // Not followed by string, backtrack by creating identifier for DATE
            return this->template create_node<Column>("DATE");
        }

        if (match(TK::TIMESTAMP)) {
            if (check(TK::STRING)) {
                auto lit = advance();
                auto lit_node = this->template create_node<Literal>(lit.text);
                return this->template create_node<CastExpr>(lit_node, "TIMESTAMP");
            }
            return this->template create_node<Column>("TIMESTAMP");
        }

        if (match(TK::TIME)) {
            if (check(TK::STRING)) {
                auto lit = advance();
                auto lit_node = this->template create_node<Literal>(lit.text);
                return this->template create_node<CastExpr>(lit_node, "TIME");
            }
            return this->template create_node<Column>("TIME");
        }

        // INTERVAL literals: INTERVAL '1 day' (bare form, unit embedded in
        // the string) or INTERVAL '2' HOUR / INTERVAL 7 DAY (value + a
        // trailing unit keyword). The unit is not a reserved word, so it
        // lexes as a plain identifier.
        if (match(TK::INTERVAL)) {
            if (!check(TK::STRING) && !check(TK::NUMBER)) {
                error("Expected a string or number literal after INTERVAL");
            }
            auto value_tok = advance();
            std::string_view unit = "";
            if (check(TK::IDENTIFIER)) {
                unit = advance().text;
            }
            return this->template create_node<IntervalLiteral>(value_tok.text, unit);
        }

        // MySQL upsert pseudo-function: VALUES(col) inside
        // ON DUPLICATE KEY UPDATE, referring to the row that would have
        // been inserted. VALUES is a reserved keyword everywhere else
        // (INSERT ... VALUES (...)), but that form is parsed directly by
        // parse_insert without going through expression parsing, so this
        // is unambiguous.
        if (check(TK::VALUES) && peek(1).type == TK::LPAREN) {
            (void)advance(); // VALUES
            (void)advance(); // (
            return parse_function_call("VALUES");
        }

        if (match(TK::CAST)) {
            expect(TK::LPAREN);
            auto expr = parse_expression();
            expect(TK::AS);
            std::string_view type_str = parse_cast_type_name();
            expect(TK::RPAREN);
            return this->template create_node<CastExpr>(expr, type_str);
        }

        if (check(TK::SAFE_CAST)) {
            (void)advance(); // consume SAFE_CAST
            expect(TK::LPAREN);
            auto expr = parse_expression();
            expect(TK::AS);
            std::string_view type_str = parse_cast_type_name();
            expect(TK::RPAREN);
            return this->template create_node<CastExpr>(expr, type_str);
        }

        if (check(TK::STRUCT_KW)) {
            (void)advance(); // consume STRUCT
            expect(TK::LPAREN);
            std::vector<SQLNode*> fields;
            if (!check(TK::RPAREN)) {
                do {
                    auto expr = parse_expression();
                    // STRUCT fields can have aliases: STRUCT(1 AS x, 'hello' AS y)
                    if (match(TK::AS)) {
                        if (check(TK::IDENTIFIER)) {
                            // Store as Alias so the alias is preserved
                            std::string_view alias = advance().text;
                            fields.push_back(this->template create_node<Alias>(expr, alias));
                        } else {
                            fields.push_back(expr);
                        }
                    } else {
                        fields.push_back(expr);
                    }
                } while (match(TK::COMMA));
            }
            expect(TK::RPAREN);
            return this->template create_node<FunctionCall>("STRUCT", fields);
        }

        // Array literal: [1, 2, 3] (BigQuery)
        if (match(TK::LBRACKET)) {
            std::vector<SQLNode*> elements;
            if (!check(TK::RBRACKET)) {
                do {
                    elements.push_back(parse_expression());
                } while (match(TK::COMMA));
            }
            expect(TK::RBRACKET);
            return this->template create_node<FunctionCall>("ARRAY", elements);
        }

        if (match(TK::EXTRACT)) {
            expect(TK::LPAREN);
            if (current().type != TK::IDENTIFIER) {
                error("Expected date/time field name (YEAR, MONTH, DAY, etc.) after EXTRACT(");
            }
            // Token text points into arena-owned source, so the string_view
            // is safe to store directly (a local std::string would dangle).
            std::string_view field = current().text;
            (void)advance(); // Acknowledge nodiscard warning
            expect(TK::FROM);
            auto expr = parse_expression();
            expect(TK::RPAREN);
            return this->template create_node<FunctionCall>(
                "EXTRACT", std::vector<SQLNode*>{this->template create_node<Literal>(field), expr});
        }

        // Unary operators (NOT, -, +)
        // Boolean NOT binds at precedence 10: tighter than AND (9) / OR (8),
        // looser than comparisons (12), so `NOT a = b AND c` parses as
        // (NOT (a = b)) AND c.
        if (match(TK::NOT)) {
            auto operand = parse_expression(kNotPrecedence);
            return this->template create_node<UnaryOp>(TK::NOT, operand);
        }

        // Arithmetic unary +/- bind above every binary operator (15), so
        // `-2 + 3` parses as (-2) + 3, not -(2 + 3).
        if (match(TK::MINUS)) {
            auto operand = parse_expression(kUnaryArithmeticPrecedence);
            return this->template create_node<UnaryOp>(TK::MINUS, operand);
        }

        if (match(TK::PLUS)) {
            auto operand = parse_expression(kUnaryArithmeticPrecedence);
            return this->template create_node<UnaryOp>(TK::PLUS, operand);
        }

        // Oracle hierarchical PRIOR operator (CONNECT BY PRIOR id = parent_id).
        // Binds like arithmetic unary +/- so `PRIOR a = b` parses as
        // (PRIOR a) = b.
        if (match(TK::PRIOR)) {
            auto operand = parse_expression(kUnaryArithmeticPrecedence);
            return this->template create_node<UnaryOp>(TK::PRIOR, operand);
        }

        // Sequence NEXTVAL('seq') / CURRVAL('seq') function-style call
        // (PostgreSQL/DB2/MariaDB/... ; the Oracle member-style seq.NEXTVAL
        // is recognized below, in the column-reference '.' handling).
        // NEXTVAL/CURRVAL are not reserved keywords, so this must be
        // disambiguated from an ordinary function call by name + LPAREN.
        if (check(TK::IDENTIFIER) &&
            (ieq(current().text, "NEXTVAL") || ieq(current().text, "CURRVAL")) &&
            peek(1).type == TK::LPAREN) {
            bool is_next = ieq(current().text, "NEXTVAL");
            (void)advance(); // NEXTVAL / CURRVAL
            expect(TK::LPAREN);
            if (!check(TK::STRING) && !check(TK::IDENTIFIER)) {
                error("Expected sequence name in NEXTVAL()/CURRVAL()");
            }
            std::string_view raw = advance().text;
            // Strip surrounding quotes so the canonical AST always holds the
            // bare sequence name, regardless of surface spelling.
            if (raw.size() >= 2 && raw.front() == '\'' && raw.back() == '\'') {
                raw = raw.substr(1, raw.size() - 2);
            }
            expect(TK::RPAREN);
            return this->template create_node<SequenceRefExpr>(raw, is_next);
        }

        // MySQL/MariaDB fulltext search: MATCH (col, ...) AGAINST ('expr' [modifier])
        // MATCH/AGAINST are not reserved keywords (soft keywords).
        if (check(TK::IDENTIFIER) && ieq(current().text, "MATCH") && peek(1).type == TK::LPAREN) {
            (void)advance(); // MATCH
            expect(TK::LPAREN);
            auto* match_node = this->template create_node<MatchAgainst>();
            do {
                if (!check(TK::IDENTIFIER)) {
                    error("Expected column name in MATCH(...)");
                }
                match_node->columns.push_back(advance().text);
            } while (match(TK::COMMA));
            expect(TK::RPAREN);

            if (!(check(TK::IDENTIFIER) && ieq(current().text, "AGAINST"))) {
                error("Expected AGAINST after MATCH(...)");
            }
            (void)advance(); // AGAINST
            expect(TK::LPAREN);
            {
                // Suppress the generic "expr IN (...)" postfix so a
                // trailing "IN NATURAL LANGUAGE MODE"/"IN BOOLEAN MODE"
                // modifier isn't mistaken for a value-list IN operator.
                ScopedNoInPostfix guard(*this);
                match_node->against_expr = parse_expression();
            }

            if (match(TK::IN)) {
                match_node->mode_specified = true;
                if (match(TK::NATURAL)) {
                    expect(TK::LANGUAGE);
                    if (!(check(TK::IDENTIFIER) && ieq(current().text, "MODE"))) {
                        error("Expected MODE after IN NATURAL LANGUAGE");
                    }
                    (void)advance(); // MODE
                    if (match(TK::WITH)) {
                        if (!(check(TK::IDENTIFIER) && ieq(current().text, "QUERY"))) {
                            error("Expected QUERY EXPANSION after WITH");
                        }
                        (void)advance(); // QUERY
                        if (!(check(TK::IDENTIFIER) && ieq(current().text, "EXPANSION"))) {
                            error("Expected EXPANSION after WITH QUERY");
                        }
                        (void)advance(); // EXPANSION
                        match_node->mode = FulltextMode::NATURAL_LANGUAGE_EXPANSION;
                    } else {
                        match_node->mode = FulltextMode::NATURAL_LANGUAGE;
                    }
                } else if (match(TK::BOOLEAN)) {
                    if (!(check(TK::IDENTIFIER) && ieq(current().text, "MODE"))) {
                        error("Expected MODE after IN BOOLEAN");
                    }
                    (void)advance(); // MODE
                    match_node->mode = FulltextMode::BOOLEAN_MODE;
                } else {
                    error("Expected NATURAL LANGUAGE MODE or BOOLEAN MODE after IN");
                }
            } else if (match(TK::WITH)) {
                match_node->mode_specified = true;
                if (!(check(TK::IDENTIFIER) && ieq(current().text, "QUERY"))) {
                    error("Expected QUERY EXPANSION after WITH");
                }
                (void)advance(); // QUERY
                if (!(check(TK::IDENTIFIER) && ieq(current().text, "EXPANSION"))) {
                    error("Expected EXPANSION after WITH QUERY");
                }
                (void)advance(); // EXPANSION
                match_node->mode = FulltextMode::QUERY_EXPANSION;
            }

            expect(TK::RPAREN);
            return match_node;
        }

        // Function call or column reference (including keywords used as identifiers)
        if (check(TK::IDENTIFIER) || check(TK::RANK) || check(TK::ORDER) || check(TK::TEMP) ||
            check(TK::LEVEL) || check(TK::COUNT) || check(TK::SUM) || check(TK::AVG) ||
            check(TK::MIN) || check(TK::MAX) || check(TK::DENSE_RANK) || check(TK::ROW_NUMBER) ||
            check(TK::NTILE) || check(TK::LEAD) || check(TK::LAG) || check(TK::FIRST_VALUE) ||
            check(TK::LAST_VALUE) || check(TK::NTH_VALUE) || check(TK::SUBSTRING) ||
            check(TK::SUBSTR) || check(TK::CONCAT_KW) || check(TK::CONCAT_WS) ||
            check(TK::LENGTH) || check(TK::TRIM) || check(TK::UPPER) || check(TK::LOWER) ||
            check(TK::REPLACE) || check(TK::REPLACE_KW) || check(TK::REPLACE_DDB) ||
            check(TK::SPLIT) || check(TK::ROUND) || check(TK::FLOOR) || check(TK::CEIL) ||
            check(TK::ABS) || check(TK::POWER) || check(TK::SQRT) || check(TK::TIMESTAMP) ||
            check(TK::DATE) || check(TK::TIME) || check(TK::DATE_TRUNC) ||
            check(TK::GENERATE_SERIES) || check(TK::UNNEST)) {
            auto first = advance();
            std::string_view name = first.text;

            // Function call: func(...)
            if (match(TK::LPAREN)) {
                return parse_function_call(name);
            }

            // table.column or column.field (but not .. range operator)
            if (check(TK::DOT) && !check_double_dot()) {
                (void)advance(); // consume DOT
                if (check(TK::STAR)) {
                    (void)advance(); // Acknowledge nodiscard warning
                    return this->template create_node<Star>(name);
                }
                // Allow keywords as column names (SQL permits this)
                if (check(TK::LPAREN) || check(TK::RPAREN) || check(TK::COMMA) ||
                    check(TK::SEMICOLON) || check(TK::EOF_TOKEN)) {
                    error("Expected column name after '.'");
                }
                auto second = advance();

                // Oracle member-style sequence reference: seq.NEXTVAL / seq.CURRVAL.
                // Gated to Oracle so an ordinary column named "nextval" (however
                // unlikely) still parses as a plain column in every other dialect.
                if (dialect_ == SQLDialect::Oracle &&
                    (ieq(second.text, "NEXTVAL") || ieq(second.text, "CURRVAL"))) {
                    return this->template create_node<SequenceRefExpr>(name,
                                                                       ieq(second.text, "NEXTVAL"));
                }

                return this->template create_node<Column>(name, second.text);
            }

            // Just a column
            return this->template create_node<Column>(name);
        }

        error("Expected expression");
    }

    /// Check if we're at a ".." range operator
    [[nodiscard]] bool check_double_dot() const noexcept {
        return check(TK::DOT) && peek(1).type == TK::DOT;
    }

    /// Case-insensitive comparison of token text against an UPPERCASE literal.
    /// Used for soft keywords recognized purely by their identifier spelling
    /// (SEQUENCE, NEXTVAL, MODE, OF, TO, CONTAINED, ...) where a reserved
    /// token would cost every dialect a common word.
    [[nodiscard]] static bool ieq(std::string_view text, std::string_view upper) noexcept {
        if (text.size() != upper.size())
            return false;
        for (size_t i = 0; i < text.size(); ++i) {
            char c = text[i];
            if (c >= 'a' && c <= 'z')
                c = static_cast<char>(c - 'a' + 'A');
            if (c != upper[i])
                return false;
        }
        return true;
    }

    /// Parse the target type of CAST(expr AS <type>) up to the CAST's own
    /// closing paren. Paren-depth aware so parameterized types like
    /// VARCHAR(10) or DECIMAL(10, 2) are captured whole - stopping at the
    /// first ')' used to leave the CAST's closing paren and everything after
    /// it (the FROM clause!) unconsumed. Returns a view into the
    /// arena-owned source, preserving original spacing.
    [[nodiscard]] std::string_view parse_cast_type_name() {
        size_t type_start = current().start;
        size_t type_end = type_start;
        int paren_depth = 0;
        while (!is_eof()) {
            if (check(TK::RPAREN)) {
                if (paren_depth == 0)
                    break; // CAST's closing paren
                paren_depth--;
            } else if (check(TK::LPAREN)) {
                paren_depth++;
            }
            type_end = current().end;
            (void)advance();
        }
        return source_.substr(type_start, type_end - type_start);
    }

    /// Parse postfix expression (array indexing, JSON operators, etc.)
    [[nodiscard]] SQLNode* parse_postfix(SQLNode* base) {
        while (true) {
            // IN operator: expr IN (value1, value2, ...) or expr IN (SELECT ...)
            if (!no_in_postfix_ && check(TK::IN)) {
                (void)advance(); // Consume IN
                base = parse_in_rest(base, /*not_in=*/false);
                continue;
            }

            // BETWEEN operator: expr BETWEEN low AND high.
            // Parsed as a special form (not via the binary-operator table):
            // both bounds are parsed above comparison precedence so the AND
            // separating them is not mistaken for boolean AND.
            if (check(TK::BETWEEN)) {
                (void)advance(); // Consume BETWEEN
                base = parse_between_rest(base, /*not_between=*/false);
                continue;
            }

            // Negated infix forms: NOT IN / NOT BETWEEN / NOT LIKE / NOT ILIKE
            if (check(TK::NOT) && (peek(1).type == TK::IN || peek(1).type == TK::BETWEEN ||
                                   peek(1).type == TK::LIKE || peek(1).type == TK::ILIKE)) {
                (void)advance(); // Consume NOT
                if (match(TK::IN)) {
                    base = parse_in_rest(base, /*not_in=*/true);
                } else if (match(TK::BETWEEN)) {
                    base = parse_between_rest(base, /*not_between=*/true);
                } else {
                    // NOT LIKE / NOT ILIKE: represent as NOT (expr LIKE pattern)
                    TK like_op = current().type;
                    (void)advance();
                    auto pattern = parse_expression(kComparisonOperandPrecedence);
                    auto like = this->template create_node<BinaryOp>(like_op, base, pattern);
                    base = this->template create_node<UnaryOp>(TK::NOT, like);
                }
                continue;
            }

            // Array indexing: expr[index], and BigQuery's subscript
            // functions expr[OFFSET(n)] (0-based), expr[ORDINAL(n)]
            // (1-based), expr[SAFE_OFFSET(n)] (0-based, NULL if out of
            // range). OFFSET/ORDINAL/SAFE_OFFSET are reserved keyword
            // tokens, not soft keywords, so they can't be mistaken for a
            // plain indexing expression that happens to call a same-named
            // function.
            if (match(TK::LBRACKET)) {
                ArraySubscript subscript = ArraySubscript::NONE;
                if ((check(TK::OFFSET) || check(TK::ORDINAL) || check(TK::SAFE_OFFSET)) &&
                    peek(1).type == TK::LPAREN) {
                    TK fn = advance().type;
                    subscript = (fn == TK::OFFSET)    ? ArraySubscript::OFFSET
                                : (fn == TK::ORDINAL) ? ArraySubscript::ORDINAL
                                                      : ArraySubscript::SAFE_OFFSET;
                    expect(TK::LPAREN);
                    auto index = parse_expression();
                    expect(TK::RPAREN);
                    expect(TK::RBRACKET);
                    auto* node = this->template create_node<ArrayIndex>(base, index);
                    node->subscript = subscript;
                    base = node;
                    continue;
                }
                auto index = parse_expression();
                expect(TK::RBRACKET);
                base = this->template create_node<ArrayIndex>(base, index);
                continue;
            }

            // JSON operators: expr->key or expr->>key
            if (match(TK::ARROW)) {
                auto key = parse_expression();
                base = this->template create_node<JsonExpr>(base, key, JsonExpr::OpType::ARROW);
                continue;
            }

            if (match(TK::LONG_ARROW)) {
                auto key = parse_expression();
                base =
                    this->template create_node<JsonExpr>(base, key, JsonExpr::OpType::LONG_ARROW);
                continue;
            }

            // Snowflake colon notation: data:field or data:field[0]
            // Only in Snowflake dialect to avoid conflicts with other uses of colon
            if (dialect_ == SQLDialect::Snowflake && match(TK::COLON)) {
                // The next token should be an identifier (field name)
                if (!check(TK::IDENTIFIER)) {
                    error("Expected field name after ':' in Snowflake JSON access");
                }
                auto field_name = advance().text;
                auto field_node = this->template create_node<Column>(field_name);
                // Create the colon access node
                base = this->template create_node<BinaryOp>(TK::COLON, base, field_node);
                // Continue loop to handle potential [0] bracket notation
                // (the continue will loop back and check for [ at the top of the while)
                continue;
            }

            // No more postfix operators
            break;
        }
        return base;
    }

    /// Parse the remainder of [NOT] IN after IN has been consumed
    [[nodiscard]] SQLNode* parse_in_rest(SQLNode* base, bool not_in) {
        expect(TK::LPAREN);

        // Check if it's a subquery or a list of values
        if (check(TK::SELECT)) {
            // IN (SELECT ...) - subquery form
            auto subquery = parse_select();
            expect(TK::RPAREN);
            return this->template create_node<InExpr>(base, std::vector<SQLNode*>{subquery},
                                                      not_in);
        }

        // IN (value1, value2, ...) - value list form
        std::vector<SQLNode*> values;
        if (!check(TK::RPAREN)) {
            do {
                values.push_back(parse_expression());
            } while (match(TK::COMMA));
        }
        expect(TK::RPAREN);
        return this->template create_node<InExpr>(base, values, not_in);
    }

    /// Parse the remainder of [NOT] BETWEEN after BETWEEN has been consumed
    [[nodiscard]] SQLNode* parse_between_rest(SQLNode* base, bool not_between) {
        auto lower = parse_expression(kComparisonOperandPrecedence);
        expect(TK::AND);
        auto upper = parse_expression(kComparisonOperandPrecedence);
        return this->template create_node<BetweenExpr>(base, lower, upper, not_between);
    }

    /// Create binary operator node (required for precedence climbing)
    [[nodiscard]] SQLNode* make_binary_operator(TK op, SQLNode* left, SQLNode* right) {
        return this->template create_node<BinaryOp>(op, left, right);
    }

    // ========================================================================
    // SQL-Specific Parsing Rules
    // ========================================================================

    /// Parse SELECT statement (may return set operation for UNION/INTERSECT/EXCEPT)
    SQLNode* parse_select() {
        SelectStmt* stmt = parse_select_body();

        // Set operations: UNION, INTERSECT, EXCEPT (left-associative)
        if (check(TK::UNION) || check(TK::INTERSECT) || check(TK::EXCEPT)) {
            return parse_set_operation(stmt);
        }

        return stmt;
    }

    /// Parse a single SELECT statement without a set-operation tail
    SelectStmt* parse_select_body() {
        auto stmt = this->template create_node<SelectStmt>();

        // WITH clause (CTEs)
        if (check(TK::WITH)) {
            stmt->with = parse_with_clause();
        }

        expect(TK::SELECT);

        // DISTINCT? / DISTINCT ON (expr, ...) (PostgreSQL)
        if (match(TK::DISTINCT)) {
            stmt->distinct = true;
            if (match(TK::ON)) {
                expect(TK::LPAREN);
                do {
                    stmt->distinct_on.push_back(parse_expression());
                } while (match(TK::COMMA));
                expect(TK::RPAREN);
            }
        }

        // TOP n (SQL Server, Access). The count is parsed as a primary
        // expression only: a full parse_expression would treat the select
        // list's leading '*' as multiplication (TOP 10 * FROM ... -> 10 * ?)
        // and reject the generator's own TOP output on re-parse.
        if (match(TK::TOP)) {
            stmt->limit = parse_prefix();
            // Optional: PERCENT ('%' operator token or PERCENT keyword/identifier)
            if (check(TK::PERCENT) || check(TK::PERCENT_KW) ||
                (check(TK::IDENTIFIER) &&
                 (current().text == "PERCENT" || current().text == "percent"))) {
                (void)advance();
                stmt->limit_percent = true;
            }
            // Optional: WITH TIES
            if (check(TK::WITH) && (peek(1).type == TK::WITH_TIES ||
                                    (peek(1).type == TK::IDENTIFIER &&
                                     (peek(1).text == "TIES" || peek(1).text == "ties")))) {
                (void)advance(); // WITH
                (void)advance(); // TIES
                stmt->limit_with_ties = true;
            }
        }

        // FIRST n [SKIP m] (Firebird, Informix)
        bool has_first = false;
        if (match(TK::FIRST)) {
            has_first = true;
            stmt->limit = parse_prefix(); // Parse just the number
            // Optional: SKIP m (offset)
            if (match(TK::SKIP)) {
                stmt->offset = parse_prefix(); // Parse just the number
            }
        }

        // SELECT list
        stmt->columns = parse_select_list();

        // Validate that we have at least one column
        if (stmt->columns.empty()) {
            error("Expected column list after SELECT");
        }

        // SELECT ... INTO target (T-SQL SELECT INTO #temp, PL/SQL SELECT INTO var)
        if (match(TK::INTO)) {
            stmt->into_table = parse_table_ref();
        }

        // FROM clause
        if (match(TK::FROM)) {
            stmt->from = parse_from_clause();
        }

        // WHERE clause
        if (match(TK::WHERE)) {
            stmt->where = parse_expression();
        }

        // Oracle hierarchical query clauses: START WITH cond / CONNECT BY
        // [NOCYCLE] cond. Oracle accepts the two clauses in either order,
        // so loop until neither matches. START is not a reserved word (it
        // lexes as an identifier), hence the two-token lookahead.
        while (true) {
            if (check(TK::CONNECT)) {
                (void)advance();
                expect(TK::BY);
                auto* connect_by = this->template create_node<ConnectByClause>();
                if (match(TK::NOCYCLE)) {
                    connect_by->nocycle = true;
                }
                connect_by->condition = parse_expression();
                stmt->connect_by = connect_by;
            } else if (check(TK::IDENTIFIER) &&
                       (current().text == "START" || current().text == "start") &&
                       peek(1).type == TK::WITH) {
                (void)advance(); // START
                (void)advance(); // WITH
                auto* start_with = this->template create_node<StartWithClause>();
                start_with->condition = parse_expression();
                stmt->start_with = start_with;
            } else {
                break;
            }
        }

        // GROUP BY, including the SQL:1999 OLAP extensions
        // (GROUPING SETS / ROLLUP / CUBE), possibly mixed with plain items
        if (match(TK::GROUP)) {
            expect(TK::BY);
            do {
                stmt->group_by.push_back(parse_group_by_item());
            } while (match(TK::COMMA));
        }

        // HAVING
        if (match(TK::HAVING)) {
            stmt->having = parse_expression();
        }

        // QUALIFY clause (Snowflake, BigQuery)
        if (match(TK::QUALIFY)) {
            auto condition = parse_expression();
            stmt->qualify = this->template create_node<QualifyClause>(condition);
        }

        // WINDOW clause: WINDOW w AS (...), w2 AS (...). WINDOW is not a
        // reserved word (it lexes as an identifier), so use the
        // soft-keyword lookahead pattern used for ROLLUP/CUBE/TABLESAMPLE.
        if (check_soft_keyword("WINDOW", "window")) {
            (void)advance();
            do {
                if (!check(TK::IDENTIFIER)) {
                    error("Expected window name after WINDOW");
                }
                auto name = advance().text;
                expect(TK::AS);
                expect(TK::LPAREN);
                auto* wspec = this->template create_node<WindowSpec>();
                if (match(TK::PARTITION)) {
                    expect(TK::BY);
                    do {
                        wspec->partition_by.push_back(parse_expression());
                    } while (match(TK::COMMA));
                }
                if (match(TK::ORDER)) {
                    expect(TK::BY);
                    auto order_items = parse_order_by_list();
                    for (auto* item : order_items) {
                        wspec->order_by.push_back(item);
                    }
                }
                if (check(TK::ROWS) || check(TK::RANGE) || check_groups_keyword()) {
                    wspec->frame = parse_frame_clause();
                }
                expect(TK::RPAREN);
                stmt->named_windows.push_back({name, wspec});
            } while (match(TK::COMMA));
        }

        // ORDER BY / ORDER SIBLINGS BY (Oracle hierarchical ordering)
        if (match(TK::ORDER)) {
            if (check(TK::IDENTIFIER) &&
                (current().text == "SIBLINGS" || current().text == "siblings")) {
                (void)advance();
                stmt->order_siblings = true;
            }
            expect(TK::BY);
            stmt->order_by = parse_order_by_list();
        }

        // LIMIT
        if (match(TK::LIMIT)) {
            stmt->limit = parse_expression();
        }

        // OFFSET n [ROW | ROWS]  (the ROW/ROWS suffix is the ANSI
        // OFFSET..FETCH form used by SQL Server / Oracle / DB2)
        if (match(TK::OFFSET)) {
            stmt->offset = parse_expression();
            if (!match(TK::ROWS)) {
                (void)match(TK::ROW);
            }
        }

        // FETCH {FIRST | NEXT} n {ROW | ROWS} ONLY  (ANSI / Oracle 12c+ /
        // DB2 / SQL Server OFFSET..FETCH) - a limit by another name
        if (match(TK::FETCH)) {
            if (!match(TK::FIRST)) {
                (void)match(TK::NEXT);
            }
            stmt->limit = parse_expression();
            if (!match(TK::ROWS)) {
                (void)match(TK::ROW);
            }
            (void)match(TK::ONLY);
        }

        // FOR UPDATE [OF col, ...] [NOWAIT | SKIP LOCKED]  (row locking)
        if (check(TK::FOR) && peek(1).type == TK::UPDATE) {
            (void)advance(); // FOR
            (void)advance(); // UPDATE
            stmt->for_update = true;

            if (match(TK::OF)) {
                do {
                    if (!check(TK::IDENTIFIER)) {
                        error("Expected column name after FOR UPDATE OF");
                    }
                    stmt->for_update_of.push_back(advance().text);
                } while (match(TK::COMMA));
            }

            if (match(TK::NOWAIT)) {
                stmt->for_update_wait = ForUpdateWait::NOWAIT;
            } else if (check(TK::SKIP) && peek(1).type == TK::LOCKED) {
                (void)advance(); // SKIP
                (void)advance(); // LOCKED
                stmt->for_update_wait = ForUpdateWait::SKIP_LOCKED;
            }
        }

        return stmt;
    }

    /// Parse SELECT list (comma-separated expressions, possibly aliased)
    std::vector<SQLNode*> parse_select_list() {
        std::vector<SQLNode*> items;

        // Parse first item
        items.push_back(parse_select_item());

        // Parse remaining items (comma-separated)
        while (match(TK::COMMA)) {
            // Strict: disallow trailing comma before FROM
            if (check(TK::FROM)) {
                error("Unexpected trailing comma before FROM");
            }
            if (is_eof()) {
                error("Expected column after comma in SELECT list");
            }
            items.push_back(parse_select_item());
        }

        return items;
    }

    /// Parse single SELECT item (expression or expression AS alias)
    SQLNode* parse_select_item() {
        // Handle SELECT *
        if (this->check(TK::STAR)) {
            (void)this->advance();
            return this->template create_node<Star>();
        }

        auto expr = this->parse_expression();

        // Check for AS alias
        if (this->match(TK::AS)) {
            // Accept almost any token as an alias (SQL allows keywords as identifiers when used as
            // aliases) The only tokens we reject are structural ones like parentheses, commas,
            // operators
            if (this->check(TK::LPAREN) || this->check(TK::RPAREN) || this->check(TK::COMMA) ||
                this->check(TK::SEMICOLON) || this->check(TK::EOF_TOKEN)) {
                this->error("Expected alias after AS");
            }
            auto alias_tok = this->advance();
            return this->template create_node<Alias>(expr, alias_tok.text);
        }

        return expr;
    }

    /// Parse a single GROUP BY item: a plain expression, ROLLUP(...),
    /// CUBE(...), or GROUPING SETS (...). ROLLUP/CUBE/GROUPING are not
    /// reserved words (they lex as identifiers), so a GROUPING(col)
    /// aggregate in the SELECT list still parses as an ordinary function
    /// call - only the two-token "GROUPING SETS" form is special here.
    SQLNode* parse_group_by_item() {
        if (check_soft_keyword("ROLLUP", "rollup") && peek(1).type == TK::LPAREN) {
            (void)advance(); // ROLLUP
            auto* rollup = this->template create_node<RollupClause>();
            parse_grouping_expr_list_into(rollup->expressions);
            return rollup;
        }

        if (check_soft_keyword("CUBE", "cube") && peek(1).type == TK::LPAREN) {
            (void)advance(); // CUBE
            auto* cube = this->template create_node<CubeClause>();
            parse_grouping_expr_list_into(cube->expressions);
            return cube;
        }

        if (check_soft_keyword("GROUPING", "grouping") && peek(1).type == TK::IDENTIFIER &&
            (peek(1).text == "SETS" || peek(1).text == "sets")) {
            (void)advance(); // GROUPING
            (void)advance(); // SETS
            return parse_grouping_sets_body();
        }

        return parse_expression();
    }

    /// Check whether the current token is a given non-reserved keyword
    /// (lexed as an identifier)
    [[nodiscard]] bool check_soft_keyword(std::string_view upper,
                                          std::string_view lower) const noexcept {
        return check(TK::IDENTIFIER) && (current().text == upper || current().text == lower);
    }

    /// Parse the parenthesized expression list of ROLLUP(...) / CUBE(...)
    void parse_grouping_expr_list_into(std::vector<SQLNode*>& out) {
        expect(TK::LPAREN);
        if (!check(TK::RPAREN)) {
            do {
                out.push_back(parse_expression());
            } while (match(TK::COMMA));
        }
        expect(TK::RPAREN);
    }

    /// Parse the body of GROUPING SETS (...) after the two keywords have
    /// been consumed. Each element is either a parenthesized (possibly
    /// empty) list of grouping items or a single bare item - which may
    /// itself be ROLLUP(...), CUBE(...), or a nested GROUPING SETS.
    SQLNode* parse_grouping_sets_body() {
        auto* grouping_sets = this->template create_node<GroupingSets>();
        expect(TK::LPAREN);
        if (!check(TK::RPAREN)) {
            do {
                std::vector<SQLNode*> set;
                if (check(TK::LPAREN)) {
                    (void)advance();
                    if (!check(TK::RPAREN)) {
                        do {
                            set.push_back(parse_group_by_item());
                        } while (match(TK::COMMA));
                    }
                    expect(TK::RPAREN);
                } else {
                    set.push_back(parse_group_by_item());
                }
                grouping_sets->sets.push_back(std::move(set));
            } while (match(TK::COMMA));
        }
        expect(TK::RPAREN);
        return grouping_sets;
    }

    /// Parse table reference (simplified for Phase C1)
    TableRef* parse_table_ref() {
        // SQL Server temporary tables: #table or ##table
        std::string_view prefix = "";
        if (this->check(TK::HASH)) {
            prefix = "#";
            (void)this->advance();
            // Check for ## (global temp table)
            if (this->check(TK::HASH)) {
                prefix = "##";
                (void)this->advance();
            }
        }

        // Accept identifiers or keywords as table names (SQL allows reserved words as identifiers
        // when quoted) Also accept TEMP, TEMPORARY, SETTINGS, and other common keywords that might
        // appear in table names
        if (!this->check(TK::IDENTIFIER) && !this->check(TK::TABLE) && !this->check(TK::DUAL) &&
            !this->check(TK::TEMP) && !this->check(TK::TEMPORARY) && !this->check(TK::LEVEL) &&
            !this->check(TK::SETTINGS)) {
            // For SQL Server temp tables with # prefix, be more permissive
            if (prefix.empty()) {
                this->error("Expected table name");
            }
            // With prefix, allow any token as table name (will consume it anyway)
        }

        auto first = this->advance();
        std::string_view first_name = first.text;

        // Handle SQL Server temp tables where name might be tokenized as multiple tokens
        // If the next token is an identifier starting with underscore, concatenate
        // (e.g., "temp" + "_data" = "temp_data", "global" + "_temp" = "global_temp")
        if (!prefix.empty() && this->check(TK::IDENTIFIER)) {
            auto next_tok = this->current();
            if (!next_tok.text.empty() && next_tok.text[0] == '_') {
                // Concatenate: first_name + _xxx = first_name_xxx
                std::string combined = std::string(first_name) + std::string(next_tok.text);
                first_name = this->arena().copy_source(combined);
                (void)this->advance(); // Consume the _xxx part
            }
        }

        // Apply prefix if present
        if (!prefix.empty()) {
            std::string prefixed = std::string(prefix) + std::string(first_name);
            first_name = this->arena().copy_source(prefixed);
        }

        // Check for database.schema.table or database.table
        if (this->match(TK::DOT)) {
            // Allow keywords as identifiers in qualified names
            if (!this->check(TK::IDENTIFIER) && !this->check(TK::TABLE) &&
                !this->check(TK::SCHEMA)) {
                this->error("Expected table name after '.'");
            }
            auto second = this->advance();
            std::string_view second_name = second.text;

            // Check for third part (database.schema.table)
            if (this->match(TK::DOT)) {
                if (!this->check(TK::IDENTIFIER) && !this->check(TK::TABLE)) {
                    this->error("Expected table name after second '.'");
                }
                auto third = this->advance();
                // Combine all three parts with dots
                std::string combined = std::string(first_name) + "." + std::string(second_name) +
                                       "." + std::string(third.text);
                return this->template create_node<TableRef>(this->arena().copy_source(combined));
            }

            return this->template create_node<TableRef>(first_name, second_name);
        }

        // Just a table name
        return this->template create_node<TableRef>(first_name);
    }

    /// Parse ORDER BY list
    std::vector<OrderByItem*> parse_order_by_list() {
        std::vector<OrderByItem*> items;

        // Parse at least one item
        items.push_back(parse_order_by_item());

        // Parse additional items (comma-separated)
        while (match(TK::COMMA)) {
            items.push_back(parse_order_by_item());
        }

        return items;
    }

    /// Parse single ORDER BY item (expression [ASC|DESC] [NULLS FIRST|NULLS LAST])
    OrderByItem* parse_order_by_item() {
        auto expr = this->parse_expression();

        bool ascending = true;
        if (this->match(TK::DESC)) {
            ascending = false;
        } else {
            // ASC is optional (default)
            (void)this->match(TK::ASC); // Acknowledge nodiscard warning
        }

        bool nulls_first = false;
        bool nulls_specified = false;
        if (this->match(TK::NULLS)) {
            if (this->match(TK::FIRST)) {
                nulls_first = true;
            } else {
                this->expect(TK::LAST);
                nulls_first = false;
            }
            nulls_specified = true;
        }

        return this->template create_node<OrderByItem>(expr, ascending, nulls_first,
                                                       nulls_specified);
    }

    /// Parse CASE expression
    CaseExpr* parse_case_expression() {
        auto case_expr = this->template create_node<CaseExpr>();

        // CASE expr? (if present, this is a "simple" CASE)
        if (!check(TK::WHEN)) {
            case_expr->case_value = parse_expression();
        }

        // Parse WHEN clauses
        while (match(TK::WHEN)) {
            auto condition = parse_expression();
            expect(TK::THEN);
            auto result = parse_expression();
            case_expr->when_clauses.push_back({condition, result});
        }

        // ELSE clause (optional)
        if (match(TK::ELSE)) {
            case_expr->else_expr = parse_expression();
        }

        expect(TK::END);
        return case_expr;
    }

    /// Parse function call (already consumed function name and opening paren)
    SQLNode* parse_function_call(std::string_view func_name) {
        std::vector<SQLNode*> args;

        // Check for DISTINCT in aggregates: COUNT(DISTINCT col)
        bool distinct = match(TK::DISTINCT);

        // Special handling for COUNT(*), SUM(*), etc.
        if (match(TK::STAR)) {
            args.push_back(this->template create_node<Star>());
        } else if (!check(TK::RPAREN)) {
            do {
                args.push_back(parse_expression());
            } while (match(TK::COMMA));
        }

        expect(TK::RPAREN);

        // Window function: func(...) OVER (...)
        if (check(TK::OVER)) {
            return parse_window_function(func_name, args);
        }

        // Regular function call
        auto func = this->template create_node<FunctionCall>(func_name, args);
        func->distinct = distinct;
        return func;
    }

    /// Parse window function (OVER clause: inline spec or a named window reference)
    WindowFunction* parse_window_function(std::string_view func_name, std::vector<SQLNode*> args) {
        expect(TK::OVER);

        // OVER w - reference to a window defined in a WINDOW clause,
        // distinguished from the inline OVER (...) form by the absence of
        // a following '('.
        if (check(TK::IDENTIFIER)) {
            auto window_func = this->template create_node<WindowFunction>(func_name, nullptr);
            window_func->args = args;
            window_func->over_name = advance().text;
            return window_func;
        }

        expect(TK::LPAREN);

        auto window_spec = this->template create_node<WindowSpec>();

        // PARTITION BY
        if (match(TK::PARTITION)) {
            expect(TK::BY);
            do {
                window_spec->partition_by.push_back(parse_expression());
            } while (match(TK::COMMA));
        }

        // ORDER BY
        if (match(TK::ORDER)) {
            expect(TK::BY);
            auto order_by_items = parse_order_by_list();
            // Convert OrderByItem* to SQLNode* for storage in WindowSpec
            for (auto* item : order_by_items) {
                window_spec->order_by.push_back(item);
            }
        }

        // Frame clause: ROWS/RANGE/GROUPS [BETWEEN ...]
        if (check(TK::ROWS) || check(TK::RANGE) || check_groups_keyword()) {
            window_spec->frame = parse_frame_clause();
        }

        expect(TK::RPAREN);

        auto window_func = this->template create_node<WindowFunction>(func_name, window_spec);
        window_func->args = args;
        return window_func;
    }

    /// Check whether the current token is the GROUPS frame keyword
    /// (GROUPS is not a reserved keyword, so it lexes as an identifier)
    [[nodiscard]] bool check_groups_keyword() const noexcept {
        return check(TK::IDENTIFIER) && (current().text == "GROUPS" || current().text == "groups");
    }

    /// Parse window frame clause: ROWS/RANGE/GROUPS [BETWEEN] ...
    FrameClause* parse_frame_clause() {
        // Frame type: ROWS, RANGE, or GROUPS
        FrameType frame_type;
        if (match(TK::ROWS)) {
            frame_type = FrameType::ROWS;
        } else if (match(TK::RANGE)) {
            frame_type = FrameType::RANGE;
        } else if (check_groups_keyword()) {
            (void)advance();
            frame_type = FrameType::GROUPS;
        } else {
            error("Expected ROWS, RANGE, or GROUPS for window frame");
        }

        // BETWEEN start AND end
        if (match(TK::BETWEEN)) {
            auto [start_bound, start_offset] = parse_frame_bound();
            expect(TK::AND);
            auto [end_bound, end_offset] = parse_frame_bound();

            auto frame = this->template create_node<FrameClause>(frame_type, start_bound);
            frame->start_offset = start_offset;
            frame->end_bound = end_bound;
            frame->end_offset = end_offset;
            frame->between_form = true;
            return frame;
        } else {
            // Single boundary (implies BETWEEN start AND CURRENT ROW)
            auto [bound, offset] = parse_frame_bound();
            auto frame = this->template create_node<FrameClause>(frame_type, bound);
            frame->start_offset = offset;
            return frame;
        }
    }

    /// Parse frame boundary: UNBOUNDED PRECEDING | N PRECEDING | CURRENT ROW | N FOLLOWING |
    /// UNBOUNDED FOLLOWING
    std::pair<FrameBound, SQLNode*> parse_frame_bound() {
        if (match(TK::UNBOUNDED)) {
            if (match(TK::PRECEDING)) {
                return {FrameBound::UNBOUNDED_PRECEDING, nullptr};
            } else if (match(TK::FOLLOWING)) {
                return {FrameBound::UNBOUNDED_FOLLOWING, nullptr};
            } else {
                error("Expected PRECEDING or FOLLOWING after UNBOUNDED");
            }
        } else if (match(TK::CURRENT)) {
            expect(TK::ROW);
            return {FrameBound::CURRENT_ROW, nullptr};
        } else {
            // N PRECEDING or N FOLLOWING
            auto offset = parse_expression();
            if (match(TK::PRECEDING)) {
                return {FrameBound::PRECEDING, offset};
            } else if (match(TK::FOLLOWING)) {
                return {FrameBound::FOLLOWING, offset};
            } else {
                error("Expected PRECEDING or FOLLOWING after frame offset");
            }
        }
    }

    /// Parse WITH clause (CTEs - Common Table Expressions)
    WithClause* parse_with_clause() {
        expect(TK::WITH);
        auto with_clause = this->template create_node<WithClause>();

        // Check for RECURSIVE keyword
        if (check(TK::RECURSIVE)) {
            (void)advance();
            with_clause->recursive = true;
        }

        // Parse CTEs
        do {
            // Check for RECURSIVE before individual CTE (non-standard but some dialects support)
            if (match(TK::RECURSIVE)) {
                with_clause->recursive = true;
            }

            // CTE name
            if (!check(TK::IDENTIFIER)) {
                error("Expected CTE name");
            }
            std::string_view cte_name = advance().text;

            // Optional column list: (col1, col2, ...)
            std::vector<std::string_view> columns;
            if (match(TK::LPAREN)) {
                do {
                    if (check(TK::IDENTIFIER)) {
                        columns.push_back(advance().text);
                    }
                } while (match(TK::COMMA));
                expect(TK::RPAREN);
            }

            expect(TK::AS);
            expect(TK::LPAREN);
            auto query = parse_select();
            expect(TK::RPAREN);

            auto cte = this->template create_node<CTE>(cte_name, query);
            cte->columns = std::move(columns);

            with_clause->ctes.push_back(cte);
        } while (match(TK::COMMA));

        return with_clause;
    }

    /// Parse FROM clause with JOINs
    SQLNode* parse_from_clause() {
        auto table = parse_table_or_subquery();

        // Handle comma-separated tables (old-style implicit CROSS JOIN) and explicit JOINs
        while (check(TK::COMMA) || check(TK::JOIN) || check(TK::INNER) || check(TK::LEFT) ||
               check(TK::RIGHT) || check(TK::FULL) || check(TK::CROSS) || check(TK::OUTER) ||
               check(TK::ASOF) || check(TK::NATURAL)) {

            // Comma-separated tables are implicit CROSS JOINs
            if (match(TK::COMMA)) {
                auto right_table = parse_table_or_subquery();
                table = this->template create_node<JoinClause>(JoinType::CROSS, table, right_table,
                                                               nullptr);
                continue;
            }

            // Explicit JOIN syntax
            JoinType join_type = JoinType::INNER;
            bool saw_apply = false;
            bool asof = false;

            // NATURAL [INNER|LEFT|RIGHT|FULL] JOIN - implicit join condition
            // on all identically-named columns; never combined with ON/USING.
            bool natural = match(TK::NATURAL);

            // ASOF prefix (DuckDB / ClickHouse): ASOF [LEFT] JOIN
            if (match(TK::ASOF)) {
                asof = true;
            }

            if (match(TK::INNER)) {
                expect(TK::JOIN);
            } else if (match(TK::LEFT)) {
                join_type = JoinType::LEFT;
                (void)match(TK::OUTER); // OUTER is optional
                expect(TK::JOIN);
            } else if (match(TK::RIGHT)) {
                join_type = JoinType::RIGHT;
                (void)match(TK::OUTER);
                expect(TK::JOIN);
            } else if (match(TK::FULL)) {
                join_type = JoinType::FULL;
                (void)match(TK::OUTER);
                expect(TK::JOIN);
            } else if (match(TK::CROSS)) {
                join_type = JoinType::CROSS;
                // SQL Server: CROSS APPLY instead of CROSS JOIN
                if (match(TK::APPLY)) {
                    saw_apply = true;
                } else {
                    expect(TK::JOIN);
                }
            } else if (match(TK::OUTER)) {
                // SQL Server: OUTER APPLY (or standalone OUTER JOIN which is non-standard)
                if (match(TK::APPLY)) {
                    join_type = JoinType::LEFT; // OUTER APPLY is like LEFT JOIN LATERAL
                    saw_apply = true;
                } else {
                    // Standalone OUTER JOIN (treat as LEFT OUTER JOIN)
                    join_type = JoinType::LEFT;
                    expect(TK::JOIN);
                }
            } else {
                (void)match(TK::JOIN);
            }

            auto right_table = parse_table_or_subquery();

            // Wrap in LATERAL node if we saw APPLY keyword
            if (saw_apply) {
                right_table = this->template create_node<LateralJoin>(right_table);
            }

            SQLNode* condition = nullptr;
            std::vector<std::string_view> using_columns;
            if (match(TK::ON)) {
                condition = parse_expression();
            } else if (match(TK::USING)) {
                expect(TK::LPAREN);
                do {
                    if (!check(TK::IDENTIFIER)) {
                        error("Expected column name in USING clause");
                    }
                    using_columns.push_back(advance().text);
                } while (match(TK::COMMA));
                expect(TK::RPAREN);
            }

            auto* join =
                this->template create_node<JoinClause>(join_type, table, right_table, condition);
            join->asof = asof;
            join->natural = natural;
            join->using_columns = std::move(using_columns);
            table = join;
        }

        return table;
    }

    /// Parse table reference or subquery in FROM clause
    SQLNode* parse_table_or_subquery() {
        // LATERAL join (PostgreSQL, Oracle 12c+)
        if (match(TK::LATERAL)) {
            // Snowflake: LATERAL FLATTEN(INPUT => expr [, PATH => 'p'] [, OUTER => bool]) [alias]
            // FLATTEN is a reserved keyword token (shared across dialects),
            // so it can't collide with an ordinary table-valued function
            // named "flatten".
            if (check(TK::FLATTEN)) {
                (void)advance(); // FLATTEN
                expect(TK::LPAREN);
                auto* flatten = this->template create_node<FlattenClause>();
                do {
                    // OUTER is a reserved keyword token (shared with OUTER
                    // JOIN) everywhere else, not a soft keyword here.
                    if (!check(TK::IDENTIFIER) && !check(TK::OUTER)) {
                        error("Expected named argument (INPUT, PATH, OUTER, ...) in FLATTEN(...)");
                    }
                    std::string_view arg_name = advance().text;
                    expect(TK::FAT_ARROW);
                    auto* value = parse_expression();
                    if (ieq(arg_name, "INPUT")) {
                        flatten->input = value;
                    } else if (ieq(arg_name, "PATH")) {
                        flatten->path = value;
                    } else if (ieq(arg_name, "OUTER")) {
                        flatten->outer = value;
                    }
                    // Other named arguments (RECURSIVE, MODE) are accepted
                    // syntactically but not modeled - FLATTEN's own default
                    // behavior applies when they're omitted.
                } while (match(TK::COMMA));
                expect(TK::RPAREN);
                if (!flatten->input) {
                    error("FLATTEN(...) requires an INPUT => argument");
                }

                // Optional bare alias (no AS keyword in Snowflake's own examples)
                if (check(TK::IDENTIFIER)) {
                    flatten->alias = advance().text;
                }

                return this->template create_node<LateralJoin>(flatten);
            }

            SQLNode* lateral_expr = nullptr;

            // LATERAL (SELECT ...) or LATERAL function_name(...) or LATERAL UNNEST(...)
            if (match(TK::LPAREN)) {
                if (check(TK::SELECT) || check(TK::WITH)) {
                    lateral_expr = parse_select();
                } else {
                    error("Expected SELECT after LATERAL (");
                }
                expect(TK::RPAREN);
            } else if (check(TK::IDENTIFIER) || check(TK::UNNEST) || check(TK::GENERATE_SERIES)) {
                // LATERAL function_call(...) including table-valued functions that are keywords
                lateral_expr = parse_expression();
            }

            // Optional alias
            std::string_view alias = "";
            if (match(TK::AS)) {
                if (check(TK::LPAREN) || check(TK::RPAREN) || check(TK::COMMA) ||
                    check(TK::SEMICOLON) || check(TK::EOF_TOKEN)) {
                    error("Expected alias after AS");
                }
                alias = advance().text;
            } else if (check(TK::IDENTIFIER)) {
                // Check if this is actually an alias or a SQL keyword
                std::string_view next_word = current().text;
                if (next_word != "WHERE" && next_word != "ORDER" && next_word != "GROUP" &&
                    next_word != "HAVING" && next_word != "LIMIT" && next_word != "UNION" &&
                    next_word != "INTERSECT" && next_word != "EXCEPT" && next_word != "JOIN" &&
                    next_word != "INNER" && next_word != "LEFT" && next_word != "RIGHT" &&
                    next_word != "FULL" && next_word != "CROSS" && next_word != "LATERAL" &&
                    next_word != "WINDOW" && next_word != "window") {
                    alias = advance().text;
                }
            }

            // Handle column list after alias: alias(col1, col2, ...)
            if (match(TK::LPAREN)) {
                // Skip the column list for now
                int depth = 1;
                while (depth > 0 && !check(TK::EOF_TOKEN)) {
                    if (match(TK::LPAREN))
                        depth++;
                    else if (match(TK::RPAREN))
                        depth--;
                    else
                        (void)advance();
                }
            }

            auto lateral_node = this->template create_node<LateralJoin>(lateral_expr);
            if (!alias.empty()) {
                // Wrap in a table-like structure with alias
                // For now, we'll just create a LateralJoin node
                // The generator will need to handle the alias
            }
            return lateral_node;
        }

        // Subquery: (SELECT ...) AS alias
        if (match(TK::LPAREN)) {
            if (check(TK::SELECT) || check(TK::WITH)) {
                auto select = parse_select();
                expect(TK::RPAREN);

                // Check for optional alias after subquery
                std::string_view alias;
                if (match(TK::AS)) {
                    if (check(TK::LPAREN) || check(TK::RPAREN) || check(TK::COMMA) ||
                        check(TK::SEMICOLON) || check(TK::EOF_TOKEN)) {
                        error("Expected alias after AS");
                    }
                    alias = advance().text;
                } else if (check(TK::IDENTIFIER)) {
                    // Alias without AS keyword
                    alias = advance().text;
                }

                return this->template create_node<SubqueryExpr>(select, alias);
            }

            // VALUES as a table source: FROM (VALUES (1, 'a'), (2, 'b')) AS v(id, name)
            if (check(TK::VALUES)) {
                (void)advance();
                auto* values = this->template create_node<ValuesClause>();
                do {
                    expect(TK::LPAREN);
                    std::vector<SQLNode*> row;
                    do {
                        row.push_back(parse_expression());
                    } while (match(TK::COMMA));
                    expect(TK::RPAREN);
                    values->rows.push_back(std::move(row));
                } while (match(TK::COMMA));
                expect(TK::RPAREN);

                if (match(TK::AS)) {
                    if (check(TK::LPAREN) || check(TK::RPAREN) || check(TK::COMMA) ||
                        check(TK::SEMICOLON) || check(TK::EOF_TOKEN)) {
                        error("Expected alias after AS");
                    }
                    values->alias = advance().text;
                } else if (check(TK::IDENTIFIER)) {
                    values->alias = advance().text;
                }

                if (match(TK::LPAREN)) {
                    do {
                        if (!check(TK::IDENTIFIER)) {
                            error("Expected column name in VALUES column list");
                        }
                        values->columns.push_back(advance().text);
                    } while (match(TK::COMMA));
                    expect(TK::RPAREN);
                }

                return values;
            }

            error("Expected SELECT subquery after '('");
        }

        // Check if this is a table-valued function: function_name(...)
        // by looking ahead for IDENTIFIER( or function_keyword(
        // Many table-valued functions like generate_series, unnest are keywords
        bool is_function_call =
            (pos_ + 1 < tokens_.size() && tokens_[pos_ + 1].type == TK::LPAREN) &&
            (check(TK::IDENTIFIER) || check(TK::GENERATE_SERIES) || check(TK::UNNEST));

        if (is_function_call) {
            // This is a function call in FROM clause (table-valued function)
            auto func_expr = parse_expression(); // This will parse the function call

            // Optional alias (but note: aliases with column lists like "t(x, y)" need special
            // handling)
            std::string_view alias = "";
            if (match(TK::AS)) {
                if (check(TK::IDENTIFIER)) {
                    alias = advance().text;
                }
            } else if (check(TK::IDENTIFIER)) {
                // Alias without AS
                std::string_view next_word = current().text;
                // Make sure this isn't a keyword
                if (next_word != "WHERE" && next_word != "ORDER" && next_word != "GROUP" &&
                    next_word != "HAVING" && next_word != "LIMIT" && next_word != "UNION" &&
                    next_word != "INTERSECT" && next_word != "EXCEPT" && next_word != "JOIN" &&
                    next_word != "INNER" && next_word != "LEFT" && next_word != "RIGHT" &&
                    next_word != "FULL" && next_word != "CROSS" && next_word != "WINDOW" &&
                    next_word != "window") {
                    alias = advance().text;

                    // Check for column list after alias: alias(col1, col2, ...)
                    if (match(TK::LPAREN)) {
                        // Skip the column list for now
                        int depth = 1;
                        while (depth > 0 && !check(TK::EOF_TOKEN)) {
                            if (match(TK::LPAREN))
                                depth++;
                            else if (match(TK::RPAREN))
                                depth--;
                            else
                                (void)advance();
                        }
                    }
                }
            }

            // Return the function call directly - it's a table-valued function
            // The generator will handle outputting it correctly
            return func_expr;
        }

        // Regular table reference
        auto table = parse_table_ref();

        // SQL:2011 system-versioned temporal table clause (T-SQL / MariaDB /
        // Azure Synapse): FOR SYSTEM_TIME AS OF <ts> | FROM <a> TO <b> |
        // BETWEEN <a> AND <b> | CONTAINED IN (<a>, <b>) | ALL. It comes
        // directly after the table name, before any alias. SYSTEM_TIME/OF/TO
        // /CONTAINED are soft keywords (matched by identifier text).
        if (check(TK::FOR) && peek(1).type == TK::IDENTIFIER && ieq(peek(1).text, "SYSTEM_TIME")) {
            (void)advance(); // FOR
            (void)advance(); // SYSTEM_TIME
            if (match(TK::AS)) {
                // OF is a reserved token (shared with INSTEAD OF triggers), not a soft keyword.
                if (!check(TK::OF)) {
                    error("Expected OF after FOR SYSTEM_TIME AS");
                }
                (void)advance(); // OF
                table->temporal_kind = TemporalKind::AS_OF;
                table->temporal_arg1 = parse_expression();
            } else if (match(TK::FROM)) {
                table->temporal_kind = TemporalKind::FROM_TO;
                table->temporal_arg1 = parse_expression();
                if (!(check(TK::IDENTIFIER) && ieq(current().text, "TO"))) {
                    error("Expected TO in FOR SYSTEM_TIME FROM ... TO ...");
                }
                (void)advance(); // TO
                table->temporal_arg2 = parse_expression();
            } else if (match(TK::BETWEEN)) {
                // Bounds parse above comparison precedence so the AND
                // separating them isn't absorbed as boolean AND (same trick
                // as the BETWEEN expression form - see parse_between_rest).
                table->temporal_kind = TemporalKind::BETWEEN_AND;
                table->temporal_arg1 = parse_expression(kComparisonOperandPrecedence);
                expect(TK::AND);
                table->temporal_arg2 = parse_expression(kComparisonOperandPrecedence);
            } else if (check(TK::IDENTIFIER) && ieq(current().text, "CONTAINED")) {
                (void)advance(); // CONTAINED
                expect(TK::IN);
                expect(TK::LPAREN);
                table->temporal_kind = TemporalKind::CONTAINED_IN;
                table->temporal_arg1 = parse_expression();
                expect(TK::COMMA);
                table->temporal_arg2 = parse_expression();
                expect(TK::RPAREN);
            } else if (match(TK::ALL)) {
                table->temporal_kind = TemporalKind::ALL;
            } else {
                error("Expected AS OF, FROM ... TO ..., BETWEEN ... AND ..., "
                      "CONTAINED IN (...), or ALL after FOR SYSTEM_TIME");
            }
        }

        // Check for optional alias: table_name AS alias or table_name alias
        if (match(TK::AS)) {
            if (check(TK::LPAREN) || check(TK::RPAREN) || check(TK::COMMA) ||
                check(TK::SEMICOLON) || check(TK::EOF_TOKEN)) {
                error("Expected alias after AS");
            }
            table->alias = advance().text;
        } else if (check(TK::IDENTIFIER)) {
            // Check if this identifier is actually an alias (not a keyword like TABLESAMPLE or
            // JOIN)
            std::string_view next_word = current().text;
            // Oracle hierarchical clause: START WITH is not an alias (START
            // lexes as an identifier). Only the two-token form is excluded,
            // so a table alias literally named "start" still works.
            const bool is_start_with =
                (next_word == "START" || next_word == "start") && peek(1).type == TK::WITH;
            if (!is_start_with && next_word != "TABLESAMPLE" && next_word != "tablesample" &&
                next_word != "JOIN" && next_word != "INNER" && next_word != "LEFT" &&
                next_word != "RIGHT" && next_word != "FULL" && next_word != "CROSS" &&
                next_word != "WHERE" && next_word != "ORDER" && next_word != "GROUP" &&
                next_word != "HAVING" && next_word != "LIMIT" && next_word != "OFFSET" &&
                next_word != "UNION" && next_word != "INTERSECT" && next_word != "EXCEPT" &&
                next_word != "WINDOW" && next_word != "window") {
                // This is an alias without AS
                table->alias = advance().text;
            }
        }

        // TABLESAMPLE? (a reserved keyword token, not a soft keyword)
        // Syntax: TABLESAMPLE [BERNOULLI|SYSTEM] (percent) [REPEATABLE (seed)]
        // - the sampling method (if present) comes *before* the parenthesized
        // percentage, not inside it.
        if (check(TK::TABLESAMPLE)) {
            (void)advance();

            // Method: BERNOULLI or SYSTEM (optional; defaults to BERNOULLI)
            SampleMethod method = SampleMethod::BERNOULLI;
            if (check(TK::IDENTIFIER)) {
                std::string_view method_str = current().text;
                if (method_str == "SYSTEM" || method_str == "system") {
                    method = SampleMethod::SYSTEM;
                    (void)advance();
                } else if (method_str == "BERNOULLI" || method_str == "bernoulli") {
                    method = SampleMethod::BERNOULLI;
                    (void)advance();
                }
            }

            expect(TK::LPAREN);
            // Percentage
            auto percent = parse_expression();
            expect(TK::RPAREN);

            auto* sample = this->template create_node<Tablesample>(table, method, percent);

            // Optional REPEATABLE(seed) - REPEATABLE is a reserved keyword
            // token (shared with transaction isolation levels), not a soft
            // keyword.
            if (check(TK::REPEATABLE)) {
                (void)advance();
                expect(TK::LPAREN);
                sample->seed = parse_expression();
                expect(TK::RPAREN);
            }

            return sample;
        }

        return table;
    }

    /// Parse set operation chain (UNION, INTERSECT, EXCEPT).
    /// Set operations are left-associative: a EXCEPT b EXCEPT c must parse
    /// as (a EXCEPT b) EXCEPT c, so each right operand is a plain SELECT
    /// (parse_select_body) and the accumulated result becomes the new left.
    SQLNode* parse_set_operation(SelectStmt* first) {
        SQLNode* left = first;

        while (check(TK::UNION) || check(TK::INTERSECT) || check(TK::EXCEPT)) {
            if (match(TK::UNION)) {
                bool all = match(TK::ALL);
                SelectStmt* right = parse_select_body();
                left = this->template create_node<UnionStmt>(left, right, all);
            } else if (match(TK::INTERSECT)) {
                bool all = match(TK::ALL);
                SelectStmt* right = parse_select_body();
                left = this->template create_node<IntersectStmt>(left, right, all);
            } else {
                expect(TK::EXCEPT);
                bool all = match(TK::ALL);
                SelectStmt* right = parse_select_body();
                left = this->template create_node<ExceptStmt>(left, right, all);
            }
        }

        return left;
    }

    /// Parse INSERT statement
    InsertStmt* parse_insert() {
        auto stmt = this->template create_node<InsertStmt>();
        expect(TK::INSERT);
        expect(TK::INTO);

        // Table name
        stmt->table = parse_table_ref();

        // Optional column list: (col1, col2, ...)
        if (match(TK::LPAREN)) {
            do {
                if (check(TK::IDENTIFIER)) {
                    stmt->columns.push_back(advance().text); // Store string_view directly
                }
            } while (match(TK::COMMA));
            expect(TK::RPAREN);
        }

        // T-SQL OUTPUT clause: between the column list and VALUES/SELECT
        if (check(TK::OUTPUT)) {
            stmt->output = parse_output_clause();
        }

        // VALUES or SELECT
        if (check(TK::SELECT) || check(TK::WITH)) {
            stmt->select_query = parse_select();
        } else {
            expect(TK::VALUES);
            // Parse value rows: VALUES (val1, val2), (val3, val4), ...
            do {
                expect(TK::LPAREN);
                std::vector<SQLNode*> row;
                do {
                    row.push_back(parse_expression());
                } while (match(TK::COMMA));
                expect(TK::RPAREN);
                stmt->values.push_back(row);
            } while (match(TK::COMMA));
        }

        // PostgreSQL upsert: ON CONFLICT [(col, ...)] DO NOTHING
        // / DO UPDATE SET col = expr, ... [WHERE cond]
        if (check(TK::ON) && peek(1).type == TK::IDENTIFIER &&
            (peek(1).text == "CONFLICT" || peek(1).text == "conflict")) {
            (void)advance(); // ON
            (void)advance(); // CONFLICT
            auto* on_conflict = this->template create_node<OnConflictClause>();

            if (match(TK::LPAREN)) {
                do {
                    if (!check(TK::IDENTIFIER)) {
                        error("Expected column name in ON CONFLICT target");
                    }
                    on_conflict->conflict_columns.push_back(advance().text);
                } while (match(TK::COMMA));
                expect(TK::RPAREN);
            }

            expect(TK::DO);
            if (check_soft_keyword("NOTHING", "nothing")) {
                (void)advance();
                on_conflict->do_nothing = true;
            } else {
                expect(TK::UPDATE);
                expect(TK::SET);
                do {
                    if (!check(TK::IDENTIFIER)) {
                        error("Expected column name in ON CONFLICT DO UPDATE SET");
                    }
                    auto col = advance().text;
                    expect(TK::EQ);
                    auto val = parse_expression();
                    on_conflict->update_assignments.push_back({col, val});
                } while (match(TK::COMMA));

                if (match(TK::WHERE)) {
                    on_conflict->where = parse_expression();
                }
            }

            stmt->on_conflict = on_conflict;
        }

        // MySQL upsert: ON DUPLICATE KEY UPDATE col = expr, ...
        if (check(TK::ON) && peek(1).type == TK::DUPLICATE) {
            (void)advance(); // ON
            (void)advance(); // DUPLICATE
            expect(TK::KEY);
            expect(TK::UPDATE);
            auto* on_dup = this->template create_node<OnDuplicateKeyClause>();
            do {
                if (!check(TK::IDENTIFIER)) {
                    error("Expected column name in ON DUPLICATE KEY UPDATE");
                }
                auto col = advance().text;
                expect(TK::EQ);
                auto val = parse_expression();
                on_dup->update_assignments.push_back({col, val});
            } while (match(TK::COMMA));
            stmt->on_duplicate_key = on_dup;
        }

        // PostgreSQL RETURNING clause (maps onto the same OutputClause AST)
        if (check(TK::RETURNING)) {
            stmt->output = parse_returning_clause();
        }

        return stmt;
    }

    /// Parse UPDATE statement
    UpdateStmt* parse_update() {
        auto stmt = this->template create_node<UpdateStmt>();
        expect(TK::UPDATE);

        // Table name
        stmt->table = parse_table_ref();

        // SET clause
        expect(TK::SET);
        do {
            if (!check(TK::IDENTIFIER)) {
                error("Expected column name in SET clause");
            }
            std::string_view column = advance().text; // Store string_view directly
            expect(TK::EQ);
            SQLNode* value = parse_expression();
            stmt->assignments.push_back({column, value});
        } while (match(TK::COMMA));

        // T-SQL OUTPUT clause: after SET, before FROM/WHERE
        if (check(TK::OUTPUT)) {
            stmt->output = parse_output_clause();
        }

        // Optional FROM clause (PostgreSQL extension)
        if (match(TK::FROM)) {
            stmt->from = parse_from_clause();
        }

        // WHERE clause
        if (match(TK::WHERE)) {
            stmt->where = parse_expression();
        }

        // PostgreSQL RETURNING clause (maps onto the same OutputClause AST)
        if (check(TK::RETURNING)) {
            stmt->output = parse_returning_clause();
        }

        return stmt;
    }

    /// Parse DELETE statement
    DeleteStmt* parse_delete() {
        auto stmt = this->template create_node<DeleteStmt>();
        expect(TK::DELETE);
        expect(TK::FROM);

        // Table name
        stmt->table = parse_table_ref();

        // T-SQL OUTPUT clause: after the target, before USING/WHERE
        if (check(TK::OUTPUT)) {
            stmt->output = parse_output_clause();
        }

        // Optional USING clause (PostgreSQL)
        if (match(TK::USING)) {
            stmt->using_clause = parse_from_clause();
        }

        // WHERE clause
        if (match(TK::WHERE)) {
            stmt->where = parse_expression();
        }

        // PostgreSQL RETURNING clause (maps onto the same OutputClause AST)
        if (check(TK::RETURNING)) {
            stmt->output = parse_returning_clause();
        }

        return stmt;
    }

    /// Parse a T-SQL OUTPUT clause: OUTPUT item, item, ...
    OutputClause* parse_output_clause() {
        expect(TK::OUTPUT);
        auto* clause = this->template create_node<OutputClause>();
        do {
            clause->items.push_back(parse_output_item());
        } while (match(TK::COMMA));
        return clause;
    }

    /// Parse one OUTPUT item: INSERTED.col / DELETED.col / INSERTED.* /
    /// DELETED.* (optionally aliased), or a plain expression item.
    /// INSERTED/DELETED qualifiers are stored canonically in uppercase.
    SQLNode* parse_output_item() {
        if (check(TK::INSERTED) || check(TK::DELETED)) {
            std::string_view qualifier = check(TK::INSERTED) ? "INSERTED" : "DELETED";
            (void)advance();
            expect(TK::DOT);
            if (match(TK::STAR)) {
                return this->template create_node<Star>(qualifier);
            }
            if (check(TK::LPAREN) || check(TK::RPAREN) || check(TK::COMMA) ||
                check(TK::SEMICOLON) || check(TK::EOF_TOKEN)) {
                error("Expected column name after INSERTED./DELETED. in OUTPUT clause");
            }
            SQLNode* col = this->template create_node<Column>(qualifier, advance().text);
            if (match(TK::AS)) {
                if (check(TK::LPAREN) || check(TK::RPAREN) || check(TK::COMMA) ||
                    check(TK::SEMICOLON) || check(TK::EOF_TOKEN)) {
                    error("Expected alias after AS");
                }
                return this->template create_node<Alias>(col, advance().text);
            }
            return col;
        }

        // Plain expression item (also covers RETURNING-style items)
        return parse_select_item();
    }

    /// Parse a PostgreSQL RETURNING clause into the shared OutputClause AST
    OutputClause* parse_returning_clause() {
        expect(TK::RETURNING);
        auto* clause = this->template create_node<OutputClause>();
        clause->from_returning = true;
        do {
            clause->items.push_back(parse_select_item());
        } while (match(TK::COMMA));
        return clause;
    }

    /// Parse MERGE statement (simplified)
    MergeStmt* parse_merge() {
        auto stmt = this->template create_node<MergeStmt>();
        expect(TK::MERGE);
        expect(TK::INTO);

        // Target table
        stmt->target = parse_table_ref();

        // Optional table alias (e.g., MERGE INTO target_table t)
        if (check(TK::IDENTIFIER) && !check(TK::USING)) {
            (void)advance(); // Skip alias
        }

        expect(TK::USING);

        // Source (table or subquery)
        stmt->source = parse_table_or_subquery();

        // Optional table alias (e.g., USING source_table s)
        if (check(TK::IDENTIFIER) && !check(TK::ON)) {
            (void)advance(); // Skip alias
        }

        expect(TK::ON);
        stmt->on_condition = parse_expression();

        // WHEN [NOT] MATCHED [BY SOURCE|BY TARGET] [AND cond] THEN
        //   UPDATE SET ... | DELETE | INSERT (...) VALUES (...) | DO NOTHING
        // A MERGE commonly has several WHEN clauses; each is collected
        // in order so none are silently dropped.
        while (check(TK::WHEN)) {
            (void)advance();

            MergeWhenClause clause;
            if (match(TK::MATCHED)) {
                clause.match_kind = MergeMatchKind::MATCHED;
            } else if (match(TK::NOT)) {
                expect(TK::MATCHED);
                // T-SQL/Azure Synapse: WHEN NOT MATCHED BY SOURCE (fires for
                // target rows with no matching source row) vs the ANSI
                // default WHEN NOT MATCHED [BY TARGET] (fires for source
                // rows with no matching target row).
                if (check(TK::BY)) {
                    (void)advance(); // BY
                    if (check(TK::IDENTIFIER) && ieq(current().text, "SOURCE")) {
                        (void)advance();
                        clause.match_kind = MergeMatchKind::NOT_MATCHED_BY_SOURCE;
                    } else if (check(TK::IDENTIFIER) && ieq(current().text, "TARGET")) {
                        (void)advance();
                        clause.match_kind = MergeMatchKind::NOT_MATCHED;
                    } else {
                        error("Expected SOURCE or TARGET after WHEN NOT MATCHED BY");
                    }
                } else {
                    clause.match_kind = MergeMatchKind::NOT_MATCHED;
                }
            } else {
                error("Expected MATCHED or NOT MATCHED after WHEN");
            }

            // Optional extra condition: WHEN MATCHED AND <cond> THEN ...
            if (match(TK::AND)) {
                clause.extra_condition = parse_expression();
            }

            expect(TK::THEN);

            const bool matched = (clause.match_kind == MergeMatchKind::MATCHED);

            if (check(TK::UPDATE)) {
                (void)advance();
                expect(TK::SET);
                clause.action = MergeActionKind::UPDATE;
                do {
                    if (!check(TK::IDENTIFIER)) {
                        error("Expected column name");
                    }
                    std::string_view col = advance().text;

                    // Handle qualified column name: table.column (e.g., t.value)
                    if (match(TK::DOT)) {
                        // Allow keywords as column names
                        if (check(TK::LPAREN) || check(TK::RPAREN) || check(TK::COMMA) ||
                            check(TK::SEMICOLON) || check(TK::EOF_TOKEN)) {
                            error("Expected column name after '.'");
                        }
                        col = advance().text; // Use the column name, discard table qualifier
                    }

                    expect(TK::EQ);
                    auto val = parse_expression();
                    clause.update_assignments.push_back({col, val});
                } while (match(TK::COMMA));
            } else if (check(TK::DELETE)) {
                (void)advance();
                clause.action = MergeActionKind::DELETE_ACTION;
            } else if (check(TK::INSERT) && !matched) {
                (void)advance();
                clause.action = MergeActionKind::INSERT;
                // INSERT (columns) VALUES (values)
                if (match(TK::LPAREN)) {
                    do {
                        if (check(TK::IDENTIFIER)) {
                            clause.insert_columns.push_back(advance().text);
                        }
                    } while (match(TK::COMMA));
                    expect(TK::RPAREN);
                }

                expect(TK::VALUES);
                expect(TK::LPAREN);
                do {
                    clause.insert_values.push_back(parse_expression());
                } while (match(TK::COMMA));
                expect(TK::RPAREN);
            } else {
                error("Expected UPDATE, DELETE, or INSERT after WHEN ... THEN");
            }

            stmt->when_clauses.push_back(std::move(clause));
        }

        return stmt;
    }

    /// Parse TRUNCATE statement
    TruncateStmt* parse_truncate() {
        auto stmt = this->template create_node<TruncateStmt>();
        expect(TK::TRUNCATE);
        (void)match(TK::TABLE); // TABLE keyword is optional

        stmt->table = parse_table_ref();

        return stmt;
    }

    /// Parse CREATE statement (dispatch to specific type)
    SQLNode* parse_create_statement() {
        expect(TK::CREATE);

        // OR REPLACE?
        bool or_replace = false;
        if (match(TK::OR)) {
            // Handle both REPLACE and REPLACE_KW tokens
            if (!match(TK::REPLACE) && !match(TK::REPLACE_KW)) {
                error("Expected REPLACE after OR");
            }
            or_replace = true;
        }

        // TEMPORARY / TEMP / GLOBAL TEMPORARY?
        bool is_temporary = false;
        bool is_global = false;
        if (check(TK::IDENTIFIER) && (current().text == "GLOBAL" || current().text == "global")) {
            (void)advance();
            is_global = true;
        }
        if (check(TK::TEMPORARY) || check(TK::TEMP)) {
            (void)advance();
            is_temporary = true;
        }

        if (check(TK::TABLE)) {
            return parse_create_table(is_temporary, is_global);
        } else if (check(TK::VIEW)) {
            return parse_create_view(or_replace);
        } else if (check(TK::INDEX)) {
            return parse_create_index();
        } else if (check(TK::SCHEMA) || check(TK::DATABASE)) {
            return parse_create_schema();
        } else if (check(TK::PROCEDURE) || check(TK::PROCEDURE_KW) || check(TK::FUNCTION)) {
            return parse_create_procedure(or_replace);
        } else if (check(TK::TRIGGER)) {
            return parse_create_trigger();
        } else if (check(TK::IDENTIFIER) &&
                   (current().text == "MODEL" || current().text == "model")) {
            return parse_create_model(or_replace);
        } else if (check(TK::PROJECTION)) {
            return parse_create_projection();
        } else if (check(TK::IDENTIFIER) &&
                   (current().text == "REFLECTION" || current().text == "reflection")) {
            return parse_create_reflection();
        } else if (check(TK::IDENTIFIER) && ieq(current().text, "SEQUENCE")) {
            return parse_create_sequence();
        }

        error("Expected TABLE, VIEW, INDEX, SCHEMA, PROCEDURE, FUNCTION, TRIGGER, MODEL, "
              "PROJECTION, REFLECTION, or SEQUENCE after CREATE");
        return nullptr;
    }

    /// Parse CREATE SEQUENCE name [START WITH n] [INCREMENT BY n]
    ///   [{MINVALUE n | NO MINVALUE}] [{MAXVALUE n | NO MAXVALUE}]
    ///   [{CYCLE | NO CYCLE}] [CACHE n]
    CreateSequenceStmt* parse_create_sequence() {
        auto stmt = this->template create_node<CreateSequenceStmt>();
        (void)advance(); // SEQUENCE (soft keyword)

        if (match(TK::IF_KW) || match(TK::IF)) {
            expect(TK::NOT);
            expect(TK::EXISTS);
            stmt->if_not_exists = true;
        }

        if (!check(TK::IDENTIFIER)) {
            error("Expected sequence name after CREATE SEQUENCE");
        }
        stmt->name = advance().text;

        while (true) {
            if (check(TK::IDENTIFIER) && ieq(current().text, "START")) {
                (void)advance();       // START
                (void)match(TK::WITH); // optional WITH
                stmt->start_with = parse_expression();
            } else if (check(TK::IDENTIFIER) && ieq(current().text, "INCREMENT")) {
                (void)advance();     // INCREMENT
                (void)match(TK::BY); // optional BY
                stmt->increment_by = parse_expression();
            } else if (match(TK::MINVALUE)) {
                stmt->min_value = parse_expression();
            } else if (match(TK::MAXVALUE)) {
                stmt->max_value = parse_expression();
            } else if (check(TK::IDENTIFIER) && ieq(current().text, "NO") &&
                       peek(1).type == TK::MINVALUE) {
                (void)advance(); // NO
                (void)advance(); // MINVALUE
                stmt->no_min_value = true;
            } else if (check(TK::IDENTIFIER) && ieq(current().text, "NO") &&
                       peek(1).type == TK::MAXVALUE) {
                (void)advance(); // NO
                (void)advance(); // MAXVALUE
                stmt->no_max_value = true;
            } else if (check(TK::IDENTIFIER) && ieq(current().text, "NO") &&
                       peek(1).type == TK::IDENTIFIER && ieq(peek(1).text, "CYCLE")) {
                (void)advance(); // NO
                (void)advance(); // CYCLE
                stmt->no_cycle = true;
            } else if (check(TK::IDENTIFIER) && ieq(current().text, "CYCLE")) {
                (void)advance(); // CYCLE
                stmt->cycle = true;
            } else if (check(TK::IDENTIFIER) && ieq(current().text, "CACHE")) {
                (void)advance(); // CACHE
                stmt->cache = parse_expression();
            } else {
                break;
            }
        }

        return stmt;
    }

    /// Parse CREATE TABLE (simplified for now)
    CreateTableStmt* parse_create_table(bool is_temporary = false, bool is_global = false) {
        auto stmt = this->template create_node<CreateTableStmt>();
        expect(TK::TABLE);

        // Set temporary flag
        stmt->temporary = is_temporary;

        // IF NOT EXISTS?
        if (match(TK::IF_KW) || match(TK::IF)) {
            expect(TK::NOT);
            expect(TK::EXISTS);
            stmt->if_not_exists = true;
        }

        // Table name - must be present (including SQL Server #temp syntax)
        // Be permissive for SQL Server temp tables with # or ## prefix
        bool has_hash_prefix = check(TK::HASH);
        if (!has_hash_prefix && !check(TK::IDENTIFIER) && !check(TK::TABLE) && !check(TK::DUAL) &&
            !check(TK::TEMP)) {
            error("Expected table name after CREATE TABLE");
        }
        stmt->table = parse_table_ref();

        // Check for AS SELECT (CREATE TABLE ... AS SELECT ...)
        if (match(TK::AS)) {
            stmt->as_select = parse_select();
            return stmt;
        }

        // Column definitions and table-level constraints:
        // (col1 type [constraints], ..., PRIMARY KEY (...), FOREIGN KEY (...), ...)
        expect(TK::LPAREN);
        if (!check(TK::RPAREN)) {
            do {
                if (check_table_constraint_start()) {
                    stmt->constraints.push_back(parse_table_constraint());
                } else {
                    stmt->columns.push_back(parse_column_def());
                }
            } while (match(TK::COMMA));
        }
        expect(TK::RPAREN);

        // Trailing dialect-specific table options (ENGINE=InnoDB,
        // AUTO_INCREMENT=n, DEFAULT CHARSET=x, COMMENT='...', DISTSTYLE KEY,
        // DISTKEY(col), SORTKEY(col), DISTRIBUTED BY (...), PARTITION BY
        // ..., TABLESPACE x, ...) are modeled as an ordered list of (name,
        // value) pairs and regenerated verbatim, rather than being consumed
        // and discarded.
        while (!check(TK::SEMICOLON) && !is_eof()) {
            if (match(TK::COMMA))
                continue; // Some dialects comma-separate options
            stmt->table_options.push_back(parse_table_option());
        }

        return stmt;
    }

    /// Is the current token the start of a recognized trailing table option
    /// keyword? Used both to detect the start of the next option and, when
    /// scanning a bare (no '=') option's value, to know where that value
    /// ends without an explicit separator (`DISTSTYLE KEY DISTKEY(id)` is
    /// two options, not one).
    [[nodiscard]] bool at_table_option_start() const noexcept {
        if (check(TK::ENGINE) || check(TK::AUTO_INCREMENT) || check(TK::CHARSET) ||
            check(TK::COLLATE) || check(TK::DISTSTYLE) || check(TK::DISTKEY) ||
            check(TK::SORTKEY) || check(TK::DISTRIBUTED) || check(TK::PARTITION) ||
            check(TK::TABLESPACE) || check(TK::DEFAULT)) {
            return true;
        }
        if (check(TK::IDENTIFIER)) {
            std::string_view t = current().text;
            return ieq(t, "COMMENT") || ieq(t, "ROW_FORMAT") || ieq(t, "COMPRESSION") ||
                   ieq(t, "CHARACTER");
        }
        return false;
    }

    /// Parse one trailing CREATE TABLE option: a (possibly multi-word) name,
    /// optionally followed by `=value`, or a bare `name value` pair.
    TableOption parse_table_option() {
        TableOption opt;

        size_t name_start = current().start;
        size_t name_end = current().end;
        (void)advance(); // First name word

        // Recognized two-word name prefixes: DEFAULT CHARSET/CHARACTER,
        // CHARACTER SET, DISTRIBUTED BY, PARTITION BY.
        if (check(TK::CHARSET) || (check(TK::IDENTIFIER) && ieq(current().text, "CHARACTER")) ||
            check(TK::BY)) {
            name_end = current().end;
            (void)advance();
            // DEFAULT CHARACTER SET (three words)
            if (check(TK::SET)) {
                name_end = current().end;
                (void)advance();
            }
        } else if (check(TK::SET) &&
                   ieq(source_.substr(name_start, name_end - name_start), "CHARACTER")) {
            name_end = current().end;
            (void)advance();
        }

        opt.name = source_.substr(name_start, name_end - name_start);

        if (match(TK::EQ)) {
            opt.has_equals = true;
            size_t val_start = current().start;
            size_t val_end = val_start;
            if (check(TK::LPAREN)) {
                int depth = 0;
                do {
                    if (check(TK::LPAREN))
                        depth++;
                    else if (check(TK::RPAREN))
                        depth--;
                    val_end = current().end;
                    (void)advance();
                } while (depth > 0 && !is_eof());
            } else if (!check(TK::SEMICOLON) && !is_eof()) {
                val_end = current().end;
                (void)advance();
            }
            opt.value = source_.substr(val_start, val_end - val_start);
        } else {
            // Bare `name value` form: capture tokens (paren-depth aware)
            // until a top-level comma/semicolon/EOF, or the start of the
            // next recognized option keyword.
            size_t val_start = current().start;
            size_t val_end = val_start;
            int depth = 0;
            bool first = true;
            while (!is_eof() && !check(TK::SEMICOLON)) {
                if (depth == 0 && check(TK::COMMA))
                    break;
                if (depth == 0 && !first && at_table_option_start())
                    break;
                if (check(TK::LPAREN))
                    depth++;
                else if (check(TK::RPAREN))
                    depth--;
                val_end = current().end;
                (void)advance();
                first = false;
            }
            opt.value = source_.substr(val_start, val_end - val_start);
        }

        return opt;
    }

    /// Check whether the current token begins a table-level constraint
    [[nodiscard]] bool check_table_constraint_start() const noexcept {
        return check(TK::CONSTRAINT) || check(TK::PRIMARY) || check(TK::FOREIGN) ||
               check(TK::CHECK) || (check(TK::UNIQUE) && peek(1).type == TK::LPAREN);
    }

    /// Parse a table-level constraint inside CREATE TABLE:
    /// [CONSTRAINT name] PRIMARY KEY (...) | FOREIGN KEY (...) REFERENCES tbl (...)
    /// | UNIQUE (...) | CHECK (expr)
    TableConstraint* parse_table_constraint() {
        auto constraint = this->template create_node<TableConstraint>();

        // Optional CONSTRAINT name prefix
        if (match(TK::CONSTRAINT)) {
            if (check(TK::IDENTIFIER)) {
                constraint->name = advance().text;
            }
        }

        if (match(TK::PRIMARY)) {
            expect(TK::KEY);
            constraint->constraint_type = TableConstraint::Type::PRIMARY_KEY;
            parse_identifier_list_into(constraint->columns);
        } else if (match(TK::FOREIGN)) {
            expect(TK::KEY);
            constraint->constraint_type = TableConstraint::Type::FOREIGN_KEY;
            parse_identifier_list_into(constraint->columns);
            expect(TK::REFERENCES);
            constraint->ref_table = parse_table_ref();
            if (check(TK::LPAREN)) {
                parse_identifier_list_into(constraint->ref_columns);
            }
            parse_foreign_key_actions(constraint->on_delete_action, constraint->on_update_action);
        } else if (match(TK::UNIQUE)) {
            constraint->constraint_type = TableConstraint::Type::UNIQUE;
            parse_identifier_list_into(constraint->columns);
        } else if (match(TK::CHECK)) {
            constraint->constraint_type = TableConstraint::Type::CHECK;
            expect(TK::LPAREN);
            constraint->check_expr = parse_expression();
            expect(TK::RPAREN);
        } else {
            error("Expected PRIMARY KEY, FOREIGN KEY, UNIQUE, or CHECK constraint");
        }

        return constraint;
    }

    /// Parse a parenthesized identifier list into `out`: (col1, col2, ...)
    void parse_identifier_list_into(std::vector<std::string_view>& out) {
        expect(TK::LPAREN);
        do {
            if (check(TK::RPAREN))
                break;
            out.push_back(advance().text);
        } while (match(TK::COMMA));
        expect(TK::RPAREN);
    }

    /// Parse optional ON DELETE / ON UPDATE referential actions
    void parse_foreign_key_actions(std::string_view& on_delete, std::string_view& on_update) {
        while (check(TK::ON) && (peek(1).type == TK::DELETE || peek(1).type == TK::UPDATE)) {
            (void)advance(); // ON
            const bool is_delete = check(TK::DELETE);
            (void)advance(); // DELETE / UPDATE

            // Action: CASCADE | RESTRICT | SET NULL | SET DEFAULT | NO ACTION.
            // Capture the action text as a source span.
            size_t action_start = current().start;
            size_t action_end = action_start;
            if (match(TK::SET)) {
                action_end = current().end;
                if (!match(TK::NULL_KW))
                    (void)match(TK::DEFAULT);
            } else if (check(TK::IDENTIFIER) &&
                       (current().text == "NO" || current().text == "no")) {
                (void)advance(); // NO
                action_end = current().end;
                if (check(TK::IDENTIFIER))
                    (void)advance(); // ACTION
            } else if (check(TK::IDENTIFIER)) {
                action_end = current().end;
                (void)advance(); // CASCADE / RESTRICT
            }

            std::string_view action = source_.substr(action_start, action_end - action_start);
            if (is_delete) {
                on_delete = action;
            } else {
                on_update = action;
            }
        }
    }

    /// Parse a single column definition inside CREATE TABLE:
    /// name type[(params)] [NOT NULL | NULL] [DEFAULT expr] [PRIMARY KEY]
    /// [UNIQUE] [AUTO_INCREMENT] [REFERENCES tbl [(col)]] [CHECK (expr)]
    ColumnDef* parse_column_def() {
        auto col = this->template create_node<ColumnDef>();

        if (check(TK::LPAREN) || check(TK::RPAREN) || check(TK::COMMA) || is_eof()) {
            error("Expected column name in CREATE TABLE");
        }
        col->name = advance().text;

        // Type: capture the source span of the type (including parameters
        // like VARCHAR(255) or DECIMAL(10, 2) and dialect modifiers like
        // UNSIGNED or DISTKEY) up to the first recognized constraint keyword,
        // comma, or the closing paren of the column list.
        size_t type_start = current().start;
        size_t type_end = type_start;
        int paren_depth = 0;
        while (!is_eof()) {
            if (paren_depth == 0 &&
                (check(TK::COMMA) || check(TK::NOT) || check(TK::NULL_KW) || check(TK::DEFAULT) ||
                 check(TK::PRIMARY) || check(TK::UNIQUE) || check(TK::REFERENCES) ||
                 check(TK::CHECK) || check(TK::CONSTRAINT) || check(TK::AUTO_INCREMENT))) {
                break;
            }
            if (check(TK::LPAREN)) {
                paren_depth++;
            } else if (check(TK::RPAREN)) {
                if (paren_depth == 0)
                    break; // Closing paren of the column list
                paren_depth--;
            }
            type_end = current().end;
            (void)advance();
        }
        col->type = source_.substr(type_start, type_end - type_start);

        // Column constraints (any order)
        while (true) {
            if (check(TK::NOT) && peek(1).type == TK::NULL_KW) {
                (void)advance();
                (void)advance();
                col->not_null = true;
            } else if (match(TK::NULL_KW)) {
                // Explicit NULL - nullable is the default, nothing to record
            } else if (match(TK::DEFAULT)) {
                col->default_value = parse_expression();
            } else if (match(TK::PRIMARY)) {
                expect(TK::KEY);
                col->primary_key = true;
            } else if (match(TK::UNIQUE)) {
                col->unique = true;
            } else if (match(TK::AUTO_INCREMENT)) {
                col->auto_increment = true;
            } else if (match(TK::REFERENCES)) {
                if (check(TK::IDENTIFIER) || check(TK::TABLE)) {
                    col->references_table = advance().text;
                } else {
                    error("Expected table name after REFERENCES");
                }
                if (check(TK::LPAREN)) {
                    parse_identifier_list_into(col->references_columns);
                }
            } else if (match(TK::CHECK)) {
                expect(TK::LPAREN);
                col->check_expr = parse_expression();
                expect(TK::RPAREN);
            } else {
                break;
            }
        }

        // Be permissive with dialect-specific trailing attributes we do not
        // model (e.g. IDENTITY(1,1), COMMENT '...'): skip until the next
        // column or the end of the column list.
        int skip_depth = 0;
        while (!is_eof()) {
            if (skip_depth == 0 && (check(TK::COMMA) || check(TK::RPAREN)))
                break;
            if (check(TK::LPAREN))
                skip_depth++;
            else if (check(TK::RPAREN))
                skip_depth--;
            (void)advance();
        }

        return col;
    }

    /// Parse CREATE VIEW
    CreateViewStmt* parse_create_view(bool or_replace) {
        auto stmt = this->template create_node<CreateViewStmt>();
        stmt->or_replace = or_replace;
        expect(TK::VIEW);

        // View name
        if (!check(TK::IDENTIFIER)) {
            error("Expected view name");
        }
        stmt->name = advance().text;

        expect(TK::AS);
        stmt->query = parse_select();

        return stmt;
    }

    /// Parse CREATE INDEX
    CreateIndexStmt* parse_create_index() {
        auto stmt = this->template create_node<CreateIndexStmt>();
        expect(TK::INDEX);

        // Index name
        if (check(TK::IDENTIFIER)) {
            stmt->index_name = advance().text; // Store string_view directly
        }

        expect(TK::ON);
        stmt->table = parse_table_ref();

        // Column list
        expect(TK::LPAREN);
        do {
            if (check(TK::IDENTIFIER)) {
                stmt->columns.push_back(advance().text); // Store string_view directly
            }
        } while (match(TK::COMMA));
        expect(TK::RPAREN);

        return stmt;
    }

    /// Parse CREATE SCHEMA/DATABASE
    CreateSchemaStmt* parse_create_schema() {
        auto stmt = this->template create_node<CreateSchemaStmt>();
        if (match(TK::SCHEMA)) {
            // CREATE SCHEMA
        } else if (match(TK::DATABASE)) {
            // CREATE DATABASE
        }

        // IF NOT EXISTS?
        if (match(TK::IF_KW) || match(TK::IF)) {
            expect(TK::NOT);
            expect(TK::EXISTS);
            stmt->if_not_exists = true;
        }

        // Schema name
        if (check(TK::IDENTIFIER)) {
            stmt->name = advance().text;
        }

        return stmt;
    }

    /// Parse CREATE PROJECTION (Vertica)
    CreateViewStmt* parse_create_projection() {
        auto stmt = this->template create_node<CreateViewStmt>();
        expect(TK::PROJECTION);

        // Projection name
        if (check(TK::IDENTIFIER)) {
            stmt->name = advance().text;
        }

        expect(TK::AS);
        stmt->query = parse_select();

        // Skip SEGMENTED BY and ALL NODES clauses
        while (!check(TK::SEMICOLON) && !is_eof()) {
            (void)advance();
        }

        return stmt;
    }

    /// Parse CREATE REFLECTION (Dremio)
    CreateViewStmt* parse_create_reflection() {
        auto stmt = this->template create_node<CreateViewStmt>();
        if (check(TK::IDENTIFIER) &&
            (current().text == "REFLECTION" || current().text == "reflection")) {
            (void)advance(); // consume REFLECTION
        }

        // Reflection name
        if (check(TK::IDENTIFIER)) {
            stmt->name = advance().text;
        }

        // ON table
        if (match(TK::ON)) {
            // Skip rest - we just need to parse without errors
            while (!check(TK::SEMICOLON) && !is_eof()) {
                (void)advance();
            }
        }

        return stmt;
    }

    /// Parse DROP statement (dispatch to specific type)
    SQLNode* parse_drop_statement() {
        expect(TK::DROP);

        if (check(TK::TABLE)) {
            return parse_drop_table();
        } else if (check(TK::VIEW)) {
            return parse_drop_view();
        } else if (check(TK::INDEX)) {
            return parse_drop_index();
        } else if (check(TK::SCHEMA) || check(TK::DATABASE)) {
            return parse_drop_schema();
        } else if (check(TK::PROCEDURE) || check(TK::PROCEDURE_KW) || check(TK::FUNCTION)) {
            return parse_drop_procedure();
        } else if (check(TK::TRIGGER)) {
            return parse_drop_trigger();
        } else if (check(TK::IDENTIFIER) &&
                   (current().text == "MODEL" || current().text == "model")) {
            return parse_drop_model();
        } else if (check(TK::IDENTIFIER) && ieq(current().text, "SEQUENCE")) {
            return parse_drop_sequence();
        }

        error("Expected TABLE, VIEW, INDEX, SCHEMA, PROCEDURE, FUNCTION, TRIGGER, MODEL, or "
              "SEQUENCE after DROP");
        return nullptr;
    }

    /// Parse DROP SEQUENCE [IF EXISTS] name
    DropSequenceStmt* parse_drop_sequence() {
        auto stmt = this->template create_node<DropSequenceStmt>();
        (void)advance(); // SEQUENCE (soft keyword)

        if (match(TK::IF_KW) || match(TK::IF)) {
            expect(TK::EXISTS);
            stmt->if_exists = true;
        }

        if (!check(TK::IDENTIFIER)) {
            error("Expected sequence name after DROP SEQUENCE");
        }
        stmt->name = advance().text;

        return stmt;
    }

    /// Parse DROP TABLE
    DropTableStmt* parse_drop_table() {
        auto stmt = this->template create_node<DropTableStmt>();
        expect(TK::TABLE);

        // IF EXISTS?
        if (match(TK::IF) || match(TK::IF_KW)) {
            if (match(TK::EXISTS) || match(TK::EXISTS_KW)) {
                stmt->if_exists = true;
            } else {
                error("Expected EXISTS after IF");
            }
        }

        stmt->table = parse_table_ref();

        return stmt;
    }

    /// Parse DROP VIEW
    DropViewStmt* parse_drop_view() {
        auto stmt = this->template create_node<DropViewStmt>();
        expect(TK::VIEW);

        // IF EXISTS?
        if (match(TK::IF_KW) || match(TK::IF)) {
            expect(TK::EXISTS);
            stmt->if_exists = true;
        }

        if (check(TK::IDENTIFIER)) {
            stmt->name = advance().text;
        }

        return stmt;
    }

    /// Parse DROP INDEX
    DropIndexStmt* parse_drop_index() {
        auto stmt = this->template create_node<DropIndexStmt>();
        expect(TK::INDEX);

        // IF EXISTS?
        if (match(TK::IF_KW) || match(TK::IF)) {
            expect(TK::EXISTS);
            stmt->if_exists = true;
        }

        if (check(TK::IDENTIFIER)) {
            stmt->index_name = advance().text; // Store string_view directly
        }

        return stmt;
    }

    /// Parse DROP SCHEMA/DATABASE
    DropSchemaStmt* parse_drop_schema() {
        auto stmt = this->template create_node<DropSchemaStmt>();
        if (match(TK::SCHEMA)) {
            // DROP SCHEMA
        } else if (match(TK::DATABASE)) {
            // DROP DATABASE
        }

        // IF EXISTS?
        if (match(TK::IF_KW) || match(TK::IF)) {
            expect(TK::EXISTS);
            stmt->if_exists = true;
        }

        if (check(TK::IDENTIFIER)) {
            stmt->name = advance().text;
        }

        return stmt;
    }

    /// Parse ALTER TABLE statement
    SQLNode* parse_alter_statement() {
        expect(TK::ALTER);

        if (check(TK::IDENTIFIER) && ieq(current().text, "SEQUENCE")) {
            return parse_alter_sequence();
        }

        auto stmt = this->template create_node<AlterTableStmt>();
        expect(TK::TABLE);

        // Table name
        stmt->table = parse_table_ref();

        // Determine operation type
        if (check(TK::ADD)) {
            (void)advance();
            stmt->operation = AlterOperation::ADD_COLUMN;

            // COLUMN keyword (optional)
            if (check(TK::IDENTIFIER) &&
                (current().text == "COLUMN" || current().text == "column")) {
                (void)advance();
            }

            // Column definition - simplified: just capture name and type
            auto col_def = this->template create_node<ColumnDef>();
            if (check(TK::IDENTIFIER)) {
                col_def->name = advance().text;
            }
            // Type
            if (check(TK::IDENTIFIER)) {
                col_def->type = advance().text;
            }
            // Skip remaining column constraints
            while (!check(TK::SEMICOLON) && !check(TK::COMMA) && !is_eof()) {
                (void)advance();
            }
            stmt->column_def = col_def;

        } else if (check(TK::DROP)) {
            (void)advance();
            stmt->operation = AlterOperation::DROP_COLUMN;

            // COLUMN keyword (optional)
            if (check(TK::IDENTIFIER) &&
                (current().text == "COLUMN" || current().text == "column")) {
                (void)advance();
            }

            // Column name
            if (check(TK::IDENTIFIER)) {
                stmt->old_name = advance().text;
            }

        } else if (check(TK::IDENTIFIER)) {
            std::string_view keyword = current().text;

            if (keyword == "MODIFY" || keyword == "modify" || keyword == "ALTER" ||
                keyword == "alter") {
                (void)advance();
                stmt->operation = AlterOperation::MODIFY_COLUMN;

                // COLUMN keyword (optional)
                if (check(TK::IDENTIFIER) &&
                    (current().text == "COLUMN" || current().text == "column")) {
                    (void)advance();
                }

                // Column definition
                auto col_def = this->template create_node<ColumnDef>();
                if (check(TK::IDENTIFIER)) {
                    col_def->name = advance().text;
                }
                // Type
                if (check(TK::IDENTIFIER)) {
                    col_def->type = advance().text;
                }
                // Skip remaining column constraints
                while (!check(TK::SEMICOLON) && !check(TK::COMMA) && !is_eof()) {
                    (void)advance();
                }
                stmt->column_def = col_def;

            } else if (keyword == "RENAME" || keyword == "rename") {
                (void)advance();

                // COLUMN or TO?
                if (check(TK::IDENTIFIER)) {
                    std::string_view next = current().text;
                    if (next == "COLUMN" || next == "column") {
                        (void)advance();
                        stmt->operation = AlterOperation::RENAME_COLUMN;

                        // Old column name
                        if (check(TK::IDENTIFIER)) {
                            stmt->old_name = advance().text;
                        }

                        // TO keyword
                        if (check(TK::IDENTIFIER) &&
                            (current().text == "TO" || current().text == "to")) {
                            (void)advance();
                        }

                        // New column name
                        if (check(TK::IDENTIFIER)) {
                            stmt->new_name = advance().text;
                        }

                    } else if (next == "TO" || next == "to") {
                        (void)advance();
                        stmt->operation = AlterOperation::RENAME_TABLE;

                        // New table name
                        if (check(TK::IDENTIFIER)) {
                            stmt->new_name = advance().text;
                        }
                    }
                }
            }
        }

        return stmt;
    }

    /// Parse ALTER SEQUENCE name RESTART [WITH n]
    AlterSequenceStmt* parse_alter_sequence() {
        auto stmt = this->template create_node<AlterSequenceStmt>();
        (void)advance(); // SEQUENCE (soft keyword)

        if (!check(TK::IDENTIFIER)) {
            error("Expected sequence name after ALTER SEQUENCE");
        }
        stmt->name = advance().text;

        if (check(TK::IDENTIFIER) && ieq(current().text, "RESTART")) {
            (void)advance(); // RESTART
            stmt->restart = true;
            if (match(TK::WITH)) {
                stmt->restart_with = parse_expression();
            }
        } else {
            error("Expected RESTART after ALTER SEQUENCE name");
        }

        return stmt;
    }

    // ========================================================================
    // Transaction Statement Parsers
    // ========================================================================

    SQLNode* parse_begin() {
        expect(TK::BEGIN);

        // Check if this is a BEGIN...END block with statements or a transaction BEGIN
        // Transaction BEGIN is typically followed by WORK/TRANSACTION or a semicolon/EOF
        if (check(TK::TRANSACTION)) {
            std::string_view type = current().text;
            (void)advance();
            return this->template create_node<BeginStmt>(type);
        }

        if (check(TK::WORK)) {
            std::string_view type = current().text;
            (void)advance();
            return this->template create_node<BeginStmt>(type);
        }

        // Check if immediately followed by semicolon/EOF - standalone transaction BEGIN
        if (check(TK::SEMICOLON) || is_eof()) {
            return this->template create_node<BeginStmt>("");
        }

        // Otherwise, this is a BEGIN...END block with statements (possibly with EXCEPTION handlers)
        std::vector<SQLNode*> statements;

        // Parse statements until EXCEPTION or END
        while (!check(TK::END) && !check(TK::EXCEPTION) && !is_eof()) {
            // Skip semicolons
            if (match(TK::SEMICOLON)) {
                continue;
            }
            statements.push_back(parse_statement());
        }

        // Check if we have EXCEPTION handlers
        if (check(TK::EXCEPTION)) {
            auto exc_block = this->template create_node<ExceptionBlock>();
            exc_block->try_statements = statements;

            expect(TK::EXCEPTION);

            // Parse WHEN handlers
            while (check(TK::WHEN) || check(TK::WHEN_KW)) {
                if (check(TK::WHEN)) {
                    (void)advance();
                } else {
                    expect(TK::WHEN_KW);
                }

                // Exception name (identifier like division_by_zero, others, etc.)
                std::string_view exception_name;
                if (check(TK::IDENTIFIER)) {
                    exception_name = advance().text;
                } else {
                    error("Expected exception name after WHEN");
                }

                expect(TK::THEN);

                // Parse handler statements until next WHEN or END
                std::vector<SQLNode*> handler_stmts;
                while (!check(TK::WHEN) && !check(TK::WHEN_KW) && !check(TK::END) && !is_eof()) {
                    // Skip semicolons
                    if (match(TK::SEMICOLON)) {
                        continue;
                    }
                    handler_stmts.push_back(parse_statement());
                }

                exc_block->handlers.push_back({exception_name, handler_stmts});
            }

            expect(TK::END);
            return exc_block;
        } else {
            // No EXCEPTION handlers - return BeginEndBlock
            auto block = this->template create_node<BeginEndBlock>();
            block->statements = statements;
            expect(TK::END);
            return block;
        }
    }

    CommitStmt* parse_commit() {
        expect(TK::COMMIT);
        return this->template create_node<CommitStmt>();
    }

    RollbackStmt* parse_rollback() {
        auto stmt = this->template create_node<RollbackStmt>();
        expect(TK::ROLLBACK);
        // Check for TO SAVEPOINT (simplified)
        if (check(TK::IDENTIFIER)) {
            std::string_view word = current().text;
            if (word == "TO" || word == "to") {
                (void)advance();
                if (check(TK::IDENTIFIER)) {
                    stmt->savepoint_name = advance().text;
                }
            }
        }
        return stmt;
    }

    SavepointStmt* parse_savepoint() {
        auto stmt = this->template create_node<SavepointStmt>();
        expect(TK::SAVEPOINT);
        if (check(TK::IDENTIFIER)) {
            stmt->name = advance().text;
        }
        return stmt;
    }

    // ========================================================================
    // Utility Statement Parsers
    // ========================================================================

    SetStmt* parse_set() {
        auto stmt = this->template create_node<SetStmt>();
        expect(TK::SET);
        do {
            // T-SQL variables lex as PARAMETER tokens (SET @i = @i + 1)
            if (!check(TK::IDENTIFIER) && !check(TK::PARAMETER)) {
                error("Expected variable name in SET statement");
            }
            std::string_view var = advance().text;
            expect(TK::EQ);
            SQLNode* val = parse_expression();
            stmt->assignments.push_back({var, val});
        } while (match(TK::COMMA));
        return stmt;
    }

    ShowStmt* parse_show() {
        auto stmt = this->template create_node<ShowStmt>();
        expect(TK::SHOW);
        if (check(TK::IDENTIFIER)) {
            stmt->what = advance().text;
        }
        if (check(TK::IDENTIFIER)) {
            stmt->target = advance().text;
        }
        return stmt;
    }

    DescribeStmt* parse_describe() {
        auto stmt = this->template create_node<DescribeStmt>();
        if (!match(TK::DESCRIBE)) {
            expect(TK::DESC);
        }
        if (check(TK::IDENTIFIER)) {
            stmt->target = advance().text;
        }
        return stmt;
    }

    ExplainStmt* parse_explain() {
        auto stmt = this->template create_node<ExplainStmt>();
        expect(TK::EXPLAIN);
        if (match(TK::ANALYZE)) {
            stmt->analyze = true;
        }
        stmt->statement = parse_statement();
        return stmt;
    }

    AnalyzeStmt* parse_analyze() {
        auto stmt = this->template create_node<AnalyzeStmt>();
        expect(TK::ANALYZE);

        // MySQL-specific: LOCAL or NO_WRITE_TO_BINLOG keywords
        if (match(TK::LOCAL)) {
            stmt->local = true;
        } else if (match(TK::NO_WRITE_TO_BINLOG)) {
            stmt->no_write_to_binlog = true;
        }

        // VERBOSE keyword (PostgreSQL)
        if (match(TK::VERBOSE)) {
            stmt->verbose = true;
        }

        // MySQL-specific: TABLE keyword
        if (check(TK::TABLE)) {
            stmt->use_table_keyword = true;
            (void)advance();
        }

        // Parse table list (optional - ANALYZE with no tables analyzes all tables)
        if (check(TK::IDENTIFIER) && !check(TK::SEMICOLON) && !is_eof()) {
            do {
                auto table = parse_table_ref();

                // Check for column specifications: table_name(col1, col2, ...)
                if (match(TK::LPAREN)) {
                    // Column list only supported for single table
                    if (!stmt->tables.empty()) {
                        error("Column list can only be specified for a single table");
                    }

                    // Check for empty column list (should error)
                    if (check(TK::RPAREN)) {
                        error("Empty column list not allowed");
                    }

                    if (!check(TK::RPAREN)) {
                        do {
                            if (check(TK::IDENTIFIER)) {
                                stmt->columns.push_back(advance().text);
                            }
                        } while (match(TK::COMMA));
                    }
                    expect(TK::RPAREN);
                }

                stmt->tables.push_back(table);
            } while (match(TK::COMMA));
        }

        return stmt;
    }

    VacuumStmt* parse_vacuum() {
        auto stmt = this->template create_node<VacuumStmt>();
        expect(TK::VACUUM);

        // Check for parenthesized options: VACUUM (FULL, VERBOSE) users
        if (match(TK::LPAREN)) {
            do {
                std::string_view option;

                // Option names can be keywords (FULL, VERBOSE, ANALYZE) or identifiers (PARALLEL,
                // FREEZE)
                if (check(TK::FULL)) {
                    option = advance().text;
                    stmt->full = true;
                    stmt->paren_options.push_back({option, ""});
                } else if (check(TK::VERBOSE)) {
                    option = advance().text;
                    stmt->verbose = true;
                    stmt->paren_options.push_back({option, ""});
                } else if (check(TK::ANALYZE)) {
                    option = advance().text;
                    stmt->analyze = true;
                    stmt->paren_options.push_back({option, ""});
                } else if (check(TK::IDENTIFIER)) {
                    option = current().text;
                    (void)advance();

                    // Check if there's a value following
                    if (!check(TK::COMMA) && !check(TK::RPAREN)) {
                        // This option has a value (e.g., PARALLEL 4)
                        std::string_view value = advance().text;
                        stmt->paren_options.push_back({option, value});
                    } else {
                        // Boolean option
                        stmt->paren_options.push_back({option, ""});

                        // Set boolean flags for known options
                        if (option == "FREEZE" || option == "freeze") {
                            stmt->freeze = true;
                        }
                    }
                }
            } while (match(TK::COMMA));
            expect(TK::RPAREN);
        } else {
            // Traditional syntax: VACUUM FULL FREEZE VERBOSE ANALYZE users
            while (true) {
                if (match(TK::FULL)) {
                    stmt->full = true;
                } else if (match(TK::VERBOSE)) {
                    stmt->verbose = true;
                } else if (match(TK::ANALYZE)) {
                    stmt->analyze = true;
                } else if (check(TK::IDENTIFIER)) {
                    std::string_view word = current().text;
                    if (word == "FREEZE" || word == "freeze") {
                        stmt->freeze = true;
                        (void)advance();
                    } else {
                        break; // Not a VACUUM option
                    }
                } else {
                    break; // No more options
                }
            }
        }

        // Parse table list (optional - VACUUM with no tables vacuums all tables)
        if (check(TK::IDENTIFIER) && !check(TK::SEMICOLON) && !is_eof()) {
            do {
                auto table = parse_table_ref();

                // Check for column specifications: table_name(col1, col2, ...)
                if (match(TK::LPAREN)) {
                    // Column list only supported for single table
                    if (!stmt->tables.empty()) {
                        error("Column list can only be specified for a single table");
                    }

                    if (!check(TK::RPAREN)) {
                        do {
                            if (check(TK::IDENTIFIER)) {
                                stmt->columns.push_back(advance().text);
                            }
                        } while (match(TK::COMMA));
                    }
                    expect(TK::RPAREN);
                }

                stmt->tables.push_back(table);
            } while (match(TK::COMMA));
        }

        return stmt;
    }

    GrantStmt* parse_grant() {
        auto stmt = this->template create_node<GrantStmt>();
        expect(TK::GRANT);
        // Parse privileges - can be keywords (SELECT, INSERT, UPDATE, DELETE, ALL, etc.) or
        // identifiers Can also have column lists: UPDATE(col1, col2) or REFERENCES(col)
        do {
            if (!is_eof() && !check(TK::ON)) {
                auto priv = advance().text;

                // Check for column-level privilege: PRIVILEGE(col1, col2, ...)
                if (check(TK::LPAREN)) {
                    // Capture the privilege with its column list as a single string
                    size_t start = priv.data() - source_.data();
                    (void)advance(); // consume LPAREN

                    // Skip to closing paren, counting nested parens
                    int paren_depth = 1;
                    bool has_content = false; // Track if there's anything between parens
                    while (!is_eof() && paren_depth > 0) {
                        if (check(TK::LPAREN))
                            paren_depth++;
                        if (check(TK::RPAREN))
                            paren_depth--;
                        if (paren_depth > 0) {
                            has_content = true; // Found at least one token
                            (void)advance();
                        }
                    }

                    if (check(TK::RPAREN)) {
                        size_t end = current().end;
                        (void)advance(); // consume RPAREN

                        // Validate that column list is not empty
                        if (!has_content) {
                            error("Column list cannot be empty in privilege specification");
                        }

                        // Store entire privilege with column list
                        std::string_view full_priv = source_.substr(start, end - start);
                        stmt->privileges.push_back(
                            this->arena().copy_source(std::string(full_priv)));
                    } else {
                        error("Expected closing parenthesis for column-level privilege");
                    }
                } else {
                    // Regular privilege without columns
                    stmt->privileges.push_back(priv);

                    // Handle multi-word privileges by checking for known second words
                    // This handles: ALL PRIVILEGES, SHOW VIEW, CREATE VIEW, LOCK TABLES, GRANT
                    // OPTION, TAKE OWNERSHIP, VIEW DEFINITION, ALTER ANY, BIGQUERY
                    // READER/EDITOR/OWNER/VIEWER
                    if (!is_eof() && !check(TK::ON) && !check(TK::COMMA)) {
                        std::string_view next = current().text;
                        bool is_multiword = false;

                        if ((priv == "ALL" || priv == "all") &&
                            (next == "PRIVILEGES" || next == "privileges")) {
                            is_multiword = true;
                        } else if ((priv == "SHOW" || priv == "show") &&
                                   (next == "VIEW" || next == "view")) {
                            is_multiword = true;
                        } else if ((priv == "CREATE" || priv == "create") &&
                                   (next == "VIEW" || next == "view")) {
                            is_multiword = true;
                        } else if ((priv == "LOCK" || priv == "lock") &&
                                   (next == "TABLES" || next == "tables")) {
                            is_multiword = true;
                        } else if ((priv == "GRANT" || priv == "grant") &&
                                   (next == "OPTION" || next == "option")) {
                            is_multiword = true;
                        } else if ((priv == "TAKE" || priv == "take") &&
                                   (next == "OWNERSHIP" || next == "ownership")) {
                            is_multiword = true;
                        } else if ((priv == "VIEW" || priv == "view") &&
                                   (next == "DEFINITION" || next == "definition")) {
                            is_multiword = true;
                        } else if ((priv == "ALTER" || priv == "alter") &&
                                   (next == "ANY" || next == "any")) {
                            // ALTER ANY is a two-word prefix for three-word privileges (ALTER ANY
                            // USER, ALTER ANY ROLE)
                            is_multiword = true;
                        } else if ((priv == "BIGQUERY" || priv == "bigquery") &&
                                   (next == "READER" || next == "reader" || next == "EDITOR" ||
                                    next == "editor" || next == "OWNER" || next == "owner" ||
                                    next == "VIEWER" || next == "viewer")) {
                            is_multiword = true;
                        }

                        if (is_multiword) {
                            stmt->privileges.push_back(advance().text); // Include second word

                            // Check for three-word privileges like "ALTER ANY USER"
                            if (!is_eof() && !check(TK::ON) && !check(TK::COMMA) &&
                                ((priv == "ALTER" || priv == "alter") ||
                                 (priv == "GRANT" || priv == "grant"))) {
                                std::string_view third = current().text;
                                if (third == "USER" || third == "user" || third == "ROLE" ||
                                    third == "role" || third == "TABLE" || third == "table" ||
                                    third == "VIEW" || third == "view" || third == "INDEX" ||
                                    third == "index" || third == "PROCEDURE" ||
                                    third == "procedure" || third == "FUNCTION" ||
                                    third == "function" || third == "SCHEMA" || third == "schema" ||
                                    third == "DATABASE" || third == "database" ||
                                    third == "SEQUENCE" || third == "sequence" || third == "FOR" ||
                                    third == "for") {
                                    stmt->privileges.push_back(
                                        advance().text); // Include third word
                                }
                            }
                        }
                    }
                }
            }
        } while (match(TK::COMMA));

        // Check if this is a role grant (no ON clause) or privilege grant (has ON clause)
        // Role grants: GRANT role_name TO user
        // Privilege grants: GRANT privilege ON object TO user
        if (check(TK::ON)) {
            (void)advance(); // consume ON

            // Parse optional object type (TABLE, SCHEMA, DATABASE, etc.) and object name(s)
            // Strategy: consume tokens until we hit TO keyword
            // Need to capture entire text including commas for multiple objects
            size_t object_start = current().start;
            size_t object_end = object_start;

            std::vector<std::string_view> object_parts;
            while (!is_eof() && current().text != "TO" && current().text != "to") {
                object_parts.push_back(current().text);
                object_end = current().end;
                (void)advance();

                // Skip DOT for qualified names (schema.table)
                if (check(TK::DOT)) {
                    (void)advance(); // Skip the DOT token
                    if (!is_eof() && current().text != "TO" && current().text != "to") {
                        // Append the next part after the dot
                        object_parts.back() = this->arena().copy_source(
                            std::string(object_parts.back()) + "." + std::string(current().text));
                        object_end = current().end;
                        (void)advance();
                    }
                }

                // Handle DOUBLE_COLON for SQL Server LOGIN::name syntax
                // Unlike DOT, we push :: as a separate qualified name (not merged with previous)
                if (!is_eof() && current().text == "::") {
                    std::string colon_name = "::";
                    object_end = current().end;
                    (void)advance(); // Skip the :: token
                    if (!is_eof() && current().text != "TO" && current().text != "to") {
                        // Create "::name" as a separate element
                        colon_name += std::string(current().text);
                        object_end = current().end;
                        (void)advance();
                    }
                    object_parts.push_back(this->arena().copy_source(colon_name));
                }

                // Skip comma for multiple objects
                if (check(TK::COMMA)) {
                    object_end = current().end;
                    (void)advance();
                }
            }

            // Check if first part is an object type keyword (TABLE, SCHEMA, DATABASE, FUNCTION,
            // etc.)
            if (object_parts.size() >= 2 &&
                (object_parts[0] == "TABLE" || object_parts[0] == "SCHEMA" ||
                 object_parts[0] == "DATABASE" || object_parts[0] == "FUNCTION" ||
                 object_parts[0] == "PROCEDURE" || object_parts[0] == "SEQUENCE" ||
                 object_parts[0] == "WAREHOUSE" || object_parts[0] == "STAGE" ||
                 object_parts[0] == "DATASET" || object_parts[0] == "LOGIN")) {
                stmt->object_type = object_parts[0];
                // Concatenate remaining parts without spaces (DOT already merged, DOUBLE_COLON
                // preserved)
                std::string name;
                for (size_t i = 1; i < object_parts.size(); ++i) {
                    name += object_parts[i];
                }
                // Trim leading/trailing whitespace
                size_t start = 0;
                while (start < name.size() && (name[start] == ' ' || name[start] == '\t'))
                    ++start;
                size_t end = name.size();
                while (end > start && (name[end - 1] == ' ' || name[end - 1] == '\t'))
                    --end;
                stmt->object_name = this->arena().copy_source(name.substr(start, end - start));
            } else {
                // No object type - use entire range as object name(s)
                stmt->object_name = this->arena().copy_source(
                    std::string(source_.substr(object_start, object_end - object_start)));
            }
        }
        // else: role grant - no ON clause, object_name remains empty

        // Expect TO keyword
        if (!is_eof() && (current().text == "TO" || current().text == "to")) {
            (void)advance();
        } else {
            error("Expected TO keyword in GRANT statement");
        }

        // Parse grantees (can be keywords like PUBLIC or identifiers)
        do {
            if (!is_eof() && !check(TK::WITH) && !check(TK::SEMICOLON)) {
                stmt->grantees.push_back(advance().text);
            }
        } while (match(TK::COMMA));

        // Validate that we have at least one grantee
        if (stmt->grantees.empty()) {
            error("Expected grantee name after TO");
        }

        if (match(TK::WITH)) {
            // WITH GRANT OPTION, WITH ADMIN OPTION, or WITH HIERARCHY OPTION
            if (check(TK::GRANT)) {
                (void)advance(); // consume GRANT
                if (check(TK::IDENTIFIER) &&
                    (current().text == "OPTION" || current().text == "option")) {
                    (void)advance();
                    stmt->with_grant_option = true;
                }
            } else if (check(TK::IDENTIFIER) &&
                       (current().text == "ADMIN" || current().text == "admin")) {
                (void)advance(); // consume ADMIN
                if (check(TK::IDENTIFIER) &&
                    (current().text == "OPTION" || current().text == "option")) {
                    (void)advance();
                    stmt->with_admin_option = true;
                }
            } else if (check(TK::IDENTIFIER) &&
                       (current().text == "HIERARCHY" || current().text == "hierarchy")) {
                (void)advance(); // consume HIERARCHY
                if (check(TK::IDENTIFIER) &&
                    (current().text == "OPTION" || current().text == "option")) {
                    (void)advance();
                    stmt->with_hierarchy_option = true;
                }
            }
        }
        return stmt;
    }

    RevokeStmt* parse_revoke() {
        auto stmt = this->template create_node<RevokeStmt>();
        expect(TK::REVOKE);

        // Check for GRANT OPTION FOR or ADMIN OPTION FOR prefixes
        if (check(TK::GRANT)) {
            (void)advance(); // consume GRANT
            if (check(TK::IDENTIFIER) &&
                (current().text == "OPTION" || current().text == "option")) {
                (void)advance(); // consume OPTION
                if (check(TK::FOR) || (check(TK::IDENTIFIER) &&
                                       (current().text == "FOR" || current().text == "for"))) {
                    (void)advance(); // consume FOR
                    stmt->grant_option_for = true;
                }
            }
        } else if (check(TK::IDENTIFIER) &&
                   (current().text == "ADMIN" || current().text == "admin")) {
            (void)advance(); // consume ADMIN
            if (check(TK::IDENTIFIER) &&
                (current().text == "OPTION" || current().text == "option")) {
                (void)advance(); // consume OPTION
                if (check(TK::FOR) || (check(TK::IDENTIFIER) &&
                                       (current().text == "FOR" || current().text == "for"))) {
                    (void)advance(); // consume FOR
                    stmt->admin_option_for = true;
                }
            }
        }

        // Parse privileges - can be keywords (SELECT, INSERT, UPDATE, DELETE, ALL, etc.) or
        // identifiers Can also have column lists: UPDATE(col1, col2) or REFERENCES(col)
        do {
            if (!is_eof() && !check(TK::ON)) {
                auto priv = advance().text;

                // Check for column-level privilege: PRIVILEGE(col1, col2, ...)
                if (check(TK::LPAREN)) {
                    // Capture the privilege with its column list as a single string
                    size_t start = priv.data() - source_.data();
                    (void)advance(); // consume LPAREN

                    // Skip to closing paren, counting nested parens
                    int paren_depth = 1;
                    bool has_content = false; // Track if there's anything between parens
                    while (!is_eof() && paren_depth > 0) {
                        if (check(TK::LPAREN))
                            paren_depth++;
                        if (check(TK::RPAREN))
                            paren_depth--;
                        if (paren_depth > 0) {
                            has_content = true; // Found at least one token
                            (void)advance();
                        }
                    }

                    if (check(TK::RPAREN)) {
                        size_t end = current().end;
                        (void)advance(); // consume RPAREN

                        // Validate that column list is not empty
                        if (!has_content) {
                            error("Column list cannot be empty in privilege specification");
                        }

                        // Store entire privilege with column list
                        std::string_view full_priv = source_.substr(start, end - start);
                        stmt->privileges.push_back(
                            this->arena().copy_source(std::string(full_priv)));
                    } else {
                        error("Expected closing parenthesis for column-level privilege");
                    }
                } else {
                    // Regular privilege without columns
                    stmt->privileges.push_back(priv);

                    // Handle multi-word privileges by checking for known second words
                    // This handles: ALL PRIVILEGES, SHOW VIEW, CREATE VIEW, LOCK TABLES, GRANT
                    // OPTION, TAKE OWNERSHIP, VIEW DEFINITION, ALTER ANY, BIGQUERY
                    // READER/EDITOR/OWNER/VIEWER
                    if (!is_eof() && !check(TK::ON) && !check(TK::COMMA)) {
                        std::string_view next = current().text;
                        bool is_multiword = false;

                        if ((priv == "ALL" || priv == "all") &&
                            (next == "PRIVILEGES" || next == "privileges")) {
                            is_multiword = true;
                        } else if ((priv == "SHOW" || priv == "show") &&
                                   (next == "VIEW" || next == "view")) {
                            is_multiword = true;
                        } else if ((priv == "CREATE" || priv == "create") &&
                                   (next == "VIEW" || next == "view")) {
                            is_multiword = true;
                        } else if ((priv == "LOCK" || priv == "lock") &&
                                   (next == "TABLES" || next == "tables")) {
                            is_multiword = true;
                        } else if ((priv == "GRANT" || priv == "grant") &&
                                   (next == "OPTION" || next == "option")) {
                            is_multiword = true;
                        } else if ((priv == "TAKE" || priv == "take") &&
                                   (next == "OWNERSHIP" || next == "ownership")) {
                            is_multiword = true;
                        } else if ((priv == "VIEW" || priv == "view") &&
                                   (next == "DEFINITION" || next == "definition")) {
                            is_multiword = true;
                        } else if ((priv == "ALTER" || priv == "alter") &&
                                   (next == "ANY" || next == "any")) {
                            // ALTER ANY is a two-word prefix for three-word privileges (ALTER ANY
                            // USER, ALTER ANY ROLE)
                            is_multiword = true;
                        } else if ((priv == "BIGQUERY" || priv == "bigquery") &&
                                   (next == "READER" || next == "reader" || next == "EDITOR" ||
                                    next == "editor" || next == "OWNER" || next == "owner" ||
                                    next == "VIEWER" || next == "viewer")) {
                            is_multiword = true;
                        }

                        if (is_multiword) {
                            stmt->privileges.push_back(advance().text); // Include second word

                            // Check for three-word privileges like "ALTER ANY USER"
                            if (!is_eof() && !check(TK::ON) && !check(TK::COMMA) &&
                                ((priv == "ALTER" || priv == "alter") ||
                                 (priv == "GRANT" || priv == "grant"))) {
                                std::string_view third = current().text;
                                if (third == "USER" || third == "user" || third == "ROLE" ||
                                    third == "role" || third == "TABLE" || third == "table" ||
                                    third == "VIEW" || third == "view" || third == "INDEX" ||
                                    third == "index" || third == "PROCEDURE" ||
                                    third == "procedure" || third == "FUNCTION" ||
                                    third == "function" || third == "SCHEMA" || third == "schema" ||
                                    third == "DATABASE" || third == "database" ||
                                    third == "SEQUENCE" || third == "sequence" || third == "FOR" ||
                                    third == "for") {
                                    stmt->privileges.push_back(
                                        advance().text); // Include third word
                                }
                            }
                        }
                    }
                }
            }
        } while (match(TK::COMMA));

        // Check if this is a role revoke (no ON clause) or privilege revoke (has ON clause)
        if (!check(TK::ON)) {
            // Role revoke: REVOKE role_name FROM user (no ON clause)
            // Skip object parsing and go straight to FROM
            expect(TK::FROM);
            // Parse grantees
            do {
                if (!is_eof() && !check(TK::SEMICOLON) &&
                    !(check(TK::IDENTIFIER) &&
                      (current().text == "CASCADE" || current().text == "cascade" ||
                       current().text == "RESTRICT" || current().text == "restrict"))) {
                    stmt->grantees.push_back(advance().text);
                }
            } while (match(TK::COMMA));

            // Validate that we have at least one grantee
            if (stmt->grantees.empty()) {
                error("Expected grantee name after FROM");
            }

            // Check for CASCADE or RESTRICT keyword
            if (check(TK::IDENTIFIER) &&
                (current().text == "CASCADE" || current().text == "cascade")) {
                (void)advance();
                stmt->cascade = true;
            } else if (check(TK::IDENTIFIER) &&
                       (current().text == "RESTRICT" || current().text == "restrict")) {
                (void)advance();
                stmt->restrict = true;
            }
            return stmt;
        }

        expect(TK::ON);
        // Parse optional object type and object name(s) - consume tokens until we hit FROM keyword
        // Need to capture entire text including commas for multiple objects
        size_t object_start = current().start;
        size_t object_end = object_start;

        std::vector<std::string_view> object_parts;
        while (!is_eof() && current().text != "FROM" && current().text != "from") {
            object_parts.push_back(current().text);
            object_end = current().end;
            (void)advance();

            // Skip DOT for qualified names (schema.table)
            if (check(TK::DOT)) {
                (void)advance(); // Skip the DOT token
                if (!is_eof() && current().text != "FROM" && current().text != "from") {
                    // Append the next part after the dot
                    object_parts.back() = this->arena().copy_source(
                        std::string(object_parts.back()) + "." + std::string(current().text));
                    object_end = current().end;
                    (void)advance();
                }
            }

            // Handle DOUBLE_COLON for SQL Server LOGIN::name syntax
            // Unlike DOT, we push :: as a separate qualified name (not merged with previous)
            if (!is_eof() && current().text == "::") {
                std::string colon_name = "::";
                object_end = current().end;
                (void)advance(); // Skip the :: token
                if (!is_eof() && current().text != "FROM" && current().text != "from") {
                    // Create "::name" as a separate element
                    colon_name += std::string(current().text);
                    object_end = current().end;
                    (void)advance();
                }
                object_parts.push_back(this->arena().copy_source(colon_name));
            }

            // Skip comma for multiple objects
            if (check(TK::COMMA)) {
                object_end = current().end;
                (void)advance();
            }
        }

        // Check if first part is an object type keyword (TABLE, SCHEMA, DATABASE, FUNCTION, etc.)
        if (object_parts.size() >= 2 &&
            (object_parts[0] == "TABLE" || object_parts[0] == "SCHEMA" ||
             object_parts[0] == "DATABASE" || object_parts[0] == "FUNCTION" ||
             object_parts[0] == "PROCEDURE" || object_parts[0] == "SEQUENCE" ||
             object_parts[0] == "LOGIN")) {
            stmt->object_type = object_parts[0];
            // Concatenate remaining parts without spaces (DOT already merged, DOUBLE_COLON
            // preserved)
            std::string name;
            for (size_t i = 1; i < object_parts.size(); ++i) {
                name += object_parts[i];
            }
            // Trim leading/trailing whitespace
            size_t start = 0;
            while (start < name.size() && (name[start] == ' ' || name[start] == '\t'))
                ++start;
            size_t end = name.size();
            while (end > start && (name[end - 1] == ' ' || name[end - 1] == '\t'))
                --end;
            stmt->object_name = this->arena().copy_source(name.substr(start, end - start));
        } else {
            // No object type - use entire range as object name(s)
            stmt->object_name = this->arena().copy_source(
                std::string(source_.substr(object_start, object_end - object_start)));
        }

        expect(TK::FROM);
        // Parse grantees
        do {
            if (!is_eof() && !check(TK::SEMICOLON) &&
                !(check(TK::IDENTIFIER) &&
                  (current().text == "CASCADE" || current().text == "cascade" ||
                   current().text == "RESTRICT" || current().text == "restrict"))) {
                stmt->grantees.push_back(advance().text);
            }
        } while (match(TK::COMMA));

        // Validate that we have at least one grantee
        if (stmt->grantees.empty()) {
            error("Expected grantee name after FROM");
        }

        // Check for CASCADE or RESTRICT keyword
        if (check(TK::IDENTIFIER) && (current().text == "CASCADE" || current().text == "cascade")) {
            (void)advance();
            stmt->cascade = true;
        } else if (check(TK::IDENTIFIER) &&
                   (current().text == "RESTRICT" || current().text == "restrict")) {
            (void)advance();
            stmt->restrict = true;
        }
        return stmt;
    }

    CallProcedureStmt* parse_call() {
        auto stmt = this->template create_node<CallProcedureStmt>();
        expect(TK::CALL);
        if (check(TK::IDENTIFIER)) {
            stmt->name = advance().text;
        }
        expect(TK::LPAREN);
        if (!check(TK::RPAREN)) {
            do {
                stmt->arguments.push_back(parse_expression());
            } while (match(TK::COMMA));
        }
        expect(TK::RPAREN);
        return stmt;
    }

    DelimiterStmt* parse_delimiter() {
        auto stmt = this->template create_node<DelimiterStmt>();
        expect(TK::DELIMITER_KW);

        // The delimiter can be any sequence of characters
        // Special case: if delimiter is semicolon itself, consume it
        std::string delimiter_str;

        if (check(TK::SEMICOLON)) {
            // Special case: DELIMITER ;
            delimiter_str = ";";
            (void)advance();
        } else {
            // Consume all tokens until semicolon/EOF and concatenate their text
            while (!is_eof() && !check(TK::SEMICOLON)) {
                const auto& tok = current();
                if (!tok.text.empty()) {
                    delimiter_str += std::string(tok.text);
                }
                (void)advance();
            }
        }

        // Copy the concatenated string into the arena
        stmt->delimiter = this->arena().copy_source(delimiter_str);

        return stmt;
    }

    DoBlock* parse_do() {
        auto stmt = this->template create_node<DoBlock>();
        expect(TK::DO);

        // Optional LANGUAGE clause - check if next token text is "LANGUAGE"
        if (!is_eof() && !check(TK::SEMICOLON)) {
            std::string_view token_text = current().text;
            // Check if current token is "LANGUAGE" (case-insensitive)
            if ((token_text.size() == 8) && (token_text[0] == 'L' || token_text[0] == 'l') &&
                (token_text[1] == 'A' || token_text[1] == 'a') &&
                (token_text[2] == 'N' || token_text[2] == 'n') &&
                (token_text[3] == 'G' || token_text[3] == 'g') &&
                (token_text[4] == 'U' || token_text[4] == 'u') &&
                (token_text[5] == 'A' || token_text[5] == 'a') &&
                (token_text[6] == 'G' || token_text[6] == 'g') &&
                (token_text[7] == 'E' || token_text[7] == 'e')) {
                (void)advance(); // consume LANGUAGE
                // Next token is the language name
                if (!is_eof() && !check(TK::SEMICOLON)) {
                    stmt->language = advance().text;
                }
            }
        }

        // Code block - capture everything from current position to end
        // The code includes the delimiters ($$, $custom$, etc.)
        size_t block_start = current().start;
        size_t block_end = block_start;

        // Capture all remaining tokens until EOF or semicolon
        while (!is_eof() && !check(TK::SEMICOLON)) {
            block_end = current().end;
            (void)advance();
        }

        // Extract the code block from source (preserves original text including delimiters)
        if (block_end > block_start) {
            stmt->code_block = source_.substr(block_start, block_end - block_start);
        }

        return stmt;
    }

    // ========================================================================
    // Dialect-Specific Statement Parsers
    // ========================================================================

    InsertStmt* parse_upsert() {
        // UPSERT is similar to INSERT - treat as INSERT for now
        auto stmt = this->template create_node<InsertStmt>();
        expect(TK::UPSERT);
        expect(TK::INTO);

        // Table name
        stmt->table = parse_table_ref();

        // Optional column list
        if (match(TK::LPAREN)) {
            do {
                if (check(TK::IDENTIFIER)) {
                    stmt->columns.push_back(advance().text);
                }
            } while (match(TK::COMMA));
            expect(TK::RPAREN);
        }

        // VALUES
        if (check(TK::SELECT) || check(TK::WITH)) {
            stmt->select_query = parse_select();
        } else {
            expect(TK::VALUES);
            do {
                expect(TK::LPAREN);
                std::vector<SQLNode*> row;
                do {
                    row.push_back(parse_expression());
                } while (match(TK::COMMA));
                expect(TK::RPAREN);
                stmt->values.push_back(row);
            } while (match(TK::COMMA));
        }

        return stmt;
    }

    ShowStmt* parse_tail() {
        // TAIL table_name (Materialize)
        auto stmt = this->template create_node<ShowStmt>();
        expect(TK::TAIL);
        if (check(TK::IDENTIFIER)) {
            stmt->what = advance().text;
        }
        return stmt;
    }

    TruncateStmt* parse_optimize() {
        // OPTIMIZE table ZORDER BY (col) (Databricks) - use TruncateStmt as generic container
        auto stmt = this->template create_node<TruncateStmt>();
        expect(TK::OPTIMIZE);
        stmt->table = parse_table_ref();
        // Skip ZORDER BY clause for now
        while (!check(TK::SEMICOLON) && !is_eof()) {
            (void)advance();
        }
        return stmt;
    }

    AnalyzeStmt* parse_compute_stats() {
        // COMPUTE STATS table (Impala/Hive) - treat as ANALYZE
        auto stmt = this->template create_node<AnalyzeStmt>();
        expect(TK::COMPUTE);
        expect(TK::STATS);
        if (check(TK::IDENTIFIER)) {
            stmt->tables.push_back(parse_table_ref());
        }
        return stmt;
    }

    TruncateStmt* parse_cache_table() {
        // CACHE TABLE table (Spark) - use TruncateStmt as generic container
        auto stmt = this->template create_node<TruncateStmt>();
        if (check(TK::IDENTIFIER) && (current().text == "CACHE" || current().text == "cache")) {
            (void)advance(); // consume CACHE
        }
        expect(TK::TABLE);
        stmt->table = parse_table_ref();
        return stmt;
    }

    // ========================================================================
    // Stored Procedure/Function Parsers
    // ========================================================================

    CreateProcedureStmt* parse_create_procedure(bool or_replace) {
        auto stmt = this->template create_node<CreateProcedureStmt>();
        stmt->or_replace = or_replace;

        // PROCEDURE or FUNCTION (handle both PROCEDURE and PROCEDURE_KW token types)
        if (match(TK::PROCEDURE) || match(TK::PROCEDURE_KW)) {
            stmt->is_function = false;
        } else if (match(TK::FUNCTION)) {
            stmt->is_function = true;
        } else {
            error("Expected PROCEDURE or FUNCTION");
        }

        // Procedure/function name (allow keywords as identifiers)
        if (!check(TK::IDENTIFIER) && !check(TK::ADD) && !check(TK::COUNT) && !check(TK::SUM) &&
            !check(TK::MAX) && !check(TK::MIN) && !check(TK::AVG)) {
            error("Expected procedure/function name");
        }
        stmt->name = advance().text;

        // Parameters: ([mode] name type, ...) or (name [mode] type, ...)
        expect(TK::LPAREN);
        if (!check(TK::RPAREN)) {
            do {
                ProcedureParameter param;

                // Check for mode before name: IN, OUT, INOUT (PostgreSQL/T-SQL style)
                if (check(TK::IN)) {
                    param.mode = advance().text;
                } else if (check(TK::IDENTIFIER)) {
                    std::string_view word = current().text;
                    if (word == "OUT" || word == "out") {
                        param.mode = advance().text;
                    } else if (word == "INOUT" || word == "inout") {
                        param.mode = advance().text;
                    }
                }

                // Parameter name
                if (check(TK::IDENTIFIER)) {
                    param.name = advance().text;
                }

                // Check for mode after name: IN, OUT, INOUT (Oracle style)
                if (param.mode.empty()) {
                    if (check(TK::IN)) {
                        param.mode = advance().text;
                    } else if (check(TK::IDENTIFIER)) {
                        std::string_view word = current().text;
                        if (word == "OUT" || word == "out") {
                            param.mode = advance().text;
                        } else if (word == "INOUT" || word == "inout") {
                            param.mode = advance().text;
                        }
                    }
                }

                // Parameter type - extract from source preserving original spacing
                // Need to count parentheses to handle types like VARCHAR(100)
                size_t type_start = current().start;
                size_t type_end = type_start;
                int paren_depth = 0;
                while (true) {
                    if (is_eof())
                        break;
                    if (check(TK::LPAREN)) {
                        paren_depth++;
                    } else if (check(TK::RPAREN)) {
                        if (paren_depth == 0)
                            break; // Parameter list closing paren
                        paren_depth--;
                    } else if (check(TK::COMMA) && paren_depth == 0) {
                        break; // Next parameter
                    }
                    type_end = current().end;
                    (void)advance();
                }
                // Extract substring from source (preserves original spacing)
                std::string_view type_view = source_.substr(type_start, type_end - type_start);
                param.type = this->arena().copy_source(std::string(type_view));

                stmt->parameters.push_back(param);
            } while (match(TK::COMMA));
        }
        expect(TK::RPAREN);

        // RETURNS type (for functions) - capture full return type including parentheses
        if (stmt->is_function &&
            (check(TK::RETURNS) || (check(TK::IDENTIFIER) && (current().text == "RETURNS" ||
                                                              current().text == "returns")))) {
            (void)advance(); // consume RETURNS

            if (!check(TK::AS) && !check(TK::BEGIN) && !is_eof()) {
                // Capture complete return type with parentheses (e.g., VARCHAR(100))
                // Similar to parameter type parsing
                size_t type_start = current().start;
                size_t type_end = type_start;
                int paren_depth = 0;

                while (!is_eof() && !check(TK::AS) && !check(TK::BEGIN) && !check(TK::LANGUAGE) &&
                       !(check(TK::IDENTIFIER) &&
                         (current().text == "LANGUAGE" || current().text == "language"))) {
                    if (check(TK::LPAREN)) {
                        paren_depth++;
                    } else if (check(TK::RPAREN)) {
                        if (paren_depth == 0)
                            break; // Not part of type
                        paren_depth--;
                    }
                    type_end = current().end;
                    (void)advance();
                }

                // Extract substring from source (preserves original spacing and parentheses)
                std::string_view type_view = source_.substr(type_start, type_end - type_start);
                stmt->return_type = this->arena().copy_source(std::string(type_view));
            }
        }

        // LANGUAGE clause (optional, PostgreSQL)
        if (check(TK::LANGUAGE) || (check(TK::IDENTIFIER) && (current().text == "LANGUAGE" ||
                                                              current().text == "language"))) {
            (void)advance(); // consume LANGUAGE
            // Accept any token type for the language name (could be keyword like plpgsql, not just
            // IDENTIFIER)
            if (!check(TK::AS) && !check(TK::BEGIN) && !is_eof()) {
                stmt->language = advance().text;
            }
        }

        // AS keyword (optional)
        (void)match(TK::AS);

        // Body: BEGIN ... END (may contain EXCEPTION handlers)
        if (check(TK::BEGIN)) {
            auto body_block = parse_begin();

            // Extract statements from the block (could be BeginEndBlock or ExceptionBlock)
            if (body_block->type == SQLNodeKind::BEGIN_END_BLOCK) {
                auto* block = static_cast<BeginEndBlock*>(body_block);
                stmt->body = block->statements;
            } else if (body_block->type == SQLNodeKind::EXCEPTION_BLOCK) {
                // If there's an EXCEPTION block, store it as a single statement in the body
                stmt->body.push_back(body_block);
            }
        }

        return stmt;
    }

    DropProcedureStmt* parse_drop_procedure() {
        auto stmt = this->template create_node<DropProcedureStmt>();

        // PROCEDURE or FUNCTION (handle both PROCEDURE and PROCEDURE_KW token types)
        if (match(TK::PROCEDURE) || match(TK::PROCEDURE_KW)) {
            stmt->is_function = false;
        } else if (match(TK::FUNCTION)) {
            stmt->is_function = true;
        } else {
            error("Expected PROCEDURE or FUNCTION");
        }

        // IF EXISTS?
        if (match(TK::IF_KW) || match(TK::IF)) {
            expect(TK::EXISTS);
            stmt->if_exists = true;
        }

        if (check(TK::IDENTIFIER)) {
            stmt->name = advance().text;
        }

        return stmt;
    }

    SQLNode* parse_declare() {
        expect(TK::DECLARE);

        // Skip whitespace tokens if tokenizer produces them
        while (!is_eof() && current().text.empty()) {
            (void)advance();
        }

        // Check if it's a cursor or variable declaration
        // Many keywords can be used as identifiers in DECLARE context.
        // T-SQL variables lex as PARAMETER tokens (DECLARE @i INT = 1).
        if (check(TK::IDENTIFIER) || check(TK::PARAMETER) || check(TK::TEMP) || check(TK::COUNT) ||
            check(TK::SUM) || check(TK::AVG) || check(TK::MIN) || check(TK::MAX) ||
            check(TK::ORDER) || check(TK::RANK)) {
            auto name_tok = current();
            (void)advance();

            // Skip whitespace after variable name too
            while (!is_eof() && current().text.empty()) {
                (void)advance();
            }

            // Check for SCROLL or CURSOR keywords (cursor declaration)
            bool is_cursor = check(TK::CURSOR) || check(TK::SCROLL);

            if (is_cursor) {
                // Cursor declaration: DECLARE name [SCROLL] CURSOR FOR query
                auto stmt = this->template create_node<DeclareCursorStmt>();
                stmt->cursor_name = name_tok.text;

                // Check for SCROLL keyword (optional, comes before CURSOR)
                if (check(TK::SCROLL)) {
                    stmt->scroll = true;
                    (void)advance();

                    // Skip whitespace
                    while (!is_eof() && current().text.empty()) {
                        (void)advance();
                    }
                }

                // CURSOR keyword (required)
                if (check(TK::CURSOR)) {
                    (void)advance();
                } else {
                    error("Expected CURSOR keyword in cursor declaration");
                }

                // FOR keyword
                if (check(TK::FOR)) {
                    (void)advance();
                    stmt->query = parse_select();
                }

                return stmt;
            } else {
                // Variable declaration: DECLARE var_name type [DEFAULT value]
                auto stmt = this->template create_node<DeclareVarStmt>();
                stmt->variable_name = name_tok.text;

                // Parse type - concatenate tokens and copy to arena
                std::string type_str;
                while (!check(TK::SEMICOLON) && !check(TK::DEFAULT) && !check(TK::EQ) &&
                       !check(TK::END) && !check(TK::COLON_EQUALS) && !is_eof()) {
                    if (!current().text.empty()) {
                        type_str += std::string(current().text);
                    }
                    (void)advance();
                }
                // Copy the type string into the arena so it persists
                stmt->type = this->arena().copy_source(type_str);

                // DEFAULT value? (supports =, :=, or DEFAULT keyword)
                if (match(TK::DEFAULT) || match(TK::EQ) || match(TK::COLON_EQUALS)) {
                    stmt->default_value = parse_expression();
                }

                return stmt;
            }
        }

        error("Expected variable or cursor name after DECLARE");
        return nullptr;
    }

    IfStmt* parse_if() {
        auto stmt = this->template create_node<IfStmt>();
        expect(TK::IF_KW);

        stmt->condition = parse_expression();
        expect(TK::THEN);

        // Parse THEN body (simplified - just parse until ELSE/END IF)
        while (!check(TK::END) && !check(TK::ELSE) && !check(TK::ELSEIF) && !check(TK::ENDIF) &&
               !is_eof()) {
            // Skip semicolons
            if (match(TK::SEMICOLON)) {
                continue;
            }
            stmt->then_stmts.push_back(parse_statement());
        }

        // ELSIF clauses - use elseif_branches field (supports multiple)
        while (check(TK::ELSEIF)) {
            (void)advance();
            SQLNode* elsif_condition = parse_expression();
            expect(TK::THEN);

            std::vector<SQLNode*> elsif_stmts;
            while (!check(TK::END) && !check(TK::ELSE) && !check(TK::ELSEIF) && !check(TK::ENDIF) &&
                   !is_eof()) {
                // Skip semicolons
                if (match(TK::SEMICOLON)) {
                    continue;
                }
                elsif_stmts.push_back(parse_statement());
            }

            stmt->elseif_branches.emplace_back(elsif_condition, elsif_stmts);
        }

        // ELSE clause
        if (match(TK::ELSE)) {
            while (!check(TK::END) && !check(TK::ENDIF) && !is_eof()) {
                // Skip semicolons
                if (match(TK::SEMICOLON)) {
                    continue;
                }
                stmt->else_stmts.push_back(parse_statement());
            }
        }

        // END IF or ENDIF
        if (match(TK::ENDIF)) {
            // ENDIF (single token) - already consumed
        } else {
            expect(TK::END);
            // Optional IF after END (use IF_KW token type, not IF)
            if (check(TK::IF_KW) ||
                (check(TK::IDENTIFIER) && (current().text == "IF" || current().text == "if"))) {
                (void)advance();
            }
        }

        return stmt;
    }

    WhileLoop* parse_while_loop() {
        auto stmt = this->template create_node<WhileLoop>();
        expect(TK::WHILE);

        stmt->condition = parse_expression();

        // T-SQL form: WHILE condition BEGIN ... END (no DO/LOOP keyword,
        // the body is a single BEGIN..END block that also terminates the
        // loop - there is no END WHILE).
        if (!check(TK::DO) && !check(TK::LOOP) && check(TK::BEGIN)) {
            auto* body_block = parse_begin();
            if (body_block->type == SQLNodeKind::BEGIN_END_BLOCK) {
                stmt->body = static_cast<BeginEndBlock*>(body_block)->statements;
            } else {
                stmt->body.push_back(body_block);
            }
            return stmt;
        }

        // DO or LOOP keyword (optional in some dialects)
        if (check(TK::DO) || check(TK::LOOP)) {
            (void)advance();
        }

        // Parse body until END WHILE, ENDWHILE, or END LOOP
        while (!check(TK::END) && !check(TK::ENDWHILE) && !is_eof()) {
            // Skip semicolons
            if (match(TK::SEMICOLON)) {
                continue;
            }
            stmt->body.push_back(parse_statement());
        }

        // END WHILE, ENDWHILE, or END LOOP
        if (match(TK::ENDWHILE)) {
            // ENDWHILE (single token) - already consumed
        } else {
            expect(TK::END);
            // Optional LOOP or WHILE after END
            if (check(TK::LOOP) || check(TK::WHILE)) {
                (void)advance();
            }
        }

        return stmt;
    }

    ForLoop* parse_for_loop() {
        auto stmt = this->template create_node<ForLoop>();
        expect(TK::FOR);

        // Loop variable
        if (check(TK::IDENTIFIER) || check(TK::TEMP)) {
            stmt->variable = advance().text;
        }

        // IN keyword
        expect(TK::IN);

        // Optional REVERSE (Oracle/PostgreSQL PL/SQL): FOR i IN REVERSE a..b LOOP
        if (check(TK::IDENTIFIER) && ieq(current().text, "REVERSE")) {
            (void)advance();
            stmt->reverse = true;
        }

        // Record iteration form (PL/pgSQL / Oracle cursor FOR loop):
        // FOR rec IN SELECT ... LOOP, or Oracle's FOR rec IN (SELECT ...) LOOP.
        // Accept both spellings regardless of dialect; the generator picks
        // the dialect-appropriate one when regenerating.
        if (check(TK::LPAREN) && (peek(1).type == TK::SELECT || peek(1).type == TK::WITH)) {
            (void)advance(); // (
            stmt->query = parse_select();
            expect(TK::RPAREN);
        } else if (check(TK::SELECT) || check(TK::WITH)) {
            stmt->query = parse_select();
        } else {
            // Range: start..end
            stmt->start_value = parse_expression();
            expect(TK::DOUBLE_DOT);
            stmt->end_value = parse_expression();
        }

        // LOOP keyword
        if (check(TK::LOOP)) {
            (void)advance();
        }

        // Parse body
        while (!check(TK::END) && !check(TK::ENDLOOP) && !is_eof()) {
            // Skip semicolons
            if (match(TK::SEMICOLON)) {
                continue;
            }
            stmt->body.push_back(parse_statement());
        }

        // END LOOP or ENDLOOP
        if (match(TK::ENDLOOP)) {
            // ENDLOOP (single token) - already consumed
        } else {
            expect(TK::END);
            // Optional LOOP after END
            if (check(TK::LOOP)) {
                (void)advance();
            }
        }

        return stmt;
    }

    LoopStmt* parse_loop() {
        auto stmt = this->template create_node<LoopStmt>();
        expect(TK::LOOP);

        // Parse body until END LOOP or ENDLOOP
        while (!check(TK::END) && !check(TK::ENDLOOP) && !is_eof()) {
            // Skip semicolons
            if (match(TK::SEMICOLON)) {
                continue;
            }
            stmt->body.push_back(parse_statement());
        }

        // END LOOP or ENDLOOP
        if (match(TK::ENDLOOP)) {
            // ENDLOOP (single token) - already consumed
        } else {
            expect(TK::END);
            // Optional LOOP after END
            if (check(TK::LOOP)) {
                (void)advance();
            }
        }

        return stmt;
    }

    RaiseStmt* parse_raise() {
        auto stmt = this->template create_node<RaiseStmt>();

        // Check if this is RAISE or SIGNAL
        if (check(TK::RAISE)) {
            (void)advance();

            // PostgreSQL RAISE: RAISE level 'message'[, format_args...]
            // Level: EXCEPTION, NOTICE, WARNING, INFO, LOG, DEBUG
            if (check(TK::IDENTIFIER) || check(TK::EXCEPTION)) {
                stmt->level = advance().text;
            }

            // Message string
            if (check(TK::STRING)) {
                stmt->message = advance().text;
            }

            // Format arguments: RAISE EXCEPTION 'value is %', 5
            while (match(TK::COMMA)) {
                stmt->args.push_back(parse_expression());
            }
        } else if (check(TK::SIGNAL)) {
            (void)advance();
            stmt->level = "SIGNAL";

            // MySQL SIGNAL: SIGNAL SQLSTATE 'value' [SET MESSAGE_TEXT = 'msg']
            if (check(TK::IDENTIFIER) &&
                (current().text == "SQLSTATE" || current().text == "sqlstate")) {
                (void)advance();
                if (check(TK::STRING)) {
                    stmt->sqlstate = advance().text;
                }
            }

            // SET MESSAGE_TEXT = 'message'
            if (check(TK::SET)) {
                (void)advance();
                // Skip to message text
                while (!check(TK::STRING) && !check(TK::SEMICOLON) && !is_eof()) {
                    (void)advance();
                }
                if (check(TK::STRING)) {
                    stmt->message = advance().text;
                }
            }
        }

        return stmt;
    }

    /// Parse T-SQL RAISERROR('message', severity, state[, args...])
    RaiseStmt* parse_raiserror() {
        auto stmt = this->template create_node<RaiseStmt>();
        stmt->tsql_raiserror = true;
        stmt->level = "EXCEPTION";

        (void)advance(); // RAISERROR (lexes as an identifier)
        expect(TK::LPAREN);

        if (check(TK::STRING)) {
            stmt->message = advance().text;
        }

        // severity, state, and optional substitution arguments
        while (match(TK::COMMA)) {
            stmt->args.push_back(parse_expression());
        }

        expect(TK::RPAREN);
        return stmt;
    }

    OpenCursorStmt* parse_open_cursor() {
        auto stmt = this->template create_node<OpenCursorStmt>();
        expect(TK::OPEN);

        if (check(TK::IDENTIFIER)) {
            stmt->cursor_name = advance().text;
        }

        // Optional cursor arguments: OPEN cur(100, 'active')
        if (match(TK::LPAREN)) {
            if (!check(TK::RPAREN)) {
                do {
                    stmt->args.push_back(parse_expression());
                } while (match(TK::COMMA));
            }
            expect(TK::RPAREN);
        }

        return stmt;
    }

    FetchCursorStmt* parse_fetch_cursor() {
        auto stmt = this->template create_node<FetchCursorStmt>();
        expect(TK::FETCH);

        // Direction keyword (NEXT, PRIOR, FIRST, LAST) - optional
        if (check(TK::NEXT)) {
            stmt->direction = advance().text;
        } else if (check(TK::PRIOR)) {
            stmt->direction = advance().text;
        } else if (check(TK::FIRST)) {
            stmt->direction = advance().text;
        } else if (check(TK::LAST)) {
            stmt->direction = advance().text;
        }

        // FROM keyword (optional)
        if (check(TK::FROM)) {
            (void)advance();
        }

        // Cursor name
        if (check(TK::IDENTIFIER)) {
            stmt->cursor_name = advance().text;
        }

        // INTO variables
        if (check(TK::INTO)) {
            (void)advance();
            do {
                if (check(TK::IDENTIFIER)) {
                    stmt->into_variables.push_back(advance().text);
                }
            } while (match(TK::COMMA));
        }

        return stmt;
    }

    CloseCursorStmt* parse_close_cursor() {
        auto stmt = this->template create_node<CloseCursorStmt>();
        expect(TK::CLOSE);

        if (check(TK::IDENTIFIER)) {
            stmt->cursor_name = advance().text;
        }

        return stmt;
    }

    ReturnStmt* parse_return() {
        expect(TK::RETURN_KW);

        // Optional return value
        SQLNode* return_value = nullptr;
        if (!check(TK::END) && !check(TK::SEMICOLON) && !check(TK::EXCEPTION) && !is_eof() &&
            !check(TK::ELSE) && !check(TK::ELSEIF) && !check(TK::ENDIF) && !check(TK::ENDLOOP) &&
            !check(TK::ENDWHILE)) {
            return_value = parse_expression();
        }

        return this->template create_node<ReturnStmt>(return_value);
    }

    BreakStmt* parse_break() {
        expect(TK::BREAK);
        return this->template create_node<BreakStmt>();
    }

    BreakStmt* parse_exit() {
        // EXIT is an alias for BREAK
        expect(TK::EXIT);
        return this->template create_node<BreakStmt>();
    }

    ContinueStmt* parse_continue() {
        expect(TK::CONTINUE);
        return this->template create_node<ContinueStmt>();
    }

    AssignmentStmt* parse_assignment() {
        auto stmt = this->template create_node<AssignmentStmt>();

        // Variable name
        if (!check(TK::IDENTIFIER)) {
            error("Expected variable name in assignment");
        }
        stmt->variable_name = advance().text;

        // := operator
        expect(TK::COLON_EQUALS);

        // Value expression
        stmt->value = parse_expression();

        return stmt;
    }

    // ========================================================================
    // Trigger Parsers
    // ========================================================================

    CreateTriggerStmt* parse_create_trigger() {
        auto stmt = this->template create_node<CreateTriggerStmt>();
        expect(TK::TRIGGER);

        // Trigger name
        if (!check(TK::IDENTIFIER)) {
            error("Expected trigger name");
        }
        stmt->name = advance().text;

        // Timing: BEFORE, AFTER, INSTEAD OF
        if (check(TK::IDENTIFIER)) {
            std::string_view timing_word = current().text;
            if (timing_word == "BEFORE" || timing_word == "before") {
                stmt->timing = TriggerTiming::BEFORE;
                (void)advance();
            } else if (timing_word == "AFTER" || timing_word == "after") {
                stmt->timing = TriggerTiming::AFTER;
                (void)advance();
            } else if (timing_word == "INSTEAD" || timing_word == "instead") {
                (void)advance();
                // OF keyword
                if (check(TK::IDENTIFIER) && (current().text == "OF" || current().text == "of")) {
                    (void)advance();
                }
                stmt->timing = TriggerTiming::INSTEAD_OF;
            }
        }

        // Event: INSERT, UPDATE, DELETE
        if (check(TK::INSERT)) {
            stmt->event = TriggerEvent::INSERT;
            (void)advance();
        } else if (check(TK::UPDATE)) {
            stmt->event = TriggerEvent::UPDATE;
            (void)advance();
        } else if (check(TK::DELETE)) {
            stmt->event = TriggerEvent::DELETE;
            (void)advance();
        }

        // ON table_name
        expect(TK::ON);
        if (check(TK::IDENTIFIER)) {
            stmt->table = advance().text;
        }

        // FOR EACH ROW (optional)
        if (check(TK::FOR)) {
            (void)advance();
            if (check(TK::IDENTIFIER) && (current().text == "EACH" || current().text == "each")) {
                (void)advance();
                if (check(TK::ROW) || (check(TK::IDENTIFIER) &&
                                       (current().text == "ROW" || current().text == "row"))) {
                    (void)advance();
                    stmt->for_each_row = true;
                }
            }
        }

        // Body: BEGIN ... END or just a statement
        if (check(TK::BEGIN)) {
            (void)advance();
            int depth = 1;
            while (depth > 0 && !is_eof()) {
                if (check(TK::BEGIN))
                    depth++;
                else if (check(TK::END))
                    depth--;
                if (depth > 0)
                    (void)advance();
            }
            expect(TK::END);
        } else {
            // Single statement - skip for now
            while (!check(TK::SEMICOLON) && !is_eof()) {
                (void)advance();
            }
        }

        return stmt;
    }

    DropTriggerStmt* parse_drop_trigger() {
        auto stmt = this->template create_node<DropTriggerStmt>();
        expect(TK::TRIGGER);

        // IF EXISTS?
        if (match(TK::IF_KW) || match(TK::IF)) {
            expect(TK::EXISTS);
            stmt->if_exists = true;
        }

        // Trigger name
        if (check(TK::IDENTIFIER)) {
            stmt->name = advance().text;
        }

        // ON table (optional, dialect-specific)
        if (match(TK::ON)) {
            if (check(TK::IDENTIFIER)) {
                stmt->table = advance().text;
            }
        }

        return stmt;
    }

    // ========================================================================
    // BigQuery ML Parsers
    // ========================================================================

    CreateModelStmt* parse_create_model(bool or_replace) {
        auto stmt = this->template create_node<CreateModelStmt>();
        stmt->or_replace = or_replace;

        // MODEL keyword
        if (check(TK::IDENTIFIER) && (current().text == "MODEL" || current().text == "model")) {
            (void)advance();
        }

        // IF NOT EXISTS?
        if (match(TK::IF_KW) || match(TK::IF)) {
            expect(TK::NOT);
            expect(TK::EXISTS);
        }

        // Model name
        if (check(TK::IDENTIFIER)) {
            stmt->model_name = advance().text;
        }

        // OPTIONS clause (simplified - just skip for now)
        if (check(TK::IDENTIFIER) && (current().text == "OPTIONS" || current().text == "options")) {
            (void)advance();
            if (match(TK::LPAREN)) {
                int paren_depth = 1;
                while (paren_depth > 0 && !is_eof()) {
                    if (check(TK::LPAREN))
                        paren_depth++;
                    else if (check(TK::RPAREN))
                        paren_depth--;
                    if (paren_depth > 0)
                        (void)advance();
                }
                expect(TK::RPAREN);
            }
        }

        // AS SELECT ...
        if (match(TK::AS)) {
            stmt->training_query = parse_select();
        }

        return stmt;
    }

    DropModelStmt* parse_drop_model() {
        auto stmt = this->template create_node<DropModelStmt>();

        // MODEL keyword
        if (check(TK::IDENTIFIER) && (current().text == "MODEL" || current().text == "model")) {
            (void)advance();
        }

        // IF EXISTS?
        if (match(TK::IF_KW) || match(TK::IF)) {
            expect(TK::EXISTS);
            stmt->if_exists = true;
        }

        // Model name
        if (check(TK::IDENTIFIER)) {
            stmt->model_name = advance().text;
        }

        return stmt;
    }

    /// Shadow token_name for better error messages (CRTP customization point)
    [[nodiscard]] std::string token_name(TK type) const {
        return std::string(libglot::sql::lex::token_type_name(type));
    }

private:
    // ========================================================================
    // Lifetime-Safe Tokenization Helper
    // ========================================================================

    /// Convert SQLDialect to TokenizerConfig
    static libglot::sql::lex::TokenizerConfig
    dialect_to_tokenizer_config(SQLDialect dialect) noexcept {
        switch (dialect) {
        case SQLDialect::SQLServer:
            return libglot::sql::lex::TokenizerConfig::sqlserver();
        case SQLDialect::MySQL:
            return libglot::sql::lex::TokenizerConfig::mysql();
        case SQLDialect::PostgreSQL:
            return libglot::sql::lex::TokenizerConfig::postgresql();
        case SQLDialect::Snowflake:
            return libglot::sql::lex::TokenizerConfig::snowflake();
        case SQLDialect::BigQuery:
            return libglot::sql::lex::TokenizerConfig::bigquery();
        default:
            // Most dialects support # comments (MySQL-style)
            // SQL Server is the exception
            return libglot::sql::lex::TokenizerConfig::default_config();
        }
    }

    struct TokenizeResult {
        std::vector<TokenType> tokens;
        std::string_view source;
    };

    /// Delegating constructor that receives pre-tokenized result
    SQLParser(libglot::Arena& arena, TokenizeResult&& result, SQLDialect dialect)
        : source_(result.source), dialect_(dialect), Base(arena, std::move(result.tokens)) {}

    /// Copy source into arena and tokenize the arena-owned copy
    /// This ensures all token string_views point to arena memory
    static TokenizeResult tokenize_and_copy(libglot::Arena& arena, std::string_view source,
                                            SQLDialect dialect) {
        auto arena_source = arena.copy_source(source);
        auto tokens = tokenize(arena, arena_source, dialect);
        return {std::move(tokens), arena_source};
    }

    // ========================================================================
    // Tokenization (uses libsqlglot's existing tokenizer)
    // ========================================================================

    static std::vector<TokenType> tokenize(libglot::Arena& arena, std::string_view source,
                                           SQLDialect dialect) {
        libglot::sql::lex::LocalStringPool pool;

        // Convert SQLDialect to TokenizerConfig
        libglot::sql::lex::TokenizerConfig config = dialect_to_tokenizer_config(dialect);

        libglot::sql::lex::Tokenizer tokenizer(source, &pool, config);
        auto tokens = tokenizer.tokenize_all();

        // Convert libglot::sql::lex::Token to libglot::Token<TokenKind>
        std::vector<TokenType> result;
        result.reserve(tokens.size());

        for (const auto& tok : tokens) {
            // Default: the raw source span. `source` is the arena-owned copy,
            // so this view is lifetime-safe.
            std::string_view token_text = tok.view(source);

            // Quoted identifiers: the tokenizer's interned text is the
            // quote-stripped (and escape-collapsed) form; the raw span still
            // carries the quote characters. Using the raw span made every
            // re-parse of generated SQL double the quoting ("""id""").
            // tok.text points into the tokenizer's LocalStringPool, which
            // dies at the end of this function, so when it differs from the
            // source span it must be copied into the arena (see LIFETIME.md).
            if (tok.text != nullptr && token_text != std::string_view(tok.text)) {
                token_text = arena.copy_source(tok.text);
            }

            result.push_back(TokenType{
                tok.type,  // type
                tok.start, // start
                tok.end,   // end
                tok.line,  // line
                tok.col,   // col
                token_text // text (quote-stripped, arena-backed)
            });
        }

        return result;
    }

    std::string_view source_;
    SQLDialect dialect_;

public:
    // Suppresses parse_postfix's unconditional `expr IN (...)` consumption
    // for the duration of a scoped guard. Needed where IN introduces a
    // trailing modifier rather than a value list right after an expression
    // parsed with parse_expression() - e.g. MySQL's
    // `AGAINST('x' IN NATURAL LANGUAGE MODE)`, where the plain
    // `check(TK::IN)` in parse_postfix would otherwise swallow the IN and
    // then fail expecting '(' for a value list.
    bool no_in_postfix_ = false;

    struct ScopedNoInPostfix {
        SQLParser& p;
        bool prev;
        explicit ScopedNoInPostfix(SQLParser& parser) : p(parser), prev(parser.no_in_postfix_) {
            p.no_in_postfix_ = true;
        }
        ~ScopedNoInPostfix() { p.no_in_postfix_ = prev; }
    };
};

} // namespace libglot::sql
