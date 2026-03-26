#pragma once

#include "ast_nodes.h"
#include "../../../../core/include/libglot/util/arena.h"
#include "../../../../libsqlglot/include/libsqlglot/tokenizer.h"
#include <string>
#include <string_view>

namespace libglot::sql {

using TK = libsqlglot::TokenType;

/// ============================================================================
/// Dialect-Specific SQL Transformations
/// ============================================================================
///
/// Transforms SQL AST from one dialect to another with dialect-specific
/// rewrites that preserve semantics but change syntax for compatibility.
///
/// Transformations:
/// - LIMIT/OFFSET → TOP/FETCH (SQL Server, T-SQL)
/// - ILIKE → LOWER() LIKE (MySQL, SQL Server)
/// - BOOLEAN → TINYINT (MySQL)
/// - STRING_AGG → GROUP_CONCAT (MySQL from PostgreSQL)
/// - ARRAY → JSON (MySQL from PostgreSQL)
/// - DATE_TRUNC → DATE_FORMAT (MySQL from PostgreSQL)
/// ============================================================================

class DialectTransformer {
public:
    explicit DialectTransformer(libglot::Arena& arena, SQLDialect target_dialect)
        : arena_(arena), target_dialect_(target_dialect) {}

    /// Transform AST for target dialect
    SQLNode* transform(SQLNode* node) {
        if (!node) return nullptr;

        switch (node->type) {
            case SQLNodeKind::SELECT_STMT:
                return transform_select(static_cast<SelectStmt*>(node));

            case SQLNodeKind::BINARY_OP:
                return transform_binary_op(static_cast<BinaryOp*>(node));

            case SQLNodeKind::FUNCTION_CALL:
                return transform_function_call(static_cast<FunctionCall*>(node));

            default:
                return node;
        }
    }

private:
    libglot::Arena& arena_;
    SQLDialect target_dialect_;

    // ========================================================================
    // SELECT Statement Transformations
    // ========================================================================

    SQLNode* transform_select(SelectStmt* stmt) {
        // Transform LIMIT/OFFSET to TOP for SQL Server
        if (target_dialect_ == SQLDialect::TSQL ||
            target_dialect_ == SQLDialect::SQLServer) {
            if (stmt->limit && !stmt->top) {
                // Move LIMIT to TOP
                stmt->top = stmt->limit;
                stmt->limit = nullptr;
            }
        }

        // Transform TOP to LIMIT for PostgreSQL/MySQL
        if ((target_dialect_ == SQLDialect::PostgreSQL ||
             target_dialect_ == SQLDialect::MySQL) && stmt->top) {
            stmt->limit = stmt->top;
            stmt->top = nullptr;
        }

        // Recursively transform columns
        for (size_t i = 0; i < stmt->columns.size(); i++) {
            stmt->columns[i] = transform(stmt->columns[i]);
        }

        // Transform WHERE clause
        if (stmt->where) {
            stmt->where = transform(stmt->where);
        }

        // Transform HAVING clause
        if (stmt->having) {
            stmt->having = transform(stmt->having);
        }

        return stmt;
    }

    // ========================================================================
    // Binary Operation Transformations
    // ========================================================================

    SQLNode* transform_binary_op(BinaryOp* op) {
        // Recursively transform operands
        op->left = transform(op->left);
        op->right = transform(op->right);

        // Transform ILIKE to LOWER() LIKE for MySQL/SQL Server
        if (op->op == TK::ILIKE) {
            if (target_dialect_ == SQLDialect::MySQL ||
                target_dialect_ == SQLDialect::TSQL ||
                target_dialect_ == SQLDialect::SQLServer) {
                // ILIKE → LOWER(left) LIKE LOWER(right)
                auto* lower_left = arena_.create<FunctionCall>(
                    "LOWER",
                    std::vector<SQLNode*>{op->left}
                );
                auto* lower_right = arena_.create<FunctionCall>(
                    "LOWER",
                    std::vector<SQLNode*>{op->right}
                );
                return arena_.create<BinaryOp>(TK::LIKE, lower_left, lower_right);
            }
        }

        return op;
    }

