#pragma once

#include "ast_nodes.h"
#include <libglot/util/arena.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace libglot::sql {

/// ============================================================================
/// Oracle CONNECT BY -> WITH RECURSIVE lowering
/// ============================================================================
///
/// Lowers a Selectstmt using Oracle's START WITH / CONNECT BY hierarchical
/// query syntax into an equivalent WITH RECURSIVE common table expression,
/// for generating hierarchical queries on dialects that have no native
/// CONNECT BY support (see SQLGenerator's optional transform arena in
/// generator.h).
///
/// For:
///   SELECT <cols> FROM t [alias] [WHERE w] START WITH s CONNECT BY [PRIOR] c
///
/// produces:
///   WITH RECURSIVE <h> AS (
///       SELECT <t_alias>.*, 1 AS <lvl> FROM t <alias> WHERE s
///     UNION ALL
///       SELECT <t_alias>.*, <h>.<lvl> + 1 FROM t <alias> JOIN <h> ON c'
///   ) SELECT <cols'> FROM <h> [WHERE w]
///
/// where <h> is a generated CTE name ("hierarchy", suffixed _2, _3, ... on
/// collision with an existing table/alias/CTE name), <lvl> ("level") is
/// added only when the original query references the LEVEL pseudo-column,
/// and c' is the CONNECT BY condition with `PRIOR expr` rewritten to `expr`
/// qualified by <h> (the parent row) and every other bare column reference
/// qualified by the source table's alias (the child row).
///
/// Forms with no clean lowering throw std::logic_error:
///   - CONNECT BY NOCYCLE (would need an explicit key column to build a
///     cycle-detection path; not derivable from the syntax alone)
///   - ORDER SIBLINGS BY (depends on the traversal path, not expressible as
///     a plain ORDER BY over the flattened hierarchy)
///   - a JOIN/multi-table FROM clause combined with CONNECT BY
///
/// CONNECT_BY_ROOT / SYS_CONNECT_BY_PATH are not handled here because the
/// parser does not accept them in the first place (their tokens are never
/// wired into parse_primary's expression grammar), so an AST reaching this
/// function can never contain them.
/// ============================================================================

namespace connect_by_lowering_detail {

using TK = libglot::sql::lex::TokenType;

/// Case-insensitive ASCII identifier comparison.
[[nodiscard]] inline bool ci_equal(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) {
        return false;
    }
    for (size_t i = 0; i < a.size(); ++i) {
        char ca = a[i];
        char cb = b[i];
        if (ca >= 'a' && ca <= 'z') {
            ca = static_cast<char>(ca - 'a' + 'A');
        }
        if (cb >= 'a' && cb <= 'z') {
            cb = static_cast<char>(cb - 'a' + 'A');
        }
        if (ca != cb) {
            return false;
        }
    }
    return true;
}

