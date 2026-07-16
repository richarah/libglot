#pragma once

#include "ast_nodes.h"
#include "dialect_traits.h"
#include "grammar.h"
#include "transforms.h"
#include <libglot/gen/generator.h>
#include <sstream>
#include <stdexcept>
#include <string>

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
    using TK = libglot::sql::lex::TokenType; // Using libsqlglot for Phase A (shim)

    // Expose base class public methods
    using Base::generate;
    using Base::reset;

    // Explicit constructor (CRTP doesn't always play nice with using Base::Base).
    // `transform_arena`, when non-null, opts the generator into lowering
    // constructs with no native syntax in the target dialect (currently:
    // Oracle/Snowflake CONNECT BY hierarchical queries) into an equivalent
    // rewrite instead of throwing std::logic_error. The arena is owned by
    // the caller and must outlive this generator; nothing is allocated from
    // it unless a lowering is actually triggered.
    explicit SQLGenerator(SQLDialect dialect, libglot::Arena* transform_arena = nullptr)
        : Base(dialect), transform_arena_(transform_arena) {}

    // ========================================================================
    // Main Visitor Dispatch (Required by GeneratorBase)
    // ========================================================================

    void visit(SQLNode* node) {
        if (!node)
            return;

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

        case SQLNodeKind::SEQUENCE_REF_EXPR:
            visit_sequence_ref_expr(static_cast<SequenceRefExpr*>(node));
            break;

        case SQLNodeKind::MATCH_AGAINST:
            visit_match_against(static_cast<MatchAgainst*>(node));
            break;

        case SQLNodeKind::FLATTEN_CLAUSE:
            visit_flatten_clause(static_cast<FlattenClause*>(node));
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

        case SQLNodeKind::CREATE_SEQUENCE_STMT:
            visit_create_sequence_stmt(static_cast<CreateSequenceStmt*>(node));
            break;

        case SQLNodeKind::DROP_SEQUENCE_STMT:
            visit_drop_sequence_stmt(static_cast<DropSequenceStmt*>(node));
            break;

        case SQLNodeKind::ALTER_SEQUENCE_STMT:
            visit_alter_sequence_stmt(static_cast<AlterSequenceStmt*>(node));
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

        case SQLNodeKind::GROUPING_SETS:
            visit_grouping_sets(static_cast<GroupingSets*>(node));
            break;

        case SQLNodeKind::ROLLUP_CLAUSE:
            visit_rollup_clause(static_cast<RollupClause*>(node));
            break;

        case SQLNodeKind::CUBE_CLAUSE:
            visit_cube_clause(static_cast<CubeClause*>(node));
            break;

        case SQLNodeKind::CONNECT_BY_CLAUSE:
            visit_connect_by_clause(static_cast<ConnectByClause*>(node));
            break;

        case SQLNodeKind::START_WITH_CLAUSE:
            visit_start_with_clause(static_cast<StartWithClause*>(node));
            break;

        case SQLNodeKind::OUTPUT_CLAUSE:
            // Standalone visit (normally emitted by the DML visitors,
            // which know the statement context): assume INSERTED rows.
            write_output_clause(static_cast<OutputClause*>(node), "INSERTED");
            break;

        case SQLNodeKind::ON_CONFLICT_CLAUSE:
            visit_on_conflict_clause(static_cast<OnConflictClause*>(node));
            break;

        case SQLNodeKind::ON_DUPLICATE_KEY_CLAUSE:
            visit_on_duplicate_key_clause(static_cast<OnDuplicateKeyClause*>(node));
            break;

        case SQLNodeKind::QUALIFY_CLAUSE:
            visit_qualify_clause(static_cast<QualifyClause*>(node));
            break;

        case SQLNodeKind::INTERVAL_LITERAL:
            visit_interval_literal(static_cast<IntervalLiteral*>(node));
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
            // A silently skipped node would drop user SQL on the floor;
            // fail loudly instead so the gap is visible and fixable.
            throw std::logic_error("SQLGenerator: unhandled AST node kind " +
                                   std::to_string(static_cast<int>(node->type)));
        }
    }

    // ========================================================================
    // Dialect-Aware Identifier Quoting (Override from Base)
    // ========================================================================

    void write_identifier(std::string_view ident) {
        const auto& feat = this->features();
        const char quote = feat.identifier_quote;

        // SQL Server style uses [identifier]; others use a symmetric quote
        // ("identifier" or `identifier`). Embedded closing-quote characters
        // are escaped by doubling so an identifier can never break out of
        // its quoting: foo]bar -> [foo]]bar], foo"bar -> "foo""bar".
        const char open = quote;
        const char close = (quote == '[') ? ']' : quote;

        this->write(open);
        for (char c : ident) {
            this->write(c);
            if (c == close) {
                this->write(close);
            }
        }
        this->write(close);
    }

    // ========================================================================
    // Node-Specific Visit Methods
    // ========================================================================