    // ========================================================================
    // Function Call Transformations
    // ========================================================================

    SQLNode* transform_function_call(FunctionCall* func) {
        // Recursively transform arguments
        for (size_t i = 0; i < func->args.size(); i++) {
            func->args[i] = transform(func->args[i]);
        }

        // STRING_AGG (PostgreSQL) → GROUP_CONCAT (MySQL)
        if (func->name == "STRING_AGG" && target_dialect_ == SQLDialect::MySQL) {
            func->name = "GROUP_CONCAT";
            // Reorder arguments if needed (PostgreSQL: STRING_AGG(expr, delimiter))
            // MySQL: GROUP_CONCAT(expr SEPARATOR delimiter) - handled in generator
            return func;
        }

        // GROUP_CONCAT (MySQL) → STRING_AGG (PostgreSQL)
        if (func->name == "GROUP_CONCAT" && target_dialect_ == SQLDialect::PostgreSQL) {
            func->name = "STRING_AGG";
            return func;
        }

        // DATE_TRUNC (PostgreSQL) → DATE_FORMAT (MySQL)
        if (func->name == "DATE_TRUNC" && target_dialect_ == SQLDialect::MySQL) {
            if (func->args.size() >= 2) {
                // Simplified transformation (real-world needs format mapping)
                func->name = "DATE_FORMAT";
                // Would need to map 'day', 'month', etc. to MySQL format strings
            }
            return func;
        }

        // CONCAT_WS (MySQL) → String concatenation (SQL Server)
        if (func->name == "CONCAT_WS" &&
            (target_dialect_ == SQLDialect::TSQL ||
             target_dialect_ == SQLDialect::SQLServer)) {
            if (func->args.size() >= 2) {
                // Extract separator (first arg)
                auto* separator = func->args[0];

                // Build concatenation with separator between each element
                SQLNode* result = func->args[1];
                for (size_t i = 2; i < func->args.size(); i++) {
                    // result + separator + args[i]
                    auto* with_sep = arena_.create<BinaryOp>(
                        TK::PLUS, result, separator
                    );
                    result = arena_.create<BinaryOp>(
                        TK::PLUS, with_sep, func->args[i]
                    );
                }
                return result;
            }
        }

        // IFNULL (MySQL) → COALESCE (PostgreSQL/SQL Server)
        if (func->name == "IFNULL" &&
            (target_dialect_ == SQLDialect::PostgreSQL ||
             target_dialect_ == SQLDialect::TSQL)) {
            func->name = "COALESCE";
            return func;
        }

        // NVL (Oracle) → COALESCE (PostgreSQL/MySQL)
        if (func->name == "NVL" &&
            (target_dialect_ == SQLDialect::PostgreSQL ||
             target_dialect_ == SQLDialect::MySQL)) {
            func->name = "COALESCE";
            return func;
        }

        // LEN (SQL Server) → LENGTH (PostgreSQL/MySQL)
        if (func->name == "LEN" &&
            (target_dialect_ == SQLDialect::PostgreSQL ||
             target_dialect_ == SQLDialect::MySQL)) {
            func->name = "LENGTH";
            return func;
        }

        // LENGTH (PostgreSQL/MySQL) → LEN (SQL Server)
        if (func->name == "LENGTH" &&
            (target_dialect_ == SQLDialect::TSQL ||
             target_dialect_ == SQLDialect::SQLServer)) {
            func->name = "LEN";
            return func;
        }

        // NOW() (MySQL) → CURRENT_TIMESTAMP (PostgreSQL/SQL Server)
        if (func->name == "NOW" &&
            (target_dialect_ == SQLDialect::PostgreSQL ||
             target_dialect_ == SQLDialect::TSQL)) {
            func->name = "CURRENT_TIMESTAMP";
            return func;
        }

        return func;
    }
};

} // namespace libglot::sql