/// Does `node`'s subtree reference the bare (unqualified) LEVEL pseudo-column
/// anywhere? Used to decide whether the lowered CTE needs a level counter.
/// Only descends into ordinary expression nodes; subquery boundaries
/// (SUBQUERY_EXPR/EXISTS_EXPR/ANY_EXPR/ALL_EXPR) are not crossed since LEVEL
/// there would refer to a different, inner query's hierarchy (if any).
[[nodiscard]] inline bool references_level(const SQLNode* node) {
    if (!node) {
        return false;
    }
    switch (node->type) {
    case SQLNodeKind::COLUMN: {
        const auto* col = static_cast<const Column*>(node);
        return col->table.empty() && ci_equal(col->column, "LEVEL");
    }
    case SQLNodeKind::BINARY_OP: {
        const auto* op = static_cast<const BinaryOp*>(node);
        return references_level(op->left) || references_level(op->right);
    }
    case SQLNodeKind::UNARY_OP:
        return references_level(static_cast<const UnaryOp*>(node)->operand);
    case SQLNodeKind::FUNCTION_CALL: {
        for (const auto* a : static_cast<const FunctionCall*>(node)->args) {
            if (references_level(a)) {
                return true;
            }
        }
        return false;
    }
    case SQLNodeKind::CASE_EXPR: {
        const auto* ce = static_cast<const CaseExpr*>(node);
        if (references_level(ce->case_value) || references_level(ce->else_expr)) {
            return true;
        }
        for (const auto& wc : ce->when_clauses) {
            if (references_level(wc.first) || references_level(wc.second)) {
                return true;
            }
        }
        return false;
    }
    case SQLNodeKind::CAST_EXPR:
        return references_level(static_cast<const CastExpr*>(node)->expr);
    case SQLNodeKind::COALESCE_EXPR: {
        for (const auto* a : static_cast<const CoalesceExpr*>(node)->args) {
            if (references_level(a)) {
                return true;
            }
        }
        return false;
    }
    case SQLNodeKind::NULLIF_EXPR: {
        const auto* n = static_cast<const NullifExpr*>(node);
        return references_level(n->expr1) || references_level(n->expr2);
    }
    case SQLNodeKind::BETWEEN_EXPR: {
        const auto* b = static_cast<const BetweenExpr*>(node);
        return references_level(b->expr) || references_level(b->lower) ||
               references_level(b->upper);
    }
    case SQLNodeKind::IN_EXPR: {
        const auto* in = static_cast<const InExpr*>(node);
        if (references_level(in->expr)) {
            return true;
        }
        for (const auto* v : in->values) {
            if (references_level(v)) {
                return true;
            }
        }
        return false;
    }
    case SQLNodeKind::ALIAS:
        return references_level(static_cast<const Alias*>(node)->expr);
    case SQLNodeKind::ARRAY_LITERAL: {
        for (const auto* e : static_cast<const ArrayLiteral*>(node)->elements) {
            if (references_level(e)) {
                return true;
            }
        }
        return false;
    }
    case SQLNodeKind::ARRAY_INDEX: {
        const auto* ai = static_cast<const ArrayIndex*>(node);
        return references_level(ai->array) || references_level(ai->index);
    }
    case SQLNodeKind::JSON_EXPR: {
        const auto* j = static_cast<const JsonExpr*>(node);
        return references_level(j->json_expr) || references_level(j->key);
    }
    case SQLNodeKind::REGEX_MATCH: {
        const auto* r = static_cast<const RegexMatch*>(node);
        return references_level(r->expr) || references_level(r->pattern);
    }
    default:
        // Literals, parameters, subqueries, window functions, etc. - no
        // bare-column LEVEL reference to find.
        return false;
    }
}

[[nodiscard]] inline bool select_references_level(const SelectStmt* stmt) {
    for (const auto* c : stmt->columns) {
        if (references_level(c)) {
            return true;
        }
    }
    if (references_level(stmt->where) || references_level(stmt->having)) {
        return true;
    }
    for (const auto* g : stmt->group_by) {
        if (references_level(g)) {
            return true;
        }
    }
    for (const auto* ob : stmt->order_by) {
        if (references_level(ob->expr)) {
            return true;
        }
    }
    if (stmt->qualify && references_level(stmt->qualify->condition)) {
        return true;
    }
    if (stmt->connect_by && references_level(stmt->connect_by->condition)) {
        return true;
    }
    if (stmt->start_with && references_level(stmt->start_with->condition)) {
        return true;
    }
    return false;
}

