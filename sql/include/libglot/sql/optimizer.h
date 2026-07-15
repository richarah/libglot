#pragma once

#include <libglot/util/arena.h>
#include "ast_nodes.h"
#include "lex/tokens.h"

#include <charconv>
#include <climits>
#include <string>
#include <string_view>
#include <vector>

namespace libglot::sql {

/// ============================================================================
/// SQL Optimizer - AST-to-AST Simplification Passes
/// ============================================================================
///
/// Three independent, individually toggleable passes over a parsed SQL AST:
///
///  1. Constant folding
///     - Integer arithmetic on integer literals (+, -, *, /, %), computed in
///       long long with explicit overflow checks. Division/modulo by zero and
///       any overflow leave the expression untouched ("skip on any doubt").
///     - String concatenation of adjacent string literals via ||, splicing
///       the raw quoted token texts so source-level '' escapes survive.
///
///  2. Boolean simplification (only when the operand is a genuine boolean
///     literal, i.e. a Literal node whose value is exactly TRUE/FALSE - never
///     a string 'TRUE' or a column):
///     - x AND TRUE -> x        x AND FALSE -> FALSE
///     - x OR FALSE -> x        x OR TRUE  -> TRUE
///     - NOT TRUE -> FALSE      NOT FALSE  -> TRUE      NOT NOT x -> x
///
///  3. WHERE-clause pruning
///     - WHERE TRUE is removed (the clause carries no filter).
///     - WHERE FALSE is deliberately PRESERVED AS-IS: dropping it would
///       change semantics, and rewriting the statement's shape (e.g. into
///       an empty result) is out of scope. The statement itself is never
///       deleted. (Design choice: no always_false marker flag is added;
///       the preserved literal WHERE FALSE *is* the marker.)
///
/// Arena awareness: every replacement node (folded literal, injected boolean
/// literal) is allocated from the same arena that owns the input AST, so the
/// optimized tree has exactly the input tree's lifetime. The parser produces
/// strict trees (no shared subtrees), so rewriting child pointers in place
/// cannot alias; when a subexpression is replaced, the original nodes are
/// simply left unreferenced in the arena.
///
/// Safety: the walker covers every statement and expression kind it knows
/// how to descend into; any unknown/unhandled node kind is returned
/// unchanged - the optimizer never throws on, mutates, or drops nodes it
/// does not understand.
/// ============================================================================

class SQLOptimizer {
public:
    using TK = libglot::sql::lex::TokenType;

    /// Per-pass toggles. All passes default to enabled.
    struct Options {
        bool fold_constants = true;      // Pass 1: constant folding
        bool simplify_booleans = true;   // Pass 2: boolean simplification
        bool prune_where = true;         // Pass 3: WHERE-clause pruning
    };

    // Two constructors instead of a defaulted Options argument: GCC rejects
    // an Options{} default argument before the enclosing class is complete.
    explicit SQLOptimizer(libglot::Arena& arena)
        : arena_(arena), options_() {}

    SQLOptimizer(libglot::Arena& arena, const Options& options)
        : arena_(arena), options_(options) {}

    /// Optimize a statement or expression tree. Returns the (possibly
    /// replaced) root; child pointers inside retained nodes are updated
    /// in place. Passing nullptr returns nullptr.
    SQLNode* optimize(SQLNode* node) {
        return opt(node);
    }

private:
    libglot::Arena& arena_;
    Options options_;

    // ========================================================================
    // Recursive walker
    // ========================================================================

