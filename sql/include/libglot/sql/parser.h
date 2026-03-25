#pragma once

#include "../../../../core/include/libglot/parse/parser.h"
#include "../../../../libsqlglot/include/libsqlglot/tokenizer.h"
#include "grammar.h"
#include "ast_nodes.h"
#include "dialect_traits.h"
#include <memory>
#include <algorithm>
#include <iostream>

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
    using TK = libsqlglot::TokenType;

    // ========================================================================
    // Construction
    // ========================================================================

    explicit SQLParser(libglot::Arena& arena, std::string_view source, SQLDialect dialect = SQLDialect::PostgreSQL)
        : SQLParser(arena, tokenize_and_copy(arena, source, dialect), dialect)
    {}

    // ========================================================================
    // Top-Level Parsing Entry Point (Required by Base)
    // ========================================================================

    SQLNode* parse_top_level() {
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
        } else if (check(TK::IDENTIFIER) && (current().text == "CACHE" || current().text == "cache")) {
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
            return this->template create_node<ExistsExpr>(static_cast<SelectStmt*>(subquery));
        }

        // ANY (subquery or expression)
        // For now, parse as a function call - the AST structure expects comparison operator
        // TODO: Properly handle as ANY expression with left operand and operator
        if (match(TK::ANY)) {
            expect(TK::LPAREN);
            std::vector<SQLNode*> args;
            if (check(TK::SELECT)) {
                args.push_back(parse_select());
            } else if (!check(TK::RPAREN)) {
                args.push_back(parse_expression());
            }
            expect(TK::RPAREN);
            return this->template create_node<FunctionCall>("ANY", args);
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
        // Note: The tokenizer may lex [elements] as a single quoted identifier token in SQL Server mode
        if (match(TK::ARRAY)) {
            // Check if we have a bracket-quoted identifier (SQL Server style) vs separate bracket tokens
            if (check(TK::IDENTIFIER) && current().text.starts_with('[') && current().text.ends_with(']')) {
                // Tokenizer lexed [node_id] as a single identifier - need to parse the interior
                // This is a limitation of the generic tokenizer
                // For now, create a simple array with the unquoted identifier
                std::string_view interior = current().text.substr(1, current().text.length() - 2);
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
                return this->template create_node<SubqueryExpr>(static_cast<SelectStmt*>(subquery));
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

        // INTERVAL expressions: INTERVAL <value> <unit>
        // Example: INTERVAL 7 DAY, INTERVAL '2 days' DAY TO SECOND
        if (match(TK::INTERVAL)) {
            // Parse the value (can be number or string)
            SQLNode* value = parse_expression();
            // Parse the unit (DAY, HOUR, MINUTE, SECOND, YEAR, MONTH, etc.)
            // The unit is typically an identifier
            std::string_view unit = "";
            if (check(TK::IDENTIFIER)) {
                unit = advance().text;
            }
            // Create an interval expression node (we'll treat it as a function call for now)
            std::vector<SQLNode*> args;
            args.push_back(value);
            if (!unit.empty()) {
                args.push_back(this->template create_node<Literal>(unit));
            }
            return this->template create_node<FunctionCall>("INTERVAL", args);
        }

        if (match(TK::CAST)) {
            expect(TK::LPAREN);
            auto expr = parse_expression();
            expect(TK::AS);

            // Parse type name
            std::string type_str;
            while (!check(TK::RPAREN) && !this->is_eof()) {
                if (!current().text.empty()) {
                    if (!type_str.empty()) type_str += " ";
                    type_str += std::string(current().text);
                }
                (void)advance();  // Acknowledge nodiscard warning
            }
            expect(TK::RPAREN);
            return this->template create_node<CastExpr>(expr, type_str);
        }

        if (check(TK::SAFE_CAST)) {
            (void)advance();  // consume SAFE_CAST
            expect(TK::LPAREN);
            auto expr = parse_expression();
            expect(TK::AS);

            // Parse type name
            std::string type_str;
            while (!check(TK::RPAREN) && !this->is_eof()) {
                if (!current().text.empty()) {
                    if (!type_str.empty()) type_str += " ";
                    type_str += std::string(current().text);
                }
                (void)advance();
            }
            expect(TK::RPAREN);
            return this->template create_node<CastExpr>(expr, type_str);
        }

        if (check(TK::STRUCT_KW)) {
            (void)advance();  // consume STRUCT
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
            std::string field(current().text);
            (void)advance();  // Acknowledge nodiscard warning
            expect(TK::FROM);
            auto expr = parse_expression();
            expect(TK::RPAREN);
            return this->template create_node<FunctionCall>("EXTRACT",
                std::vector<SQLNode*>{this->template create_node<Literal>(field), expr});
        }

        // Unary operators (NOT, -, +)
        if (match(TK::NOT)) {
            auto operand = parse_expression();
            return this->template create_node<UnaryOp>(TK::NOT, operand);
        }

        if (match(TK::MINUS)) {
            auto operand = parse_expression();
            return this->template create_node<UnaryOp>(TK::MINUS, operand);
        }

        if (match(TK::PLUS)) {
            auto operand = parse_expression();
            return this->template create_node<UnaryOp>(TK::PLUS, operand);
        }

        // Function call or column reference (including keywords used as identifiers)
        if (check(TK::IDENTIFIER) || check(TK::RANK) || check(TK::ORDER) || check(TK::TEMP) || check(TK::LEVEL) ||
            check(TK::COUNT) || check(TK::SUM) || check(TK::AVG) || check(TK::MIN) || check(TK::MAX) ||
            check(TK::DENSE_RANK) || check(TK::ROW_NUMBER) || check(TK::NTILE) ||
            check(TK::LEAD) || check(TK::LAG) || check(TK::FIRST_VALUE) || check(TK::LAST_VALUE) || check(TK::NTH_VALUE) ||
            check(TK::SUBSTRING) || check(TK::SUBSTR) || check(TK::CONCAT_KW) || check(TK::CONCAT_WS) || check(TK::LENGTH) || check(TK::TRIM) ||
            check(TK::UPPER) || check(TK::LOWER) || check(TK::REPLACE) || check(TK::REPLACE_KW) || check(TK::REPLACE_DDB) || check(TK::SPLIT) ||
            check(TK::ROUND) || check(TK::FLOOR) || check(TK::CEIL) || check(TK::ABS) || check(TK::POWER) || check(TK::SQRT) ||
            check(TK::TIMESTAMP) || check(TK::DATE) || check(TK::TIME) ||
            check(TK::GENERATE_SERIES) || check(TK::UNNEST)) {
            auto first = advance();
            std::string_view name = first.text;

            // Function call: func(...)
            if (match(TK::LPAREN)) {
                return parse_function_call(name);
            }

            // table.column or column.field (but not .. range operator)
            if (check(TK::DOT) && !check_double_dot()) {
                (void)advance();  // consume DOT
                if (check(TK::STAR)) {
                    (void)advance();  // Acknowledge nodiscard warning
                    return this->template create_node<Star>(name);
                }
                // Allow keywords as column names (SQL permits this)
                if (check(TK::LPAREN) || check(TK::RPAREN) || check(TK::COMMA) ||
                    check(TK::SEMICOLON) || check(TK::EOF_TOKEN)) {
                    error("Expected column name after '.'");
                }
                auto second = advance();
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

    /// Parse postfix expression (array indexing, JSON operators, etc.)
    [[nodiscard]] SQLNode* parse_postfix(SQLNode* base) {
        while (true) {
            // IN operator: expr IN (value1, value2, ...) or expr IN (SELECT ...)
            if (check(TK::IN)) {
                (void)advance();  // Consume IN
                expect(TK::LPAREN);

                // Check if it's a subquery or a list of values
                if (check(TK::SELECT)) {
                    // IN (SELECT ...) - subquery form
                    auto subquery = parse_select();
                    expect(TK::RPAREN);
                    auto in_expr = this->template create_node<InExpr>(base, std::vector<SQLNode*>{subquery});
                    base = in_expr;
                } else {
                    // IN (value1, value2, ...) - value list form
                    std::vector<SQLNode*> values;
                    if (!check(TK::RPAREN)) {
                        do {
                            values.push_back(parse_expression());
                        } while (match(TK::COMMA));
                    }
                    expect(TK::RPAREN);
                    auto in_expr = this->template create_node<InExpr>(base, values);
                    base = in_expr;
                }
                continue;
            }

            // Array indexing: expr[index]
            if (match(TK::LBRACKET)) {
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
                base = this->template create_node<JsonExpr>(base, key, JsonExpr::OpType::LONG_ARROW);
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

    /// Create binary operator node (required for precedence climbing)
    [[nodiscard]] SQLNode* make_binary_operator(TK op, SQLNode* left, SQLNode* right) {
        return this->template create_node<BinaryOp>(op, left, right);
    }

    // ========================================================================
    // SQL-Specific Parsing Rules
    // ========================================================================

    /// Parse SELECT statement (may return set operation for UNION/INTERSECT/EXCEPT)
    SQLNode* parse_select() {
        auto stmt = this->template create_node<SelectStmt>();

        // WITH clause (CTEs)
        if (check(TK::WITH)) {
            stmt->with = parse_with_clause();
        }

        expect(TK::SELECT);

        // DISTINCT?
        if (match(TK::DISTINCT)) {
            stmt->distinct = true;
        }

        // TOP n (SQL Server, Access)
        if (match(TK::TOP)) {
            stmt->limit = parse_expression();
            // Optional: PERCENT, WITH TIES
            if (match(TK::PERCENT)) {
                // Store that this is a percentage (we'd need to track this in AST)
            }
            // WITH TIES would require tracking in AST as well
            // For now, we'll skip these modifiers and just parse the TOP value
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

        // FROM clause
        if (match(TK::FROM)) {
            stmt->from = parse_from_clause();
        }

        // WHERE clause
        if (match(TK::WHERE)) {
            stmt->where = parse_expression();
        }

        // GROUP BY
        if (match(TK::GROUP)) {
            expect(TK::BY);
            do {
                stmt->group_by.push_back(parse_expression());
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

        // ORDER BY
        if (match(TK::ORDER)) {
            expect(TK::BY);
            stmt->order_by = parse_order_by_list();
        }

        // LIMIT
        if (match(TK::LIMIT)) {
            stmt->limit = parse_expression();
        }

        // OFFSET
        if (match(TK::OFFSET)) {
            stmt->offset = parse_expression();
        }

        // Set operations: UNION, INTERSECT, EXCEPT
        if (check(TK::UNION) || check(TK::INTERSECT) || check(TK::EXCEPT)) {
            return parse_set_operation(stmt);
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
            // Accept almost any token as an alias (SQL allows keywords as identifiers when used as aliases)
            // The only tokens we reject are structural ones like parentheses, commas, operators
            if (this->check(TK::LPAREN) || this->check(TK::RPAREN) || this->check(TK::COMMA) ||
                this->check(TK::SEMICOLON) || this->check(TK::EOF_TOKEN)) {
                this->error("Expected alias after AS");
            }
            auto alias_tok = this->advance();
            return this->template create_node<Alias>(expr, alias_tok.text);
        }

        return expr;
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

        // Accept identifiers or keywords as table names (SQL allows reserved words as identifiers when quoted)
        // Also accept TEMP, TEMPORARY, SETTINGS, and other common keywords that might appear in table names
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
            if (!this->check(TK::IDENTIFIER) && !this->check(TK::TABLE) && !this->check(TK::SCHEMA)) {
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
                std::string combined = std::string(first_name) + "." + std::string(second_name) + "." + std::string(third.text);
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

    /// Parse single ORDER BY item (expression [ASC|DESC])
    OrderByItem* parse_order_by_item() {
        auto expr = this->parse_expression();

        bool ascending = true;
        if (this->match(TK::DESC)) {
            ascending = false;
        } else {
            // ASC is optional (default)
            (void)this->match(TK::ASC);  // Acknowledge nodiscard warning
        }

        return this->template create_node<OrderByItem>(expr, ascending);
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

    /// Parse window function (OVER clause)
    WindowFunction* parse_window_function(std::string_view func_name, std::vector<SQLNode*> args) {
        expect(TK::OVER);
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

        // Frame clause: ROWS/RANGE [BETWEEN ...]
        if (check(TK::ROWS) || check(TK::RANGE)) {
            window_spec->frame = parse_frame_clause();
        }

        expect(TK::RPAREN);

        auto window_func = this->template create_node<WindowFunction>(func_name, window_spec);
        window_func->args = args;
        return window_func;
    }

    /// Parse window frame clause: ROWS/RANGE [BETWEEN] ...
    FrameClause* parse_frame_clause() {
        // Frame type: ROWS or RANGE
        FrameType frame_type;
        if (match(TK::ROWS)) {
            frame_type = FrameType::ROWS;
        } else if (match(TK::RANGE)) {
            frame_type = FrameType::RANGE;
        } else {
            error("Expected ROWS or RANGE for window frame");
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
            return frame;
        } else {
            // Single boundary (implies BETWEEN start AND CURRENT ROW)
            auto [bound, offset] = parse_frame_bound();
            auto frame = this->template create_node<FrameClause>(frame_type, bound);
            frame->start_offset = offset;
            return frame;
        }
    }

    /// Parse frame boundary: UNBOUNDED PRECEDING | N PRECEDING | CURRENT ROW | N FOLLOWING | UNBOUNDED FOLLOWING
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
            auto query = static_cast<SelectStmt*>(parse_select());
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
               check(TK::RIGHT) || check(TK::FULL) || check(TK::CROSS) || check(TK::OUTER)) {

            // Comma-separated tables are implicit CROSS JOINs
            if (match(TK::COMMA)) {
                auto right_table = parse_table_or_subquery();
                table = this->template create_node<JoinClause>(JoinType::CROSS, table, right_table, nullptr);
                continue;
            }

            // Explicit JOIN syntax
            JoinType join_type = JoinType::INNER;
            bool saw_apply = false;

            if (match(TK::INNER)) {
                expect(TK::JOIN);
            } else if (match(TK::LEFT)) {
                join_type = JoinType::LEFT;
                (void)match(TK::OUTER);  // OUTER is optional
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
                    join_type = JoinType::LEFT;  // OUTER APPLY is like LEFT JOIN LATERAL
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
            if (match(TK::ON)) {
                condition = parse_expression();
            }

            table = this->template create_node<JoinClause>(join_type, table, right_table, condition);
        }

        return table;
    }

    /// Parse table reference or subquery in FROM clause
    SQLNode* parse_table_or_subquery() {
        // LATERAL join (PostgreSQL, Oracle 12c+)
        if (match(TK::LATERAL)) {
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
                    next_word != "FULL" && next_word != "CROSS" && next_word != "LATERAL") {
                    alias = advance().text;
                }
            }

            // Handle column list after alias: alias(col1, col2, ...)
            if (match(TK::LPAREN)) {
                // Skip the column list for now
                int depth = 1;
                while (depth > 0 && !check(TK::EOF_TOKEN)) {
                    if (match(TK::LPAREN)) depth++;
                    else if (match(TK::RPAREN)) depth--;
                    else (void)advance();
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
            error("Expected SELECT subquery after '('");
        }

        // Check if this is a table-valued function: function_name(...)
        // by looking ahead for IDENTIFIER( or function_keyword(
        // Many table-valued functions like generate_series, unnest are keywords
        bool is_function_call = (pos_ + 1 < tokens_.size() && tokens_[pos_ + 1].type == TK::LPAREN) &&
                                (check(TK::IDENTIFIER) || check(TK::GENERATE_SERIES) || check(TK::UNNEST));

        if (is_function_call) {
            // This is a function call in FROM clause (table-valued function)
            auto func_expr = parse_expression();  // This will parse the function call

            // Optional alias (but note: aliases with column lists like "t(x, y)" need special handling)
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
                    next_word != "FULL" && next_word != "CROSS") {
                    alias = advance().text;

                    // Check for column list after alias: alias(col1, col2, ...)
                    if (match(TK::LPAREN)) {
                        // Skip the column list for now
                        int depth = 1;
                        while (depth > 0 && !check(TK::EOF_TOKEN)) {
                            if (match(TK::LPAREN)) depth++;
                            else if (match(TK::RPAREN)) depth--;
                            else (void)advance();
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

        // Check for optional alias: table_name AS alias or table_name alias
        if (match(TK::AS)) {
            if (check(TK::LPAREN) || check(TK::RPAREN) || check(TK::COMMA) ||
                check(TK::SEMICOLON) || check(TK::EOF_TOKEN)) {
                error("Expected alias after AS");
            }
            table->alias = advance().text;
        } else if (check(TK::IDENTIFIER)) {
            // Check if this identifier is actually an alias (not a keyword like TABLESAMPLE or JOIN)
            std::string_view next_word = current().text;
            if (next_word != "TABLESAMPLE" && next_word != "tablesample" &&
                next_word != "JOIN" && next_word != "INNER" && next_word != "LEFT" &&
                next_word != "RIGHT" && next_word != "FULL" && next_word != "CROSS" &&
                next_word != "WHERE" && next_word != "ORDER" && next_word != "GROUP" &&
                next_word != "HAVING" && next_word != "LIMIT" && next_word != "OFFSET" &&
                next_word != "UNION" && next_word != "INTERSECT" && next_word != "EXCEPT") {
                // This is an alias without AS
                table->alias = advance().text;
            }
        }

        // TABLESAMPLE?
        if (check(TK::IDENTIFIER) && (current().text == "TABLESAMPLE" || current().text == "tablesample")) {
            (void)advance();
            expect(TK::LPAREN);

            // Method: BERNOULLI or SYSTEM (optional)
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

            // Percentage
            auto percent = parse_expression();
            expect(TK::RPAREN);

            return this->template create_node<Tablesample>(method, percent);
        }

        return table;
    }

    /// Parse set operation (UNION, INTERSECT, EXCEPT)
    SQLNode* parse_set_operation(SelectStmt* left) {
        bool all = false;
        SQLNode* result = nullptr;

        if (match(TK::UNION)) {
            all = match(TK::ALL);
            auto right = static_cast<SelectStmt*>(parse_select());
            result = this->template create_node<UnionStmt>(left, right, all);
        } else if (match(TK::INTERSECT)) {
            all = match(TK::ALL);
            auto right = static_cast<SelectStmt*>(parse_select());
            result = this->template create_node<IntersectStmt>(left, right, all);
        } else if (match(TK::EXCEPT)) {
            all = match(TK::ALL);
            auto right = static_cast<SelectStmt*>(parse_select());
            result = this->template create_node<ExceptStmt>(left, right, all);
        } else {
            error("Expected UNION, INTERSECT, or EXCEPT");
        }

        return result;
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
                    stmt->columns.push_back(advance().text);  // Store string_view directly
                }
            } while (match(TK::COMMA));
            expect(TK::RPAREN);
        }

        // VALUES or SELECT
        if (check(TK::SELECT) || check(TK::WITH)) {
            stmt->select_query = static_cast<SelectStmt*>(parse_select());
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
            std::string_view column = advance().text;  // Store string_view directly
            expect(TK::EQ);
            SQLNode* value = parse_expression();
            stmt->assignments.push_back({column, value});
        } while (match(TK::COMMA));

        // Optional FROM clause (PostgreSQL extension)
        if (match(TK::FROM)) {
            stmt->from = parse_from_clause();
        }

        // WHERE clause
        if (match(TK::WHERE)) {
            stmt->where = parse_expression();
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

        // Optional USING clause (PostgreSQL)
        if (match(TK::USING)) {
            stmt->using_clause = parse_from_clause();
        }

        // WHERE clause
        if (match(TK::WHERE)) {
            stmt->where = parse_expression();
        }

        return stmt;
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
            (void)advance();  // Skip alias
        }

        expect(TK::USING);

        // Source (table or subquery)
        stmt->source = parse_table_or_subquery();

        // Optional table alias (e.g., USING source_table s)
        if (check(TK::IDENTIFIER) && !check(TK::ON)) {
            (void)advance();  // Skip alias
        }

        expect(TK::ON);
        stmt->on_condition = parse_expression();

        // WHEN MATCHED/NOT MATCHED clauses (simplified - just parse first one)
        if (check(TK::WHEN)) {
            (void)advance();

            bool matched = false;
            if (match(TK::MATCHED)) {
                matched = true;
            } else if (match(TK::NOT)) {
                expect(TK::MATCHED);
                matched = false;
            }

            expect(TK::THEN);

            // Action: UPDATE SET ... or INSERT ...
            if (check(TK::UPDATE) && matched) {
                (void)advance();
                expect(TK::SET);
                // Parse SET assignments
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
                        col = advance().text;  // Use the column name, discard table qualifier
                    }

                    expect(TK::EQ);
                    auto val = parse_expression();
                    stmt->update_assignments.push_back({col, val});
                } while (match(TK::COMMA));
            } else if (check(TK::INSERT) && !matched) {
                (void)advance();
                // INSERT (columns) VALUES (values)
                if (match(TK::LPAREN)) {
                    do {
                        if (check(TK::IDENTIFIER)) {
                            stmt->insert_columns.push_back(advance().text);
                        }
                    } while (match(TK::COMMA));
                    expect(TK::RPAREN);
                }

                expect(TK::VALUES);
                expect(TK::LPAREN);
                do {
                    stmt->insert_values.push_back(parse_expression());
                } while (match(TK::COMMA));
                expect(TK::RPAREN);
            }
        }

        return stmt;
    }

    /// Parse TRUNCATE statement
    TruncateStmt* parse_truncate() {
        auto stmt = this->template create_node<TruncateStmt>();
        expect(TK::TRUNCATE);
        (void)match(TK::TABLE);  // TABLE keyword is optional

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
        } else if (check(TK::IDENTIFIER) && (current().text == "MODEL" || current().text == "model")) {
            return parse_create_model(or_replace);
        } else if (check(TK::PROJECTION)) {
            return parse_create_projection();
        } else if (check(TK::IDENTIFIER) && (current().text == "REFLECTION" || current().text == "reflection")) {
            return parse_create_reflection();
        }

        error("Expected TABLE, VIEW, INDEX, SCHEMA, PROCEDURE, FUNCTION, TRIGGER, MODEL, PROJECTION, or REFLECTION after CREATE");
        return nullptr;
    }

    /// Parse CREATE TABLE (simplified for now)
    CreateTableStmt* parse_create_table(bool is_temporary = false, bool is_global = false) {
        auto stmt = this->template create_node<CreateTableStmt>();
        expect(TK::TABLE);

        // Set temporary flag
        stmt->temporary = is_temporary;

        // IF NOT EXISTS?
        if (match(TK::IF)) {
            expect(TK::NOT);
            expect(TK::EXISTS);
            stmt->if_not_exists = true;
        }

        // Table name - must be present (including SQL Server #temp syntax)
        // Be permissive for SQL Server temp tables with # or ## prefix
        bool has_hash_prefix = check(TK::HASH);
        if (!has_hash_prefix && !check(TK::IDENTIFIER) && !check(TK::TABLE) && !check(TK::DUAL) && !check(TK::TEMP)) {
            error("Expected table name after CREATE TABLE");
        }
        stmt->table = parse_table_ref();

        // Check for AS SELECT (CREATE TABLE ... AS SELECT ...)
        if (match(TK::AS)) {
            stmt->as_select = static_cast<SelectStmt*>(parse_select());
            return stmt;
        }

        // Column definitions: (col1 type, col2 type, ...)
        expect(TK::LPAREN);
        // For now, skip parsing column definitions - just consume tokens until )
        int paren_depth = 1;
        while (paren_depth > 0 && !is_eof()) {
            if (check(TK::LPAREN)) paren_depth++;
            else if (check(TK::RPAREN)) paren_depth--;
            (void)advance();
        }

        return stmt;
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
        stmt->query = static_cast<SelectStmt*>(parse_select());

        return stmt;
    }

    /// Parse CREATE INDEX
    CreateIndexStmt* parse_create_index() {
        auto stmt = this->template create_node<CreateIndexStmt>();
        expect(TK::INDEX);

        // Index name
        if (check(TK::IDENTIFIER)) {
            stmt->index_name = advance().text;  // Store string_view directly
        }

        expect(TK::ON);
        stmt->table = parse_table_ref();

        // Column list
        expect(TK::LPAREN);
        do {
            if (check(TK::IDENTIFIER)) {
                stmt->columns.push_back(advance().text);  // Store string_view directly
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
        if (match(TK::IF)) {
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
        stmt->query = static_cast<SelectStmt*>(parse_select());

        // Skip SEGMENTED BY and ALL NODES clauses
        while (!check(TK::SEMICOLON) && !is_eof()) {
            (void)advance();
        }

        return stmt;
    }

    /// Parse CREATE REFLECTION (Dremio)
    CreateViewStmt* parse_create_reflection() {
        auto stmt = this->template create_node<CreateViewStmt>();
        if (check(TK::IDENTIFIER) && (current().text == "REFLECTION" || current().text == "reflection")) {
            (void)advance();  // consume REFLECTION
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
        } else if (check(TK::IDENTIFIER) && (current().text == "MODEL" || current().text == "model")) {
            return parse_drop_model();
        }

        error("Expected TABLE, VIEW, INDEX, SCHEMA, PROCEDURE, FUNCTION, TRIGGER, or MODEL after DROP");
        return nullptr;
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
        if (match(TK::IF)) {
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
        if (match(TK::IF)) {
            expect(TK::EXISTS);
            stmt->if_exists = true;
        }

        if (check(TK::IDENTIFIER)) {
            stmt->index_name = advance().text;  // Store string_view directly
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
        if (match(TK::IF)) {
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
        auto stmt = this->template create_node<AlterTableStmt>();
        expect(TK::ALTER);
        expect(TK::TABLE);

        // Table name
        stmt->table = parse_table_ref();

        // Determine operation type
        if (check(TK::ADD)) {
            (void)advance();
            stmt->operation = AlterOperation::ADD_COLUMN;

            // COLUMN keyword (optional)
            if (check(TK::IDENTIFIER) && (current().text == "COLUMN" || current().text == "column")) {
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
            if (check(TK::IDENTIFIER) && (current().text == "COLUMN" || current().text == "column")) {
                (void)advance();
            }

            // Column name
            if (check(TK::IDENTIFIER)) {
                stmt->old_name = advance().text;
            }

        } else if (check(TK::IDENTIFIER)) {
            std::string_view keyword = current().text;

            if (keyword == "MODIFY" || keyword == "modify" || keyword == "ALTER" || keyword == "alter") {
                (void)advance();
                stmt->operation = AlterOperation::MODIFY_COLUMN;

                // COLUMN keyword (optional)
                if (check(TK::IDENTIFIER) && (current().text == "COLUMN" || current().text == "column")) {
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
                        if (check(TK::IDENTIFIER) && (current().text == "TO" || current().text == "to")) {
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
            statements.push_back(parse_top_level());
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
                    handler_stmts.push_back(parse_top_level());
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
            if (!check(TK::IDENTIFIER)) {
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
        stmt->statement = parse_top_level();
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

                // Option names can be keywords (FULL, VERBOSE, ANALYZE) or identifiers (PARALLEL, FREEZE)
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
                        break;  // Not a VACUUM option
                    }
                } else {
                    break;  // No more options
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
        // Parse privileges - can be keywords (SELECT, INSERT, UPDATE, DELETE, ALL, etc.) or identifiers
        // Can also have column lists: UPDATE(col1, col2) or REFERENCES(col)
        do {
            if (!is_eof() && !check(TK::ON)) {
                auto priv = advance().text;

                // Check for column-level privilege: PRIVILEGE(col1, col2, ...)
                if (check(TK::LPAREN)) {
                    // Capture the privilege with its column list as a single string
                    size_t start = priv.data() - source_.data();
                    (void)advance();  // consume LPAREN

                    // Skip to closing paren, counting nested parens
                    int paren_depth = 1;
                    bool has_content = false;  // Track if there's anything between parens
                    while (!is_eof() && paren_depth > 0) {
                        if (check(TK::LPAREN)) paren_depth++;
                        if (check(TK::RPAREN)) paren_depth--;
                        if (paren_depth > 0) {
                            has_content = true;  // Found at least one token
                            (void)advance();
                        }
                    }

                    if (check(TK::RPAREN)) {
                        size_t end = current().end;
                        (void)advance();  // consume RPAREN

                        // Validate that column list is not empty
                        if (!has_content) {
                            error("Column list cannot be empty in privilege specification");
                        }

                        // Store entire privilege with column list
                        std::string_view full_priv = source_.substr(start, end - start);
                        stmt->privileges.push_back(this->arena().copy_source(std::string(full_priv)));
                    } else {
                        error("Expected closing parenthesis for column-level privilege");
                    }
                } else {
                    // Regular privilege without columns
                    stmt->privileges.push_back(priv);

                    // Handle multi-word privileges by checking for known second words
                    // This handles: ALL PRIVILEGES, SHOW VIEW, CREATE VIEW, LOCK TABLES, GRANT OPTION,
                    // TAKE OWNERSHIP, VIEW DEFINITION, ALTER ANY, BIGQUERY READER/EDITOR/OWNER/VIEWER
                    if (!is_eof() && !check(TK::ON) && !check(TK::COMMA)) {
                        std::string_view next = current().text;
                        bool is_multiword = false;

                        if ((priv == "ALL" || priv == "all") && (next == "PRIVILEGES" || next == "privileges")) {
                            is_multiword = true;
                        } else if ((priv == "SHOW" || priv == "show") && (next == "VIEW" || next == "view")) {
                            is_multiword = true;
                        } else if ((priv == "CREATE" || priv == "create") && (next == "VIEW" || next == "view")) {
                            is_multiword = true;
                        } else if ((priv == "LOCK" || priv == "lock") && (next == "TABLES" || next == "tables")) {
                            is_multiword = true;
                        } else if ((priv == "GRANT" || priv == "grant") && (next == "OPTION" || next == "option")) {
                            is_multiword = true;
                        } else if ((priv == "TAKE" || priv == "take") && (next == "OWNERSHIP" || next == "ownership")) {
                            is_multiword = true;
                        } else if ((priv == "VIEW" || priv == "view") && (next == "DEFINITION" || next == "definition")) {
                            is_multiword = true;
                        } else if ((priv == "ALTER" || priv == "alter") && (next == "ANY" || next == "any")) {
                            // ALTER ANY is a two-word prefix for three-word privileges (ALTER ANY USER, ALTER ANY ROLE)
                            is_multiword = true;
                        } else if ((priv == "BIGQUERY" || priv == "bigquery") &&
                                   (next == "READER" || next == "reader" || next == "EDITOR" || next == "editor" ||
                                    next == "OWNER" || next == "owner" || next == "VIEWER" || next == "viewer")) {
                            is_multiword = true;
                        }

                        if (is_multiword) {
                            stmt->privileges.push_back(advance().text);  // Include second word

                            // Check for three-word privileges like "ALTER ANY USER"
                            if (!is_eof() && !check(TK::ON) && !check(TK::COMMA) &&
                                ((priv == "ALTER" || priv == "alter") || (priv == "GRANT" || priv == "grant"))) {
                                std::string_view third = current().text;
                                if (third == "USER" || third == "user" || third == "ROLE" || third == "role" ||
                                    third == "TABLE" || third == "table" || third == "VIEW" || third == "view" ||
                                    third == "INDEX" || third == "index" || third == "PROCEDURE" || third == "procedure" ||
                                    third == "FUNCTION" || third == "function" || third == "SCHEMA" || third == "schema" ||
                                    third == "DATABASE" || third == "database" || third == "SEQUENCE" || third == "sequence" ||
                                    third == "FOR" || third == "for") {
                                    stmt->privileges.push_back(advance().text);  // Include third word
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
            (void)advance();  // consume ON

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
                    (void)advance();  // Skip the DOT token
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
                    (void)advance();  // Skip the :: token
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

            // Check if first part is an object type keyword (TABLE, SCHEMA, DATABASE, FUNCTION, etc.)
            if (object_parts.size() >= 2 &&
                (object_parts[0] == "TABLE" || object_parts[0] == "SCHEMA" || object_parts[0] == "DATABASE" ||
                 object_parts[0] == "FUNCTION" || object_parts[0] == "PROCEDURE" || object_parts[0] == "SEQUENCE" ||
                 object_parts[0] == "WAREHOUSE" || object_parts[0] == "STAGE" || object_parts[0] == "DATASET" ||
                 object_parts[0] == "LOGIN")) {
                stmt->object_type = object_parts[0];
                // Concatenate remaining parts without spaces (DOT already merged, DOUBLE_COLON preserved)
                std::string name;
                for (size_t i = 1; i < object_parts.size(); ++i) {
                    name += object_parts[i];
                }
                // Trim leading/trailing whitespace
                size_t start = 0;
                while (start < name.size() && (name[start] == ' ' || name[start] == '\t')) ++start;
                size_t end = name.size();
                while (end > start && (name[end-1] == ' ' || name[end-1] == '\t')) --end;
                stmt->object_name = this->arena().copy_source(name.substr(start, end - start));
            } else {
                // No object type - use entire range as object name(s)
                stmt->object_name = this->arena().copy_source(std::string(source_.substr(object_start, object_end - object_start)));
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
                (void)advance();  // consume GRANT
                if (check(TK::IDENTIFIER) && (current().text == "OPTION" || current().text == "option")) {
                    (void)advance();
                    stmt->with_grant_option = true;
                }
            } else if (check(TK::IDENTIFIER) && (current().text == "ADMIN" || current().text == "admin")) {
                (void)advance();  // consume ADMIN
                if (check(TK::IDENTIFIER) && (current().text == "OPTION" || current().text == "option")) {
                    (void)advance();
                    stmt->with_admin_option = true;
                }
            } else if (check(TK::IDENTIFIER) && (current().text == "HIERARCHY" || current().text == "hierarchy")) {
                (void)advance();  // consume HIERARCHY
                if (check(TK::IDENTIFIER) && (current().text == "OPTION" || current().text == "option")) {
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
            (void)advance();  // consume GRANT
            if (check(TK::IDENTIFIER) && (current().text == "OPTION" || current().text == "option")) {
                (void)advance();  // consume OPTION
                if (check(TK::FOR) || (check(TK::IDENTIFIER) && (current().text == "FOR" || current().text == "for"))) {
                    (void)advance();  // consume FOR
                    stmt->grant_option_for = true;
                }
            }
        } else if (check(TK::IDENTIFIER) && (current().text == "ADMIN" || current().text == "admin")) {
            (void)advance();  // consume ADMIN
            if (check(TK::IDENTIFIER) && (current().text == "OPTION" || current().text == "option")) {
                (void)advance();  // consume OPTION
                if (check(TK::FOR) || (check(TK::IDENTIFIER) && (current().text == "FOR" || current().text == "for"))) {
                    (void)advance();  // consume FOR
                    stmt->admin_option_for = true;
                }
            }
        }

        // Parse privileges - can be keywords (SELECT, INSERT, UPDATE, DELETE, ALL, etc.) or identifiers
        // Can also have column lists: UPDATE(col1, col2) or REFERENCES(col)
        do {
            if (!is_eof() && !check(TK::ON)) {
                auto priv = advance().text;

                // Check for column-level privilege: PRIVILEGE(col1, col2, ...)
                if (check(TK::LPAREN)) {
                    // Capture the privilege with its column list as a single string
                    size_t start = priv.data() - source_.data();
                    (void)advance();  // consume LPAREN

                    // Skip to closing paren, counting nested parens
                    int paren_depth = 1;
                    bool has_content = false;  // Track if there's anything between parens
                    while (!is_eof() && paren_depth > 0) {
                        if (check(TK::LPAREN)) paren_depth++;
                        if (check(TK::RPAREN)) paren_depth--;
                        if (paren_depth > 0) {
                            has_content = true;  // Found at least one token
                            (void)advance();
                        }
                    }

                    if (check(TK::RPAREN)) {
                        size_t end = current().end;
                        (void)advance();  // consume RPAREN

                        // Validate that column list is not empty
                        if (!has_content) {
                            error("Column list cannot be empty in privilege specification");
                        }

                        // Store entire privilege with column list
                        std::string_view full_priv = source_.substr(start, end - start);
                        stmt->privileges.push_back(this->arena().copy_source(std::string(full_priv)));
                    } else {
                        error("Expected closing parenthesis for column-level privilege");
                    }
                } else {
                    // Regular privilege without columns
                    stmt->privileges.push_back(priv);

                    // Handle multi-word privileges by checking for known second words
                    // This handles: ALL PRIVILEGES, SHOW VIEW, CREATE VIEW, LOCK TABLES, GRANT OPTION,
                    // TAKE OWNERSHIP, VIEW DEFINITION, ALTER ANY, BIGQUERY READER/EDITOR/OWNER/VIEWER
                    if (!is_eof() && !check(TK::ON) && !check(TK::COMMA)) {
                        std::string_view next = current().text;
                        bool is_multiword = false;

                        if ((priv == "ALL" || priv == "all") && (next == "PRIVILEGES" || next == "privileges")) {
                            is_multiword = true;
                        } else if ((priv == "SHOW" || priv == "show") && (next == "VIEW" || next == "view")) {
                            is_multiword = true;
                        } else if ((priv == "CREATE" || priv == "create") && (next == "VIEW" || next == "view")) {
                            is_multiword = true;
                        } else if ((priv == "LOCK" || priv == "lock") && (next == "TABLES" || next == "tables")) {
                            is_multiword = true;
                        } else if ((priv == "GRANT" || priv == "grant") && (next == "OPTION" || next == "option")) {
                            is_multiword = true;
                        } else if ((priv == "TAKE" || priv == "take") && (next == "OWNERSHIP" || next == "ownership")) {
                            is_multiword = true;
                        } else if ((priv == "VIEW" || priv == "view") && (next == "DEFINITION" || next == "definition")) {
                            is_multiword = true;
                        } else if ((priv == "ALTER" || priv == "alter") && (next == "ANY" || next == "any")) {
                            // ALTER ANY is a two-word prefix for three-word privileges (ALTER ANY USER, ALTER ANY ROLE)
                            is_multiword = true;
                        } else if ((priv == "BIGQUERY" || priv == "bigquery") &&
                                   (next == "READER" || next == "reader" || next == "EDITOR" || next == "editor" ||
                                    next == "OWNER" || next == "owner" || next == "VIEWER" || next == "viewer")) {
                            is_multiword = true;
                        }

                        if (is_multiword) {
                            stmt->privileges.push_back(advance().text);  // Include second word

                            // Check for three-word privileges like "ALTER ANY USER"
                            if (!is_eof() && !check(TK::ON) && !check(TK::COMMA) &&
                                ((priv == "ALTER" || priv == "alter") || (priv == "GRANT" || priv == "grant"))) {
                                std::string_view third = current().text;
                                if (third == "USER" || third == "user" || third == "ROLE" || third == "role" ||
                                    third == "TABLE" || third == "table" || third == "VIEW" || third == "view" ||
                                    third == "INDEX" || third == "index" || third == "PROCEDURE" || third == "procedure" ||
                                    third == "FUNCTION" || third == "function" || third == "SCHEMA" || third == "schema" ||
                                    third == "DATABASE" || third == "database" || third == "SEQUENCE" || third == "sequence" ||
                                    third == "FOR" || third == "for") {
                                    stmt->privileges.push_back(advance().text);  // Include third word
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
                    !(check(TK::IDENTIFIER) && (current().text == "CASCADE" || current().text == "cascade" ||
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
            } else if (check(TK::IDENTIFIER) && (current().text == "RESTRICT" || current().text == "restrict")) {
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
                (void)advance();  // Skip the DOT token
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
                (void)advance();  // Skip the :: token
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
            (object_parts[0] == "TABLE" || object_parts[0] == "SCHEMA" || object_parts[0] == "DATABASE" ||
             object_parts[0] == "FUNCTION" || object_parts[0] == "PROCEDURE" || object_parts[0] == "SEQUENCE" ||
             object_parts[0] == "LOGIN")) {
            stmt->object_type = object_parts[0];
            // Concatenate remaining parts without spaces (DOT already merged, DOUBLE_COLON preserved)
            std::string name;
            for (size_t i = 1; i < object_parts.size(); ++i) {
                name += object_parts[i];
            }
            // Trim leading/trailing whitespace
            size_t start = 0;
            while (start < name.size() && (name[start] == ' ' || name[start] == '\t')) ++start;
            size_t end = name.size();
            while (end > start && (name[end-1] == ' ' || name[end-1] == '\t')) --end;
            stmt->object_name = this->arena().copy_source(name.substr(start, end - start));
        } else {
            // No object type - use entire range as object name(s)
            stmt->object_name = this->arena().copy_source(std::string(source_.substr(object_start, object_end - object_start)));
        }

        expect(TK::FROM);
        // Parse grantees
        do {
            if (!is_eof() && !check(TK::SEMICOLON) &&
                !(check(TK::IDENTIFIER) && (current().text == "CASCADE" || current().text == "cascade" ||
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
        } else if (check(TK::IDENTIFIER) && (current().text == "RESTRICT" || current().text == "restrict")) {
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
            if ((token_text.size() == 8) &&
                (token_text[0] == 'L' || token_text[0] == 'l') &&
                (token_text[1] == 'A' || token_text[1] == 'a') &&
                (token_text[2] == 'N' || token_text[2] == 'n') &&
                (token_text[3] == 'G' || token_text[3] == 'g') &&
                (token_text[4] == 'U' || token_text[4] == 'u') &&
                (token_text[5] == 'A' || token_text[5] == 'a') &&
                (token_text[6] == 'G' || token_text[6] == 'g') &&
                (token_text[7] == 'E' || token_text[7] == 'e')) {
                (void)advance();  // consume LANGUAGE
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
            stmt->select_query = static_cast<SelectStmt*>(parse_select());
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
            (void)advance();  // consume CACHE
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
        if (!check(TK::IDENTIFIER) && !check(TK::ADD) && !check(TK::COUNT) &&
            !check(TK::SUM) && !check(TK::MAX) && !check(TK::MIN) && !check(TK::AVG)) {
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
                    if (is_eof()) break;
                    if (check(TK::LPAREN)) {
                        paren_depth++;
                    } else if (check(TK::RPAREN)) {
                        if (paren_depth == 0) break;  // Parameter list closing paren
                        paren_depth--;
                    } else if (check(TK::COMMA) && paren_depth == 0) {
                        break;  // Next parameter
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
        if (stmt->is_function && (check(TK::RETURNS) ||
            (check(TK::IDENTIFIER) && (current().text == "RETURNS" || current().text == "returns")))) {
            (void)advance();  // consume RETURNS

            if (!check(TK::AS) && !check(TK::BEGIN) && !is_eof()) {
                // Capture complete return type with parentheses (e.g., VARCHAR(100))
                // Similar to parameter type parsing
                size_t type_start = current().start;
                size_t type_end = type_start;
                int paren_depth = 0;

                while (!is_eof() && !check(TK::AS) && !check(TK::BEGIN) &&
                       !check(TK::LANGUAGE) &&
                       !(check(TK::IDENTIFIER) && (current().text == "LANGUAGE" || current().text == "language"))) {
                    if (check(TK::LPAREN)) {
                        paren_depth++;
                    } else if (check(TK::RPAREN)) {
                        if (paren_depth == 0) break;  // Not part of type
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
        if (check(TK::LANGUAGE) || (check(TK::IDENTIFIER) && (current().text == "LANGUAGE" || current().text == "language"))) {
            (void)advance();  // consume LANGUAGE
            // Accept any token type for the language name (could be keyword like plpgsql, not just IDENTIFIER)
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
        if (match(TK::IF)) {
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
        // Many keywords can be used as identifiers in DECLARE context
        if (check(TK::IDENTIFIER) || check(TK::TEMP) || check(TK::COUNT) ||
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
                    stmt->query = static_cast<SelectStmt*>(parse_select());
                }

                return stmt;
            } else {
                // Variable declaration: DECLARE var_name type [DEFAULT value]
                auto stmt = this->template create_node<DeclareVarStmt>();
                stmt->variable_name = name_tok.text;

                // Parse type - concatenate tokens and copy to arena
                std::string type_str;
                while (!check(TK::SEMICOLON) && !check(TK::DEFAULT) &&
                       !check(TK::EQ) && !check(TK::END) && !check(TK::COLON_EQUALS) && !is_eof()) {
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
        while (!check(TK::END) && !check(TK::ELSE) && !check(TK::ELSEIF) && !check(TK::ENDIF) && !is_eof()) {
            // Skip semicolons
            if (match(TK::SEMICOLON)) {
                continue;
            }
            stmt->then_stmts.push_back(parse_top_level());
        }

        // ELSIF clauses - treat as nested IF in else_stmts (supports multiple)
        while (check(TK::ELSEIF)) {
            (void)advance();
            auto elsif_stmt = this->template create_node<IfStmt>();
            elsif_stmt->condition = parse_expression();
            expect(TK::THEN);

            while (!check(TK::END) && !check(TK::ELSE) && !check(TK::ELSEIF) && !check(TK::ENDIF) && !is_eof()) {
                // Skip semicolons
                if (match(TK::SEMICOLON)) {
                    continue;
                }
                elsif_stmt->then_stmts.push_back(parse_top_level());
            }

            stmt->else_stmts.push_back(elsif_stmt);
        }

        // ELSE clause
        if (match(TK::ELSE)) {
            while (!check(TK::END) && !check(TK::ENDIF) && !is_eof()) {
                // Skip semicolons
                if (match(TK::SEMICOLON)) {
                    continue;
                }
                stmt->else_stmts.push_back(parse_top_level());
            }
        }

        // END IF or ENDIF
        if (match(TK::ENDIF)) {
            // ENDIF (single token) - already consumed
        } else {
            expect(TK::END);
            // Optional IF after END (use IF_KW token type, not IF)
            if (check(TK::IF_KW) || (check(TK::IDENTIFIER) && (current().text == "IF" || current().text == "if"))) {
                (void)advance();
            }
        }

        return stmt;
    }

    WhileLoop* parse_while_loop() {
        auto stmt = this->template create_node<WhileLoop>();
        expect(TK::WHILE);

        stmt->condition = parse_expression();

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
            stmt->body.push_back(parse_top_level());
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

        // Range: start..end
        stmt->start_value = parse_expression();
        expect(TK::DOUBLE_DOT);
        stmt->end_value = parse_expression();

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
            stmt->body.push_back(parse_top_level());
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
            stmt->body.push_back(parse_top_level());
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

            // PostgreSQL RAISE: RAISE level 'message'
            // Level: EXCEPTION, NOTICE, WARNING, INFO, LOG, DEBUG
            if (check(TK::IDENTIFIER) || check(TK::EXCEPTION)) {
                stmt->level = advance().text;
            }

            // Message string
            if (check(TK::STRING)) {
                stmt->message = advance().text;
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

    OpenCursorStmt* parse_open_cursor() {
        auto stmt = this->template create_node<OpenCursorStmt>();
        expect(TK::OPEN);

        if (check(TK::IDENTIFIER)) {
            stmt->cursor_name = advance().text;
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
            !check(TK::ELSE) && !check(TK::ELSEIF) && !check(TK::ENDIF) &&
            !check(TK::ENDLOOP) && !check(TK::ENDWHILE)) {
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
                if (check(TK::ROW) || (check(TK::IDENTIFIER) && (current().text == "ROW" || current().text == "row"))) {
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
                if (check(TK::BEGIN)) depth++;
                else if (check(TK::END)) depth--;
                if (depth > 0) (void)advance();
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
        if (match(TK::IF)) {
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
        if (match(TK::IF)) {
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
                    if (check(TK::LPAREN)) paren_depth++;
                    else if (check(TK::RPAREN)) paren_depth--;
                    if (paren_depth > 0) (void)advance();
                }
                expect(TK::RPAREN);
            }
        }

        // AS SELECT ...
        if (match(TK::AS)) {
            stmt->training_query = static_cast<SelectStmt*>(parse_select());
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
        if (match(TK::IF)) {
            expect(TK::EXISTS);
            stmt->if_exists = true;
        }

        // Model name
        if (check(TK::IDENTIFIER)) {
            stmt->model_name = advance().text;
        }

        return stmt;
    }

    /// Override token_name for better error messages
    [[nodiscard]] std::string token_name(TK type) const override {
        return std::string(libsqlglot::token_type_name(type));
    }

private:
    // ========================================================================
    // Lifetime-Safe Tokenization Helper
    // ========================================================================

    /// Convert SQLDialect to TokenizerConfig
    static libsqlglot::TokenizerConfig dialect_to_tokenizer_config(SQLDialect dialect) noexcept {
        switch (dialect) {
            case SQLDialect::SQLServer:
                return libsqlglot::TokenizerConfig::sqlserver();
            case SQLDialect::MySQL:
                return libsqlglot::TokenizerConfig::mysql();
            case SQLDialect::PostgreSQL:
                return libsqlglot::TokenizerConfig::postgresql();
            case SQLDialect::Snowflake:
                return libsqlglot::TokenizerConfig::snowflake();
            default:
                // Most dialects support # comments (MySQL-style)
                // SQL Server is the exception
                return libsqlglot::TokenizerConfig::default_config();
        }
    }

    struct TokenizeResult {
        std::vector<TokenType> tokens;
        std::string_view source;
    };

    /// Delegating constructor that receives pre-tokenized result
    SQLParser(libglot::Arena& arena, TokenizeResult&& result, SQLDialect dialect)
        : source_(result.source)
        , dialect_(dialect)
        , Base(arena, std::move(result.tokens))
    {}

    /// Copy source into arena and tokenize the arena-owned copy
    /// This ensures all token string_views point to arena memory
    static TokenizeResult tokenize_and_copy(libglot::Arena& arena, std::string_view source, SQLDialect dialect) {
        auto arena_source = arena.copy_source(source);
        auto tokens = tokenize(arena_source, dialect);
        return {std::move(tokens), arena_source};
    }

    // ========================================================================
    // Tokenization (uses libsqlglot's existing tokenizer)
    // ========================================================================

    static std::vector<TokenType> tokenize(std::string_view source, SQLDialect dialect) {
        libsqlglot::LocalStringPool pool;

        // Convert SQLDialect to TokenizerConfig
        libsqlglot::TokenizerConfig config = dialect_to_tokenizer_config(dialect);

        libsqlglot::Tokenizer tokenizer(source, &pool, config);
        auto tokens = tokenizer.tokenize_all();

        // Convert libsqlglot::Token to libglot::Token<TokenKind>
        std::vector<TokenType> result;
        result.reserve(tokens.size());

        for (const auto& tok : tokens) {
            // Extract text from source using start/end offsets (libsqlglot's text is pool-allocated)
            std::string_view token_text = tok.view(source);

            result.push_back(TokenType{
                tok.type,     // type
                tok.start,    // start
                tok.end,      // end
                tok.line,     // line
                tok.col,      // col
                token_text    // text (from source, not pool)
            });
        }

        return result;
    }

    std::string_view source_;
    SQLDialect dialect_;
};

} // namespace libglot::sql
