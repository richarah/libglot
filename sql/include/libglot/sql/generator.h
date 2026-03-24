#pragma once

#include "../../../../core/include/libglot/gen/generator.h"
#include "dialect_traits.h"
#include "ast_nodes.h"
#include "grammar.h"
#include <sstream>

namespace libglot::sql {

/// ============================================================================
/// SQL Generator Specification (for GeneratorBase template)
/// ============================================================================

struct SQLGeneratorSpec {
    using AstNodeType = SQLNode;
    using NodeKind = SQLNodeKind;
    using DialectTraitsType = SQLDialectTraits;
};

/// ============================================================================
/// SQL Generator - CRTP Instantiation of GeneratorBase
/// ============================================================================
///
/// Emits SQL text from AST nodes with dialect-specific formatting.
/// For Phase C1, supports generating:
///   SELECT col AS alias FROM table WHERE col = 1 ORDER BY col LIMIT 10
///
/// Validates that dialect dispatch works correctly (e.g., ` vs " for identifiers).
/// ============================================================================

class SQLGenerator : public libglot::GeneratorBase<SQLGeneratorSpec, SQLGenerator> {
public:
    using Base = libglot::GeneratorBase<SQLGeneratorSpec, SQLGenerator>;
    using TK = libsqlglot::TokenType;  // Using libsqlglot for Phase A (shim)

    // Expose base class public methods
    using Base::generate;
    using Base::reset;

    // Explicit constructor (CRTP doesn't always play nice with using Base::Base)
    explicit SQLGenerator(SQLDialect dialect, const typename Base::Options& opts = typename Base::Options{})
        : Base(dialect, opts)
    {}

    // ========================================================================
    // Main Visitor Dispatch (Required by GeneratorBase)
    // ========================================================================