/// Rewrite an expression tree for the outer, post-hierarchy SELECT list:
///   - a bare LEVEL reference becomes a bare reference to `lvl_name` (only
///     when `has_level` is set - otherwise LEVEL cannot appear, since a
///     query referencing it always sets has_level)
///   - a column qualified with `from_alias` is re-qualified to `to_name`
///   - unqualified columns and everything else pass through unchanged
///
/// Returns the original pointer when no rewrite was needed anywhere in the
/// subtree, so unaffected structure is shared rather than copied; the input
/// AST is never mutated.
[[nodiscard]] inline SQLNode* requalify(libglot::Arena& arena, SQLNode* node,
                                        std::string_view from_alias, std::string_view to_name,
                                        bool has_level, std::string_view lvl_name) {
    if (!node) {
        return nullptr;
    }
    switch (node->type) {
    case SQLNodeKind::COLUMN: {
        auto* col = static_cast<Column*>(node);
        if (has_level && col->table.empty() && ci_equal(col->column, "LEVEL")) {
            return arena.create<Column>(lvl_name);
        }
        if (!col->table.empty() && ci_equal(col->table, from_alias)) {
            return arena.create<Column>(to_name, col->column);
        }
        return node;
    }
    case SQLNodeKind::STAR: {
        auto* st = static_cast<Star*>(node);
        if (!st->table.empty() && ci_equal(st->table, from_alias)) {
            return arena.create<Star>(to_name);
        }
        return node;
    }
    case SQLNodeKind::BINARY_OP: {
        auto* op = static_cast<BinaryOp*>(node);
        SQLNode* l = requalify(arena, op->left, from_alias, to_name, has_level, lvl_name);
        SQLNode* r = requalify(arena, op->right, from_alias, to_name, has_level, lvl_name);
        if (l == op->left && r == op->right) {
            return node;
        }
        return arena.create<BinaryOp>(op->op, l, r);
    }
    case SQLNodeKind::UNARY_OP: {
        auto* op = static_cast<UnaryOp*>(node);
        SQLNode* operand = requalify(arena, op->operand, from_alias, to_name, has_level, lvl_name);
        if (operand == op->operand) {
            return node;
        }
        return arena.create<UnaryOp>(op->op, operand);
    }
    case SQLNodeKind::FUNCTION_CALL: {
        auto* fc = static_cast<FunctionCall*>(node);
        std::vector<SQLNode*> new_args;
        new_args.reserve(fc->args.size());
        bool changed = false;
        for (auto* a : fc->args) {
            SQLNode* na = requalify(arena, a, from_alias, to_name, has_level, lvl_name);
            changed = changed || (na != a);
            new_args.push_back(na);
        }
        if (!changed) {
            return node;
        }
        return arena.create<FunctionCall>(fc->name, std::move(new_args), fc->distinct);
    }
    case SQLNodeKind::ALIAS: {
        auto* al = static_cast<Alias*>(node);
        SQLNode* e = requalify(arena, al->expr, from_alias, to_name, has_level, lvl_name);
        if (e == al->expr) {
            return node;
        }
        return arena.create<Alias>(e, al->alias);
    }
    case SQLNodeKind::CAST_EXPR: {
        auto* c = static_cast<CastExpr*>(node);
        SQLNode* e = requalify(arena, c->expr, from_alias, to_name, has_level, lvl_name);
        if (e == c->expr) {
            return node;
        }
        return arena.create<CastExpr>(e, c->target_type);
    }
    case SQLNodeKind::CASE_EXPR: {
        auto* ce = static_cast<CaseExpr*>(node);
        auto* new_ce = arena.create<CaseExpr>();
        bool changed = false;
        SQLNode* cv = requalify(arena, ce->case_value, from_alias, to_name, has_level, lvl_name);
        changed = changed || (cv != ce->case_value);
        new_ce->case_value = cv;
        for (auto& wc : ce->when_clauses) {
            SQLNode* w = requalify(arena, wc.first, from_alias, to_name, has_level, lvl_name);
            SQLNode* t = requalify(arena, wc.second, from_alias, to_name, has_level, lvl_name);
            changed = changed || (w != wc.first) || (t != wc.second);
            new_ce->when_clauses.emplace_back(w, t);
        }
        SQLNode* el = requalify(arena, ce->else_expr, from_alias, to_name, has_level, lvl_name);
        changed = changed || (el != ce->else_expr);
        new_ce->else_expr = el;
        return changed ? static_cast<SQLNode*>(new_ce) : node;
    }
    case SQLNodeKind::COALESCE_EXPR: {
        auto* co = static_cast<CoalesceExpr*>(node);
        std::vector<SQLNode*> new_args;
        new_args.reserve(co->args.size());
        bool changed = false;
        for (auto* a : co->args) {
            SQLNode* na = requalify(arena, a, from_alias, to_name, has_level, lvl_name);
            changed = changed || (na != a);
            new_args.push_back(na);
        }
        if (!changed) {
            return node;
        }
        return arena.create<CoalesceExpr>(std::move(new_args));
    }
    case SQLNodeKind::NULLIF_EXPR: {
        auto* n = static_cast<NullifExpr*>(node);
        SQLNode* e1 = requalify(arena, n->expr1, from_alias, to_name, has_level, lvl_name);
        SQLNode* e2 = requalify(arena, n->expr2, from_alias, to_name, has_level, lvl_name);
        if (e1 == n->expr1 && e2 == n->expr2) {
            return node;
        }
        return arena.create<NullifExpr>(e1, e2);
    }
    case SQLNodeKind::BETWEEN_EXPR: {
        auto* b = static_cast<BetweenExpr*>(node);
        SQLNode* e = requalify(arena, b->expr, from_alias, to_name, has_level, lvl_name);
        SQLNode* lo = requalify(arena, b->lower, from_alias, to_name, has_level, lvl_name);
        SQLNode* hi = requalify(arena, b->upper, from_alias, to_name, has_level, lvl_name);
        if (e == b->expr && lo == b->lower && hi == b->upper) {
            return node;
        }
        return arena.create<BetweenExpr>(e, lo, hi, b->not_between);
    }
    case SQLNodeKind::IN_EXPR: {
        auto* in = static_cast<InExpr*>(node);
        SQLNode* e = requalify(arena, in->expr, from_alias, to_name, has_level, lvl_name);
        std::vector<SQLNode*> new_values;
        new_values.reserve(in->values.size());
        bool changed = (e != in->expr);
        for (auto* v : in->values) {
            SQLNode* nv = requalify(arena, v, from_alias, to_name, has_level, lvl_name);
            changed = changed || (nv != v);
            new_values.push_back(nv);
        }
        if (!changed) {
            return node;
        }
        return arena.create<InExpr>(e, std::move(new_values), in->not_in);
    }
    case SQLNodeKind::ARRAY_LITERAL: {
        auto* al = static_cast<ArrayLiteral*>(node);
        std::vector<SQLNode*> new_elems;
        new_elems.reserve(al->elements.size());
        bool changed = false;
        for (auto* e : al->elements) {
            SQLNode* ne = requalify(arena, e, from_alias, to_name, has_level, lvl_name);
            changed = changed || (ne != e);
            new_elems.push_back(ne);
        }
        if (!changed) {
            return node;
        }
        return arena.create<ArrayLiteral>(std::move(new_elems));
    }
    case SQLNodeKind::ARRAY_INDEX: {
        auto* ai = static_cast<ArrayIndex*>(node);
        SQLNode* arr = requalify(arena, ai->array, from_alias, to_name, has_level, lvl_name);
        SQLNode* idx = requalify(arena, ai->index, from_alias, to_name, has_level, lvl_name);
        if (arr == ai->array && idx == ai->index) {
            return node;
        }
        auto* new_ai = arena.create<ArrayIndex>(arr, idx);
        new_ai->subscript = ai->subscript;
        return new_ai;
    }
    case SQLNodeKind::JSON_EXPR: {
        auto* j = static_cast<JsonExpr*>(node);
        SQLNode* je = requalify(arena, j->json_expr, from_alias, to_name, has_level, lvl_name);
        SQLNode* k = requalify(arena, j->key, from_alias, to_name, has_level, lvl_name);
        if (je == j->json_expr && k == j->key) {
            return node;
        }
        return arena.create<JsonExpr>(je, k, j->op_type);
    }
    case SQLNodeKind::REGEX_MATCH: {
        auto* r = static_cast<RegexMatch*>(node);
        SQLNode* e = requalify(arena, r->expr, from_alias, to_name, has_level, lvl_name);
        SQLNode* p = requalify(arena, r->pattern, from_alias, to_name, has_level, lvl_name);
        if (e == r->expr && p == r->pattern) {
            return node;
        }
        return arena.create<RegexMatch>(e, p, r->similar_to);
    }
    default:
        // Literals, parameters, subqueries (SUBQUERY_EXPR/EXISTS_EXPR/
        // ANY_EXPR/ALL_EXPR - a different scope), window functions, etc.
        // pass through unchanged.
        return node;
    }
}