private:
    /// Set from the constructor's `transform_arena` parameter; see the
    /// constructor's doc comment. Null means "no lowering: throw instead".
    libglot::Arena* transform_arena_ = nullptr;

    /// PostgreSQL's ON CONFLICT DO UPDATE pseudo-relation "excluded" is a
    /// case-folded bare identifier, not a real table: quoting it (e.g.
    /// "EXCLUDED") would make PostgreSQL look for a literal table named
    /// EXCLUDED instead of resolving the special row image. Emit it
    /// unquoted, like the T-SQL INSERTED/DELETED qualifiers.
    static bool is_excluded_qualifier(std::string_view table) noexcept {
        return table == "EXCLUDED" || table == "excluded";
    }

    void visit_column(Column* col) {
        if (!col->table.empty()) {
            if (is_excluded_qualifier(col->table)) {
                this->write("EXCLUDED");
            } else {
                write_identifier(col->table);
            }
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

        // Datetime keyword expressions - these are function-like keywords,
        // not string literals ('CURRENT_TIMESTAMP' would be a plain string).
        if (val == "CURRENT_TIMESTAMP" || val == "CURRENT_DATE" || val == "CURRENT_TIME") {
            this->write(val);
            return;
        }

        // Hex (0x1F) and binary (0b1010) numeric literals - emit verbatim
        // (the digit heuristic below rejects the x/b marker and would quote
        // them as strings).
        if (is_hex_or_binary_literal(val)) {
            this->write(val);
            return;
        }

        // String literal from the parser: the token text carries the outer
        // quotes and source-level doubled quotes ('O''Brien'). Unescape the
        // content and re-emit through write_string_literal so every embedded
        // single quote in the output is doubled - a literal must never be
        // able to terminate its own quoting (SQL injection).
        if (val.size() >= 2 && val.front() == '\'' && val.back() == '\'') {
            std::string content;
            content.reserve(val.size() - 2);
            for (size_t i = 1; i + 1 < val.size(); ++i) {
                content.push_back(val[i]);
                if (val[i] == '\'' && i + 2 < val.size() && val[i + 1] == '\'') {
                    ++i; // Collapse source-level doubled quote
                }
            }
            this->write_string_literal(content);
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
            this->write(val); // Emit as-is
        } else {
            // Quote as string literal (doubles embedded single quotes)
            this->write_string_literal(val);
        }
    }

    /// Is `val` a hex (0x...) or binary (0b...) numeric literal?
    static bool is_hex_or_binary_literal(std::string_view val) noexcept {
        if (val.size() < 3 || val[0] != '0')
            return false;
        const char marker = val[1];
        if (marker == 'x' || marker == 'X') {
            for (size_t i = 2; i < val.size(); ++i) {
                const char c = val[i];
                if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
                    return false;
                }
            }
            return true;
        }
        if (marker == 'b' || marker == 'B') {
            for (size_t i = 2; i < val.size(); ++i) {
                if (val[i] != '0' && val[i] != '1')
                    return false;
            }
            return true;
        }
        return false;
    }

    // ========================================================================
    // Expression Precedence (for parenthesization)
    // ========================================================================

    /// Precedence assigned to atomic / self-delimiting expressions
    /// (literals, columns, function calls, parenthesized subqueries, ...)
    static constexpr int kAtomPrecedence = 100;
    /// Boolean NOT and arithmetic unary +/- (mirrors grammar.h's doc levels)
    static constexpr int kNotPrecedence = 10;
    static constexpr int kUnaryArithmeticPrecedence = 16;
    /// Comparison level: BETWEEN / IN / LIKE forms bind here
    static constexpr int kComparisonPrecedence = 12;

    /// Binary operator precedence, looked up in grammar.h's operator table
    /// so parser and generator cannot drift apart.
    static int binary_precedence(TK op) noexcept {
        const int prec = libglot::get_precedence<SQLGrammarSpec>(op);
        // Operators outside the table (e.g. Snowflake ':') are postfix-like
        // path accessors that bind tightest - treat them as atomic.
        return prec < 0 ? kAtomPrecedence : prec;
    }

    /// Precedence of the top-level operator of an expression node
    static int expr_precedence(const SQLNode* node) noexcept {
        switch (node->type) {
        case SQLNodeKind::BINARY_OP:
            return binary_precedence(static_cast<const BinaryOp*>(node)->op);
        case SQLNodeKind::UNARY_OP:
            return static_cast<const UnaryOp*>(node)->op == TK::NOT ? kNotPrecedence
                                                                    : kUnaryArithmeticPrecedence;
        case SQLNodeKind::BETWEEN_EXPR:
        case SQLNodeKind::IN_EXPR:
            return kComparisonPrecedence;
        default:
            return kAtomPrecedence;
        }
    }

    /// Visit an operand, wrapping it in parentheses when its top-level
    /// operator binds looser than the surrounding context requires.
    void write_operand(SQLNode* operand, int min_precedence) {
        if (operand && expr_precedence(operand) < min_precedence) {
            this->write('(');
            visit(operand);
            this->write(')');
        } else {
            visit(operand);
        }
    }

    void visit_binary_op(BinaryOp* op) {
        // ILIKE polyfill for dialects without native ILIKE (MySQL, BigQuery,
        // SQL Server, ...): transform to LOWER(col) LIKE LOWER(pattern)
        if (op->op == TK::ILIKE && !this->features().supports_ilike) {
            this->write("LOWER");
            this->write('(');
            visit(op->left);
            this->write(')');
            this->space();
            this->write("LIKE");
            this->space();
            this->write("LOWER");
            this->write('(');
            visit(op->right);
            this->write(')');
            return;
        }

        // Snowflake JSON path access prints without spaces: data:field.
        // Other dialects have no ':' path operator - and worse, most of them
        // re-lex ':name' as a host parameter, so emitting it would produce
        // SQL that cannot round-trip (found by fuzz_sql_roundtrip).
        if (op->op == TK::COLON) {
            if (this->dialect() != SQLDialect::Snowflake) {
                throw std::logic_error("':' JSON path access requires the Snowflake dialect; "
                                       "use -> / ->> operators for other dialects");
            }
            visit(op->left);
            this->write(':');
            visit(op->right);
            return;
        }

        // Standard binary operator with precedence-aware parenthesization.
        // All table operators are left-associative: the left operand may
        // bind equally, the right operand must bind strictly tighter -
        // otherwise (a OR b) AND c would regenerate as a OR b AND c.
        const int prec = binary_precedence(op->op);
        write_operand(op->left, prec);
        this->space();
        this->write(operator_string(op->op));
        this->space();
        if (op->op == TK::IS) {
            // The right side of IS is NULL / NOT NULL / TRUE / ... - the
            // NOT there is part of the IS [NOT] form, never parenthesized.
            visit(op->right);
        } else {
            write_operand(op->right, prec + 1);
        }
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

        if (tbl->temporal_kind != TemporalKind::NONE) {
            write_temporal_clause(tbl);
        }

        // Output alias if present
        if (!tbl->alias.empty()) {
            this->space();
            this->write("AS");
            this->space();
            write_identifier(tbl->alias);
        }
    }

    /// SQL:2011 system-versioned temporal table clause: only T-SQL (SQL
    /// Server / Azure Synapse) and MariaDB (which adopted the same syntax)
    /// support it; every other dialect throws.
    void write_temporal_clause(TableRef* tbl) {
        const auto d = this->dialect();
        if (d != SQLDialect::SQLServer && d != SQLDialect::AzureSynapse &&
            d != SQLDialect::MariaDB) {
            throw std::logic_error(
                "FOR SYSTEM_TIME (system-versioned temporal tables) has no equivalent in " +
                std::string(SQLDialectTraits::name(d)));
        }
        this->space();
        this->write("FOR SYSTEM_TIME");
        switch (tbl->temporal_kind) {
        case TemporalKind::AS_OF:
            this->space();
            this->write("AS OF");
            this->space();
            visit(tbl->temporal_arg1);
            break;
        case TemporalKind::FROM_TO:
            this->space();
            this->write("FROM");
            this->space();
            visit(tbl->temporal_arg1);
            this->space();
            this->write("TO");
            this->space();
            visit(tbl->temporal_arg2);
            break;
        case TemporalKind::BETWEEN_AND:
            this->space();
            this->write("BETWEEN");
            this->space();
            visit(tbl->temporal_arg1);
            this->space();
            this->write("AND");
            this->space();
            visit(tbl->temporal_arg2);
            break;
        case TemporalKind::CONTAINED_IN:
            this->space();
            this->write("CONTAINED IN");
            this->space();
            this->write('(');
            visit(tbl->temporal_arg1);
            this->write(',');
            this->space();
            visit(tbl->temporal_arg2);
            this->write(')');
            break;
        case TemporalKind::ALL:
            this->space();
            this->write("ALL");
            break;
        case TemporalKind::NONE:
            break;
        }
    }

    /// Dialects with no NULLS FIRST/LAST syntax at all (MySQL family and
    /// T-SQL). Rather than silently reordering nulls differently than the
    /// source query intended, an explicit NULLS FIRST/LAST is a hard
    /// error here - the caller must rewrite it by hand (e.g. an
    /// `ORDER BY (col IS NULL), col` / `ISNULL()` prefix expression).
    static bool lacks_nulls_ordering(SQLDialect d) noexcept {
        return d == SQLDialect::MySQL || d == SQLDialect::MariaDB || d == SQLDialect::SQLServer ||
               d == SQLDialect::AzureSynapse;
    }

    void visit_order_by_item(OrderByItem* item) {
        visit(item->expr);
        if (!item->ascending) {
            this->space();
            this->write("DESC");
        }
        // ASC is default, no need to emit
        if (item->nulls_specified) {
            if (lacks_nulls_ordering(this->dialect())) {
                throw std::logic_error(
                    "NULLS FIRST/LAST has no equivalent syntax in " +
                    std::string(SQLDialectTraits::name(this->dialect())) +
                    "; rewrite the ORDER BY with an explicit IS NULL/ISNULL prefix expression");
            }
            this->space();
            this->write(item->nulls_first ? "NULLS FIRST" : "NULLS LAST");
        }
    }

    void visit_select_stmt(SelectStmt* stmt) {
        // Oracle hierarchical queries (START WITH / CONNECT BY) only have
        // native syntax in Oracle and Snowflake. For every other dialect,
        // either lower to an equivalent WITH RECURSIVE CTE (when a
        // transform arena was supplied to the constructor) or fail loudly
        // rather than emit silently broken SQL. This dispatch has to run
        // before any output is written for `stmt` below: visiting the
        // lowered statement writes its own output from scratch, and if we
        // let the ordinary path below start writing first (WITH/SELECT/
        // columns/FROM/...) that partial output would be left dangling
        // ahead of the lowered statement's own WITH RECURSIVE preamble.
        if (stmt->start_with || stmt->connect_by) {
            const auto hier_dialect = this->dialect();
            if (hier_dialect != SQLDialect::Oracle && hier_dialect != SQLDialect::Snowflake) {
                if (transform_arena_) {
                    visit(lower_connect_by(*transform_arena_, stmt));
                    return;
                }
                throw std::logic_error(
                    "CONNECT BY requires the Oracle or Snowflake dialect; rewrite the "
                    "hierarchical query as a recursive CTE for " +
                    std::string(SQLDialectTraits::name(hier_dialect)) +
                    ", or construct SQLGenerator with a transform arena to lower "
                    "automatically");
            }
        }

        // WITH clause (CTEs)
        if (stmt->with && !stmt->with->ctes.empty()) {
            this->write("WITH");
            if (stmt->with->recursive) {
                this->space();
                this->write("RECURSIVE");
            }
            this->space();
            this->write_list(stmt->with->ctes, [this](CTE* cte) { visit(cte); });
            this->space();
        }

        this->write("SELECT");

        // Row-limiting strategy is dialect-specific (SQLFeatures::
        // supports_limit_offset):
        //  - T-SQL (SQL Server / Azure Synapse): TOP n, or - when an OFFSET
        //    is present AND there is an ORDER BY (T-SQL requires one) -
        //    ORDER BY ... OFFSET m ROWS FETCH NEXT n ROWS ONLY. With an
        //    OFFSET but no ORDER BY there is no valid T-SQL form; we emit
        //    plain TOP n and drop the offset (documented limitation).
        //  - Firebird / Informix: FIRST n [SKIP m] before the column list.
        //  - Oracle 12c+ / DB2 9.7+ / Derby (supports_limit_offset=false):
        //    [OFFSET m ROWS] FETCH FIRST/NEXT n ROWS ONLY.
        //  - Everything else: LIMIT n [OFFSET m].
        const auto select_dialect = this->dialect();
        const bool tsql_limit =
            (select_dialect == SQLDialect::SQLServer || select_dialect == SQLDialect::AzureSynapse);
        const bool first_skip_limit =
            (select_dialect == SQLDialect::Firebird || select_dialect == SQLDialect::Informix);
        const bool tsql_offset_fetch = tsql_limit && stmt->offset && !stmt->order_by.empty();

        // DISTINCT / DISTINCT ON (expr, ...) - PostgreSQL only
        if (!stmt->distinct_on.empty()) {
            if (select_dialect != SQLDialect::PostgreSQL) {
                throw std::logic_error("DISTINCT ON is PostgreSQL-specific; not supported for " +
                                       std::string(SQLDialectTraits::name(select_dialect)));
            }
            this->space();
            this->write("DISTINCT ON");
            this->space();
            this->write('(');
            this->write_list(stmt->distinct_on, [this](SQLNode* expr) { visit(expr); });
            this->write(')');
        } else if (stmt->distinct) {
            this->space();
            this->write("DISTINCT");
        }

        // TOP n (SQL Server) - output before column list
        if (stmt->limit && tsql_limit && !tsql_offset_fetch) {
            this->space();
            this->write("TOP");
            this->space();
            visit(stmt->limit);
            if (stmt->limit_percent) {
                this->space();
                this->write("PERCENT");
            }
            if (stmt->limit_with_ties) {
                this->space();
                this->write("WITH TIES");
            }
        }

        // FIRST n [SKIP m] (Firebird, Informix) - output before column list
        if (stmt->limit && first_skip_limit) {
            this->space();
            this->write("FIRST");
            this->space();
            visit(stmt->limit);
            if (stmt->offset) {
                this->space();
                this->write("SKIP");
                this->space();
                visit(stmt->offset);
            }
        }

        this->space();

        // Columns
        this->write_list(stmt->columns, [this](SQLNode* col) { visit(col); });

        // SELECT ... INTO target
        if (stmt->into_table) {
            this->space();
            this->write("INTO");
            this->space();
            visit(stmt->into_table);
        }

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

        // Oracle hierarchical clauses. Canonical emission order is
        // START WITH before CONNECT BY regardless of the parsed order.
        // Dialect support (Oracle/Snowflake only) and the lowering fallback
        // were already handled at the very top of this function, before any
        // output was written - reaching here means it's safe to emit as-is.
        if (stmt->start_with) {
            this->space();
            visit_start_with_clause(stmt->start_with);
        }
        if (stmt->connect_by) {
            this->space();
            visit_connect_by_clause(stmt->connect_by);
        }

        // GROUP BY clause
        if (!stmt->group_by.empty()) {
            this->space();
            this->write("GROUP BY");
            this->space();
            this->write_list(stmt->group_by, [this](SQLNode* expr) { visit(expr); });
        }

        // HAVING clause
        if (stmt->having) {
            this->space();
            this->write("HAVING");
            this->space();
            visit(stmt->having);
        }

        // QUALIFY clause (Snowflake, BigQuery, DuckDB - a post-window-
        // function filter with no ANSI equivalent; other dialects would
        // need it rewritten as a wrapping subquery, so fail loudly).
        if (stmt->qualify) {
            if (select_dialect != SQLDialect::Snowflake && select_dialect != SQLDialect::BigQuery &&
                select_dialect != SQLDialect::DuckDB) {
                throw std::logic_error(
                    "QUALIFY requires Snowflake, BigQuery, or DuckDB; rewrite as "
                    "a wrapping subquery with a WHERE filter for " +
                    std::string(SQLDialectTraits::name(select_dialect)));
            }
            this->space();
            visit_qualify_clause(stmt->qualify);
        }

        // WINDOW clause: WINDOW w AS (...), w2 AS (...)
        if (!stmt->named_windows.empty()) {
            this->space();
            this->write("WINDOW");
            this->space();
            this->write_list(stmt->named_windows,
                             [this](const std::pair<std::string_view, WindowSpec*>& nw) {
                                 write_identifier(nw.first);
                                 this->space();
                                 this->write("AS");
                                 this->space();
                                 visit(nw.second);
                             });
        }

        // ORDER BY clause (ORDER SIBLINGS BY for Oracle hierarchical queries)
        if (!stmt->order_by.empty()) {
            this->space();
            this->write(stmt->order_siblings ? "ORDER SIBLINGS BY" : "ORDER BY");
            this->space();
            this->write_list(stmt->order_by, [this](OrderByItem* item) { visit(item); });
        }

        // Row-limiting clauses after ORDER BY (see the strategy comment at
        // the top of this function). TOP / FIRST..SKIP were already emitted
        // before the column list for their dialects.
        if (tsql_offset_fetch) {
            // T-SQL: ORDER BY ... OFFSET m ROWS [FETCH NEXT n ROWS ONLY]
            this->space();
            this->write("OFFSET");
            this->space();
            visit(stmt->offset);
            this->space();
            this->write("ROWS");
            if (stmt->limit) {
                this->space();
                this->write("FETCH NEXT");
                this->space();
                visit(stmt->limit);
                this->space();
                this->write("ROWS ONLY");
            }
        } else if (!tsql_limit && !first_skip_limit) {
            if (this->features().supports_limit_offset) {
                // LIMIT n [OFFSET m]
                if (stmt->limit) {
                    this->space();
                    this->write("LIMIT");
                    this->space();
                    visit(stmt->limit);
                }
                if (stmt->offset) {
                    this->space();
                    this->write("OFFSET");
                    this->space();
                    visit(stmt->offset);
                }
            } else {
                // Oracle 12c+ / DB2 9.7+ / Derby:
                // [OFFSET m ROWS] FETCH FIRST/NEXT n ROWS ONLY
                if (stmt->offset) {
                    this->space();
                    this->write("OFFSET");
                    this->space();
                    visit(stmt->offset);
                    this->space();
                    this->write("ROWS");
                    if (stmt->limit) {
                        this->space();
                        this->write("FETCH NEXT");
                        this->space();
                        visit(stmt->limit);
                        this->space();
                        this->write("ROWS ONLY");
                    }
                } else if (stmt->limit) {
                    this->space();
                    this->write("FETCH FIRST");
                    this->space();
                    visit(stmt->limit);
                    this->space();
                    this->write("ROWS ONLY");
                }
            }
        }

        // FOR UPDATE [OF col, ...] [NOWAIT | SKIP LOCKED]
        if (stmt->for_update) {
            this->space();
            this->write("FOR UPDATE");
            if (!stmt->for_update_of.empty()) {
                this->space();
                this->write("OF");
                this->space();
                this->write_list(stmt->for_update_of,
                                 [this](std::string_view col) { write_identifier(col); });
            }
            switch (stmt->for_update_wait) {
            case ForUpdateWait::NOWAIT:
                this->space();
                this->write("NOWAIT");
                break;
            case ForUpdateWait::SKIP_LOCKED:
                this->space();
                this->write("SKIP LOCKED");
                break;
            case ForUpdateWait::NONE:
                break;
            }
        }
    }

    // ========================================================================
    // Expression Visitors
    // ========================================================================

    void visit_star(Star* star) {
        if (!star->table.empty()) {
            if (is_excluded_qualifier(star->table)) {
                this->write("EXCLUDED");
            } else {
                write_identifier(star->table);
            }
            this->write('.');
        }
        this->write('*');

        const auto d = this->dialect();
        if (!star->except_columns.empty()) {
            if (d != SQLDialect::BigQuery && d != SQLDialect::DuckDB) {
                throw std::logic_error("SELECT * EXCEPT (...) is BigQuery/DuckDB-specific; it has "
                                        "no equivalent in " +
                                        std::string(SQLDialectTraits::name(d)));
            }
            this->space();
            this->write("EXCEPT");
            this->space();
            this->write('(');
            this->write_list(star->except_columns,
                              [this](std::string_view col) { write_identifier(col); });
            this->write(')');
        }
        if (!star->exclude_columns.empty()) {
            if (d != SQLDialect::DuckDB) {
                throw std::logic_error(
                    "SELECT * EXCLUDE (...) is DuckDB-specific; it has no equivalent in " +
                    std::string(SQLDialectTraits::name(d)));
            }
            this->space();
            this->write("EXCLUDE");
            this->space();
            this->write('(');
            this->write_list(star->exclude_columns,
                              [this](std::string_view col) { write_identifier(col); });
            this->write(')');
        }
        if (!star->replace_items.empty()) {
            if (d != SQLDialect::BigQuery && d != SQLDialect::DuckDB) {
                throw std::logic_error("SELECT * REPLACE (...) is BigQuery/DuckDB-specific; it has "
                                        "no equivalent in " +
                                        std::string(SQLDialectTraits::name(d)));
            }
            this->space();
            this->write("REPLACE");
            this->space();
            this->write('(');
            this->write_list(star->replace_items, [this](SQLNode* item) { visit(item); });
            this->write(')');
        }
    }

    void visit_parameter(Parameter* param) { this->write(param->name); }

    void visit_unary_op(UnaryOp* op) {
        if (op->op == TK::NOT) {
            // Boolean NOT binds looser than comparisons: NOT a = 1 is fine,
            // but NOT (a AND b) needs the parentheses.
            this->write("NOT");
            this->space();
            write_operand(op->operand, kNotPrecedence);
        } else if (op->op == TK::PRIOR) {
            // Oracle hierarchical PRIOR: keyword operator, needs a space
            // before its operand (unlike arithmetic +/-)
            this->write("PRIOR");
            this->space();
            write_operand(op->operand, kUnaryArithmeticPrecedence);
        } else {
            // Arithmetic unary +/- bind tightest: -2 stays -2, while a
            // negated binary expression is parenthesized: -(2 + 3).
            this->write(unary_operator_string(op->op));
            write_operand(op->operand, kUnaryArithmeticPrecedence);
        }
    }

    void visit_function_call(FunctionCall* func) {
        // EXTRACT(field FROM expr): the field is a bare keyword and the
        // operand a regular expression - EXTRACT('YEAR', 'CURRENT_DATE')
        // is not valid SQL in any dialect.
        if (func->name == "EXTRACT" && func->args.size() == 2 && func->args[0] &&
            func->args[0]->type == SQLNodeKind::LITERAL) {
            this->write("EXTRACT");
            this->write('(');
            this->write(static_cast<Literal*>(func->args[0])->value);
            this->space();
            this->write("FROM");
            this->space();
            visit(func->args[1]);
            this->write(')');
            return;
        }

        // STRUCT(...) is a BigQuery type constructor; every other dialect
        // either has no equivalent or a different literal syntax entirely.
        if (func->name == "STRUCT" && this->dialect() != SQLDialect::BigQuery) {
            throw std::logic_error("STRUCT(...) literal has no equivalent outside BigQuery in " +
                                   std::string(SQLDialectTraits::name(this->dialect())));
        }

        this->write(func->name);
        this->write('(');

        if (func->distinct) {
            this->write("DISTINCT");
            this->space();
        }

        this->write_list(func->args, [this](SQLNode* arg) { visit(arg); });

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
            visit(when.first); // condition
            this->space();
            this->write("THEN");
            this->space();
            visit(when.second); // result
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
        if (cast->is_safe) {
            // SAFE_CAST returns NULL on conversion failure instead of
            // raising an error; that's not the same operation as CAST, so
            // silently downgrading it outside BigQuery would change query
            // semantics. No other modeled dialect has an exact equivalent.
            if (this->dialect() != SQLDialect::BigQuery) {
                throw std::logic_error(
                    "SAFE_CAST has no error-suppressing equivalent outside BigQuery in " +
                    std::string(SQLDialectTraits::name(this->dialect())));
            }
            this->write("SAFE_CAST");
        } else {
            this->write("CAST");
        }
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
        this->write_list(coalesce->args, [this](SQLNode* arg) { visit(arg); });
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
        // Subject and bounds sit above the comparison level; a looser
        // operand (e.g. a boolean expression) must be parenthesized so the
        // bounds' AND separator stays unambiguous.
        write_operand(between->expr, kComparisonPrecedence + 1);
        this->space();
        if (between->not_between) {
            this->write("NOT");
            this->space();
        }
        this->write("BETWEEN");
        this->space();
        write_operand(between->lower, kComparisonPrecedence + 1);
        this->space();
        this->write("AND");
        this->space();
        write_operand(between->upper, kComparisonPrecedence + 1);
    }

    void visit_in_expr(InExpr* in_expr) {
        write_operand(in_expr->expr, kComparisonPrecedence + 1);
        this->space();
        if (in_expr->not_in) {
            this->write("NOT");
            this->space();
        }
        this->write("IN");
        this->space();
        this->write('(');

        // values vector may contain a subquery or literal values
        this->write_list(in_expr->values, [this](SQLNode* val) { visit(val); });

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

        // Derived-table alias: (SELECT a FROM t) AS x
        if (!subquery->alias.empty()) {
            this->space();
            this->write("AS");
            this->space();
            write_identifier(subquery->alias);
        }
    }

    void visit_window_function(WindowFunction* wf) {
        this->write(wf->function_name);
        this->write('(');

        this->write_list(wf->args, [this](SQLNode* arg) { visit(arg); });

        this->write(')');
        this->space();
        this->write("OVER");
        this->space();

        if (!wf->over_name.empty()) {
            write_identifier(wf->over_name);
        } else if (wf->over) {
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
            this->write_list(spec->partition_by, [this](SQLNode* expr) { visit(expr); });
            need_space = true;
        }

        // ORDER BY
        if (!spec->order_by.empty()) {
            if (need_space)
                this->space();
            this->write("ORDER BY");
            this->space();
            this->write_list(spec->order_by, [this](SQLNode* expr) {
                visit(expr); // Will dispatch to visit_order_by_item if it's an OrderByItem
            });
            need_space = true;
        }

        // Frame clause (ROWS/RANGE/GROUPS)
        if (spec->frame) {
            if (need_space)
                this->space();

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

            // Regenerate the actual parsed frame bounds
            this->space();
            if (spec->frame->between_form) {
                this->write("BETWEEN");
                this->space();
                write_frame_bound(spec->frame->start_bound, spec->frame->start_offset);
                this->space();
                this->write("AND");
                this->space();
                write_frame_bound(spec->frame->end_bound, spec->frame->end_offset);
            } else {
                write_frame_bound(spec->frame->start_bound, spec->frame->start_offset);
            }
        }

        this->write(')');
    }

    /// Emit one window frame bound: UNBOUNDED PRECEDING/FOLLOWING,
    /// CURRENT ROW, or <offset> PRECEDING/FOLLOWING
    void write_frame_bound(FrameBound bound, SQLNode* offset) {
        switch (bound) {
        case FrameBound::UNBOUNDED_PRECEDING:
            this->write("UNBOUNDED PRECEDING");
            break;
        case FrameBound::UNBOUNDED_FOLLOWING:
            this->write("UNBOUNDED FOLLOWING");
            break;
        case FrameBound::CURRENT_ROW:
            this->write("CURRENT ROW");
            break;
        case FrameBound::PRECEDING:
            if (offset) {
                visit(offset);
                this->space();
            }
            this->write("PRECEDING");
            break;
        case FrameBound::FOLLOWING:
            if (offset) {
                visit(offset);
                this->space();
            }
            this->write("FOLLOWING");
            break;
        }
    }

    void visit_cte(CTE* cte) {
        write_identifier(cte->name);

        // Column list
        if (!cte->columns.empty()) {
            this->space();
            this->write('(');
            this->write_list(cte->columns, [this](std::string_view col) { write_identifier(col); });
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

        // Check if right table is LATERAL - use APPLY syntax for SQL Server
        bool is_lateral =
            (join->right_table && join->right_table->type == SQLNodeKind::LATERAL_JOIN);
        const auto dialect = this->dialect();

        if (is_lateral && dialect == SQLDialect::SQLServer) {
            // SQL Server uses CROSS APPLY or OUTER APPLY instead of LATERAL
            if (join->join_type == JoinType::CROSS || join->join_type == JoinType::INNER) {
                this->write("CROSS APPLY");
            } else if (join->join_type == JoinType::LEFT) {
                this->write("OUTER APPLY");
            } else {
                this->write("CROSS APPLY"); // Fallback
            }
            this->space();
            // For APPLY, don't output LATERAL keyword, just the subquery
            auto* lateral = static_cast<LateralJoin*>(join->right_table);
            visit(lateral->table_expr);
        } else {
            // Standard JOIN syntax
            if (join->natural) {
                this->write("NATURAL");
                this->space();
            }
            if (join->asof) {
                // ASOF [LEFT] JOIN (DuckDB / ClickHouse)
                this->write("ASOF ");
            }
            switch (join->join_type) {
            case JoinType::INNER:
                this->write(join->asof ? "JOIN" : "INNER JOIN");
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
            } else if (!join->using_columns.empty()) {
                this->space();
                this->write("USING");
                this->space();
                this->write('(');
                this->write_list(join->using_columns,
                                 [this](std::string_view col) { write_identifier(col); });
                this->write(')');
            }
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
            this->write_list(stmt->columns,
                             [this](std::string_view col) { write_identifier(col); });
            this->write(')');
        }

        // T-SQL: OUTPUT sits between the column list and VALUES/SELECT
        if (stmt->output && is_tsql_dialect(this->dialect())) {
            this->space();
            write_output_clause(stmt->output, "INSERTED");
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
                this->write_list(row, [this](SQLNode* val) { visit(val); });
                this->write(')');
            });
        }

        // PostgreSQL upsert (ON CONFLICT) / MySQL upsert (ON DUPLICATE KEY
        // UPDATE) - each is dialect-gated in its own visitor.
        if (stmt->on_conflict) {
            this->space();
            visit_on_conflict_clause(stmt->on_conflict);
        }
        if (stmt->on_duplicate_key) {
            this->space();
            visit_on_duplicate_key_clause(stmt->on_duplicate_key);
        }

        // Other dialects: RETURNING at the end of the statement
        if (stmt->output && !is_tsql_dialect(this->dialect())) {
            this->space();
            write_output_clause(stmt->output, "INSERTED");
        }
    }

    /// PostgreSQL: INSERT ... ON CONFLICT [(col, ...)] DO NOTHING
    /// / DO UPDATE SET col = expr, ... [WHERE cond]. Cross-dialect
    /// transpilation (e.g. targeting MySQL's ON DUPLICATE KEY UPDATE) is
    /// not attempted - the conflict target and EXCLUDED semantics do not
    /// map over cleanly - so any dialect other than PostgreSQL throws.
    void visit_on_conflict_clause(OnConflictClause* clause) {
        if (this->dialect() != SQLDialect::PostgreSQL) {
            throw std::logic_error("ON CONFLICT is PostgreSQL-specific (MySQL uses ON DUPLICATE "
                                   "KEY UPDATE); transpiling it to " +
                                   std::string(SQLDialectTraits::name(this->dialect())) +
                                   " is not supported");
        }
        this->write("ON CONFLICT");
        if (!clause->conflict_columns.empty()) {
            this->space();
            this->write('(');
            this->write_list(clause->conflict_columns,
                             [this](std::string_view col) { write_identifier(col); });
            this->write(')');
        }
        this->space();
        this->write("DO");
        this->space();
        if (clause->do_nothing) {
            this->write("NOTHING");
        } else {
            this->write("UPDATE SET");
            this->space();
            this->write_list(clause->update_assignments, [this](const auto& assign) {
                write_identifier(assign.first);
                this->space();
                this->write('=');
                this->space();
                visit(assign.second);
            });
            if (clause->where) {
                this->space();
                this->write("WHERE");
                this->space();
                visit(clause->where);
            }
        }
    }

    /// MySQL: INSERT ... ON DUPLICATE KEY UPDATE col = expr, ... Cross-
    /// dialect transpilation (e.g. targeting PostgreSQL's ON CONFLICT) is
    /// not attempted - MySQL has no conflict-target column list to infer
    /// a unique constraint from - so any dialect other than MySQL/MariaDB
    /// throws.
    void visit_on_duplicate_key_clause(OnDuplicateKeyClause* clause) {
        if (this->dialect() != SQLDialect::MySQL && this->dialect() != SQLDialect::MariaDB) {
            throw std::logic_error("ON DUPLICATE KEY UPDATE is MySQL-specific (PostgreSQL uses ON "
                                   "CONFLICT); transpiling it to " +
                                   std::string(SQLDialectTraits::name(this->dialect())) +
                                   " is not supported");
        }
        this->write("ON DUPLICATE KEY UPDATE");
        this->space();
        this->write_list(clause->update_assignments, [this](const auto& assign) {
            write_identifier(assign.first);
            this->space();
            this->write('=');
            this->space();
            visit(assign.second);
        });
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
            write_identifier(assign.first); // column name
            this->space();
            this->write('=');
            this->space();
            visit(assign.second); // value
        });

        // T-SQL: OUTPUT sits after SET, before FROM/WHERE
        if (stmt->output && is_tsql_dialect(this->dialect())) {
            this->space();
            write_output_clause(stmt->output, "INSERTED");
        }

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

        // Other dialects: RETURNING at the end of the statement
        if (stmt->output && !is_tsql_dialect(this->dialect())) {
            this->space();
            write_output_clause(stmt->output, "INSERTED");
        }
    }

    void visit_delete_stmt(DeleteStmt* stmt) {
        this->write("DELETE FROM");
        this->space();
        visit(stmt->table);

        // T-SQL: OUTPUT sits after the target, before USING/WHERE
        if (stmt->output && is_tsql_dialect(this->dialect())) {
            this->space();
            write_output_clause(stmt->output, "DELETED");
        }

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

        // Other dialects: RETURNING at the end of the statement
        if (stmt->output && !is_tsql_dialect(this->dialect())) {
            this->space();
            write_output_clause(stmt->output, "DELETED");
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

        const auto d = this->dialect();
        for (const auto& clause : stmt->when_clauses) {
            if (clause.match_kind == MergeMatchKind::NOT_MATCHED_BY_SOURCE &&
                d != SQLDialect::SQLServer && d != SQLDialect::AzureSynapse) {
                throw std::logic_error(
                    "MERGE ... WHEN NOT MATCHED BY SOURCE has no equivalent outside T-SQL in " +
                    std::string(SQLDialectTraits::name(d)));
            }

            this->space();
            this->write("WHEN");
            this->space();
            switch (clause.match_kind) {
            case MergeMatchKind::MATCHED:
                this->write("MATCHED");
                break;
            case MergeMatchKind::NOT_MATCHED:
                this->write("NOT MATCHED");
                break;
            case MergeMatchKind::NOT_MATCHED_BY_SOURCE:
                this->write("NOT MATCHED BY SOURCE");
                break;
            }

            if (clause.extra_condition) {
                this->space();
                this->write("AND");
                this->space();
                visit(clause.extra_condition);
            }

            this->space();
            this->write("THEN");
            this->space();

            switch (clause.action) {
            case MergeActionKind::UPDATE:
                this->write("UPDATE SET");
                this->space();
                this->write_list(clause.update_assignments, [this](const auto& assign) {
                    write_identifier(assign.first); // column name
                    this->space();
                    this->write('=');
                    this->space();
                    visit(assign.second); // value
                });
                break;
            case MergeActionKind::DELETE_ACTION:
                this->write("DELETE");
                break;
            case MergeActionKind::INSERT:
                this->write("INSERT");
                if (!clause.insert_columns.empty()) {
                    this->space();
                    this->write('(');
                    this->write_list(clause.insert_columns,
                                     [this](std::string_view col) { write_identifier(col); });
                    this->write(')');
                }
                this->space();
                this->write("VALUES");
                this->space();
                this->write('(');
                this->write_list(clause.insert_values, [this](SQLNode* val) { visit(val); });
                this->write(')');
                break;
            case MergeActionKind::DO_NOTHING:
                this->write("DO NOTHING");
                break;
            }
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
            if (stmt->global_temporary) {
                this->write("GLOBAL");
                this->space();
            }
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

        // AS SELECT clause?
        if (stmt->as_select) {
            this->space();
            this->write("AS");
            this->space();
            visit(stmt->as_select);
        } else {
            // Column definitions and table-level constraints
            this->space();
            this->write('(');
            bool first = true;
            for (auto* col : stmt->columns) {
                if (!first) {
                    this->write(',');
                    this->space();
                }
                first = false;
                visit_column_def(col);
            }
            for (auto* constraint : stmt->constraints) {
                if (!first) {
                    this->write(',');
                    this->space();
                }
                first = false;
                visit_table_constraint(constraint);
            }
            this->write(')');

            // Trailing dialect-specific table options (ENGINE=, DISTSTYLE,
            // PARTITION BY, ...), regenerated verbatim in the order parsed.
            for (const auto& opt : stmt->table_options) {
                this->space();
                this->write(opt.name);
                if (opt.has_equals) {
                    this->write('=');
                    this->write(opt.value);
                } else if (!opt.value.empty()) {
                    // A parenthesized value directly follows its name
                    // (DISTKEY(col)); a word-like value gets a separating
                    // space (DISTSTYLE KEY, PARTITION BY RANGE (...)).
                    if (opt.value.front() != '(') {
                        this->space();
                    }
                    this->write(opt.value);
                }
            }
        }
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
        this->write_list(stmt->columns, [this](std::string_view col) { write_identifier(col); });
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
        case TK::NOT:
            return "NOT";
        case TK::MINUS:
            return "-";
        case TK::PLUS:
            return "+";
        case TK::PRIOR:
            return "PRIOR";
        // IS NULL / IS NOT NULL are handled as binary operators in most SQL parsers
        default:
            throw std::logic_error(
                std::string("SQLGenerator: no unary operator string for token '") +
                std::string(libglot::sql::lex::token_type_name(op)) + "'");
        }
    }

    // ========================================================================
    // Operator String Conversion
    // ========================================================================

    static const char* operator_string(TK op) {
        switch (op) {
        case TK::EQ:
            return "=";
        case TK::NULL_SAFE_EQ:
            return "<=>";
        case TK::NEQ:
            return "<>";
        case TK::LT:
            return "<";
        case TK::LTE:
            return "<=";
        case TK::GT:
            return ">";
        case TK::GTE:
            return ">=";
        case TK::PLUS:
            return "+";
        case TK::MINUS:
            return "-";
        case TK::STAR:
            return "*";
        case TK::SLASH:
            return "/";
        case TK::PERCENT:
            return "%";
        case TK::CARET:
            return "^";
        case TK::AND:
            return "AND";
        case TK::OR:
            return "OR";
        case TK::NOT:
            return "NOT";
        case TK::LIKE:
            return "LIKE";
        case TK::ILIKE:
            return "ILIKE";
        case TK::IN:
            return "IN";
        case TK::BETWEEN:
            return "BETWEEN";
        case TK::CONCAT:
            return "||";
        case TK::IS:
            return "IS";

        // JSON operators (PostgreSQL)
        case TK::ARROW:
            return "->";
        case TK::LONG_ARROW:
            return "->>";
        case TK::HASH_ARROW:
            return "#>";
        case TK::HASH_LONG_ARROW:
            return "#>>";
        case TK::AT_GT:
            return "@>";
        case TK::LT_AT:
            return "<@";
        case TK::QUESTION:
            return "?";

        // Snowflake JSON access operator
        case TK::COLON:
            return ":";

        default:
            // Returning a placeholder here would silently corrupt the
            // generated SQL; fail loudly instead.
            throw std::logic_error(std::string("SQLGenerator: no operator string for token '") +
                                   std::string(libglot::sql::lex::token_type_name(op)) + "'");
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
        this->write_list(arr->elements, [this](SQLNode* elem) { visit(elem); });
        this->write(']');
    }

    void visit_array_index(ArrayIndex* idx) {
        visit(idx->array);
        this->write('[');
        if (idx->subscript != ArraySubscript::NONE) {
            if (this->dialect() != SQLDialect::BigQuery) {
                throw std::logic_error(
                    "Array subscript functions (OFFSET/ORDINAL/SAFE_OFFSET) are BigQuery-specific; "
                    "plain arr[index] has different (0- vs 1-based) semantics in " +
                    std::string(SQLDialectTraits::name(this->dialect())));
            }
            switch (idx->subscript) {
            case ArraySubscript::OFFSET:
                this->write("OFFSET");
                break;
            case ArraySubscript::ORDINAL:
                this->write("ORDINAL");
                break;
            case ArraySubscript::SAFE_OFFSET:
                this->write("SAFE_OFFSET");
                break;
            case ArraySubscript::NONE:
                break;
            }
            this->write('(');
            visit(idx->index);
            this->write(')');
        } else {
            visit(idx->index);
        }
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

    /// Dialects with no native sequence object at all. Every other dialect
    /// modeled here (PostgreSQL, Oracle, SQL Server, DB2, MariaDB, Firebird,
    /// Snowflake, ...) accepts the CREATE/DROP/ALTER SEQUENCE syntax parsed
    /// above closely enough to regenerate it verbatim.
    static bool lacks_sequences(SQLDialect d) noexcept {
        return d == SQLDialect::MySQL || d == SQLDialect::SQLite;
    }

    void visit_sequence_ref_expr(SequenceRefExpr* seq) {
        const auto d = this->dialect();
        if (lacks_sequences(d)) {
            throw std::logic_error(
                "Sequences (" + std::string(seq->is_next ? "NEXTVAL" : "CURRVAL") +
                ") have no equivalent in " + std::string(SQLDialectTraits::name(d)));
        }
        if (d == SQLDialect::Oracle) {
            // Oracle member-style: seq.NEXTVAL / seq.CURRVAL
            write_identifier(seq->sequence_name);
            this->write('.');
            this->write(seq->is_next ? "NEXTVAL" : "CURRVAL");
        } else if (d == SQLDialect::DB2 || d == SQLDialect::SQLServer) {
            // SQL:2003 sequence expression: NEXT VALUE FOR seq / DB2's
            // PREVIOUS VALUE FOR seq (CURRVAL equivalent). SQL Server has
            // no session-scoped "current value" syntax at all.
            if (!seq->is_next && d == SQLDialect::SQLServer) {
                throw std::logic_error(
                    "CURRVAL has no equivalent in SQL Server (no session-scoped current "
                    "sequence value; use NEXT VALUE FOR, or read the value back separately)");
            }
            this->write(seq->is_next ? "NEXT VALUE FOR" : "PREVIOUS VALUE FOR");
            this->space();
            write_identifier(seq->sequence_name);
        } else {
            // Function-style: nextval('seq') / currval('seq')
            this->write(seq->is_next ? "NEXTVAL" : "CURRVAL");
            this->write('(');
            this->write_string_literal(seq->sequence_name);
            this->write(')');
        }
    }

    void visit_match_against(MatchAgainst* m) {
        const auto d = this->dialect();
        if (d != SQLDialect::MySQL && d != SQLDialect::MariaDB) {
            throw std::logic_error(
                "MATCH ... AGAINST (fulltext search) has no equivalent outside MySQL/MariaDB in " +
                std::string(SQLDialectTraits::name(d)));
        }
        this->write("MATCH");
        this->space();
        this->write('(');
        this->write_list(m->columns, [this](std::string_view col) { write_identifier(col); });
        this->write(')');
        this->space();
        this->write("AGAINST");
        this->space();
        this->write('(');
        visit(m->against_expr);
        if (m->mode_specified) {
            this->space();
            switch (m->mode) {
            case FulltextMode::NATURAL_LANGUAGE:
                this->write("IN NATURAL LANGUAGE MODE");
                break;
            case FulltextMode::NATURAL_LANGUAGE_EXPANSION:
                this->write("IN NATURAL LANGUAGE MODE WITH QUERY EXPANSION");
                break;
            case FulltextMode::BOOLEAN_MODE:
                this->write("IN BOOLEAN MODE");
                break;
            case FulltextMode::QUERY_EXPANSION:
                this->write("WITH QUERY EXPANSION");
                break;
            }
        }
        this->write(')');
    }

    void visit_flatten_clause(FlattenClause* f) {
        const auto d = this->dialect();
        if (d != SQLDialect::Snowflake) {
            throw std::logic_error("LATERAL FLATTEN has no equivalent outside Snowflake in " +
                                   std::string(SQLDialectTraits::name(d)));
        }
        this->write("FLATTEN");
        this->write('(');
        this->write("INPUT");
        this->space();
        this->write("=>");
        this->space();
        visit(f->input);
        if (f->path) {
            this->write(',');
            this->space();
            this->write("PATH");
            this->space();
            this->write("=>");
            this->space();
            visit(f->path);
        }
        if (f->outer) {
            this->write(',');
            this->space();
            this->write("OUTER");
            this->space();
            this->write("=>");
            this->space();
            visit(f->outer);
        }
        this->write(')');
        if (!f->alias.empty()) {
            this->space();
            write_identifier(f->alias);
        }
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
        // As a FROM-clause table source, VALUES needs to be wrapped in its
        // own parens with a required alias: (VALUES (...), (...)) AS v(c1, c2).
        const bool as_table_source = !values->alias.empty();
        if (as_table_source) {
            this->write('(');
        }
        this->write("VALUES");
        this->space();
        this->write_list(values->rows, [this](const std::vector<SQLNode*>& row) {
            this->write('(');
            this->write_list(row, [this](SQLNode* val) { visit(val); });
            this->write(')');
        });
        if (as_table_source) {
            this->write(')');
            this->space();
            this->write("AS");
            this->space();
            write_identifier(values->alias);
            if (!values->columns.empty()) {
                this->write('(');
                this->write_list(values->columns,
                                 [this](std::string_view col) { write_identifier(col); });
                this->write(')');
            }
        }
    }

    void visit_tablesample(Tablesample* sample) {
        if (this->dialect() == SQLDialect::MySQL || this->dialect() == SQLDialect::MariaDB) {
            throw std::logic_error("TABLESAMPLE has no equivalent in " +
                                   std::string(SQLDialectTraits::name(this->dialect())));
        }
        visit(sample->table_expr);
        this->space();
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
        this->write('(');
        visit(sample->percent);
        this->write(')');
        if (sample->seed) {
            this->space();
            this->write("REPEATABLE");
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
            if (stmt->column_def)
                visit_column_def(stmt->column_def);
            break;
        case AlterOperation::DROP_COLUMN:
            this->write("DROP COLUMN");
            this->space();
            write_identifier(stmt->old_name);
            break;
        case AlterOperation::MODIFY_COLUMN:
            this->write("MODIFY COLUMN");
            this->space();
            if (stmt->column_def)
                visit_column_def(stmt->column_def);
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

    void visit_create_sequence_stmt(CreateSequenceStmt* stmt) {
        if (lacks_sequences(this->dialect())) {
            throw std::logic_error("CREATE SEQUENCE has no equivalent in " +
                                   std::string(SQLDialectTraits::name(this->dialect())));
        }
        this->write("CREATE SEQUENCE");
        if (stmt->if_not_exists) {
            this->space();
            this->write("IF NOT EXISTS");
        }
        this->space();
        write_identifier(stmt->name);
        if (stmt->start_with) {
            this->space();
            this->write("START WITH");
            this->space();
            visit(stmt->start_with);
        }
        if (stmt->increment_by) {
            this->space();
            this->write("INCREMENT BY");
            this->space();
            visit(stmt->increment_by);
        }
        if (stmt->min_value) {
            this->space();
            this->write("MINVALUE");
            this->space();
            visit(stmt->min_value);
        } else if (stmt->no_min_value) {
            this->space();
            this->write("NO MINVALUE");
        }
        if (stmt->max_value) {
            this->space();
            this->write("MAXVALUE");
            this->space();
            visit(stmt->max_value);
        } else if (stmt->no_max_value) {
            this->space();
            this->write("NO MAXVALUE");
        }
        if (stmt->cycle) {
            this->space();
            this->write("CYCLE");
        } else if (stmt->no_cycle) {
            this->space();
            this->write("NO CYCLE");
        }
        if (stmt->cache) {
            this->space();
            this->write("CACHE");
            this->space();
            visit(stmt->cache);
        }
    }

    void visit_drop_sequence_stmt(DropSequenceStmt* stmt) {
        if (lacks_sequences(this->dialect())) {
            throw std::logic_error("DROP SEQUENCE has no equivalent in " +
                                   std::string(SQLDialectTraits::name(this->dialect())));
        }
        this->write("DROP SEQUENCE");
        if (stmt->if_exists) {
            this->space();
            this->write("IF EXISTS");
        }
        this->space();
        write_identifier(stmt->name);
    }

    void visit_alter_sequence_stmt(AlterSequenceStmt* stmt) {
        if (lacks_sequences(this->dialect())) {
            throw std::logic_error("ALTER SEQUENCE has no equivalent in " +
                                   std::string(SQLDialectTraits::name(this->dialect())));
        }
        this->write("ALTER SEQUENCE");
        this->space();
        write_identifier(stmt->name);
        if (stmt->restart) {
            this->space();
            this->write("RESTART");
            if (stmt->restart_with) {
                this->space();
                this->write("WITH");
                this->space();
                visit(stmt->restart_with);
            }
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
        if (!col->references_table.empty()) {
            this->space();
            this->write("REFERENCES");
            this->space();
            write_identifier(col->references_table);
            if (!col->references_columns.empty()) {
                this->space();
                this->write('(');
                this->write_list(col->references_columns,
                                 [this](std::string_view ref_col) { write_identifier(ref_col); });
                this->write(')');
            }
        }
        if (col->check_expr) {
            this->space();
            this->write("CHECK");
            this->space();
            this->write('(');
            visit(col->check_expr);
            this->write(')');
        }
    }

    void visit_table_constraint(TableConstraint* constraint) {
        if (!constraint->name.empty()) {
            this->write("CONSTRAINT");
            this->space();
            write_identifier(constraint->name);
            this->space();
        }
        switch (constraint->constraint_type) {
        case TableConstraint::Type::PRIMARY_KEY:
            this->write("PRIMARY KEY");
            this->space();
            this->write('(');
            this->write_list(constraint->columns,
                             [this](std::string_view col) { write_identifier(col); });
            this->write(')');
            break;
        case TableConstraint::Type::FOREIGN_KEY:
            this->write("FOREIGN KEY");
            this->space();
            this->write('(');
            this->write_list(constraint->columns,
                             [this](std::string_view col) { write_identifier(col); });
            this->write(')');
            this->space();
            this->write("REFERENCES");
            this->space();
            if (constraint->ref_table)
                visit(constraint->ref_table);
            if (!constraint->ref_columns.empty()) {
                this->space();
                this->write('(');
                this->write_list(constraint->ref_columns,
                                 [this](std::string_view col) { write_identifier(col); });
                this->write(')');
            }
            if (!constraint->on_delete_action.empty()) {
                this->space();
                this->write("ON DELETE");
                this->space();
                this->write(constraint->on_delete_action);
            }
            if (!constraint->on_update_action.empty()) {
                this->space();
                this->write("ON UPDATE");
                this->space();
                this->write(constraint->on_update_action);
            }
            break;
        case TableConstraint::Type::UNIQUE:
            this->write("UNIQUE");
            this->space();
            this->write('(');
            this->write_list(constraint->columns,
                             [this](std::string_view col) { write_identifier(col); });
            this->write(')');
            break;
        case TableConstraint::Type::CHECK:
            this->write("CHECK");
            this->space();
            this->write('(');
            if (constraint->check_expr)
                visit(constraint->check_expr);
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
        this->write_list(spec->columns, [this](std::string_view col) { write_identifier(col); });
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
        if (stmt->table)
            visit(stmt->table);
        this->space();
        this->write('(');
        this->write_list(stmt->columns, [this](SQLNode* col) { visit(col); });
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
            } else if (stmt->transaction_type == "transaction" ||
                       stmt->transaction_type == "TRANSACTION") {
                this->write("TRANSACTION");
            } else {
                this->write(stmt->transaction_type);
            }
        }
    }

    void visit_commit_stmt(CommitStmt*) { this->write("COMMIT"); }

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
            // Parameter-style variables (@x, :x, $x) are written verbatim;
            // quoting them would produce an invalid target ([@x]).
            std::string_view name = assign.first;
            if (!name.empty() && (name[0] == '@' || name[0] == ':' || name[0] == '$')) {
                this->write(name);
            } else {
                write_identifier(name);
            }
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
        if (stmt->statement)
            visit(stmt->statement);
    }

    void visit_analyze_stmt(AnalyzeStmt* stmt) {
        this->write("ANALYZE");

        // MySQL: LOCAL or NO_WRITE_TO_BINLOG
        if (stmt->local) {
            this->space();
            this->write("LOCAL");
        }
        if (stmt->no_write_to_binlog) {
            this->space();
            this->write("NO_WRITE_TO_BINLOG");
        }

        // VERBOSE option
        if (stmt->verbose) {
            this->space();
            this->write("VERBOSE");
        }

        // MySQL: TABLE keyword
        if (stmt->use_table_keyword) {
            this->space();
            this->write("TABLE");
        }

        // Tables (optional - if empty, analyzes all tables)
        if (!stmt->tables.empty()) {
            this->space();
            bool first = true;
            for (auto* table : stmt->tables) {
                if (!first) {
                    this->write(',');
                    this->space();
                }
                first = false;

                // Write table name (unquoted for tests)
                if (!table->database.empty()) {
                    this->write(table->database);
                    this->write('.');
                }
                this->write(table->table);

                // Column specifications (only for single table)
                if (!stmt->columns.empty() && stmt->tables.size() == 1) {
                    this->write('(');
                    bool first_col = true;
                    for (const auto& col : stmt->columns) {
                        if (!first_col) {
                            this->write(',');
                            this->space();
                        }
                        first_col = false;
                        this->write(col);
                    }
                    this->write(')');
                }
            }
        }
    }

    void visit_vacuum_stmt(VacuumStmt* stmt) {
        this->write("VACUUM");

        // Parenthesized options (PostgreSQL 9.0+)
        if (!stmt->paren_options.empty()) {
            this->space();
            this->write('(');
            bool first = true;
            for (const auto& opt : stmt->paren_options) {
                if (!first) {
                    this->write(',');
                    this->space();
                }
                first = false;
                this->write(opt.first); // option name
                if (!opt.second.empty()) {
                    this->space();
                    this->write(opt.second); // option value
                }
            }
            this->write(')');
        } else {
            // Traditional option syntax
            if (stmt->full) {
                this->space();
                this->write("FULL");
            }
            if (stmt->freeze) {
                this->space();
                this->write("FREEZE");
            }
            if (stmt->verbose) {
                this->space();
                this->write("VERBOSE");
            }
            if (stmt->analyze) {
                this->space();
                this->write("ANALYZE");
            }
        }

        // Tables (optional - if empty, vacuums all tables)
        if (!stmt->tables.empty()) {
            this->space();
            bool first = true;
            for (auto* table : stmt->tables) {
                if (!first) {
                    this->write(',');
                    this->space();
                }
                first = false;

                // Write table name (unquoted for tests)
                if (!table->database.empty()) {
                    this->write(table->database);
                    this->write('.');
                }
                this->write(table->table);

                // Column specifications (only for single table)
                if (!stmt->columns.empty() && stmt->tables.size() == 1) {
                    this->write('(');
                    bool first_col = true;
                    for (const auto& col : stmt->columns) {
                        if (!first_col) {
                            this->write(',');
                            this->space();
                        }
                        first_col = false;
                        this->write(col);
                    }
                    this->write(')');
                }
            }
        }
    }

    void visit_grant_stmt(GrantStmt* stmt) {
        this->write("GRANT");
        this->space();

        // Output privileges, combining multi-word privileges (separated by spaces, not commas)
        // Multi-word privileges are stored as consecutive elements: ["SHOW", "VIEW"], ["ALTER",
        // "ANY", "USER"] We need to output them with spaces between words within a privilege, and
        // commas between privileges
        for (size_t i = 0; i < stmt->privileges.size(); ++i) {
            if (i > 0) {
                // Determine if previous was part of same privilege or separate privilege
                // Heuristic: known second/third words don't start a new privilege
                std::string_view curr = stmt->privileges[i];
                bool is_continuation =
                    (curr == "PRIVILEGES" || curr == "privileges" || curr == "VIEW" ||
                     curr == "view" || curr == "TABLES" || curr == "tables" || curr == "OPTION" ||
                     curr == "option" || curr == "OWNERSHIP" || curr == "ownership" ||
                     curr == "DEFINITION" || curr == "definition" || curr == "ANY" ||
                     curr == "any" || curr == "READER" || curr == "reader" || curr == "EDITOR" ||
                     curr == "editor" || curr == "OWNER" || curr == "owner" || curr == "VIEWER" ||
                     curr == "viewer" || curr == "USER" || curr == "user" || curr == "ROLE" ||
                     curr == "role" || curr == "TABLE" || curr == "table" || curr == "INDEX" ||
                     curr == "index" || curr == "PROCEDURE" || curr == "procedure" ||
                     curr == "FUNCTION" || curr == "function" || curr == "SCHEMA" ||
                     curr == "schema" || curr == "DATABASE" || curr == "database" ||
                     curr == "SEQUENCE" || curr == "sequence" || curr == "FOR" || curr == "for");

                if (is_continuation) {
                    this->space(); // Space within multi-word privilege
                } else {
                    this->write(','); // Comma between separate privileges
                    this->space();
                }
            }
            this->write(stmt->privileges[i]);
        }
        // Only write ON clause for privilege grants (not role grants)
        if (!stmt->object_name.empty()) {
            this->space();
            this->write("ON");
            if (!stmt->object_type.empty()) {
                this->space();
                this->write(stmt->object_type); // Object type is keyword, don't quote
            }
            // Trim and write object name (handles LOGIN ::sa → LOGIN::sa)
            std::string_view obj_name = stmt->object_name;
            while (!obj_name.empty() && (obj_name[0] == ' ' || obj_name[0] == '\t')) {
                obj_name = obj_name.substr(1);
            }
            if (!obj_name.empty()) {
                // Only add space if obj_name doesn't start with punctuation
                if (obj_name[0] != ':' && obj_name[0] != '.' && obj_name[0] != ',') {
                    this->space();
                }
                this->write(obj_name); // Object name is identifier but tests expect it unquoted
            }
        }
        this->space();
        this->write("TO");
        this->space();
        this->write_list(stmt->grantees, [this](std::string_view grantee) {
            this->write(grantee); // Grantees can be PUBLIC keyword, don't quote
        });
        if (stmt->with_grant_option) {
            this->space();
            this->write("WITH GRANT OPTION");
        } else if (stmt->with_admin_option) {
            this->space();
            this->write("WITH ADMIN OPTION");
        } else if (stmt->with_hierarchy_option) {
            this->space();
            this->write("WITH HIERARCHY OPTION");
        }
    }

    void visit_revoke_stmt(RevokeStmt* stmt) {
        this->write("REVOKE");
        this->space();

        // Output GRANT OPTION FOR or ADMIN OPTION FOR prefix if present
        if (stmt->grant_option_for) {
            this->write("GRANT OPTION FOR");
            this->space();
        } else if (stmt->admin_option_for) {
            this->write("ADMIN OPTION FOR");
            this->space();
        }

        // Output privileges, combining multi-word privileges (separated by spaces, not commas)
        // Multi-word privileges are stored as consecutive elements: ["SHOW", "VIEW"], ["ALTER",
        // "ANY", "USER"] We need to output them with spaces between words within a privilege, and
        // commas between privileges
        for (size_t i = 0; i < stmt->privileges.size(); ++i) {
            if (i > 0) {
                // Determine if previous was part of same privilege or separate privilege
                // Heuristic: known second/third words don't start a new privilege
                std::string_view curr = stmt->privileges[i];
                bool is_continuation =
                    (curr == "PRIVILEGES" || curr == "privileges" || curr == "VIEW" ||
                     curr == "view" || curr == "TABLES" || curr == "tables" || curr == "OPTION" ||
                     curr == "option" || curr == "OWNERSHIP" || curr == "ownership" ||
                     curr == "DEFINITION" || curr == "definition" || curr == "ANY" ||
                     curr == "any" || curr == "READER" || curr == "reader" || curr == "EDITOR" ||
                     curr == "editor" || curr == "OWNER" || curr == "owner" || curr == "VIEWER" ||
                     curr == "viewer" || curr == "USER" || curr == "user" || curr == "ROLE" ||
                     curr == "role" || curr == "TABLE" || curr == "table" || curr == "INDEX" ||
                     curr == "index" || curr == "PROCEDURE" || curr == "procedure" ||
                     curr == "FUNCTION" || curr == "function" || curr == "SCHEMA" ||
                     curr == "schema" || curr == "DATABASE" || curr == "database" ||
                     curr == "SEQUENCE" || curr == "sequence" || curr == "FOR" || curr == "for");

                if (is_continuation) {
                    this->space(); // Space within multi-word privilege
                } else {
                    this->write(','); // Comma between separate privileges
                    this->space();
                }
            }
            this->write(stmt->privileges[i]);
        }
        // Only write ON clause for privilege revokes (not role revokes)
        if (!stmt->object_name.empty()) {
            this->space();
            this->write("ON");
            if (!stmt->object_type.empty()) {
                this->space();
                this->write(stmt->object_type); // Object type is keyword, don't quote
            }
            // Trim and write object name (handles LOGIN ::sa → LOGIN::sa)
            std::string_view obj_name = stmt->object_name;
            while (!obj_name.empty() && (obj_name[0] == ' ' || obj_name[0] == '\t')) {
                obj_name = obj_name.substr(1);
            }
            if (!obj_name.empty()) {
                // Only add space if obj_name doesn't start with punctuation
                if (obj_name[0] != ':' && obj_name[0] != '.' && obj_name[0] != ',') {
                    this->space();
                }
                this->write(obj_name); // Object name is identifier but tests expect it unquoted
            }
        }
        this->space();
        this->write("FROM");
        this->space();
        this->write_list(stmt->grantees, [this](std::string_view grantee) {
            this->write(grantee); // Grantees can be PUBLIC keyword, don't quote
        });
        if (stmt->cascade) {
            this->space();
            this->write("CASCADE");
        } else if (stmt->restrict) {
            this->space();
            this->write("RESTRICT");
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
        // Procedure names in CREATE are not quoted
        this->write(stmt->name);

        // Parameters
        this->write('(');
        if (!stmt->parameters.empty()) {
            bool first = true;
            for (const auto& param : stmt->parameters) {
                if (!first) {
                    this->write(',');
                    this->space();
                }
                first = false;

                // Output mode if specified (IN, OUT, INOUT)
                if (!param.mode.empty()) {
                    this->write(param.mode);
                    this->space();
                }

                // Output parameter name and type
                this->write(param.name);
                this->space();
                this->write(param.type);
            }
        }
        this->write(')');

        if (stmt->is_function && !stmt->return_type.empty()) {
            this->space();
            this->write("RETURNS");
            this->space();
            this->write(stmt->return_type);
        }

        // LANGUAGE clause (PostgreSQL, etc.)
        if (!stmt->language.empty()) {
            this->space();
            this->write("LANGUAGE");
            this->space();
            this->write(stmt->language);
        }

        this->space();

        // Output body:
        // If body contains a single ExceptionBlock or BeginEndBlock, visit it directly (it handles
        // BEGIN...END) Otherwise wrap in BEGIN...END
        if (stmt->body.size() == 1 && (stmt->body[0]->type == SQLNodeKind::EXCEPTION_BLOCK ||
                                       stmt->body[0]->type == SQLNodeKind::BEGIN_END_BLOCK)) {
            this->space();
            visit(stmt->body[0]);
        } else {
            // Multiple statements or simple statements - wrap in BEGIN...END
            this->write("BEGIN");
            write_statement_body(stmt->body);
            this->space();
            this->write("END");
        }
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
        // Procedure names in DROP are not quoted
        this->write(stmt->name);
    }

    void visit_call_procedure_stmt(CallProcedureStmt* stmt) {
        this->write("CALL");
        this->space();
        // Procedure names in CALL are not quoted
        this->write(stmt->name);
        this->write('(');
        this->write_list(stmt->arguments, [this](SQLNode* arg) { visit(arg); });
        this->write(')');
    }

    void visit_declare_var_stmt(DeclareVarStmt* stmt) {
        const auto dialect = this->dialect();
        this->write("DECLARE");
        this->space();
        // Variable names in DECLARE are not quoted
        this->write(stmt->variable_name);
        this->space();
        this->write(stmt->type);
        if (stmt->default_value) {
            this->space();
            // T-SQL uses the initializer form: DECLARE @x INT = 5
            if (dialect == SQLDialect::SQLServer || dialect == SQLDialect::AzureSynapse) {
                this->write('=');
            } else {
                this->write("DEFAULT");
            }
            this->space();
            visit(stmt->default_value);
        }
    }

    void visit_declare_cursor_stmt(DeclareCursorStmt* stmt) {
        this->write("DECLARE");
        this->space();
        // Cursor names in DECLARE are not quoted
        this->write(stmt->cursor_name);
        this->space();
        if (stmt->scroll) {
            this->write("SCROLL");
            this->space();
        }
        this->write("CURSOR FOR");
        this->space();
        if (stmt->query)
            visit(stmt->query);
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
            if (stmt->value)
                visit(stmt->value);
        } else {
            // PostgreSQL, Oracle, BigQuery use x := 10
            this->write(stmt->variable_name);
            this->space();
            this->write(":=");
            this->space();
            if (stmt->value)
                visit(stmt->value);
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
        if (stmt->condition)
            visit(stmt->condition);
        this->space();
        this->write("THEN");
        write_statement_body(stmt->then_stmts);

        // Handle ELSEIF clauses using the proper elseif_branches field
        for (const auto& elsif_branch : stmt->elseif_branches) {
            this->space();
            this->write("ELSEIF");
            this->space();
            if (elsif_branch.first)
                visit(elsif_branch.first); // condition
            this->space();
            this->write("THEN");
            write_statement_body(elsif_branch.second);
        }

        // Handle ELSE clause
        if (!stmt->else_stmts.empty()) {
            this->space();
            this->write("ELSE");
            write_statement_body(stmt->else_stmts);
        }

        this->space();
        this->write("END IF");
    }

    void visit_while_loop(WhileLoop* loop) {
        const auto dialect = this->dialect();

        this->write("WHILE");
        this->space();
        if (loop->condition)
            visit(loop->condition);
        this->space();

        if (dialect == SQLDialect::SQLServer || dialect == SQLDialect::AzureSynapse) {
            // T-SQL: WHILE condition BEGIN ... END
            this->write("BEGIN");
            write_statement_body(loop->body);
            this->space();
            this->write("END");
        } else if (dialect == SQLDialect::PostgreSQL || dialect == SQLDialect::Oracle) {
            // PL/pgSQL and PL/SQL: WHILE condition LOOP ... END LOOP
            this->write("LOOP");
            write_statement_body(loop->body);
            this->space();
            this->write("END LOOP");
        } else {
            // MySQL / ANSI SQL/PSM: WHILE condition DO ... END WHILE
            this->write("DO");
            write_statement_body(loop->body);
            this->space();
            this->write("END WHILE");
        }
    }

    void visit_for_loop(ForLoop* loop) {
        const auto dialect = this->dialect();

        // Record iteration form (FOR rec IN SELECT ... LOOP): PL/pgSQL and
        // Oracle PL/SQL both have native cursor FOR loops (Oracle requires
        // the query in parens; PostgreSQL does not), but T-SQL has no direct
        // equivalent short of a real cursor - throw rather than silently
        // mis-lowering it.
        if (loop->query) {
            if (dialect == SQLDialect::SQLServer || dialect == SQLDialect::AzureSynapse) {
                throw std::logic_error("FOR record IN SELECT loops have no direct T-SQL equivalent "
                                       "(rewrite using a DECLARE CURSOR / FETCH loop)");
            }
            this->write("FOR");
            this->space();
            this->write(loop->variable);
            this->space();
            this->write("IN");
            this->space();
            if (dialect == SQLDialect::Oracle) {
                this->write('(');
                visit(loop->query);
                this->write(')');
            } else {
                visit(loop->query);
            }
            this->space();
            this->write("LOOP");
            write_statement_body(loop->body);
            this->space();
            this->write("END LOOP");
            return;
        }

        // T-SQL doesn't support FOR..IN..LOOP syntax - transpile to a
        // counter WHILE loop. The whole lowering is wrapped in BEGIN..END so
        // it stays a single re-parseable statement, and the exact shape
        // matches what re-parsing + re-generating the lowered form produces
        // (fixed-point property).
        if (dialect == SQLDialect::SQLServer || dialect == SQLDialect::AzureSynapse) {
            // BEGIN DECLARE @variable INT = start_value;
            this->write("BEGIN DECLARE @");
            this->write(loop->variable);
            this->space();
            this->write("INT =");
            this->space();
            if (loop->start_value)
                visit(loop->start_value);
            this->write(';');
            this->space();

            // WHILE @variable <= end_value (>= when REVERSE)
            this->write("WHILE @");
            this->write(loop->variable);
            this->space();
            this->write(loop->reverse ? ">=" : "<=");
            this->space();
            if (loop->end_value)
                visit(loop->end_value);
            this->space();

            // BEGIN body; SET @variable = @variable +/- 1; END; END
            this->write("BEGIN");
            write_statement_body(loop->body);
            this->space();
            this->write("SET @");
            this->write(loop->variable);
            this->space();
            this->write("= @");
            this->write(loop->variable);
            this->space();
            this->write(loop->reverse ? "- 1; END; END" : "+ 1; END; END");
        } else {
            // Other dialects support FOR loops natively
            this->write("FOR");
            this->space();
            // Loop variable in FOR declaration is not quoted
            this->write(loop->variable);
            this->space();
            this->write("IN");
            this->space();
            if (loop->reverse) {
                this->write("REVERSE");
                this->space();
            }
            if (loop->start_value)
                visit(loop->start_value);
            this->write("..");
            if (loop->end_value)
                visit(loop->end_value);
            this->space();
            this->write("LOOP");
            write_statement_body(loop->body);
            this->space();
            this->write("END LOOP");
        }
    }

    /// Emit a procedural statement body: each statement is preceded by a
    /// space and terminated with a semicolon (procedural SQL requires
    /// statement terminators inside blocks).
    void write_statement_body(const std::vector<SQLNode*>& stmts) {
        for (auto* s : stmts) {
            this->space();
            visit(s);
            this->write(';');
        }
    }

    void visit_loop_stmt(LoopStmt* loop) {
        this->write("LOOP");
        write_statement_body(loop->body);
        this->space();
        this->write("END LOOP");
    }

    void visit_break_stmt(BreakStmt*) { this->write("BREAK"); }

    void visit_continue_stmt(ContinueStmt*) { this->write("CONTINUE"); }

    void visit_begin_end_block(BeginEndBlock* block) {
        this->write("BEGIN");
        write_statement_body(block->statements);
        this->space();
        this->write("END");
    }

    void visit_do_block(DoBlock* block) {
        this->write("DO");

        // Optional LANGUAGE clause
        if (!block->language.empty()) {
            this->space();
            this->write("LANGUAGE");
            this->space();
            this->write(block->language);
        }

        // Write the raw code block (already includes delimiters)
        if (!block->code_block.empty()) {
            this->space();
            this->write(block->code_block);
        }
    }

    void visit_exception_block(ExceptionBlock* block) {
        this->write("BEGIN");
        write_statement_body(block->try_statements);
        for (const auto& handler : block->handlers) {
            this->space();
            this->write("EXCEPTION WHEN");
            this->space();
            this->write(handler.first);
            this->space();
            this->write("THEN");
            write_statement_body(handler.second);
        }
        this->space();
        this->write("END");
    }

    void visit_raise_stmt(RaiseStmt* stmt) {
        const auto dialect = this->dialect();

        // T-SQL has no RAISE/SIGNAL - use RAISERROR('msg', severity, state)
        if (dialect == SQLDialect::SQLServer || dialect == SQLDialect::AzureSynapse) {
            this->write("RAISERROR(");
            if (!stmt->message.empty()) {
                this->write(stmt->message);
            } else {
                this->write("'Error'");
            }
            if (stmt->tsql_raiserror) {
                // Round-trip: args already carry severity, state[, subst args]
                for (auto* arg : stmt->args) {
                    this->write(',');
                    this->space();
                    visit(arg);
                }
            } else {
                // Lowered from RAISE/SIGNAL: severity 16 (user error),
                // state 1, then any RAISE format args as substitution args.
                this->write(", 16, 1");
                for (auto* arg : stmt->args) {
                    this->write(',');
                    this->space();
                    visit(arg);
                }
            }
            this->write(')');
            return;
        }

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
                    this->write("'45000'"); // Generic user-defined error
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
            // PostgreSQL, Oracle, ANSI use RAISE
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
                    write_raise_level(stmt->level);
                }
                if (!stmt->message.empty()) {
                    this->space();
                    this->write(stmt->message);
                }
                // Format arguments: RAISE EXCEPTION 'value is %', 5.
                // Args parsed from T-SQL RAISERROR are severity/state
                // numbers, not format args - drop those.
                if (!stmt->tsql_raiserror) {
                    for (auto* arg : stmt->args) {
                        this->write(',');
                        this->space();
                        visit(arg);
                    }
                }
            }
        }
    }

    /// Emit a RAISE level. Real level keywords (EXCEPTION, NOTICE, ...) are
    /// written bare; anything else - e.g. a level that was parsed from a
    /// quoted identifier - is written through write_identifier so it stays
    /// re-lexable (a bare token with special characters would not round-trip).
    void write_raise_level(std::string_view level) {
        static constexpr std::string_view kLevels[] = {"EXCEPTION", "NOTICE", "WARNING",
                                                        "INFO",      "LOG",    "DEBUG",
                                                        "ASSERT"};
        for (std::string_view kw : kLevels) {
            if (level.size() == kw.size()) {
                bool eq = true;
                for (size_t i = 0; i < level.size(); ++i) {
                    if (ascii_upper(level[i]) != kw[i]) {
                        eq = false;
                        break;
                    }
                }
                if (eq) {
                    this->write(level);
                    return;
                }
            }
        }
        write_identifier(level);
    }

    static constexpr char ascii_upper(char c) noexcept {
        return (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c;
    }

    void visit_open_cursor_stmt(OpenCursorStmt* stmt) {
        this->write("OPEN");
        this->space();
        // Cursor names in OPEN are not quoted
        this->write(stmt->cursor_name);
        if (!stmt->args.empty()) {
            this->write('(');
            this->write_list(stmt->args, [this](SQLNode* arg) { visit(arg); });
            this->write(')');
        }
    }

    void visit_fetch_cursor_stmt(FetchCursorStmt* stmt) {
        this->write("FETCH");
        if (!stmt->direction.empty()) {
            this->space();
            this->write(stmt->direction);
        }
        this->space();
        // Cursor names in FETCH are not quoted
        this->write(stmt->cursor_name);
        if (!stmt->into_variables.empty()) {
            this->space();
            this->write("INTO");
            this->space();
            this->write_list(stmt->into_variables, [this](std::string_view var) {
                // Variable names in INTO are not quoted
                this->write(var);
            });
        }
    }

    void visit_close_cursor_stmt(CloseCursorStmt* stmt) {
        this->write("CLOSE");
        this->space();
        // Cursor names in CLOSE are not quoted
        this->write(stmt->cursor_name);
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
        if (pivot->aggregate)
            visit_function_call(pivot->aggregate);
        this->space();
        this->write("FOR");
        this->space();
        if (pivot->pivot_column)
            visit(pivot->pivot_column);
        this->space();
        this->write("IN");
        this->space();
        this->write('(');
        this->write_list(pivot->pivot_values, [this](SQLNode* val) { visit(val); });
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
        this->write_list(unpivot->unpivot_columns,
                         [this](std::string_view col) { write_identifier(col); });
        this->write(')');
        this->write(')');
    }

    // ========================================================================
    // Grouping Extensions (SQL:1999 T431)
    // ========================================================================

    void visit_rollup_clause(RollupClause* rollup) {
        this->write("ROLLUP(");
        this->write_list(rollup->expressions, [this](SQLNode* expr) { visit(expr); });
        this->write(')');
    }

    void visit_cube_clause(CubeClause* cube) {
        this->write("CUBE(");
        this->write_list(cube->expressions, [this](SQLNode* expr) { visit(expr); });
        this->write(')');
    }

    void visit_grouping_sets(GroupingSets* grouping_sets) {
        this->write("GROUPING SETS (");
        bool first = true;
        for (const auto& set : grouping_sets->sets) {
            if (!first) {
                this->write(',');
                this->space();
            }
            first = false;
            // A set holding exactly one ROLLUP/CUBE/GROUPING SETS element is
            // emitted bare (nested combination); everything else - including
            // the empty grouping set () - is emitted parenthesized.
            if (set.size() == 1 && set[0] &&
                (set[0]->type == SQLNodeKind::ROLLUP_CLAUSE ||
                 set[0]->type == SQLNodeKind::CUBE_CLAUSE ||
                 set[0]->type == SQLNodeKind::GROUPING_SETS)) {
                visit(set[0]);
            } else {
                this->write('(');
                this->write_list(set, [this](SQLNode* expr) { visit(expr); });
                this->write(')');
            }
        }
        this->write(')');
    }

    // ========================================================================
    // Oracle Hierarchical Query Visitors
    // ========================================================================

    void visit_start_with_clause(StartWithClause* clause) {
        this->write("START WITH");
        this->space();
        visit(clause->condition);
    }

    void visit_connect_by_clause(ConnectByClause* clause) {
        this->write("CONNECT BY");
        this->space();
        if (clause->nocycle) {
            this->write("NOCYCLE");
            this->space();
        }
        visit(clause->condition);
    }

    void visit_qualify_clause(QualifyClause* clause) {
        this->write("QUALIFY");
        this->space();
        visit(clause->condition);
    }

    void visit_interval_literal(IntervalLiteral* lit) {
        this->write("INTERVAL");
        this->space();
        this->write(lit->value);
        if (!lit->unit.empty()) {
            this->space();
            this->write(lit->unit);
        }
    }

    // ========================================================================
    // OUTPUT / RETURNING Clause
    // ========================================================================

    /// Is this a T-SQL dialect (native OUTPUT clause)?
    static bool is_tsql_dialect(SQLDialect d) noexcept {
        return d == SQLDialect::SQLServer || d == SQLDialect::AzureSynapse;
    }

    /// Emit an OUTPUT/RETURNING clause. `default_qualifier` is the row
    /// image an unqualified item refers to: "INSERTED" for INSERT/UPDATE,
    /// "DELETED" for DELETE.
    ///
    /// - T-SQL dialects emit the OUTPUT form, qualifying bare items with
    ///   the default qualifier.
    /// - Every other dialect emits RETURNING with the qualifier stripped.
    ///   That is only sound when all items reference the statement's own
    ///   result rows (INSERTED for INSERT/UPDATE, DELETED for DELETE);
    ///   references to the other row image - e.g. DELETED.x in an UPDATE
    ///   (the pre-update values) - have no RETURNING equivalent and throw
    ///   std::logic_error.
    void write_output_clause(OutputClause* clause, std::string_view default_qualifier) {
        if (is_tsql_dialect(this->dialect())) {
            this->write("OUTPUT");
            this->space();
            this->write_list(clause->items, [this, default_qualifier](SQLNode* item) {
                write_tsql_output_item(item, default_qualifier);
            });
        } else {
            this->write("RETURNING");
            this->space();
            this->write_list(clause->items, [this, default_qualifier](SQLNode* item) {
                write_returning_item(item, default_qualifier);
            });
        }
    }

    /// Emit one T-SQL OUTPUT item, qualifying bare column/star references
    /// with the statement's default row image (INSERTED/DELETED).
    void write_tsql_output_item(SQLNode* item, std::string_view default_qualifier) {
        switch (item->type) {
        case SQLNodeKind::ALIAS: {
            auto* alias = static_cast<Alias*>(item);
            write_tsql_output_item(alias->expr, default_qualifier);
            this->space();
            this->write("AS");
            this->space();
            write_identifier(alias->alias);
            return;
        }
        case SQLNodeKind::STAR: {
            auto* star = static_cast<Star*>(item);
            std::string_view qualifier = star->table.empty() ? default_qualifier : star->table;
            if (qualifier == "INSERTED" || qualifier == "DELETED") {
                this->write(qualifier);
                this->write(".*");
                return;
            }
            break;
        }
        case SQLNodeKind::COLUMN: {
            auto* col = static_cast<Column*>(item);
            std::string_view qualifier = col->table.empty() ? default_qualifier : col->table;
            if (qualifier == "INSERTED" || qualifier == "DELETED") {
                this->write(qualifier);
                this->write('.');
                write_identifier(col->column);
                return;
            }
            break;
        }
        default:
            break;
        }
        visit(item);
    }

    /// Emit one RETURNING item, stripping the statement's own row-image
    /// qualifier. A reference to the *other* row image cannot be expressed
    /// with RETURNING and throws std::logic_error.
    void write_returning_item(SQLNode* item, std::string_view allowed_qualifier) {
        switch (item->type) {
        case SQLNodeKind::ALIAS: {
            auto* alias = static_cast<Alias*>(item);
            write_returning_item(alias->expr, allowed_qualifier);
            this->space();
            this->write("AS");
            this->space();
            write_identifier(alias->alias);
            return;
        }
        case SQLNodeKind::STAR: {
            auto* star = static_cast<Star*>(item);
            require_returning_qualifier(star->table, allowed_qualifier);
            if (star->table == "INSERTED" || star->table == "DELETED") {
                this->write('*');
                return;
            }
            break;
        }
        case SQLNodeKind::COLUMN: {
            auto* col = static_cast<Column*>(item);
            require_returning_qualifier(col->table, allowed_qualifier);
            if (col->table == "INSERTED" || col->table == "DELETED") {
                write_identifier(col->column);
                return;
            }
            break;
        }
        default:
            break;
        }
        visit(item);
    }

    /// Throw when an OUTPUT row-image qualifier cannot be transpiled to
    /// RETURNING (i.e. it names the other row image than the statement
    /// itself returns - including OUTPUT clauses mixing INSERTED and
    /// DELETED, which only T-SQL can express).
    static void require_returning_qualifier(std::string_view qualifier,
                                            std::string_view allowed_qualifier) {
        if ((qualifier == "INSERTED" || qualifier == "DELETED") && qualifier != allowed_qualifier) {
            throw std::logic_error("OUTPUT " + std::string(qualifier) +
                                   ".* references require a T-SQL dialect (SQL Server); RETURNING "
                                   "only exposes " +
                                   std::string(allowed_qualifier) + " rows for this statement");
        }
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
        if (stmt->training_query)
            visit(stmt->training_query);
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
        if (expr->input_query)
            visit(expr->input_query);
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
        if (expr->evaluation_query)
            visit(expr->evaluation_query);
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