    void visit(SQLNode* node) {
        if (!node) return;

        switch (node->type) {
            // ================================================================
            // Expressions
            // ================================================================
            case SQLNodeKind::COLUMN:
                visit_column(static_cast<Column*>(node));
                break;

            case SQLNodeKind::LITERAL:
                visit_literal(static_cast<Literal*>(node));
                break;

            case SQLNodeKind::STAR:
                visit_star(static_cast<Star*>(node));
                break;

            case SQLNodeKind::PARAMETER:
                visit_parameter(static_cast<Parameter*>(node));
                break;

            case SQLNodeKind::BINARY_OP:
                visit_binary_op(static_cast<BinaryOp*>(node));
                break;

            case SQLNodeKind::UNARY_OP:
                visit_unary_op(static_cast<UnaryOp*>(node));
                break;

            case SQLNodeKind::FUNCTION_CALL:
                visit_function_call(static_cast<FunctionCall*>(node));
                break;

            case SQLNodeKind::CASE_EXPR:
                visit_case_expr(static_cast<CaseExpr*>(node));
                break;

            case SQLNodeKind::CAST_EXPR:
                visit_cast_expr(static_cast<CastExpr*>(node));
                break;

            case SQLNodeKind::COALESCE_EXPR:
                visit_coalesce_expr(static_cast<CoalesceExpr*>(node));
                break;

            case SQLNodeKind::NULLIF_EXPR:
                visit_nullif_expr(static_cast<NullifExpr*>(node));
                break;

            case SQLNodeKind::BETWEEN_EXPR:
                visit_between_expr(static_cast<BetweenExpr*>(node));
                break;

            case SQLNodeKind::IN_EXPR:
                visit_in_expr(static_cast<InExpr*>(node));
                break;

            case SQLNodeKind::EXISTS_EXPR:
                visit_exists_expr(static_cast<ExistsExpr*>(node));
                break;

            case SQLNodeKind::SUBQUERY_EXPR:
                visit_subquery_expr(static_cast<SubqueryExpr*>(node));
                break;

            case SQLNodeKind::WINDOW_FUNCTION:
                visit_window_function(static_cast<WindowFunction*>(node));
                break;

            case SQLNodeKind::WINDOW_SPEC:
                visit_window_spec(static_cast<WindowSpec*>(node));
                break;

            case SQLNodeKind::ALIAS:
                visit_alias(static_cast<Alias*>(node));
                break;

            case SQLNodeKind::ANY_EXPR:
                visit_any_expr(static_cast<AnyExpr*>(node));
                break;

            case SQLNodeKind::ALL_EXPR:
                visit_all_expr(static_cast<AllExpr*>(node));
                break;

            case SQLNodeKind::ARRAY_LITERAL:
                visit_array_literal(static_cast<ArrayLiteral*>(node));
                break;

            case SQLNodeKind::ARRAY_INDEX:
                visit_array_index(static_cast<ArrayIndex*>(node));
                break;

            case SQLNodeKind::JSON_EXPR:
                visit_json_expr(static_cast<JsonExpr*>(node));
                break;

            case SQLNodeKind::REGEX_MATCH:
                visit_regex_match(static_cast<RegexMatch*>(node));
                break;

            // ================================================================
            // FROM Clause Elements
            // ================================================================
            case SQLNodeKind::TABLE_REF:
                visit_table_ref(static_cast<TableRef*>(node));
                break;

            case SQLNodeKind::JOIN_CLAUSE:
                visit_join_clause(static_cast<JoinClause*>(node));
                break;

            case SQLNodeKind::LATERAL_JOIN:
                visit_lateral_join(static_cast<LateralJoin*>(node));
                break;

            case SQLNodeKind::VALUES_CLAUSE:
                visit_values_clause(static_cast<ValuesClause*>(node));
                break;

            case SQLNodeKind::TABLESAMPLE:
                visit_tablesample(static_cast<Tablesample*>(node));
                break;

            // ================================================================
            // Query Structure
            // ================================================================
            case SQLNodeKind::SELECT_STMT:
                visit_select_stmt(static_cast<SelectStmt*>(node));
                break;

            case SQLNodeKind::CTE:
                visit_cte(static_cast<CTE*>(node));
                break;

            case SQLNodeKind::ORDER_BY_ITEM:
                visit_order_by_item(static_cast<OrderByItem*>(node));
                break;

            // ================================================================
            // Set Operations
            // ================================================================
            case SQLNodeKind::UNION_STMT:
                visit_union_stmt(static_cast<UnionStmt*>(node));
                break;

            case SQLNodeKind::INTERSECT_STMT:
                visit_intersect_stmt(static_cast<IntersectStmt*>(node));
                break;

            case SQLNodeKind::EXCEPT_STMT:
                visit_except_stmt(static_cast<ExceptStmt*>(node));
                break;

            // ================================================================
            // DML Statements
            // ================================================================
            case SQLNodeKind::INSERT_STMT:
                visit_insert_stmt(static_cast<InsertStmt*>(node));
                break;

            case SQLNodeKind::UPDATE_STMT:
                visit_update_stmt(static_cast<UpdateStmt*>(node));
                break;

            case SQLNodeKind::DELETE_STMT:
                visit_delete_stmt(static_cast<DeleteStmt*>(node));
                break;

            case SQLNodeKind::MERGE_STMT:
                visit_merge_stmt(static_cast<MergeStmt*>(node));
                break;

            case SQLNodeKind::TRUNCATE_STMT:
                visit_truncate_stmt(static_cast<TruncateStmt*>(node));
                break;

            // ================================================================
            // DDL Statements
            // ================================================================
            case SQLNodeKind::CREATE_TABLE_STMT:
                visit_create_table_stmt(static_cast<CreateTableStmt*>(node));
                break;

            case SQLNodeKind::CREATE_VIEW_STMT:
                visit_create_view_stmt(static_cast<CreateViewStmt*>(node));
                break;

            case SQLNodeKind::CREATE_INDEX_STMT:
                visit_create_index_stmt(static_cast<CreateIndexStmt*>(node));
                break;

            case SQLNodeKind::CREATE_SCHEMA_STMT:
                visit_create_schema_stmt(static_cast<CreateSchemaStmt*>(node));
                break;

            case SQLNodeKind::DROP_TABLE_STMT:
                visit_drop_table_stmt(static_cast<DropTableStmt*>(node));
                break;

            case SQLNodeKind::DROP_VIEW_STMT:
                visit_drop_view_stmt(static_cast<DropViewStmt*>(node));
                break;

            case SQLNodeKind::DROP_INDEX_STMT:
                visit_drop_index_stmt(static_cast<DropIndexStmt*>(node));
                break;

            case SQLNodeKind::DROP_SCHEMA_STMT:
                visit_drop_schema_stmt(static_cast<DropSchemaStmt*>(node));
                break;

            case SQLNodeKind::ALTER_TABLE_STMT:
                visit_alter_table_stmt(static_cast<AlterTableStmt*>(node));
                break;

            case SQLNodeKind::COLUMN_DEF:
                visit_column_def(static_cast<ColumnDef*>(node));
                break;

            case SQLNodeKind::TABLE_CONSTRAINT:
                visit_table_constraint(static_cast<TableConstraint*>(node));
                break;

            case SQLNodeKind::CREATE_TABLESPACE_STMT:
                visit_create_tablespace_stmt(static_cast<CreateTablespaceStmt*>(node));
                break;

            case SQLNodeKind::PARTITION_SPEC:
                visit_partition_spec(static_cast<PartitionSpec*>(node));
                break;

            case SQLNodeKind::CREATE_INDEX_ADV:
                visit_create_index_adv(static_cast<CreateIndexAdv*>(node));
                break;

            // ================================================================
            // Transaction Statements
            // ================================================================
            case SQLNodeKind::BEGIN_STMT:
                visit_begin_stmt(static_cast<BeginStmt*>(node));
                break;

            case SQLNodeKind::COMMIT_STMT:
                visit_commit_stmt(static_cast<CommitStmt*>(node));
                break;

            case SQLNodeKind::ROLLBACK_STMT:
                visit_rollback_stmt(static_cast<RollbackStmt*>(node));
                break;

            case SQLNodeKind::SAVEPOINT_STMT:
                visit_savepoint_stmt(static_cast<SavepointStmt*>(node));
                break;

            // ================================================================
            // Utility Statements
            // ================================================================
            case SQLNodeKind::SET_STMT:
                visit_set_stmt(static_cast<SetStmt*>(node));
                break;

            case SQLNodeKind::SHOW_STMT:
                visit_show_stmt(static_cast<ShowStmt*>(node));
                break;

            case SQLNodeKind::DESCRIBE_STMT:
                visit_describe_stmt(static_cast<DescribeStmt*>(node));
                break;

            case SQLNodeKind::EXPLAIN_STMT:
                visit_explain_stmt(static_cast<ExplainStmt*>(node));
                break;

            case SQLNodeKind::ANALYZE_STMT:
                visit_analyze_stmt(static_cast<AnalyzeStmt*>(node));
                break;

            case SQLNodeKind::VACUUM_STMT:
                visit_vacuum_stmt(static_cast<VacuumStmt*>(node));
                break;

            case SQLNodeKind::GRANT_STMT:
                visit_grant_stmt(static_cast<GrantStmt*>(node));
                break;

            case SQLNodeKind::REVOKE_STMT:
                visit_revoke_stmt(static_cast<RevokeStmt*>(node));
                break;

            // ================================================================
            // Stored Procedures & Functions
            // ================================================================
            case SQLNodeKind::CREATE_PROCEDURE_STMT:
                visit_create_procedure_stmt(static_cast<CreateProcedureStmt*>(node));
                break;

            case SQLNodeKind::DROP_PROCEDURE_STMT:
                visit_drop_procedure_stmt(static_cast<DropProcedureStmt*>(node));
                break;

            case SQLNodeKind::CALL_PROCEDURE_STMT:
                visit_call_procedure_stmt(static_cast<CallProcedureStmt*>(node));
                break;

            case SQLNodeKind::DECLARE_VAR_STMT:
                visit_declare_var_stmt(static_cast<DeclareVarStmt*>(node));
                break;

            case SQLNodeKind::DECLARE_CURSOR_STMT:
                visit_declare_cursor_stmt(static_cast<DeclareCursorStmt*>(node));
                break;

            case SQLNodeKind::ASSIGNMENT_STMT:
                visit_assignment_stmt(static_cast<AssignmentStmt*>(node));
                break;

            case SQLNodeKind::RETURN_STMT:
                visit_return_stmt(static_cast<ReturnStmt*>(node));
                break;

            case SQLNodeKind::IF_STMT:
                visit_if_stmt(static_cast<IfStmt*>(node));
                break;

            case SQLNodeKind::WHILE_LOOP:
                visit_while_loop(static_cast<WhileLoop*>(node));
                break;

            case SQLNodeKind::FOR_LOOP:
                visit_for_loop(static_cast<ForLoop*>(node));
                break;

            case SQLNodeKind::LOOP_STMT:
                visit_loop_stmt(static_cast<LoopStmt*>(node));
                break;

            case SQLNodeKind::BREAK_STMT:
                visit_break_stmt(static_cast<BreakStmt*>(node));
                break;

            case SQLNodeKind::CONTINUE_STMT:
                visit_continue_stmt(static_cast<ContinueStmt*>(node));
                break;

            case SQLNodeKind::BEGIN_END_BLOCK:
                visit_begin_end_block(static_cast<BeginEndBlock*>(node));
                break;

            case SQLNodeKind::DO_BLOCK:
                visit_do_block(static_cast<DoBlock*>(node));
                break;

            case SQLNodeKind::EXCEPTION_BLOCK:
                visit_exception_block(static_cast<ExceptionBlock*>(node));
                break;

            case SQLNodeKind::RAISE_STMT:
                visit_raise_stmt(static_cast<RaiseStmt*>(node));
                break;

            case SQLNodeKind::OPEN_CURSOR_STMT:
                visit_open_cursor_stmt(static_cast<OpenCursorStmt*>(node));
                break;

            case SQLNodeKind::FETCH_CURSOR_STMT:
                visit_fetch_cursor_stmt(static_cast<FetchCursorStmt*>(node));
                break;

            case SQLNodeKind::CLOSE_CURSOR_STMT:
                visit_close_cursor_stmt(static_cast<CloseCursorStmt*>(node));
                break;

            case SQLNodeKind::DELIMITER_STMT:
                visit_delimiter_stmt(static_cast<DelimiterStmt*>(node));
                break;

            // ================================================================
            // Triggers
            // ================================================================
            case SQLNodeKind::CREATE_TRIGGER_STMT:
                visit_create_trigger_stmt(static_cast<CreateTriggerStmt*>(node));
                break;

            case SQLNodeKind::DROP_TRIGGER_STMT:
                visit_drop_trigger_stmt(static_cast<DropTriggerStmt*>(node));
                break;

            // ================================================================
            // Advanced Features
            // ================================================================
            case SQLNodeKind::PIVOT_CLAUSE:
                visit_pivot_clause(static_cast<PivotClause*>(node));
                break;

            case SQLNodeKind::UNPIVOT_CLAUSE:
                visit_unpivot_clause(static_cast<UnpivotClause*>(node));
                break;

            // ================================================================
            // BigQuery ML
            // ================================================================
            case SQLNodeKind::CREATE_MODEL_STMT:
                visit_create_model_stmt(static_cast<CreateModelStmt*>(node));
                break;

            case SQLNodeKind::DROP_MODEL_STMT:
                visit_drop_model_stmt(static_cast<DropModelStmt*>(node));
                break;

            case SQLNodeKind::ML_PREDICT_EXPR:
                visit_ml_predict_expr(static_cast<MLPredictExpr*>(node));
                break;

            case SQLNodeKind::ML_EVALUATE_EXPR:
                visit_ml_evaluate_expr(static_cast<MLEvaluateExpr*>(node));
                break;

            case SQLNodeKind::ML_TRAINING_INFO_EXPR:
                visit_ml_training_info_expr(static_cast<MLTrainingInfoExpr*>(node));
                break;

            default:
                // Unknown node type - skip
                break;
        }
    }