/// Force every Column leaf reachable from `node` to be qualified with
/// `qualifier`, discarding whatever qualifier (if any) it already had.
/// Used to rewrite one side of a CONNECT BY condition once we already know
/// which row (child == source alias, parent == hierarchy CTE) it refers to.
[[nodiscard]] inline SQLNode* qualify_all_columns(libglot::Arena& arena, SQLNode* node,
                                                  std::string_view qualifier) {
    if (!node) {
        return nullptr;
    }
    switch (node->type) {
    case SQLNodeKind::COLUMN: {
        auto* col = static_cast<Column*>(node);
        return arena.create<Column>(qualifier, col->column);
    }
    case SQLNodeKind::BINARY_OP: {
        auto* op = static_cast<BinaryOp*>(node);
        return arena.create<BinaryOp>(op->op, qualify_all_columns(arena, op->left, qualifier),
                                      qualify_all_columns(arena, op->right, qualifier));
    }
    case SQLNodeKind::UNARY_OP: {
        auto* op = static_cast<UnaryOp*>(node);
        return arena.create<UnaryOp>(op->op, qualify_all_columns(arena, op->operand, qualifier));
    }
    case SQLNodeKind::FUNCTION_CALL: {
        auto* fc = static_cast<FunctionCall*>(node);
        std::vector<SQLNode*> args;
        args.reserve(fc->args.size());
        for (auto* a : fc->args) {
            args.push_back(qualify_all_columns(arena, a, qualifier));
        }
        return arena.create<FunctionCall>(fc->name, std::move(args), fc->distinct);
    }
    case SQLNodeKind::CAST_EXPR: {
        auto* c = static_cast<CastExpr*>(node);
        return arena.create<CastExpr>(qualify_all_columns(arena, c->expr, qualifier),
                                      c->target_type);
    }
    default:
        // Literals, parameters, and anything more exotic than a plain
        // comparison/function tree pass through unchanged rather than risk
        // an incorrect rewrite - CONNECT BY conditions are, in practice,
        // always a simple (optionally AND-ed) comparison chain.
        return node;
    }
}