    SQLNode* opt(SQLNode* node) {
        if (!node) return nullptr;

        switch (node->type) {
            // ================================================================
            // Leaf expressions - nothing to do
            // ================================================================
            case SQLNodeKind::LITERAL:
            case SQLNodeKind::COLUMN:
            case SQLNodeKind::STAR:
            case SQLNodeKind::PARAMETER:
            case SQLNodeKind::TABLE_REF:
                return node;

            // ================================================================
            // Operators
            // ================================================================
            case SQLNodeKind::BINARY_OP: {
                auto* op = static_cast<BinaryOp*>(node);
                op->left = opt(op->left);
                op->right = opt(op->right);
                if (options_.fold_constants) {
                    if (SQLNode* folded = fold_binary(op)) return folded;
                }
                if (options_.simplify_booleans) {
                    if (SQLNode* simplified = simplify_boolean_binary(op)) return simplified;
                }
                return op;
            }

            case SQLNodeKind::UNARY_OP: {
                auto* op = static_cast<UnaryOp*>(node);
                op->operand = opt(op->operand);
                if (options_.simplify_booleans && op->op == TK::NOT) {
                    if (is_bool_literal(op->operand, "TRUE")) return make_bool_literal(false);
                    if (is_bool_literal(op->operand, "FALSE")) return make_bool_literal(true);
                    if (op->operand && op->operand->type == SQLNodeKind::UNARY_OP) {
                        auto* inner = static_cast<UnaryOp*>(op->operand);
                        if (inner->op == TK::NOT) return inner->operand;  // NOT NOT x -> x
                    }
                }
                if (options_.fold_constants && op->op == TK::MINUS) {
                    // Fold unary minus of an integer literal so nested
                    // arithmetic like -2 + 3 becomes foldable
                    long long value = 0;
                    if (is_int_literal(op->operand, value) && value != LLONG_MIN) {
                        return make_int_literal(-value);
                    }
                }
                return op;
            }

            // ================================================================
            // Composite expressions
            // ================================================================
            case SQLNodeKind::FUNCTION_CALL: {
                auto* fn = static_cast<FunctionCall*>(node);
                opt_each(fn->args);
                return fn;
            }

            case SQLNodeKind::CASE_EXPR: {
                auto* c = static_cast<CaseExpr*>(node);
                c->case_value = opt(c->case_value);
                for (auto& when : c->when_clauses) {
                    when.first = opt(when.first);
                    when.second = opt(when.second);
                }
                c->else_expr = opt(c->else_expr);
                return c;
            }

            case SQLNodeKind::CAST_EXPR: {
                auto* c = static_cast<CastExpr*>(node);
                c->expr = opt(c->expr);
                return c;
            }

            case SQLNodeKind::COALESCE_EXPR: {
                auto* c = static_cast<CoalesceExpr*>(node);
                opt_each(c->args);
                return c;
            }

            case SQLNodeKind::NULLIF_EXPR: {
                auto* n = static_cast<NullifExpr*>(node);
                n->expr1 = opt(n->expr1);
                n->expr2 = opt(n->expr2);
                return n;
            }

            case SQLNodeKind::BETWEEN_EXPR: {
                auto* b = static_cast<BetweenExpr*>(node);
                b->expr = opt(b->expr);
                b->lower = opt(b->lower);
                b->upper = opt(b->upper);
                return b;
            }

            case SQLNodeKind::IN_EXPR: {
                auto* in = static_cast<InExpr*>(node);
                in->expr = opt(in->expr);
                opt_each(in->values);
                return in;
            }

            case SQLNodeKind::EXISTS_EXPR: {
                auto* e = static_cast<ExistsExpr*>(node);
                e->subquery = opt(e->subquery);
                return e;
            }

            case SQLNodeKind::ANY_EXPR: {
                auto* a = static_cast<AnyExpr*>(node);
                a->left = opt(a->left);
                a->subquery = opt(a->subquery);
                return a;
            }

            case SQLNodeKind::ALL_EXPR: {
                auto* a = static_cast<AllExpr*>(node);
                a->left = opt(a->left);
                a->subquery = opt(a->subquery);
                return a;
            }

            case SQLNodeKind::SUBQUERY_EXPR: {
                auto* s = static_cast<SubqueryExpr*>(node);
                s->query = opt(s->query);
                return s;
            }

            case SQLNodeKind::ARRAY_LITERAL: {
                auto* a = static_cast<ArrayLiteral*>(node);
                opt_each(a->elements);
                return a;
            }

            case SQLNodeKind::ARRAY_INDEX: {
                auto* a = static_cast<ArrayIndex*>(node);
                a->array = opt(a->array);
                a->index = opt(a->index);
                return a;
            }

            case SQLNodeKind::JSON_EXPR: {
                auto* j = static_cast<JsonExpr*>(node);
                j->json_expr = opt(j->json_expr);
                j->key = opt(j->key);
                return j;
            }

            case SQLNodeKind::REGEX_MATCH: {
                auto* r = static_cast<RegexMatch*>(node);
                r->expr = opt(r->expr);
                r->pattern = opt(r->pattern);
                return r;
            }

            case SQLNodeKind::ALIAS: {
                auto* a = static_cast<Alias*>(node);
                a->expr = opt(a->expr);
                return a;
            }

            // ================================================================
            // Window functions
            // ================================================================
            case SQLNodeKind::WINDOW_FUNCTION: {
                auto* w = static_cast<WindowFunction*>(node);
                opt_each(w->args);
                if (w->over) (void)opt(w->over);
                return w;
            }

            case SQLNodeKind::WINDOW_SPEC: {
                auto* spec = static_cast<WindowSpec*>(node);
                opt_each(spec->partition_by);
                opt_each(spec->order_by);
                if (spec->frame) {
                    spec->frame->start_offset = opt(spec->frame->start_offset);
                    spec->frame->end_offset = opt(spec->frame->end_offset);
                }
                return spec;
            }

            // ================================================================
            // FROM clause elements
            // ================================================================
            case SQLNodeKind::JOIN_CLAUSE: {
                auto* j = static_cast<JoinClause*>(node);
                j->left_table = opt(j->left_table);
                j->right_table = opt(j->right_table);
                j->condition = opt(j->condition);
                return j;
            }

            case SQLNodeKind::LATERAL_JOIN: {
                auto* l = static_cast<LateralJoin*>(node);
                l->table_expr = opt(l->table_expr);
                return l;
            }

            case SQLNodeKind::VALUES_CLAUSE: {
                auto* v = static_cast<ValuesClause*>(node);
                for (auto& row : v->rows) opt_each(row);
                return v;
            }

            case SQLNodeKind::TABLESAMPLE: {
                auto* t = static_cast<Tablesample*>(node);
                t->percent = opt(t->percent);
                t->seed = opt(t->seed);
                return t;
            }

            // ================================================================
            // Grouping extensions
            // ================================================================
            case SQLNodeKind::GROUPING_SETS: {
                auto* g = static_cast<GroupingSets*>(node);
                for (auto& set : g->sets) opt_each(set);
                return g;
            }

            case SQLNodeKind::ROLLUP_CLAUSE: {
                auto* r = static_cast<RollupClause*>(node);
                opt_each(r->expressions);
                return r;
            }

            case SQLNodeKind::CUBE_CLAUSE: {
                auto* c = static_cast<CubeClause*>(node);
                opt_each(c->expressions);
                return c;
            }

            // ================================================================
            // Query structure
            // ================================================================
            case SQLNodeKind::SELECT_STMT: {
                auto* stmt = static_cast<SelectStmt*>(node);
                if (stmt->with) {
                    for (auto* cte : stmt->with->ctes) {
                        if (cte) cte->query = opt(cte->query);
                    }
                }
                opt_each(stmt->columns);
                stmt->from = opt(stmt->from);
                stmt->where = prune_where(opt(stmt->where));
                opt_each(stmt->group_by);
                stmt->having = opt(stmt->having);
                if (stmt->qualify) stmt->qualify->condition = opt(stmt->qualify->condition);
                for (auto* item : stmt->order_by) {
                    if (item) item->expr = opt(item->expr);
                }
                stmt->limit = opt(stmt->limit);
                stmt->offset = opt(stmt->offset);
                if (stmt->start_with) stmt->start_with->condition = opt(stmt->start_with->condition);
                if (stmt->connect_by) stmt->connect_by->condition = opt(stmt->connect_by->condition);
                return stmt;
            }

            case SQLNodeKind::CTE: {
                auto* cte = static_cast<CTE*>(node);
                cte->query = opt(cte->query);
                return cte;
            }

            case SQLNodeKind::ORDER_BY_ITEM: {
                auto* item = static_cast<OrderByItem*>(node);
                item->expr = opt(item->expr);
                return item;
            }

            case SQLNodeKind::QUALIFY_CLAUSE: {
                auto* q = static_cast<QualifyClause*>(node);
                q->condition = opt(q->condition);
                return q;
            }

            // ================================================================
            // Set operations
            // ================================================================
            case SQLNodeKind::UNION_STMT: {
                auto* u = static_cast<UnionStmt*>(node);
                u->left = opt(u->left);
                u->right = opt(u->right);
                return u;
            }

            case SQLNodeKind::INTERSECT_STMT: {
                auto* i = static_cast<IntersectStmt*>(node);
                i->left = opt(i->left);
                i->right = opt(i->right);
                return i;
            }

            case SQLNodeKind::EXCEPT_STMT: {
                auto* e = static_cast<ExceptStmt*>(node);
                e->left = opt(e->left);
                e->right = opt(e->right);
                return e;
            }

            // ================================================================
            // DML statements
            // ================================================================
            case SQLNodeKind::INSERT_STMT: {
                auto* stmt = static_cast<InsertStmt*>(node);
                for (auto& row : stmt->values) opt_each(row);
                stmt->select_query = opt(stmt->select_query);
                if (stmt->output) opt_each(stmt->output->items);
                return stmt;
            }

            case SQLNodeKind::UPDATE_STMT: {
                auto* stmt = static_cast<UpdateStmt*>(node);
                for (auto& assign : stmt->assignments) {
                    assign.second = opt(assign.second);
                }
                stmt->from = opt(stmt->from);
                stmt->where = prune_where(opt(stmt->where));
                if (stmt->output) opt_each(stmt->output->items);
                return stmt;
            }

            case SQLNodeKind::DELETE_STMT: {
                auto* stmt = static_cast<DeleteStmt*>(node);
                stmt->using_clause = opt(stmt->using_clause);
                stmt->where = prune_where(opt(stmt->where));
                if (stmt->output) opt_each(stmt->output->items);
                return stmt;
            }

            case SQLNodeKind::MERGE_STMT: {
                auto* stmt = static_cast<MergeStmt*>(node);
                stmt->source = opt(stmt->source);
                stmt->on_condition = opt(stmt->on_condition);
                for (auto& clause : stmt->when_clauses) {
                    clause.extra_condition = opt(clause.extra_condition);
                    for (auto& assign : clause.update_assignments) {
                        assign.second = opt(assign.second);
                    }
                    opt_each(clause.insert_values);
                }
                return stmt;
            }

            // ================================================================
            // DDL with embedded queries/expressions
            // ================================================================
            case SQLNodeKind::CREATE_TABLE_STMT: {
                auto* stmt = static_cast<CreateTableStmt*>(node);
                stmt->as_select = opt(stmt->as_select);
                for (auto* col : stmt->columns) {
                    if (col) {
                        col->default_value = opt(col->default_value);
                        col->check_expr = opt(col->check_expr);
                    }
                }
                for (auto* constraint : stmt->constraints) {
                    if (constraint) constraint->check_expr = opt(constraint->check_expr);
                }
                return stmt;
            }

            case SQLNodeKind::CREATE_VIEW_STMT: {
                auto* stmt = static_cast<CreateViewStmt*>(node);
                stmt->query = opt(stmt->query);
                return stmt;
            }

            case SQLNodeKind::EXPLAIN_STMT: {
                auto* stmt = static_cast<ExplainStmt*>(node);
                stmt->statement = opt(stmt->statement);
                return stmt;
            }

            // ================================================================
            // Procedural statements
            // ================================================================
            case SQLNodeKind::IF_STMT: {
                auto* stmt = static_cast<IfStmt*>(node);
                stmt->condition = opt(stmt->condition);
                opt_each(stmt->then_stmts);
                for (auto& branch : stmt->elseif_branches) {
                    branch.first = opt(branch.first);
                    opt_each(branch.second);
                }
                opt_each(stmt->else_stmts);
                return stmt;
            }

            case SQLNodeKind::WHILE_LOOP: {
                auto* loop = static_cast<WhileLoop*>(node);
                loop->condition = opt(loop->condition);
                opt_each(loop->body);
                return loop;
            }

            case SQLNodeKind::FOR_LOOP: {
                auto* loop = static_cast<ForLoop*>(node);
                loop->start_value = opt(loop->start_value);
                loop->end_value = opt(loop->end_value);
                opt_each(loop->body);
                return loop;
            }

            case SQLNodeKind::LOOP_STMT: {
                auto* loop = static_cast<LoopStmt*>(node);
                opt_each(loop->body);
                return loop;
            }

            case SQLNodeKind::BEGIN_END_BLOCK: {
                auto* block = static_cast<BeginEndBlock*>(node);
                opt_each(block->statements);
                return block;
            }

            case SQLNodeKind::EXCEPTION_BLOCK: {
                auto* block = static_cast<ExceptionBlock*>(node);
                opt_each(block->try_statements);
                for (auto& handler : block->handlers) {
                    opt_each(handler.second);
                }
                return block;
            }

            case SQLNodeKind::RETURN_STMT: {
                auto* stmt = static_cast<ReturnStmt*>(node);
                stmt->return_value = opt(stmt->return_value);
                return stmt;
            }

            case SQLNodeKind::ASSIGNMENT_STMT: {
                auto* stmt = static_cast<AssignmentStmt*>(node);
                stmt->value = opt(stmt->value);
                return stmt;
            }

            case SQLNodeKind::DECLARE_VAR_STMT: {
                auto* stmt = static_cast<DeclareVarStmt*>(node);
                stmt->default_value = opt(stmt->default_value);
                return stmt;
            }

            case SQLNodeKind::DECLARE_CURSOR_STMT: {
                auto* stmt = static_cast<DeclareCursorStmt*>(node);
                stmt->query = opt(stmt->query);
                return stmt;
            }

            case SQLNodeKind::SET_STMT: {
                auto* stmt = static_cast<SetStmt*>(node);
                for (auto& assign : stmt->assignments) {
                    assign.second = opt(assign.second);
                }
                return stmt;
            }

            case SQLNodeKind::CALL_PROCEDURE_STMT: {
                auto* stmt = static_cast<CallProcedureStmt*>(node);
                opt_each(stmt->arguments);
                return stmt;
            }

            case SQLNodeKind::RAISE_STMT: {
                auto* stmt = static_cast<RaiseStmt*>(node);
                opt_each(stmt->args);
                return stmt;
            }

            case SQLNodeKind::OPEN_CURSOR_STMT: {
                auto* stmt = static_cast<OpenCursorStmt*>(node);
                opt_each(stmt->args);
                return stmt;
            }

            case SQLNodeKind::CREATE_PROCEDURE_STMT: {
                auto* stmt = static_cast<CreateProcedureStmt*>(node);
                opt_each(stmt->body);
                return stmt;
            }

            case SQLNodeKind::CREATE_TRIGGER_STMT: {
                auto* stmt = static_cast<CreateTriggerStmt*>(node);
                opt_each(stmt->body);
                return stmt;
            }

            // ================================================================
            // Everything else (DROP/GRANT/SHOW/transactions/...) carries no
            // optimizable expressions - and unknown future kinds must never
            // crash the walker. Return unchanged.
            // ================================================================
            default:
                return node;
        }
    }