    // ========================================================================
    // Dialect-Aware Identifier Quoting (Override from Base)
    // ========================================================================

    void write_identifier(std::string_view ident) {
        const auto& feat = this->features();
        const char quote = feat.identifier_quote;

        if (quote == '[') {
            // SQL Server style: [identifier]
            this->write('[');
            this->write(ident);
            this->write(']');
        } else {
            // Standard/MySQL/Postgres style: "identifier" or `identifier`
            this->write(quote);
            this->write(ident);
            this->write(quote);
        }
    }

    // ========================================================================
    // Node-Specific Visit Methods
    // ========================================================================

private:
    void visit_column(Column* col) {
        if (!col->table.empty()) {
            write_identifier(col->table);
            write('.');
        }
        write_identifier(col->column);
    }

    void visit_literal(Literal* lit) {
        std::string_view val = lit->value;

        // Handle special keywords
        if (val == "NULL") {
            this->write("NULL");
            return;
        }

        if (val == "TRUE") {
            this->write(this->features().true_literal);
            return;
        }

        if (val == "FALSE") {
            this->write(this->features().false_literal);
            return;
        }

        // Check if it's already quoted (string literals from parser)
        if (!val.empty() && val[0] == '\'') {
            this->write(val);  // Already quoted
            return;
        }

        // Check if it's a number (simple heuristic)
        bool is_number = true;
        for (char c : val) {
            if (!(c >= '0' && c <= '9') && c != '.' && c != '-' && c != 'e' && c != 'E') {
                is_number = false;
                break;
            }
        }

        if (is_number) {
            this->write(val);  // Emit as-is
        } else {
            // Quote as string literal
            this->write('\'');
            this->write(val);
            this->write('\'');
        }
    }

    void visit_binary_op(BinaryOp* op) {
        visit(op->left);
        this->space();
        this->write(operator_string(op->op));
        this->space();
        visit(op->right);
    }

    void visit_alias(Alias* alias) {
        visit(alias->expr);
        this->space();
        this->write("AS");
        this->space();
        write_identifier(alias->alias);
    }

    void visit_table_ref(TableRef* tbl) {
        if (!tbl->database.empty()) {
            write_identifier(tbl->database);
            this->write('.');
        }
        write_identifier(tbl->table);

        // Output alias if present
        if (!tbl->alias.empty()) {
            this->space();
            this->write("AS");
            this->space();
            write_identifier(tbl->alias);
        }
    }

    void visit_order_by_item(OrderByItem* item) {
        visit(item->expr);
        if (!item->ascending) {
            this->space();
            this->write("DESC");
        }
        // ASC is default, no need to emit
    }

    void visit_select_stmt(SelectStmt* stmt) {
        // WITH clause (CTEs)
        if (stmt->with && !stmt->with->ctes.empty()) {
            this->write("WITH");
            this->space();
            this->write_list(stmt->with->ctes, [this](CTE* cte) {
                visit(cte);
            });
            this->space();
        }

        this->write("SELECT");
        this->space();

        // Columns
        this->write_list(stmt->columns, [this](SQLNode* col) {
            visit(col);
        });

        // FROM clause
        if (stmt->from) {
            this->space();
            this->write("FROM");
            this->space();
            visit(stmt->from);
        }

        // WHERE clause
        if (stmt->where) {
            this->space();
            this->write("WHERE");
            this->space();
            visit(stmt->where);
        }

        // GROUP BY clause
        if (!stmt->group_by.empty()) {
            this->space();
            this->write("GROUP BY");
            this->space();
            this->write_list(stmt->group_by, [this](SQLNode* expr) {
                visit(expr);
            });
        }

        // HAVING clause
        if (stmt->having) {
            this->space();
            this->write("HAVING");
            this->space();
            visit(stmt->having);
        }

        // ORDER BY clause
        if (!stmt->order_by.empty()) {
            this->space();
            this->write("ORDER BY");
            this->space();
            this->write_list(stmt->order_by, [this](OrderByItem* item) {
                visit(item);
            });
        }

        // LIMIT clause
        if (stmt->limit) {
            this->space();
            this->write("LIMIT");
            this->space();
            visit(stmt->limit);
        }

        // OFFSET clause
        if (stmt->offset) {
            this->space();
            this->write("OFFSET");
            this->space();
            visit(stmt->offset);
        }
    }

    // ========================================================================
    // Expression Visitors
    // ========================================================================

    void visit_star(Star* star) {
        if (!star->table.empty()) {
            write_identifier(star->table);
            this->write('.');
        }
        this->write('*');
    }

    void visit_parameter(Parameter* param) {
        this->write(param->name);
    }

    void visit_unary_op(UnaryOp* op) {
        const char* op_str = unary_operator_string(op->op);

        // Always use prefix notation
        this->write(op_str);
        this->space();
        visit(op->operand);
    }

    void visit_function_call(FunctionCall* func) {
        this->write(func->name);
        this->write('(');

        if (func->distinct) {
            this->write("DISTINCT");
            this->space();
        }

        this->write_list(func->args, [this](SQLNode* arg) {
            visit(arg);
        });

        this->write(')');
    }

    void visit_case_expr(CaseExpr* case_expr) {
        this->write("CASE");

        if (case_expr->case_value) {
            this->space();
            visit(case_expr->case_value);
        }

        // when_clauses is vector<pair<SQLNode*, SQLNode*>>
        for (const auto& when : case_expr->when_clauses) {
            this->space();
            this->write("WHEN");
            this->space();
            visit(when.first);  // condition
            this->space();
            this->write("THEN");
            this->space();
            visit(when.second);  // result
        }

        if (case_expr->else_expr) {
            this->space();
            this->write("ELSE");
            this->space();
            visit(case_expr->else_expr);
        }

        this->space();
        this->write("END");
    }

    void visit_cast_expr(CastExpr* cast) {
        this->write("CAST");
        this->write('(');
        visit(cast->expr);
        this->space();
        this->write("AS");
        this->space();
        this->write(cast->target_type);
        this->write(')');
    }

    void visit_coalesce_expr(CoalesceExpr* coalesce) {
        this->write("COALESCE");
        this->write('(');
        this->write_list(coalesce->args, [this](SQLNode* arg) {
            visit(arg);
        });
        this->write(')');
    }

    void visit_nullif_expr(NullifExpr* nullif) {
        this->write("NULLIF");
        this->write('(');
        visit(nullif->expr1);
        this->write(',');
        this->space();
        visit(nullif->expr2);
        this->write(')');
    }

    void visit_between_expr(BetweenExpr* between) {
        visit(between->expr);
        this->space();
        if (between->not_between) {
            this->write("NOT");
            this->space();
        }
        this->write("BETWEEN");
        this->space();
        visit(between->lower);
        this->space();
        this->write("AND");
        this->space();
        visit(between->upper);
    }