/// Rewrite a CONNECT BY condition into the ON condition of the recursive
/// member's self-join: `PRIOR expr` becomes `expr` qualified by the
/// hierarchy CTE (the parent row); everything else is qualified by the
/// source table's alias (the child row).
[[nodiscard]] inline SQLNode* rewrite_connect_by_condition(libglot::Arena& arena, SQLNode* node,
                                                           std::string_view child_alias,
                                                           std::string_view hier_name) {
    if (!node) {
        return nullptr;
    }
    if (node->type == SQLNodeKind::UNARY_OP) {
        auto* op = static_cast<UnaryOp*>(node);
        if (op->op == TK::PRIOR) {
            return qualify_all_columns(arena, op->operand, hier_name);
        }
        return arena.create<UnaryOp>(
            op->op, rewrite_connect_by_condition(arena, op->operand, child_alias, hier_name));
    }
    if (node->type == SQLNodeKind::BINARY_OP) {
        auto* op = static_cast<BinaryOp*>(node);
        return arena.create<BinaryOp>(
            op->op, rewrite_connect_by_condition(arena, op->left, child_alias, hier_name),
            rewrite_connect_by_condition(arena, op->right, child_alias, hier_name));
    }
    // A plain column, literal, or function-call leaf on the non-PRIOR side
    // of a comparison - belongs to the child row.
    return qualify_all_columns(arena, node, child_alias);
}

} // namespace connect_by_lowering_detail