    /// Optimize each element of a node list in place
    void opt_each(std::vector<SQLNode*>& nodes) {
        for (auto& n : nodes) {
            n = opt(n);
        }
    }

    // ========================================================================
    // Pass 3: WHERE-clause pruning
    // ========================================================================

    /// WHERE TRUE -> no WHERE clause. WHERE FALSE is preserved as-is
    /// (see the class comment for the rationale).
    SQLNode* prune_where(SQLNode* where) {
        if (options_.prune_where && is_bool_literal(where, "TRUE")) {
            return nullptr;
        }
        return where;
    }

    // ========================================================================
    // Pass 1: constant folding
    // ========================================================================

    /// Try to fold a binary operation over literals. Returns nullptr when
    /// no (safe) fold applies.
    SQLNode* fold_binary(BinaryOp* op) {
        // Integer arithmetic with overflow / division-by-zero guards
        long long lhs = 0;
        long long rhs = 0;
        if (is_int_literal(op->left, lhs) && is_int_literal(op->right, rhs)) {
            long long result = 0;
            bool ok = false;
            switch (op->op) {
                case TK::PLUS:
                    ok = !__builtin_add_overflow(lhs, rhs, &result);
                    break;
                case TK::MINUS:
                    ok = !__builtin_sub_overflow(lhs, rhs, &result);
                    break;
                case TK::STAR:
                    ok = !__builtin_mul_overflow(lhs, rhs, &result);
                    break;
                case TK::SLASH:
                    // Guard division by zero and LLONG_MIN / -1 overflow
                    ok = (rhs != 0) && !(lhs == LLONG_MIN && rhs == -1);
                    if (ok) result = lhs / rhs;
                    break;
                case TK::PERCENT:
                    ok = (rhs != 0) && !(lhs == LLONG_MIN && rhs == -1);
                    if (ok) result = lhs % rhs;
                    break;
                default:
                    break;
            }
            if (ok) {
                return make_int_literal(result);
            }
        }

        // String concatenation: 'foo' || 'bar' -> 'foobar'. The literal
        // values are the raw quoted token texts, so splice at the quotes
        // and source-level '' escapes are preserved verbatim.
        if (op->op == TK::CONCAT) {
            std::string_view left_text;
            std::string_view right_text;
            if (is_string_literal(op->left, left_text) &&
                is_string_literal(op->right, right_text)) {
                std::string merged;
                merged.reserve(left_text.size() + right_text.size() - 2);
                merged.append(left_text.substr(0, left_text.size() - 1));  // 'foo
                merged.append(right_text.substr(1));                       // bar'
                return arena_.create<Literal>(arena_.copy_source(merged));
            }
        }

        return nullptr;
    }