    void visit_in_expr(InExpr* in_expr) {
        visit(in_expr->expr);
        this->space();
        if (in_expr->not_in) {
            this->write("NOT");
            this->space();
        }
        this->write("IN");
        this->space();
        this->write('(');

        // values vector may contain a subquery or literal values
        this->write_list(in_expr->values, [this](SQLNode* val) {
            visit(val);
        });

        this->write(')');
    }

    void visit_exists_expr(ExistsExpr* exists) {
        this->write("EXISTS");
        this->space();
        this->write('(');
        visit(exists->subquery);
        this->write(')');
    }

    void visit_subquery_expr(SubqueryExpr* subquery) {
        this->write('(');
        visit(subquery->query);
        this->write(')');
    }

    void visit_window_function(WindowFunction* wf) {
        this->write(wf->function_name);
        this->write('(');

        this->write_list(wf->args, [this](SQLNode* arg) {
            visit(arg);
        });

        this->write(')');
        this->space();
        this->write("OVER");
        this->space();

        if (wf->over) {
            visit(wf->over);
        } else {
            this->write("()");
        }
    }

    void visit_window_spec(WindowSpec* spec) {
        this->write('(');

        bool need_space = false;

        // PARTITION BY
        if (!spec->partition_by.empty()) {
            this->write("PARTITION BY");
            this->space();
            this->write_list(spec->partition_by, [this](SQLNode* expr) {
                visit(expr);
            });
            need_space = true;
        }

        // ORDER BY
        if (!spec->order_by.empty()) {
            if (need_space) this->space();
            this->write("ORDER BY");
            this->space();
            this->write_list(spec->order_by, [this](SQLNode* expr) {
                visit(expr);  // Will dispatch to visit_order_by_item if it's an OrderByItem
            });
            need_space = true;
        }

        // Frame clause (ROWS/RANGE)
        if (spec->frame) {
            if (need_space) this->space();

            switch (spec->frame->frame_type) {
                case FrameType::ROWS:
                    this->write("ROWS");
                    break;
                case FrameType::RANGE:
                    this->write("RANGE");
                    break;
                case FrameType::GROUPS:
                    this->write("GROUPS");
                    break;
            }

            // Frame bounds (simplified - just write the spec)
            this->space();
            this->write("BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW");
        }

        this->write(')');
    }

    void visit_cte(CTE* cte) {
        write_identifier(cte->name);

        // Column list
        if (!cte->columns.empty()) {
            this->space();
            this->write('(');
            this->write_list(cte->columns, [this](std::string_view col) {
                write_identifier(col);
            });
            this->write(')');
        }

        this->space();
        this->write("AS");
        this->space();
        this->write('(');
        visit(cte->query);
        this->write(')');
    }

    // ========================================================================
    // FROM Clause Visitors
    // ========================================================================

    void visit_join_clause(JoinClause* join) {
        visit(join->left_table);
        this->space();

        // Join type
        switch (join->join_type) {
            case JoinType::INNER:
                this->write("INNER JOIN");
                break;
            case JoinType::LEFT:
                this->write("LEFT JOIN");
                break;
            case JoinType::RIGHT:
                this->write("RIGHT JOIN");
                break;
            case JoinType::FULL:
                this->write("FULL JOIN");
                break;
            case JoinType::CROSS:
                this->write("CROSS JOIN");
                break;
            default:
                this->write("JOIN");
                break;
        }

        this->space();
        visit(join->right_table);

        // ON/USING condition
        if (join->condition) {
            this->space();
            this->write("ON");
            this->space();
            visit(join->condition);
        }
    }

    // ========================================================================
    // Set Operation Visitors
    // ========================================================================

    void visit_union_stmt(UnionStmt* union_stmt) {
        visit(union_stmt->left);
        this->space();
        this->write("UNION");
        if (union_stmt->all) {
            this->space();
            this->write("ALL");
        }
        this->space();
        visit(union_stmt->right);
    }

    void visit_intersect_stmt(IntersectStmt* intersect) {
        visit(intersect->left);
        this->space();
        this->write("INTERSECT");
        if (intersect->all) {
            this->space();
            this->write("ALL");
        }
        this->space();
        visit(intersect->right);
    }

    void visit_except_stmt(ExceptStmt* except) {
        visit(except->left);
        this->space();
        this->write("EXCEPT");
        if (except->all) {
            this->space();
            this->write("ALL");
        }
        this->space();
        visit(except->right);
    }

    // ========================================================================
    // DML Statement Visitors
    // ========================================================================

    void visit_insert_stmt(InsertStmt* stmt) {
        this->write("INSERT INTO");
        this->space();
        visit(stmt->table);

        // Column list
        if (!stmt->columns.empty()) {
            this->space();
            this->write('(');
            this->write_list(stmt->columns, [this](std::string_view col) {
                write_identifier(col);
            });
            this->write(')');
        }

        this->space();

        // VALUES or SELECT
        if (stmt->select_query) {
            visit(stmt->select_query);
        } else if (!stmt->values.empty()) {
            this->write("VALUES");
            this->space();
            this->write_list(stmt->values, [this](const std::vector<SQLNode*>& row) {
                this->write('(');
                this->write_list(row, [this](SQLNode* val) {
                    visit(val);
                });
                this->write(')');
            });
        }
    }

    void visit_update_stmt(UpdateStmt* stmt) {
        this->write("UPDATE");
        this->space();
        visit(stmt->table);
        this->space();
        this->write("SET");
        this->space();

        // SET assignments (std::pair<string_view, SQLNode*>)
        this->write_list(stmt->assignments, [this](const auto& assign) {
            write_identifier(assign.first);  // column name
            this->space();
            this->write('=');
            this->space();
            visit(assign.second);  // value
        });

        // FROM clause (PostgreSQL)
        if (stmt->from) {
            this->space();
            this->write("FROM");
            this->space();
            visit(stmt->from);
        }

        // WHERE clause
        if (stmt->where) {
            this->space();
            this->write("WHERE");
            this->space();
            visit(stmt->where);
        }
    }

    void visit_delete_stmt(DeleteStmt* stmt) {
        this->write("DELETE FROM");
        this->space();
        visit(stmt->table);

        // USING clause (PostgreSQL)
        if (stmt->using_clause) {
            this->space();
            this->write("USING");
            this->space();
            visit(stmt->using_clause);
        }

        // WHERE clause
        if (stmt->where) {
            this->space();
            this->write("WHERE");
            this->space();
            visit(stmt->where);
        }
    }

    void visit_merge_stmt(MergeStmt* stmt) {
        this->write("MERGE INTO");
        this->space();
        visit(stmt->target);
        this->space();
        this->write("USING");
        this->space();
        visit(stmt->source);
        this->space();
        this->write("ON");
        this->space();
        visit(stmt->on_condition);

        // WHEN MATCHED (UPDATE) - assignments are std::pair<string_view, SQLNode*>
        if (!stmt->update_assignments.empty()) {
            this->space();
            this->write("WHEN MATCHED THEN UPDATE SET");
            this->space();
            this->write_list(stmt->update_assignments, [this](const auto& assign) {
                write_identifier(assign.first);  // column name
                this->space();
                this->write('=');
                this->space();
                visit(assign.second);  // value
            });
        }

        // WHEN NOT MATCHED (INSERT)
        if (!stmt->insert_values.empty()) {
            this->space();
            this->write("WHEN NOT MATCHED THEN INSERT");

            if (!stmt->insert_columns.empty()) {
                this->space();
                this->write('(');
                this->write_list(stmt->insert_columns, [this](std::string_view col) {
                    write_identifier(col);
                });
                this->write(')');
            }

            this->space();
            this->write("VALUES");
            this->space();
            this->write('(');
            this->write_list(stmt->insert_values, [this](SQLNode* val) {
                visit(val);
            });
            this->write(')');
        }
    }

