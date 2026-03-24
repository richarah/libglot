#pragma once

#include "../../../../core/include/libglot/parse/parser.h"
#include "../../../../libsqlglot/include/libsqlglot/tokenizer.h"
#include "grammar.h"
#include "ast_nodes.h"
#include <memory>
#include <algorithm>

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

    explicit SQLParser(libglot::Arena& arena, std::string_view source)
        : SQLParser(arena, tokenize_and_copy(arena, source))
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

        // Array literal: ARRAY[1, 2, 3]
        if (match(TK::ARRAY)) {
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
        if (check(TK::IDENTIFIER) || check(TK::RANK) || check(TK::ORDER) || check(TK::TEMP) ||
            check(TK::COUNT) || check(TK::SUM) || check(TK::AVG) || check(TK::MIN) || check(TK::MAX) ||
            check(TK::DENSE_RANK) || check(TK::ROW_NUMBER) || check(TK::NTILE) ||
            check(TK::LEAD) || check(TK::LAG) || check(TK::FIRST_VALUE) || check(TK::LAST_VALUE) || check(TK::NTH_VALUE) ||
            check(TK::SUBSTRING) || check(TK::SUBSTR) || check(TK::CONCAT_KW) || check(TK::CONCAT_WS) || check(TK::LENGTH) || check(TK::TRIM) ||
            check(TK::UPPER) || check(TK::LOWER) || check(TK::REPLACE) || check(TK::REPLACE_KW) || check(TK::REPLACE_DDB) || check(TK::SPLIT) ||
            check(TK::ROUND) || check(TK::FLOOR) || check(TK::CEIL) || check(TK::ABS) || check(TK::POWER) || check(TK::SQRT)) {
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
                if (!check(TK::IDENTIFIER)) {
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

        // SELECT list
        stmt->columns = parse_select_list();

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
        return parse_list(
            [this]() { return parse_select_item(); },
            TK::FROM  // Terminates when we hit FROM (or EOF)
        );
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
            // Accept identifiers or keywords as aliases (SQL allows keywords as identifiers when used as aliases)
            if (!this->check(TK::IDENTIFIER) && !this->check(TK::RANK) && !this->check(TK::SUM) &&
                !this->check(TK::COUNT) && !this->check(TK::MAX) && !this->check(TK::MIN)) {
                this->error("Expected alias after AS");
            }
            auto alias_tok = this->advance();
            return this->template create_node<Alias>(expr, alias_tok.text);
        }

        return expr;
    }

    /// Parse table reference (simplified for Phase C1)
    TableRef* parse_table_ref() {
        // Accept identifiers or keywords as table names (SQL allows reserved words as identifiers when quoted)
        if (!this->check(TK::IDENTIFIER) && !this->check(TK::TABLE) && !this->check(TK::DUAL)) {
            this->error("Expected table name");
        }

        auto first = this->advance();
        std::string_view first_name = first.text;

        // Check for database.table
        if (this->match(TK::DOT)) {
            if (!this->check(TK::IDENTIFIER) && !this->check(TK::TABLE)) {
                this->error("Expected table name after '.'");
            }
            auto second = this->advance();
            return this->template create_node<TableRef>(first_name, second.text);
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
        auto func = this->template create_node<FunctionCall>(std::string(func_name), args);
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

        // Parse CTEs
        do {
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

        // JOIN?
        while (check(TK::JOIN) || check(TK::INNER) || check(TK::LEFT) ||
               check(TK::RIGHT) || check(TK::FULL) || check(TK::CROSS)) {
            JoinType join_type = JoinType::INNER;

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
                expect(TK::JOIN);
            } else {
                match(TK::JOIN);
            }

            auto right_table = parse_table_or_subquery();

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
        // LATERAL join
        if (check(TK::IDENTIFIER) && (current().text == "LATERAL" || current().text == "lateral")) {
            (void)advance();
            SQLNode* lateral_expr = nullptr;

            if (match(TK::LPAREN)) {
                if (check(TK::SELECT) || check(TK::WITH)) {
                    lateral_expr = parse_select();
                } else {
                    error("Expected SELECT after LATERAL (");
                }
                expect(TK::RPAREN);
            }

            return this->template create_node<LateralJoin>(lateral_expr);
        }

        // Subquery: (SELECT ...) AS alias
        if (match(TK::LPAREN)) {
            if (check(TK::SELECT) || check(TK::WITH)) {
                auto select = parse_select();
                expect(TK::RPAREN);

                // Check for optional alias after subquery
                std::string_view alias;
                if (match(TK::AS)) {
                    if (!check(TK::IDENTIFIER)) {
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

        // Regular table reference
        auto table = parse_table_ref();

        // Check for optional alias: table_name AS alias or table_name alias
        if (match(TK::AS)) {
            if (!check(TK::IDENTIFIER)) {
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
                        if (!check(TK::IDENTIFIER)) {
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
            expect(TK::REPLACE);
            or_replace = true;
        }

        if (check(TK::TABLE)) {
            return parse_create_table();
        } else if (check(TK::VIEW)) {
            return parse_create_view(or_replace);
        } else if (check(TK::INDEX)) {
            return parse_create_index();
        } else if (check(TK::SCHEMA) || check(TK::DATABASE)) {
            return parse_create_schema();
        } else if (check(TK::PROCEDURE) || check(TK::FUNCTION)) {
            return parse_create_procedure(or_replace);
        } else if (check(TK::TRIGGER)) {
            return parse_create_trigger();
        } else if (check(TK::IDENTIFIER) && (current().text == "MODEL" || current().text == "model")) {
            return parse_create_model(or_replace);
        }

        error("Expected TABLE, VIEW, INDEX, SCHEMA, PROCEDURE, FUNCTION, TRIGGER, or MODEL after CREATE");
        return nullptr;
    }

    /// Parse CREATE TABLE (simplified for now)
    CreateTableStmt* parse_create_table() {
        auto stmt = this->template create_node<CreateTableStmt>();
        expect(TK::TABLE);

        // IF NOT EXISTS?
        if (match(TK::IF)) {
            expect(TK::NOT);
            expect(TK::EXISTS);
            stmt->if_not_exists = true;
        }

        // Table name
        stmt->table = parse_table_ref();

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
        } else if (check(TK::PROCEDURE) || check(TK::FUNCTION)) {
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
        if (match(TK::IF)) {
            expect(TK::EXISTS);
            stmt->if_exists = true;
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

        // Otherwise, this is a BEGIN...END block with statements
        auto block = this->template create_node<BeginEndBlock>();

        // Parse statements until END
        while (!check(TK::END) && !is_eof()) {
            // Skip semicolons
            if (match(TK::SEMICOLON)) {
                continue;
            }
            block->statements.push_back(parse_top_level());
        }

        expect(TK::END);

        return block;
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
        if (match(TK::VERBOSE)) {
            stmt->verbose = true;
        }
        if (check(TK::IDENTIFIER)) {
            stmt->table = advance().text;
        }
        return stmt;
    }

    VacuumStmt* parse_vacuum() {
        auto stmt = this->template create_node<VacuumStmt>();
        expect(TK::VACUUM);
        if (match(TK::FULL)) {
            stmt->full = true;
        }
        if (match(TK::ANALYZE)) {
            stmt->analyze = true;
        }
        if (check(TK::IDENTIFIER)) {
            stmt->table = advance().text;
        }
        return stmt;
    }

    GrantStmt* parse_grant() {
        auto stmt = this->template create_node<GrantStmt>();
        expect(TK::GRANT);
        // Parse privileges
        do {
            if (check(TK::IDENTIFIER)) {
                stmt->privileges.push_back(advance().text);
            }
        } while (match(TK::COMMA));
        expect(TK::ON);
        if (check(TK::IDENTIFIER)) {
            stmt->object_type = advance().text;
        }
        if (check(TK::IDENTIFIER)) {
            stmt->object_name = advance().text;
        }
        // Expect TO keyword (as identifier)
        if (check(TK::IDENTIFIER) && (current().text == "TO" || current().text == "to")) {
            (void)advance();
        }
        do {
            if (check(TK::IDENTIFIER)) {
                stmt->grantees.push_back(advance().text);
            }
        } while (match(TK::COMMA));
        if (match(TK::WITH)) {
            expect(TK::GRANT);
            // WITH GRANT OPTION (OPTION will be identifier)
            if (check(TK::IDENTIFIER) && (current().text == "OPTION" || current().text == "option")) {
                (void)advance();
                stmt->with_grant_option = true;
            }
        }
        return stmt;
    }

    RevokeStmt* parse_revoke() {
        auto stmt = this->template create_node<RevokeStmt>();
        expect(TK::REVOKE);
        // Parse privileges
        do {
            if (check(TK::IDENTIFIER)) {
                stmt->privileges.push_back(advance().text);
            }
        } while (match(TK::COMMA));
        expect(TK::ON);
        if (check(TK::IDENTIFIER)) {
            stmt->object_type = advance().text;
        }
        if (check(TK::IDENTIFIER)) {
            stmt->object_name = advance().text;
        }
        expect(TK::FROM);
        do {
            if (check(TK::IDENTIFIER)) {
                stmt->grantees.push_back(advance().text);
            }
        } while (match(TK::COMMA));
        // Check for CASCADE keyword (as identifier)
        if (check(TK::IDENTIFIER) && (current().text == "CASCADE" || current().text == "cascade")) {
            (void)advance();
            stmt->cascade = true;
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

    // ========================================================================
    // Stored Procedure/Function Parsers
    // ========================================================================

    CreateProcedureStmt* parse_create_procedure(bool or_replace) {
        auto stmt = this->template create_node<CreateProcedureStmt>();
        stmt->or_replace = or_replace;

        // PROCEDURE or FUNCTION
        if (match(TK::PROCEDURE)) {
            stmt->is_function = false;
        } else if (match(TK::FUNCTION)) {
            stmt->is_function = true;
        } else {
            error("Expected PROCEDURE or FUNCTION");
        }

        // Procedure/function name
        if (!check(TK::IDENTIFIER)) {
            error("Expected procedure/function name");
        }
        stmt->name = advance().text;

        // Parameters: (param1 type, param2 type, ...) - just skip for now
        expect(TK::LPAREN);
        int paren_depth = 1;
        while (paren_depth > 0 && !is_eof()) {
            if (check(TK::LPAREN)) paren_depth++;
            else if (check(TK::RPAREN)) paren_depth--;
            if (paren_depth > 0) (void)advance();
        }
        expect(TK::RPAREN);

        // RETURNS type (for functions) - store first token as return_type
        if (stmt->is_function && check(TK::IDENTIFIER) &&
            (current().text == "RETURNS" || current().text == "returns")) {
            (void)advance();
            if (!check(TK::AS) && !check(TK::BEGIN) && !is_eof()) {
                stmt->return_type = advance().text;
            }
            // Skip remaining type tokens
            while (!check(TK::AS) && !check(TK::BEGIN) && !is_eof()) {
                (void)advance();
            }
        }

        // AS or BEGIN keyword
        if (!match(TK::AS)) {
            (void)match(TK::BEGIN);
        }

        // Body (for now, just skip to END - full parsing would be recursive)
        int depth = 1;
        while (depth > 0 && !is_eof()) {
            if (check(TK::BEGIN)) depth++;
            else if (check(TK::END)) depth--;
            if (depth > 0) (void)advance();
        }
        expect(TK::END);

        return stmt;
    }

    DropProcedureStmt* parse_drop_procedure() {
        auto stmt = this->template create_node<DropProcedureStmt>();

        // PROCEDURE or FUNCTION
        if (match(TK::PROCEDURE)) {
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

            if (check(TK::IDENTIFIER) &&
                (current().text == "CURSOR" || current().text == "cursor")) {
                // Cursor declaration
                auto stmt = this->template create_node<DeclareCursorStmt>();
                stmt->cursor_name = name_tok.text;
                (void)advance(); // consume CURSOR

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

                // Parse type - copy CAST pattern exactly but without spaces
                std::string type_str;
                while (!check(TK::SEMICOLON) && !check(TK::DEFAULT) &&
                       !check(TK::EQ) && !check(TK::END) && !check(TK::COLON_EQUALS) && !is_eof()) {
                    if (!current().text.empty()) {
                        type_str += std::string(current().text);
                    }
                    (void)advance();
                }
                stmt->type = type_str;

                // DEFAULT value?
                if (match(TK::DEFAULT) || match(TK::EQ)) {
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

        // NEXT keyword (optional)
        if (check(TK::IDENTIFIER) &&
            (current().text == "NEXT" || current().text == "next")) {
            (void)advance();
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
        if (!check(TK::END) && !check(TK::SEMICOLON) && !is_eof() &&
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

    struct TokenizeResult {
        std::vector<TokenType> tokens;
        std::string_view source;
    };

    /// Delegating constructor that receives pre-tokenized result
    SQLParser(libglot::Arena& arena, TokenizeResult&& result)
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
    // Tokenization (uses libsqlglot's existing tokenizer)
    // ========================================================================

    static std::vector<TokenType> tokenize(std::string_view source) {
        libsqlglot::LocalStringPool pool;
        libsqlglot::Tokenizer tokenizer(source, &pool);
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
};

} // namespace libglot::sql