    // ========================================================================
    // Pass 2: boolean simplification
    // ========================================================================

    /// Simplify AND/OR against genuine boolean literals. Returns nullptr
    /// when no rule applies. Returned nodes are existing subtrees (or the
    /// literal itself), never fresh aliases of shared state.
    SQLNode* simplify_boolean_binary(BinaryOp* op) {
        if (op->op == TK::AND) {
            if (is_bool_literal(op->left, "TRUE")) return op->right;   // TRUE AND x -> x
            if (is_bool_literal(op->right, "TRUE")) return op->left;   // x AND TRUE -> x
            if (is_bool_literal(op->left, "FALSE")) return op->left;   // FALSE AND x -> FALSE
            if (is_bool_literal(op->right, "FALSE")) return op->right; // x AND FALSE -> FALSE
        } else if (op->op == TK::OR) {
            if (is_bool_literal(op->left, "FALSE")) return op->right;  // FALSE OR x -> x
            if (is_bool_literal(op->right, "FALSE")) return op->left;  // x OR FALSE -> x
            if (is_bool_literal(op->left, "TRUE")) return op->left;    // TRUE OR x -> TRUE
            if (is_bool_literal(op->right, "TRUE")) return op->right;  // x OR TRUE -> TRUE
        }
        return nullptr;
    }