    void visit_truncate_stmt(TruncateStmt* stmt) {
        this->write("TRUNCATE TABLE");
        this->space();
        visit(stmt->table);
    }

    // ========================================================================
    // DDL Statement Visitors
    // ========================================================================

    void visit_create_table_stmt(CreateTableStmt* stmt) {
        this->write("CREATE");
        this->space();

        if (stmt->temporary) {
            this->write("TEMPORARY");
            this->space();
        }

        this->write("TABLE");

        if (stmt->if_not_exists) {
            this->space();
            this->write("IF NOT EXISTS");
        }

        this->space();
        visit(stmt->table);

        // For now, emit a simplified placeholder
        // Full column definition support would go here
        this->space();
        this->write("(...)");
    }

    void visit_create_view_stmt(CreateViewStmt* stmt) {
        this->write("CREATE");

        if (stmt->or_replace) {
            this->space();
            this->write("OR REPLACE");
        }

        this->space();
        this->write("VIEW");

        if (stmt->if_not_exists) {
            this->space();
            this->write("IF NOT EXISTS");
        }

        this->space();
        write_identifier(stmt->name);

        this->space();
        this->write("AS");
        this->space();
        visit(stmt->query);
    }

    void visit_create_index_stmt(CreateIndexStmt* stmt) {
        this->write("CREATE");

        if (stmt->unique) {
            this->space();
            this->write("UNIQUE");
        }

        this->space();
        this->write("INDEX");

        if (stmt->if_not_exists) {
            this->space();
            this->write("IF NOT EXISTS");
        }

        this->space();
        write_identifier(stmt->index_name);
        this->space();
        this->write("ON");
        this->space();
        visit(stmt->table);
        this->space();
        this->write('(');
        this->write_list(stmt->columns, [this](std::string_view col) {
            write_identifier(col);
        });
        this->write(')');
    }

    void visit_create_schema_stmt(CreateSchemaStmt* stmt) {
        this->write("CREATE");
        this->space();
        this->write("SCHEMA");

        if (stmt->if_not_exists) {
            this->space();
            this->write("IF NOT EXISTS");
        }

        this->space();
        write_identifier(stmt->name);
    }

    void visit_drop_table_stmt(DropTableStmt* stmt) {
        this->write("DROP TABLE");

        if (stmt->if_exists) {
            this->space();
            this->write("IF EXISTS");
        }

        this->space();
        visit(stmt->table);
    }

    void visit_drop_view_stmt(DropViewStmt* stmt) {
        this->write("DROP VIEW");

        if (stmt->if_exists) {
            this->space();
            this->write("IF EXISTS");
        }

        this->space();
        write_identifier(stmt->name);
    }

    void visit_drop_index_stmt(DropIndexStmt* stmt) {
        this->write("DROP INDEX");

        if (stmt->if_exists) {
            this->space();
            this->write("IF EXISTS");
        }

        this->space();
        write_identifier(stmt->index_name);
    }

    void visit_drop_schema_stmt(DropSchemaStmt* stmt) {
        this->write("DROP SCHEMA");

        if (stmt->if_exists) {
            this->space();
            this->write("IF EXISTS");
        }

        this->space();
        write_identifier(stmt->name);
    }

    // ========================================================================
    // Operator String Conversion
    // ========================================================================

    static const char* unary_operator_string(TK op) {
        switch (op) {
            case TK::NOT: return "NOT";
            case TK::MINUS: return "-";
            case TK::PLUS: return "+";
            // IS NULL / IS NOT NULL are handled as binary operators in most SQL parsers
            default: return "?";
        }
    }

    // ========================================================================
    // Operator String Conversion
    // ========================================================================

    static const char* operator_string(TK op) {
        switch (op) {
            case TK::EQ: return "=";
            case TK::NEQ: return "<>";
            case TK::LT: return "<";
            case TK::LTE: return "<=";
            case TK::GT: return ">";
            case TK::GTE: return ">=";
            case TK::PLUS: return "+";
            case TK::MINUS: return "-";
            case TK::STAR: return "*";
            case TK::SLASH: return "/";
            case TK::PERCENT: return "%";
            case TK::AND: return "AND";
            case TK::OR: return "OR";
            case TK::NOT: return "NOT";
            case TK::LIKE: return "LIKE";
            case TK::ILIKE: return "ILIKE";
            case TK::IN: return "IN";
            case TK::BETWEEN: return "BETWEEN";
            case TK::CONCAT: return "||";
            case TK::IS: return "IS";
            default: return "?";
        }
    }

    // ========================================================================
    // Additional Expression Visitors
    // ========================================================================

    void visit_any_expr(AnyExpr* expr) {
        visit(expr->left);
        this->space();
        this->write(operator_string(expr->comparison_op));
        this->space();
        this->write("ANY");
        this->space();
        this->write('(');
        visit(expr->subquery);
        this->write(')');
    }

    void visit_all_expr(AllExpr* expr) {
        visit(expr->left);
        this->space();
        this->write(operator_string(expr->comparison_op));
        this->space();
        this->write("ALL");
        this->space();
        this->write('(');
        visit(expr->subquery);
        this->write(')');
    }

    void visit_array_literal(ArrayLiteral* arr) {
        this->write('[');
        this->write_list(arr->elements, [this](SQLNode* elem) {
            visit(elem);
        });
        this->write(']');
    }

    void visit_array_index(ArrayIndex* idx) {
        visit(idx->array);
        this->write('[');
        visit(idx->index);
        this->write(']');
    }

    void visit_json_expr(JsonExpr* json) {
        visit(json->json_expr);
        switch (json->op_type) {
            case JsonExpr::OpType::ARROW:
                this->write("->");
                break;
            case JsonExpr::OpType::LONG_ARROW:
                this->write("->>");
                break;
            case JsonExpr::OpType::HASH_ARROW:
                this->write("#>");
                break;
            case JsonExpr::OpType::HASH_LONG_ARROW:
                this->write("#>>");
                break;
        }
        visit(json->key);
    }

    void visit_regex_match(RegexMatch* regex) {
        visit(regex->expr);
        this->space();
        if (regex->similar_to) {
            this->write("SIMILAR TO");
        } else {
            this->write("REGEXP");
        }
        this->space();
        visit(regex->pattern);
    }

    // ========================================================================
    // Additional FROM Clause Visitors
    // ========================================================================

    void visit_lateral_join(LateralJoin* lateral) {
        this->write("LATERAL");
        this->space();
        visit(lateral->table_expr);
    }

    void visit_values_clause(ValuesClause* values) {
        this->write("VALUES");
        this->space();
        this->write_list(values->rows, [this](const std::vector<SQLNode*>& row) {
            this->write('(');
            this->write_list(row, [this](SQLNode* val) {
                visit(val);
            });
            this->write(')');
        });
    }

    void visit_tablesample(Tablesample* sample) {
        this->write("TABLESAMPLE");
        this->space();
        switch (sample->method) {
            case SampleMethod::BERNOULLI:
                this->write("BERNOULLI");
                break;
            case SampleMethod::SYSTEM:
                this->write("SYSTEM");
                break;
        }
        this->space();
        this->write('(');
        visit(sample->percent);
        this->write(')');
        if (sample->seed) {
            this->space();
            this->write("REPEATABLE");
            this->space();
            this->write('(');
            visit(sample->seed);
            this->write(')');
        }
    }