/// Lower START WITH / CONNECT BY into an equivalent WITH RECURSIVE query.
/// Returns a new SelectStmt allocated from `arena`; the input AST (`stmt`
/// and everything it points to) is never mutated. Throws std::logic_error
/// for forms with no clean lowering (NOCYCLE, ORDER SIBLINGS BY, and
/// JOIN/multi-table FROM clauses - see the file-level comment above).
[[nodiscard]] inline SelectStmt* lower_connect_by(libglot::Arena& arena, const SelectStmt* stmt) {
    namespace detail = connect_by_lowering_detail;
    using TK = libglot::sql::lex::TokenType;

    if (!stmt->connect_by) {
        throw std::logic_error("lower_connect_by requires a CONNECT BY clause");
    }
    if (stmt->connect_by->nocycle) {
        throw std::logic_error(
            "CONNECT BY NOCYCLE has no clean lowering to a recursive CTE: NOCYCLE needs an "
            "explicit key column to build a cycle-detection path, which cannot be derived "
            "from the CONNECT BY syntax alone");
    }
    if (stmt->order_siblings) {
        throw std::logic_error(
            "ORDER SIBLINGS BY has no clean lowering to a recursive CTE: sibling order "
            "depends on the traversal path, which a plain ORDER BY over the flattened "
            "hierarchy cannot express");
    }
    if (!stmt->from || stmt->from->type != SQLNodeKind::TABLE_REF) {
        throw std::logic_error(
            "CONNECT BY lowering only supports a single-table FROM clause: a JOIN or "
            "multi-table FROM has no clean lowering to a recursive CTE");
    }

    auto* table_ref = static_cast<TableRef*>(stmt->from);
    const std::string_view source_alias =
        table_ref->alias.empty() ? table_ref->table : table_ref->alias;

    // Pick a CTE name that doesn't collide with the source table, its
    // alias, or any CTE already defined on this statement.
    std::string_view hier_name = "hierarchy";
    auto collides = [&](std::string_view name) {
        if (detail::ci_equal(name, table_ref->table) || detail::ci_equal(name, source_alias)) {
            return true;
        }
        if (stmt->with) {
            for (const auto* cte : stmt->with->ctes) {
                if (detail::ci_equal(name, cte->name)) {
                    return true;
                }
            }
        }
        return false;
    };
    if (collides(hier_name)) {
        int suffix = 2;
        std::string composed;
        do {
            composed = "hierarchy_" + std::to_string(suffix++);
        } while (collides(composed));
        hier_name = arena.copy_source(composed);
    }

    const bool has_level = detail::select_references_level(stmt);
    constexpr std::string_view kLevelCol = "level";

    // ---- Anchor member: SELECT <alias>.*, 1 AS level FROM t alias WHERE s
    auto* anchor = arena.create<SelectStmt>();
    anchor->columns.push_back(arena.create<Star>(source_alias));
    if (has_level) {
        anchor->columns.push_back(arena.create<Alias>(arena.create<Literal>("1"), kLevelCol));
    }
    anchor->from = table_ref;
    anchor->where = stmt->start_with ? stmt->start_with->condition : nullptr;

    // ---- Recursive member: SELECT <alias>.*, hier.level + 1
    //                        FROM t alias JOIN hier ON c'
    auto* recursive = arena.create<SelectStmt>();
    recursive->columns.push_back(arena.create<Star>(source_alias));
    if (has_level) {
        auto* lvl_ref = arena.create<Column>(hier_name, kLevelCol);
        recursive->columns.push_back(
            arena.create<BinaryOp>(TK::PLUS, lvl_ref, arena.create<Literal>("1")));
    }
    SQLNode* join_condition = detail::rewrite_connect_by_condition(
        arena, stmt->connect_by->condition, source_alias, hier_name);
    auto* hier_ref = arena.create<TableRef>(hier_name);
    recursive->from =
        arena.create<JoinClause>(JoinType::INNER, table_ref, hier_ref, join_condition);

    // ---- WITH RECURSIVE hier AS (anchor UNION ALL recursive)
    auto* union_stmt = arena.create<UnionStmt>(anchor, recursive, /*is_all=*/true);
    auto* cte = arena.create<CTE>(hier_name, union_stmt);
    auto* with = arena.create<WithClause>();
    with->recursive = true;
    with->ctes.push_back(cte);

    // ---- Outer SELECT: cols' FROM hier [WHERE w]
    auto* outer = arena.create<SelectStmt>();
    outer->with = with;
    outer->columns.reserve(stmt->columns.size());
    for (auto* c : stmt->columns) {
        outer->columns.push_back(
            detail::requalify(arena, c, source_alias, hier_name, has_level, kLevelCol));
    }
    outer->from = arena.create<TableRef>(hier_name);
    outer->where = stmt->where; // Oracle applies WHERE after the hierarchy is built.

    // Everything else that can legally sit alongside CONNECT BY on a
    // SelectStmt is orthogonal to the lowering and carries over unchanged.
    outer->group_by = stmt->group_by;
    outer->having = stmt->having;
    outer->qualify = stmt->qualify;
    outer->order_by = stmt->order_by;
    outer->limit = stmt->limit;
    outer->offset = stmt->offset;
    outer->distinct = stmt->distinct;
    outer->distinct_on = stmt->distinct_on;
    outer->named_windows = stmt->named_windows;
    outer->limit_percent = stmt->limit_percent;
    outer->limit_with_ties = stmt->limit_with_ties;
    outer->for_update = stmt->for_update;
    outer->for_update_of = stmt->for_update_of;
    outer->for_update_wait = stmt->for_update_wait;
    outer->into_table = stmt->into_table;

    return outer;
}

} // namespace libglot::sql