    // ========================================================================
    // Literal classification helpers
    // ========================================================================

    /// Is this a genuine boolean literal (parsed from the TRUE/FALSE
    /// keywords)? String literals like 'TRUE' carry their quotes in the
    /// value and never match.
    static bool is_bool_literal(const SQLNode* node, std::string_view keyword) noexcept {
        return node && node->type == SQLNodeKind::LITERAL &&
               static_cast<const Literal*>(node)->value == keyword;
    }

    /// Is this an integer literal (optionally negative, from an earlier
    /// fold)? Rejects floats (1.5), exponents (1e10), hex/binary (0x1F),
    /// strings, and anything that does not parse completely as a base-10
    /// long long.
    static bool is_int_literal(const SQLNode* node, long long& value) noexcept {
        if (!node || node->type != SQLNodeKind::LITERAL) return false;
        std::string_view text = static_cast<const Literal*>(node)->value;
        if (text.empty()) return false;
        const char* first = text.data();
        const char* last = text.data() + text.size();
        auto [ptr, ec] = std::from_chars(first, last, value, 10);
        return ec == std::errc{} && ptr == last;
    }

    /// Is this a string literal? The parser stores string tokens with their
    /// outer quotes intact ('foo'), which is what we check for.
    static bool is_string_literal(const SQLNode* node, std::string_view& text) noexcept {
        if (!node || node->type != SQLNodeKind::LITERAL) return false;
        std::string_view value = static_cast<const Literal*>(node)->value;
        if (value.size() < 2 || value.front() != '\'' || value.back() != '\'') return false;
        text = value;
        return true;
    }

    // ========================================================================
    // Arena-backed replacement node factories
    // ========================================================================

    SQLNode* make_int_literal(long long value) {
        return arena_.create<Literal>(arena_.copy_source(std::to_string(value)));
    }

    SQLNode* make_bool_literal(bool value) {
        // "TRUE"/"FALSE" are static string literals; a Literal's
        // string_view may point at them safely.
        return arena_.create<Literal>(value ? std::string_view{"TRUE"}
                                            : std::string_view{"FALSE"});
    }
};

} // namespace libglot::sql