    // ========================================================================
    // Additional DDL Visitors
    // ========================================================================

    void visit_alter_table_stmt(AlterTableStmt* stmt) {
        this->write("ALTER TABLE");
        this->space();
        visit(stmt->table);
        this->space();

        switch (stmt->operation) {
            case AlterOperation::ADD_COLUMN:
                this->write("ADD COLUMN");
                this->space();
                if (stmt->column_def) visit_column_def(stmt->column_def);
                break;
            case AlterOperation::DROP_COLUMN:
                this->write("DROP COLUMN");
                this->space();
                write_identifier(stmt->old_name);
                break;
            case AlterOperation::MODIFY_COLUMN:
                this->write("MODIFY COLUMN");
                this->space();
                if (stmt->column_def) visit_column_def(stmt->column_def);
                break;
            case AlterOperation::RENAME_COLUMN:
                this->write("RENAME COLUMN");
                this->space();
                write_identifier(stmt->old_name);
                this->space();
                this->write("TO");
                this->space();
                write_identifier(stmt->new_name);
                break;
            case AlterOperation::RENAME_TABLE:
                this->write("RENAME TO");
                this->space();
                write_identifier(stmt->new_name);
                break;
        }
    }

    void visit_column_def(ColumnDef* col) {
        write_identifier(col->name);
        this->space();
        this->write(col->type);
        if (col->not_null) {
            this->space();
            this->write("NOT NULL");
        }
        if (col->primary_key) {
            this->space();
            this->write("PRIMARY KEY");
        }
        if (col->unique) {
            this->space();
            this->write("UNIQUE");
        }
        if (col->auto_increment) {
            this->space();
            this->write("AUTO_INCREMENT");
        }
        if (col->default_value) {
            this->space();
            this->write("DEFAULT");
            this->space();
            visit(col->default_value);
        }
    }

    void visit_table_constraint(TableConstraint* constraint) {
        switch (constraint->constraint_type) {
            case TableConstraint::Type::PRIMARY_KEY:
                this->write("PRIMARY KEY");
                this->space();
                this->write('(');
                this->write_list(constraint->columns, [this](std::string_view col) {
                    write_identifier(col);
                });
                this->write(')');
                break;
            case TableConstraint::Type::FOREIGN_KEY:
                this->write("FOREIGN KEY");
                this->space();
                this->write('(');
                this->write_list(constraint->columns, [this](std::string_view col) {
                    write_identifier(col);
                });
                this->write(')');
                this->space();
                this->write("REFERENCES");
                this->space();
                if (constraint->ref_table) visit(constraint->ref_table);
                this->space();
                this->write('(');
                this->write_list(constraint->ref_columns, [this](std::string_view col) {
                    write_identifier(col);
                });
                this->write(')');
                break;
            case TableConstraint::Type::UNIQUE:
                this->write("UNIQUE");
                this->space();
                this->write('(');
                this->write_list(constraint->columns, [this](std::string_view col) {
                    write_identifier(col);
                });
                this->write(')');
                break;
            case TableConstraint::Type::CHECK:
                this->write("CHECK");
                this->space();
                this->write('(');
                if (constraint->check_expr) visit(constraint->check_expr);
                this->write(')');
                break;
        }
    }

    void visit_create_tablespace_stmt(CreateTablespaceStmt* stmt) {
        this->write("CREATE TABLESPACE");
        this->space();
        write_identifier(stmt->name);
        this->space();
        this->write("LOCATION");
        this->space();
        this->write('\'');
        this->write(stmt->location);
        this->write('\'');
    }

    void visit_partition_spec(PartitionSpec* spec) {
        this->write("PARTITION BY");
        this->space();
        switch (spec->type) {
            case PartitionType::RANGE:
                this->write("RANGE");
                break;
            case PartitionType::LIST:
                this->write("LIST");
                break;
            case PartitionType::HASH:
                this->write("HASH");
                break;
        }
        this->space();
        this->write('(');
        this->write_list(spec->columns, [this](std::string_view col) {
            write_identifier(col);
        });
        this->write(')');
    }

    void visit_create_index_adv(CreateIndexAdv* stmt) {
        this->write("CREATE");
        if (stmt->unique) {
            this->space();
            this->write("UNIQUE");
        }
        this->space();
        this->write("INDEX");
        if (stmt->concurrently) {
            this->space();
            this->write("CONCURRENTLY");
        }
        this->space();
        write_identifier(stmt->index_name);
        this->space();
        this->write("ON");
        this->space();
        if (stmt->table) visit(stmt->table);
        this->space();
        this->write('(');
        this->write_list(stmt->columns, [this](SQLNode* col) {
            visit(col);
        });
        this->write(')');
        if (stmt->where_clause) {
            this->space();
            this->write("WHERE");
            this->space();
            visit(stmt->where_clause);
        }
    }

    // ========================================================================
    // Transaction Statement Visitors
    // ========================================================================

    void visit_begin_stmt(BeginStmt* stmt) {
        this->write("BEGIN");
        if (!stmt->transaction_type.empty()) {
            this->space();
            // Normalize to uppercase for consistency
            if (stmt->transaction_type == "work" || stmt->transaction_type == "WORK") {
                this->write("WORK");
            } else if (stmt->transaction_type == "transaction" || stmt->transaction_type == "TRANSACTION") {
                this->write("TRANSACTION");
            } else {
                this->write(stmt->transaction_type);
            }
        }
    }

    void visit_commit_stmt(CommitStmt*) {
        this->write("COMMIT");
    }

    void visit_rollback_stmt(RollbackStmt* stmt) {
        this->write("ROLLBACK");
        if (!stmt->savepoint_name.empty()) {
            this->space();
            this->write("TO");
            this->space();
            write_identifier(stmt->savepoint_name);
        }
    }

    void visit_savepoint_stmt(SavepointStmt* stmt) {
        this->write("SAVEPOINT");
        this->space();
        write_identifier(stmt->name);
    }

    // ========================================================================
    // Utility Statement Visitors
    // ========================================================================

    void visit_set_stmt(SetStmt* stmt) {
        this->write("SET");
        this->space();
        this->write_list(stmt->assignments, [this](const auto& assign) {
            write_identifier(assign.first);
            this->space();
            this->write('=');
            this->space();
            visit(assign.second);
        });
    }

    void visit_show_stmt(ShowStmt* stmt) {
        this->write("SHOW");
        this->space();
        this->write(stmt->what);
        if (!stmt->target.empty()) {
            this->space();
            write_identifier(stmt->target);
        }
    }

    void visit_describe_stmt(DescribeStmt* stmt) {
        this->write("DESCRIBE");
        this->space();
        write_identifier(stmt->target);
    }

    void visit_explain_stmt(ExplainStmt* stmt) {
        this->write("EXPLAIN");
        if (stmt->analyze) {
            this->space();
            this->write("ANALYZE");
        }
        this->space();
        if (stmt->statement) visit(stmt->statement);
    }

    void visit_analyze_stmt(AnalyzeStmt* stmt) {
        this->write("ANALYZE");
        if (stmt->verbose) {
            this->space();
            this->write("VERBOSE");
        }
        this->space();
        write_identifier(stmt->table);
    }

    void visit_vacuum_stmt(VacuumStmt* stmt) {
        this->write("VACUUM");
        if (stmt->full) {
            this->space();
            this->write("FULL");
        }
        if (stmt->analyze) {
            this->space();
            this->write("ANALYZE");
        }
        this->space();
        write_identifier(stmt->table);
    }

    void visit_grant_stmt(GrantStmt* stmt) {
        this->write("GRANT");
        this->space();
        this->write_list(stmt->privileges, [this](std::string_view priv) {
            write_identifier(priv);
        });
        this->space();
        this->write("ON");
        this->space();
        this->write(stmt->object_type);
        this->space();
        write_identifier(stmt->object_name);
        this->space();
        this->write("TO");
        this->space();
        this->write_list(stmt->grantees, [this](std::string_view grantee) {
            write_identifier(grantee);
        });
        if (stmt->with_grant_option) {
            this->space();
            this->write("WITH GRANT OPTION");
        }
    }

    void visit_revoke_stmt(RevokeStmt* stmt) {
        this->write("REVOKE");
        this->space();
        this->write_list(stmt->privileges, [this](std::string_view priv) {
            write_identifier(priv);
        });
        this->space();
        this->write("ON");
        this->space();
        this->write(stmt->object_type);
        this->space();
        write_identifier(stmt->object_name);
        this->space();
        this->write("FROM");
        this->space();
        this->write_list(stmt->grantees, [this](std::string_view grantee) {
            write_identifier(grantee);
        });
        if (stmt->cascade) {
            this->space();
            this->write("CASCADE");
        }
    }

    // ========================================================================
    // Stored Procedure & Function Visitors
    // ========================================================================

    void visit_create_procedure_stmt(CreateProcedureStmt* stmt) {
        this->write("CREATE");
        if (stmt->or_replace) {
            this->space();
            this->write("OR REPLACE");
        }
        this->space();
        if (stmt->is_function) {
            this->write("FUNCTION");
        } else {
            this->write("PROCEDURE");
        }
        this->space();
        write_identifier(stmt->name);
        this->write("()");
        if (stmt->is_function && !stmt->return_type.empty()) {
            this->space();
            this->write("RETURNS");
            this->space();
            this->write(stmt->return_type);
        }
        this->space();
        this->write("AS");
        this->space();
        this->write("$$");
        for (auto* s : stmt->body) {
            this->space();
            visit(s);
        }
        this->space();
        this->write("$$");
        this->space();
        this->write("LANGUAGE");
        this->space();
        this->write(stmt->language);
    }

    void visit_drop_procedure_stmt(DropProcedureStmt* stmt) {
        this->write("DROP");
        this->space();
        if (stmt->is_function) {
            this->write("FUNCTION");
        } else {
            this->write("PROCEDURE");
        }
        if (stmt->if_exists) {
            this->space();
            this->write("IF EXISTS");
        }
        this->space();
        write_identifier(stmt->name);
    }

    void visit_call_procedure_stmt(CallProcedureStmt* stmt) {
        this->write("CALL");
        this->space();
        // Procedure names in CALL are not quoted
        this->write(stmt->name);
        this->write('(');
        this->write_list(stmt->arguments, [this](SQLNode* arg) {
            visit(arg);
        });
        this->write(')');
    }

    void visit_declare_var_stmt(DeclareVarStmt* stmt) {
        this->write("DECLARE");
        this->space();
        // Variable names in DECLARE are not quoted
        this->write(stmt->variable_name);
        this->space();
        this->write(stmt->type);
        if (stmt->default_value) {
            this->space();
            this->write(":=");
            this->space();
            visit(stmt->default_value);
        }
    }

    void visit_declare_cursor_stmt(DeclareCursorStmt* stmt) {
        this->write("DECLARE");
        this->space();
        write_identifier(stmt->cursor_name);
        this->space();
        this->write("CURSOR FOR");
        this->space();
        if (stmt->query) visit(stmt->query);
    }

    void visit_assignment_stmt(AssignmentStmt* stmt) {
        // Dialect-specific assignment syntax
        const auto dialect = this->dialect();
        if (dialect == SQLDialect::MySQL || dialect == SQLDialect::SQLServer) {
            // MySQL and SQL Server use SET x = 10
            this->write("SET");
            this->space();
            this->write(stmt->variable_name);
            this->space();
            this->write('=');
            this->space();
            if (stmt->value) visit(stmt->value);
        } else {
            // PostgreSQL, Oracle, BigQuery use x := 10
            this->write(stmt->variable_name);
            this->space();
            this->write(":=");
            this->space();
            if (stmt->value) visit(stmt->value);
        }
    }

    void visit_return_stmt(ReturnStmt* stmt) {
        this->write("RETURN");
        if (stmt->return_value) {
            this->space();
            visit(stmt->return_value);
        }
    }

    void visit_if_stmt(IfStmt* stmt) {
        this->write("IF");
        this->space();
        if (stmt->condition) visit(stmt->condition);
        this->space();
        this->write("THEN");
        for (auto* s : stmt->then_stmts) {
            this->space();
            visit(s);
        }

        // Handle ELSEIF clauses (stored as nested IfStmt in else_stmts)
        // and ELSE clause (regular statements)
        bool wrote_else = false;
        for (auto* s : stmt->else_stmts) {
            if (s->type == SQLNodeKind::IF_STMT) {
                // This is an ELSEIF clause
                auto* elsif = static_cast<IfStmt*>(s);
                this->space();
                this->write("ELSEIF");
                this->space();
                if (elsif->condition) visit(elsif->condition);
                this->space();
                this->write("THEN");
                for (auto* then_stmt : elsif->then_stmts) {
                    this->space();
                    visit(then_stmt);
                }
            } else {
                // Regular ELSE clause - write ELSE keyword once
                if (!wrote_else) {
                    this->space();
                    this->write("ELSE");
                    wrote_else = true;
                }
                this->space();
                visit(s);
            }
        }

        this->space();
        this->write("END IF");
    }

    void visit_while_loop(WhileLoop* loop) {
        this->write("WHILE");
        this->space();
        if (loop->condition) visit(loop->condition);
        this->space();
        this->write("DO");
        for (auto* s : loop->body) {
            this->space();
            visit(s);
        }
        this->space();
        this->write("END WHILE");
    }

    void visit_for_loop(ForLoop* loop) {
        this->write("FOR");
        this->space();
        // Loop variable in FOR declaration is not quoted
        this->write(loop->variable);
        this->space();
        this->write("IN");
        this->space();
        if (loop->start_value) visit(loop->start_value);
        this->write("..");
        if (loop->end_value) visit(loop->end_value);
        this->space();
        this->write("LOOP");
        for (auto* s : loop->body) {
            this->space();
            visit(s);
        }
        this->space();
        this->write("END LOOP");
    }

    void visit_loop_stmt(LoopStmt* loop) {
        this->write("LOOP");
        for (auto* s : loop->body) {
            this->space();
            visit(s);
        }
        this->space();
        this->write("END LOOP");
    }

    void visit_break_stmt(BreakStmt*) {
        this->write("BREAK");
    }

    void visit_continue_stmt(ContinueStmt*) {
        this->write("CONTINUE");
    }

    void visit_begin_end_block(BeginEndBlock* block) {
        this->write("BEGIN");
        for (auto* s : block->statements) {
            this->space();
            visit(s);
        }
        this->space();
        this->write("END");
    }

    void visit_do_block(DoBlock* block) {
        this->write("DO");
        this->space();
        this->write("$$");
        for (auto* s : block->statements) {
            this->space();
            visit(s);
        }
        this->space();
        this->write("$$");
    }

    void visit_exception_block(ExceptionBlock* block) {
        this->write("BEGIN");
        for (auto* s : block->try_statements) {
            this->space();
            visit(s);
        }
        for (const auto& handler : block->handlers) {
            this->space();
            this->write("EXCEPTION WHEN");
            this->space();
            this->write(handler.first);
            this->space();
            this->write("THEN");
            for (auto* s : handler.second) {
                this->space();
                visit(s);
            }
        }
        this->space();
        this->write("END");
    }

    void visit_raise_stmt(RaiseStmt* stmt) {
        const auto dialect = this->dialect();

        // MySQL uses SIGNAL, PostgreSQL uses RAISE
        if (dialect == SQLDialect::MySQL) {
            // Convert PostgreSQL RAISE to MySQL SIGNAL
            if (stmt->level == "SIGNAL" || !stmt->sqlstate.empty()) {
                // Already a SIGNAL statement
                this->write("SIGNAL SQLSTATE");
                this->space();
                if (!stmt->sqlstate.empty()) {
                    this->write(stmt->sqlstate);
                } else {
                    this->write("'45000'");  // Generic user-defined error
                }
                if (!stmt->message.empty()) {
                    this->space();
                    this->write("SET MESSAGE_TEXT =");
                    this->space();
                    this->write(stmt->message);
                }
            } else {
                // Convert RAISE to SIGNAL
                this->write("SIGNAL SQLSTATE '45000'");
                if (!stmt->message.empty()) {
                    this->space();
                    this->write("SET MESSAGE_TEXT =");
                    this->space();
                    this->write(stmt->message);
                }
            }
        } else {
            // PostgreSQL, Oracle, SQL Server use RAISE
            if (stmt->level == "SIGNAL" && !stmt->sqlstate.empty()) {
                // Convert MySQL SIGNAL to PostgreSQL RAISE
                this->write("RAISE EXCEPTION");
                if (!stmt->message.empty()) {
                    this->space();
                    this->write(stmt->message);
                }
            } else {
                // Regular RAISE statement
                this->write("RAISE");
                if (!stmt->level.empty() && stmt->level != "SIGNAL") {
                    this->space();
                    this->write(stmt->level);
                }
                if (!stmt->message.empty()) {
                    this->space();
                    this->write(stmt->message);
                }
            }
        }
    }

    void visit_open_cursor_stmt(OpenCursorStmt* stmt) {
        this->write("OPEN");
        this->space();
        write_identifier(stmt->cursor_name);
    }

    void visit_fetch_cursor_stmt(FetchCursorStmt* stmt) {
        this->write("FETCH");
        this->space();
        write_identifier(stmt->cursor_name);
        if (!stmt->into_variables.empty()) {
            this->space();
            this->write("INTO");
            this->space();
            this->write_list(stmt->into_variables, [this](std::string_view var) {
                write_identifier(var);
            });
        }
    }

    void visit_close_cursor_stmt(CloseCursorStmt* stmt) {
        this->write("CLOSE");
        this->space();
        write_identifier(stmt->cursor_name);
    }

    void visit_delimiter_stmt(DelimiterStmt* stmt) {
        this->write("DELIMITER");
        this->space();
        this->write(stmt->delimiter);
    }

    // ========================================================================
    // Trigger Visitors
    // ========================================================================

    void visit_create_trigger_stmt(CreateTriggerStmt* stmt) {
        this->write("CREATE TRIGGER");
        this->space();
        write_identifier(stmt->name);
        this->space();
        switch (stmt->timing) {
            case TriggerTiming::BEFORE:
                this->write("BEFORE");
                break;
            case TriggerTiming::AFTER:
                this->write("AFTER");
                break;
            case TriggerTiming::INSTEAD_OF:
                this->write("INSTEAD OF");
                break;
        }
        this->space();
        switch (stmt->event) {
            case TriggerEvent::INSERT:
                this->write("INSERT");
                break;
            case TriggerEvent::UPDATE:
                this->write("UPDATE");
                break;
            case TriggerEvent::DELETE:
                this->write("DELETE");
                break;
        }
        this->space();
        this->write("ON");
        this->space();
        write_identifier(stmt->table);
        if (stmt->for_each_row) {
            this->space();
            this->write("FOR EACH ROW");
        }
        for (auto* s : stmt->body) {
            this->space();
            visit(s);
        }
    }

    void visit_drop_trigger_stmt(DropTriggerStmt* stmt) {
        this->write("DROP TRIGGER");
        if (stmt->if_exists) {
            this->space();
            this->write("IF EXISTS");
        }
        this->space();
        write_identifier(stmt->name);
        if (!stmt->table.empty()) {
            this->space();
            this->write("ON");
            this->space();
            write_identifier(stmt->table);
        }
    }

    // ========================================================================
    // Advanced Feature Visitors
    // ========================================================================

    void visit_pivot_clause(PivotClause* pivot) {
        this->write("PIVOT");
        this->space();
        this->write('(');
        if (pivot->aggregate) visit_function_call(pivot->aggregate);
        this->space();
        this->write("FOR");
        this->space();
        if (pivot->pivot_column) visit(pivot->pivot_column);
        this->space();
        this->write("IN");
        this->space();
        this->write('(');
        this->write_list(pivot->pivot_values, [this](SQLNode* val) {
            visit(val);
        });
        this->write(')');
        this->write(')');
    }

    void visit_unpivot_clause(UnpivotClause* unpivot) {
        this->write("UNPIVOT");
        this->space();
        this->write('(');
        write_identifier(unpivot->value_column);
        this->space();
        this->write("FOR");
        this->space();
        write_identifier(unpivot->name_column);
        this->space();
        this->write("IN");
        this->space();
        this->write('(');
        this->write_list(unpivot->unpivot_columns, [this](std::string_view col) {
            write_identifier(col);
        });
        this->write(')');
        this->write(')');
    }

    // ========================================================================
    // BigQuery ML Visitors
    // ========================================================================

    void visit_create_model_stmt(CreateModelStmt* stmt) {
        this->write("CREATE");
        if (stmt->or_replace) {
            this->space();
            this->write("OR REPLACE");
        }
        this->space();
        this->write("MODEL");
        this->space();
        write_identifier(stmt->model_name);
        this->space();
        this->write("OPTIONS");
        this->space();
        this->write('(');
        this->write("model_type=");
        this->write('\'');
        this->write(stmt->model_type);
        this->write('\'');
        this->write(')');
        this->space();
        this->write("AS");
        this->space();
        if (stmt->training_query) visit(stmt->training_query);
    }

    void visit_drop_model_stmt(DropModelStmt* stmt) {
        this->write("DROP MODEL");
        if (stmt->if_exists) {
            this->space();
            this->write("IF EXISTS");
        }
        this->space();
        write_identifier(stmt->model_name);
    }

    void visit_ml_predict_expr(MLPredictExpr* expr) {
        this->write("ML.PREDICT");
        this->write('(');
        this->write("MODEL");
        this->space();
        write_identifier(expr->model_name);
        this->write(',');
        this->space();
        if (expr->input_query) visit(expr->input_query);
        this->write(')');
    }

    void visit_ml_evaluate_expr(MLEvaluateExpr* expr) {
        this->write("ML.EVALUATE");
        this->write('(');
        this->write("MODEL");
        this->space();
        write_identifier(expr->model_name);
        this->write(',');
        this->space();
        if (expr->evaluation_query) visit(expr->evaluation_query);
        this->write(')');
    }

    void visit_ml_training_info_expr(MLTrainingInfoExpr* expr) {
        this->write("ML.TRAINING_INFO");
        this->write('(');
        this->write("MODEL");
        this->space();
        write_identifier(expr->model_name);
        this->write(')');
    }
};

} // namespace libglot::sql
