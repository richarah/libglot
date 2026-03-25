#pragma once

#include "../../../../core/include/libglot/ast/node.h"
#include "../../../../core/include/libglot/util/arena.h"
#include "../../../../libsqlglot/include/libsqlglot/tokens.h"  // For TokenType (Phase A shim)
#include "tokens.h"
#include <vector>
#include <string_view>
#include <utility>

namespace libglot::sql {

/// ============================================================================
/// SQL AST Node Types - Complete SQL92-SQL:2023 Coverage
/// ============================================================================
/// Comprehensive AST supporting:
/// - DML: SELECT, INSERT, UPDATE, DELETE, MERGE
/// - DDL: CREATE/DROP/ALTER TABLE, VIEW, INDEX, SCHEMA, DATABASE
/// - DQL: CTEs, window functions, set operations, subqueries
/// - Procedural: stored procedures, functions, triggers, cursors
/// - Advanced: PIVOT/UNPIVOT, TABLESAMPLE, QUALIFY, LATERAL joins
/// - Dialect-specific: BigQuery ML, T-SQL features, PL/SQL constructs
/// ============================================================================

/// ============================================================================
/// Node Kind Enumeration
/// ============================================================================

enum class SQLNodeKind : uint16_t {
    // ========================================================================
    // Literals & Basic Expressions
    // ========================================================================
    LITERAL,          // Literal value (number, string, NULL, TRUE, FALSE)
    COLUMN,           // Column reference (table.column)
    STAR,             // SELECT * or table.*
    PARAMETER,        // Placeholder (?, $1, :name, @name)

    // ========================================================================
    // Operators
    // ========================================================================
    BINARY_OP,        // Binary operator (=, <, +, AND, OR, etc.)
    UNARY_OP,         // Unary operator (NOT, IS NULL, etc.)

    // ========================================================================
    // Expressions
    // ========================================================================
    FUNCTION_CALL,    // Function call: func(args)
    CASE_EXPR,        // CASE WHEN ... THEN ... ELSE ... END
    CAST_EXPR,        // CAST(expr AS type)
    COALESCE_EXPR,    // COALESCE(expr1, expr2, ...)
    NULLIF_EXPR,      // NULLIF(expr1, expr2)
    BETWEEN_EXPR,     // expr BETWEEN low AND high
    IN_EXPR,          // expr IN (values)
    EXISTS_EXPR,      // EXISTS (subquery)
    ANY_EXPR,         // ANY(subquery)
    ALL_EXPR,         // ALL(subquery)
    SUBQUERY_EXPR,    // (SELECT ...)
    ARRAY_LITERAL,    // [1, 2, 3]
    ARRAY_INDEX,      // array[index]
    JSON_EXPR,        // JSON operations (->, ->>, #>, #>>)
    REGEX_MATCH,      // REGEXP, RLIKE, SIMILAR TO
    ALIAS,            // expr AS alias

    // ========================================================================
    // Window Functions
    // ========================================================================
    WINDOW_FUNCTION,  // Window function with OVER clause
    WINDOW_SPEC,      // OVER (PARTITION BY ... ORDER BY ... ROWS/RANGE ...)
    PARTITION_BY,     // PARTITION BY clause
    FRAME_CLAUSE,     // ROWS/RANGE frame specification

    // ========================================================================
    // Table References & Joins
    // ========================================================================
    TABLE_REF,        // Table reference (database.schema.table AS alias)
    JOIN_CLAUSE,      // JOIN operation
    LATERAL_JOIN,     // LATERAL subquery
    VALUES_CLAUSE,    // VALUES (row1), (row2), ...
    TABLESAMPLE,      // TABLESAMPLE (percent)

    // ========================================================================
    // SELECT Components
    // ========================================================================
    SELECT_STMT,      // SELECT statement
    CTE,              // Common Table Expression
    WITH_CLAUSE,      // WITH clause (contains CTEs)
    ORDER_BY_ITEM,    // ORDER BY item (expr ASC/DESC)
    LIMIT_CLAUSE,     // LIMIT/OFFSET clause
    QUALIFY_CLAUSE,   // QUALIFY (window function filter)

    // ========================================================================
    // Set Operations
    // ========================================================================
    UNION_STMT,       // UNION / UNION ALL
    INTERSECT_STMT,   // INTERSECT
    EXCEPT_STMT,      // EXCEPT / MINUS

    // ========================================================================
    // DML Statements
    // ========================================================================
    INSERT_STMT,      // INSERT INTO table VALUES / SELECT
    UPDATE_STMT,      // UPDATE table SET ... WHERE
    DELETE_STMT,      // DELETE FROM table WHERE
    MERGE_STMT,       // MERGE (UPSERT)
    TRUNCATE_STMT,    // TRUNCATE TABLE

    // ========================================================================
    // DDL Statements - Tables & Indexes
    // ========================================================================
    CREATE_TABLE_STMT,    // CREATE TABLE
    DROP_TABLE_STMT,      // DROP TABLE
    ALTER_TABLE_STMT,     // ALTER TABLE
    COLUMN_DEF,           // Column definition (for CREATE TABLE)
    TABLE_CONSTRAINT,     // Table constraint (PRIMARY KEY, FOREIGN KEY, etc.)
    CREATE_INDEX_STMT,    // CREATE INDEX
    DROP_INDEX_STMT,      // DROP INDEX

    // ========================================================================
    // DDL Statements - Views & Schemas
    // ========================================================================
    CREATE_VIEW_STMT,     // CREATE VIEW
    DROP_VIEW_STMT,       // DROP VIEW
    CREATE_SCHEMA_STMT,   // CREATE SCHEMA / DATABASE
    DROP_SCHEMA_STMT,     // DROP SCHEMA / DATABASE
    CREATE_DATABASE_STMT, // CREATE DATABASE (alias for schema)
    DROP_DATABASE_STMT,   // DROP DATABASE (alias for schema)

    // ========================================================================
    // DDL Statements - Advanced
    // ========================================================================
    CREATE_TABLESPACE_STMT,  // CREATE TABLESPACE
    PARTITION_SPEC,          // Table partitioning specification
    CREATE_INDEX_ADV,        // Advanced CREATE INDEX (partial, concurrent)

    // ========================================================================
    // Transaction Statements
    // ========================================================================
    BEGIN_STMT,           // BEGIN / START TRANSACTION
    COMMIT_STMT,          // COMMIT
    ROLLBACK_STMT,        // ROLLBACK
    SAVEPOINT_STMT,       // SAVEPOINT

    // ========================================================================
    // Utility Statements
    // ========================================================================
    SET_STMT,             // SET variable = value
    SHOW_STMT,            // SHOW TABLES / DATABASES / etc.
    DESCRIBE_STMT,        // DESCRIBE table
    EXPLAIN_STMT,         // EXPLAIN query
    ANALYZE_STMT,         // ANALYZE table
    VACUUM_STMT,          // VACUUM table
    GRANT_STMT,           // GRANT privileges
    REVOKE_STMT,          // REVOKE privileges

    // ========================================================================
    // Stored Procedures & Functions
    // ========================================================================
    CREATE_PROCEDURE_STMT,   // CREATE PROCEDURE / FUNCTION
    DROP_PROCEDURE_STMT,     // DROP PROCEDURE / FUNCTION
    CALL_PROCEDURE_STMT,     // CALL procedure_name
    DECLARE_VAR_STMT,        // DECLARE variable
    DECLARE_CURSOR_STMT,     // DECLARE CURSOR
    ASSIGNMENT_STMT,         // Variable assignment (SET var = val, var := val)
    RETURN_STMT,             // RETURN expression
    IF_STMT,                 // IF condition THEN ... END IF
    WHILE_LOOP,              // WHILE condition DO ... END WHILE
    FOR_LOOP,                // FOR var IN ... LOOP ... END LOOP
    LOOP_STMT,               // LOOP ... END LOOP (infinite loop)
    BREAK_STMT,              // BREAK / EXIT
    CONTINUE_STMT,           // CONTINUE
    BEGIN_END_BLOCK,         // BEGIN ... END block
    DO_BLOCK,                // DO $$ ... $$ (PostgreSQL anonymous block)
    EXCEPTION_BLOCK,         // EXCEPTION handler block
    RAISE_STMT,              // RAISE / SIGNAL error
    OPEN_CURSOR_STMT,        // OPEN cursor
    FETCH_CURSOR_STMT,       // FETCH cursor
    CLOSE_CURSOR_STMT,       // CLOSE cursor
    DELIMITER_STMT,          // DELIMITER command (MySQL)

    // ========================================================================
    // Triggers
    // ========================================================================
    CREATE_TRIGGER_STMT,     // CREATE TRIGGER
    DROP_TRIGGER_STMT,       // DROP TRIGGER

    // ========================================================================
    // Advanced Features
    // ========================================================================
    PIVOT_CLAUSE,            // PIVOT
    UNPIVOT_CLAUSE,          // UNPIVOT

    // ========================================================================
    // BigQuery ML
    // ========================================================================
    CREATE_MODEL_STMT,       // CREATE MODEL (BigQuery ML)
    DROP_MODEL_STMT,         // DROP MODEL
    ML_PREDICT_EXPR,         // ML.PREDICT()
    ML_EVALUATE_EXPR,        // ML.EVALUATE()
    ML_TRAINING_INFO_EXPR,   // ML.TRAINING_INFO()

    // ========================================================================
    // Sentinel
    // ========================================================================
    NODE_KIND_COUNT
};

/// ============================================================================
/// Forward Declarations
/// ============================================================================

struct SQLNode;

// Literals & Basic Expressions
struct Literal;
struct Column;
struct Star;
struct Parameter;

// Operators
struct BinaryOp;
struct UnaryOp;

// Expressions
struct FunctionCall;
struct CaseExpr;
struct CastExpr;
struct CoalesceExpr;
struct NullifExpr;
struct BetweenExpr;
struct InExpr;
struct ExistsExpr;
struct AnyExpr;
struct AllExpr;
struct SubqueryExpr;
struct ArrayLiteral;
struct ArrayIndex;
struct JsonExpr;
struct RegexMatch;
struct Alias;

// Window Functions
struct WindowFunction;
struct WindowSpec;
struct PartitionBy;
struct FrameClause;

// Table References & Joins
struct TableRef;
struct JoinClause;
struct LateralJoin;
struct ValuesClause;
struct Tablesample;

// SELECT Components
struct SelectStmt;
struct CTE;
struct WithClause;
struct OrderByItem;
struct LimitClause;
struct QualifyClause;

// Set Operations
struct UnionStmt;
struct IntersectStmt;
struct ExceptStmt;

// DML Statements
struct InsertStmt;
struct UpdateStmt;
struct DeleteStmt;
struct MergeStmt;
struct TruncateStmt;

// DDL Statements
struct CreateTableStmt;
struct DropTableStmt;
struct AlterTableStmt;
struct ColumnDef;
struct TableConstraint;
struct CreateIndexStmt;
struct DropIndexStmt;
struct CreateViewStmt;
struct DropViewStmt;
struct CreateSchemaStmt;
struct DropSchemaStmt;
struct CreateTablespaceStmt;
struct PartitionSpec;
struct CreateIndexAdv;

// Transaction Statements
struct BeginStmt;
struct CommitStmt;
struct RollbackStmt;
struct SavepointStmt;

// Utility Statements
struct SetStmt;
struct ShowStmt;
struct DescribeStmt;
struct ExplainStmt;
struct AnalyzeStmt;
struct VacuumStmt;
struct GrantStmt;
struct RevokeStmt;

// Stored Procedures & Functions
struct CreateProcedureStmt;
struct DropProcedureStmt;
struct CallProcedureStmt;
struct DeclareVarStmt;
struct DeclareCursorStmt;
struct AssignmentStmt;
struct ReturnStmt;
struct IfStmt;
struct WhileLoop;
struct ForLoop;
struct LoopStmt;
struct BreakStmt;
struct ContinueStmt;
struct BeginEndBlock;
struct DoBlock;
struct ExceptionBlock;
struct RaiseStmt;
struct OpenCursorStmt;
struct FetchCursorStmt;
struct CloseCursorStmt;
struct DelimiterStmt;

// Triggers
struct CreateTriggerStmt;
struct DropTriggerStmt;

// Advanced Features
struct PivotClause;
struct UnpivotClause;

// BigQuery ML
struct CreateModelStmt;
struct DropModelStmt;
struct MLPredictExpr;
struct MLEvaluateExpr;
struct MLTrainingInfoExpr;

/// ============================================================================
/// Base SQL Node (CRTP)
/// ============================================================================

struct SQLNode : libglot::AstNodeBase<SQLNode, SQLNodeKind> {
    using Base = libglot::AstNodeBase<SQLNode, SQLNodeKind>;
    using Base::Base;

    /// Source location (optional, for error reporting)
    libglot::SourceLocation loc{};
};

// Verify concept satisfaction
static_assert(libglot::AstNode<SQLNode>, "SQLNode must satisfy AstNode concept");

/// ============================================================================
/// Literals & Basic Expressions
/// ============================================================================

struct Literal : SQLNode {
    std::string_view value;

    explicit Literal(std::string_view val)
        : SQLNode(SQLNodeKind::LITERAL), value(val) {}
};

struct Column : SQLNode {
    std::string_view table;   // Optional table qualifier
    std::string_view column;

    explicit Column(std::string_view col)
        : SQLNode(SQLNodeKind::COLUMN), column(col) {}

    Column(std::string_view tbl, std::string_view col)
        : SQLNode(SQLNodeKind::COLUMN), table(tbl), column(col) {}
};

struct Star : SQLNode {
    std::string_view table;  // Optional table qualifier (for table.*)

    Star() : SQLNode(SQLNodeKind::STAR) {}
    explicit Star(std::string_view tbl)
        : SQLNode(SQLNodeKind::STAR), table(tbl) {}
};

struct Parameter : SQLNode {
    std::string_view name;  // ?, $1, :name, @name

    explicit Parameter(std::string_view n)
        : SQLNode(SQLNodeKind::PARAMETER), name(n) {}
};

/// ============================================================================
/// Operators
/// ============================================================================

struct BinaryOp : SQLNode {
    libsqlglot::TokenType op;  // Using libsqlglot for Phase A (shim)
    SQLNode* left;
    SQLNode* right;

    BinaryOp(libsqlglot::TokenType operation, SQLNode* l, SQLNode* r)
        : SQLNode(SQLNodeKind::BINARY_OP), op(operation), left(l), right(r) {}
};

struct UnaryOp : SQLNode {
    libsqlglot::TokenType op;  // Using libsqlglot for Phase A (shim)
    SQLNode* operand;

    UnaryOp(libsqlglot::TokenType operation, SQLNode* expr)
        : SQLNode(SQLNodeKind::UNARY_OP), op(operation), operand(expr) {}
};

/// ============================================================================
/// Expressions
/// ============================================================================

struct FunctionCall : SQLNode {
    std::string_view name;
    std::vector<SQLNode*> args;
    bool distinct;

    FunctionCall(std::string_view n, std::vector<SQLNode*> a, bool d = false)
        : SQLNode(SQLNodeKind::FUNCTION_CALL), name(n), args(std::move(a)), distinct(d) {}
};

struct CaseExpr : SQLNode {
    SQLNode* case_value;  // Optional (for simple CASE expr WHEN ...)
    std::vector<std::pair<SQLNode*, SQLNode*>> when_clauses;  // (condition, result)
    SQLNode* else_expr;

    CaseExpr()
        : SQLNode(SQLNodeKind::CASE_EXPR), case_value(nullptr), else_expr(nullptr) {}
};

struct CastExpr : SQLNode {
    SQLNode* expr;
    std::string_view target_type;

    CastExpr(SQLNode* e, std::string_view type)
        : SQLNode(SQLNodeKind::CAST_EXPR), expr(e), target_type(type) {}
};

struct CoalesceExpr : SQLNode {
    std::vector<SQLNode*> args;

    explicit CoalesceExpr(std::vector<SQLNode*> a)
        : SQLNode(SQLNodeKind::COALESCE_EXPR), args(std::move(a)) {}
};

struct NullifExpr : SQLNode {
    SQLNode* expr1;
    SQLNode* expr2;

    NullifExpr(SQLNode* e1, SQLNode* e2)
        : SQLNode(SQLNodeKind::NULLIF_EXPR), expr1(e1), expr2(e2) {}
};

struct BetweenExpr : SQLNode {
    SQLNode* expr;
    SQLNode* lower;
    SQLNode* upper;
    bool not_between;

    BetweenExpr(SQLNode* e, SQLNode* l, SQLNode* u, bool neg = false)
        : SQLNode(SQLNodeKind::BETWEEN_EXPR), expr(e), lower(l), upper(u), not_between(neg) {}
};

struct InExpr : SQLNode {
    SQLNode* expr;
    std::vector<SQLNode*> values;  // Or subquery
    bool not_in;

    InExpr(SQLNode* e, std::vector<SQLNode*> vals, bool neg = false)
        : SQLNode(SQLNodeKind::IN_EXPR), expr(e), values(std::move(vals)), not_in(neg) {}
};

struct ExistsExpr : SQLNode {
    SQLNode* subquery;  // SelectStmt
    bool not_exists;

    ExistsExpr(SQLNode* sq, bool neg = false)
        : SQLNode(SQLNodeKind::EXISTS_EXPR), subquery(sq), not_exists(neg) {}
};

struct AnyExpr : SQLNode {
    SQLNode* left;
    libsqlglot::TokenType comparison_op;  // Using libsqlglot for Phase A (shim)
    SQLNode* subquery;

    AnyExpr(SQLNode* l, libsqlglot::TokenType op, SQLNode* sq)
        : SQLNode(SQLNodeKind::ANY_EXPR), left(l), comparison_op(op), subquery(sq) {}
};

struct AllExpr : SQLNode {
    SQLNode* left;
    libsqlglot::TokenType comparison_op;  // Using libsqlglot for Phase A (shim)
    SQLNode* subquery;

    AllExpr(SQLNode* l, libsqlglot::TokenType op, SQLNode* sq)
        : SQLNode(SQLNodeKind::ALL_EXPR), left(l), comparison_op(op), subquery(sq) {}
};

struct SubqueryExpr : SQLNode {
    SQLNode* query;  // SelectStmt
    std::string_view alias;  // Optional alias (for subqueries in FROM clause)

    explicit SubqueryExpr(SQLNode* q, std::string_view a = "")
        : SQLNode(SQLNodeKind::SUBQUERY_EXPR), query(q), alias(a) {}
};

struct ArrayLiteral : SQLNode {
    std::vector<SQLNode*> elements;

    explicit ArrayLiteral(std::vector<SQLNode*> elems = {})
        : SQLNode(SQLNodeKind::ARRAY_LITERAL), elements(std::move(elems)) {}
};

struct ArrayIndex : SQLNode {
    SQLNode* array;
    SQLNode* index;

    ArrayIndex(SQLNode* arr, SQLNode* idx)
        : SQLNode(SQLNodeKind::ARRAY_INDEX), array(arr), index(idx) {}
};

struct JsonExpr : SQLNode {
    enum class OpType { ARROW, LONG_ARROW, HASH_ARROW, HASH_LONG_ARROW };

    SQLNode* json_expr;
    SQLNode* key;
    OpType op_type;

    JsonExpr(SQLNode* expr, SQLNode* k, OpType op)
        : SQLNode(SQLNodeKind::JSON_EXPR), json_expr(expr), key(k), op_type(op) {}
};

struct RegexMatch : SQLNode {
    SQLNode* expr;
    SQLNode* pattern;
    bool similar_to;  // SIMILAR TO vs REGEXP/RLIKE

    RegexMatch(SQLNode* e, SQLNode* pat, bool sim = false)
        : SQLNode(SQLNodeKind::REGEX_MATCH), expr(e), pattern(pat), similar_to(sim) {}
};

struct Alias : SQLNode {
    SQLNode* expr;
    std::string_view alias;

    Alias(SQLNode* e, std::string_view a)
        : SQLNode(SQLNodeKind::ALIAS), expr(e), alias(a) {}
};

/// ============================================================================
/// Window Functions
/// ============================================================================

enum class FrameType { ROWS, RANGE, GROUPS };
enum class FrameBound { UNBOUNDED_PRECEDING, UNBOUNDED_FOLLOWING, CURRENT_ROW, PRECEDING, FOLLOWING };

struct FrameClause : SQLNode {
    FrameType frame_type;
    FrameBound start_bound;
    SQLNode* start_offset;  // nullptr for UNBOUNDED/CURRENT
    FrameBound end_bound;
    SQLNode* end_offset;

    FrameClause(FrameType ft, FrameBound sb)
        : SQLNode(SQLNodeKind::FRAME_CLAUSE), frame_type(ft), start_bound(sb),
          start_offset(nullptr), end_bound(FrameBound::CURRENT_ROW), end_offset(nullptr) {}
};

struct WindowSpec : SQLNode {
    std::vector<SQLNode*> partition_by;
    std::vector<SQLNode*> order_by;
    FrameClause* frame;

    WindowSpec()
        : SQLNode(SQLNodeKind::WINDOW_SPEC), frame(nullptr) {}
};

struct WindowFunction : SQLNode {
    std::string_view function_name;  // ROW_NUMBER, RANK, LEAD, LAG, etc.
    std::vector<SQLNode*> args;
    WindowSpec* over;

    WindowFunction(std::string_view fn, WindowSpec* w)
        : SQLNode(SQLNodeKind::WINDOW_FUNCTION), function_name(fn), over(w) {}
};

/// ============================================================================
/// Table References & Joins
/// ============================================================================

struct TableRef : SQLNode {
    std::string_view database;  // Optional
    std::string_view schema;    // Optional
    std::string_view table;
    std::string_view alias;     // Optional

    explicit TableRef(std::string_view tbl)
        : SQLNode(SQLNodeKind::TABLE_REF), table(tbl) {}

    // Two-argument constructor: database.table (for parse_table_ref)
    TableRef(std::string_view db, std::string_view tbl)
        : SQLNode(SQLNodeKind::TABLE_REF), database(db), table(tbl) {}

    // Three-argument constructor: database.schema.table
    TableRef(std::string_view db, std::string_view sch, std::string_view tbl)
        : SQLNode(SQLNodeKind::TABLE_REF), database(db), schema(sch), table(tbl) {}
};

enum class JoinType { INNER, LEFT, RIGHT, FULL, CROSS, NATURAL };

struct JoinClause : SQLNode {
    JoinType join_type;
    SQLNode* left_table;
    SQLNode* right_table;
    SQLNode* condition;  // ON condition or USING columns

    JoinClause(JoinType jt, SQLNode* l, SQLNode* r, SQLNode* cond = nullptr)
        : SQLNode(SQLNodeKind::JOIN_CLAUSE), join_type(jt),
          left_table(l), right_table(r), condition(cond) {}
};

struct LateralJoin : SQLNode {
    SQLNode* table_expr;  // Subquery or table function

    explicit LateralJoin(SQLNode* expr)
        : SQLNode(SQLNodeKind::LATERAL_JOIN), table_expr(expr) {}
};

struct ValuesClause : SQLNode {
    std::vector<std::vector<SQLNode*>> rows;

    ValuesClause()
        : SQLNode(SQLNodeKind::VALUES_CLAUSE) {}
};

enum class SampleMethod { BERNOULLI, SYSTEM };

struct Tablesample : SQLNode {
    SampleMethod method;
    SQLNode* percent;
    SQLNode* seed;  // Optional

    Tablesample(SampleMethod m, SQLNode* p)
        : SQLNode(SQLNodeKind::TABLESAMPLE), method(m), percent(p), seed(nullptr) {}
};

/// ============================================================================
/// SELECT Components
/// ============================================================================

struct SelectStmt : SQLNode {
    WithClause* with;                     // WITH clause (CTEs)
    std::vector<SQLNode*> columns;        // SELECT columns
    SQLNode* from;                        // FROM clause
    SQLNode* where;                       // WHERE condition
    std::vector<SQLNode*> group_by;       // GROUP BY
    SQLNode* having;                      // HAVING
    QualifyClause* qualify;               // QUALIFY
    std::vector<OrderByItem*> order_by;   // ORDER BY
    SQLNode* limit;                       // LIMIT
    SQLNode* offset;                      // OFFSET
    bool distinct;

    SelectStmt()
        : SQLNode(SQLNodeKind::SELECT_STMT), with(nullptr), from(nullptr), where(nullptr),
          having(nullptr), qualify(nullptr), limit(nullptr), offset(nullptr), distinct(false) {}
};

struct CTE : SQLNode {
    std::string_view name;
    std::vector<std::string_view> columns;  // Optional column list
    SelectStmt* query;

    CTE(std::string_view n, SelectStmt* q)
        : SQLNode(SQLNodeKind::CTE), name(n), query(q) {}
};

struct WithClause : SQLNode {
    std::vector<CTE*> ctes;
    bool recursive;

    WithClause()
        : SQLNode(SQLNodeKind::WITH_CLAUSE), recursive(false) {}
};

struct OrderByItem : SQLNode {
    SQLNode* expr;
    bool ascending;
    bool nulls_first;  // NULLS FIRST / NULLS LAST

    OrderByItem(SQLNode* e, bool asc = true, bool nf = false)
        : SQLNode(SQLNodeKind::ORDER_BY_ITEM), expr(e), ascending(asc), nulls_first(nf) {}
};

struct LimitClause : SQLNode {
    SQLNode* limit;
    SQLNode* offset;

    LimitClause(SQLNode* l, SQLNode* o = nullptr)
        : SQLNode(SQLNodeKind::LIMIT_CLAUSE), limit(l), offset(o) {}
};

struct QualifyClause : SQLNode {
    SQLNode* condition;

    explicit QualifyClause(SQLNode* cond)
        : SQLNode(SQLNodeKind::QUALIFY_CLAUSE), condition(cond) {}
};

/// ============================================================================
/// Set Operations
/// ============================================================================

struct UnionStmt : SQLNode {
    SelectStmt* left;
    SelectStmt* right;
    bool all;

    UnionStmt(SelectStmt* l, SelectStmt* r, bool is_all = false)
        : SQLNode(SQLNodeKind::UNION_STMT), left(l), right(r), all(is_all) {}
};

struct IntersectStmt : SQLNode {
    SelectStmt* left;
    SelectStmt* right;
    bool all;

    IntersectStmt(SelectStmt* l, SelectStmt* r, bool is_all = false)
        : SQLNode(SQLNodeKind::INTERSECT_STMT), left(l), right(r), all(is_all) {}
};

struct ExceptStmt : SQLNode {
    SelectStmt* left;
    SelectStmt* right;
    bool all;

    ExceptStmt(SelectStmt* l, SelectStmt* r, bool is_all = false)
        : SQLNode(SQLNodeKind::EXCEPT_STMT), left(l), right(r), all(is_all) {}
};

/// ============================================================================
/// DML Statements
/// ============================================================================

struct InsertStmt : SQLNode {
    TableRef* table;
    std::vector<std::string_view> columns;          // Optional column list
    std::vector<std::vector<SQLNode*>> values;      // VALUES rows
    SelectStmt* select_query;                       // INSERT ... SELECT

    InsertStmt()
        : SQLNode(SQLNodeKind::INSERT_STMT), table(nullptr), select_query(nullptr) {}
};

struct UpdateStmt : SQLNode {
    TableRef* table;
    std::vector<std::pair<std::string_view, SQLNode*>> assignments;  // SET column = value
    SQLNode* where;
    SQLNode* from;  // FROM clause (for joins)

    UpdateStmt()
        : SQLNode(SQLNodeKind::UPDATE_STMT), table(nullptr), where(nullptr), from(nullptr) {}
};

struct DeleteStmt : SQLNode {
    TableRef* table;
    SQLNode* where;
    SQLNode* using_clause;  // USING clause (for joins)

    DeleteStmt()
        : SQLNode(SQLNodeKind::DELETE_STMT), table(nullptr), where(nullptr), using_clause(nullptr) {}
};

struct MergeStmt : SQLNode {
    TableRef* target;
    SQLNode* source;
    SQLNode* on_condition;
    std::vector<std::pair<std::string_view, SQLNode*>> update_assignments;  // WHEN MATCHED UPDATE
    std::vector<std::string_view> insert_columns;
    std::vector<SQLNode*> insert_values;

    MergeStmt()
        : SQLNode(SQLNodeKind::MERGE_STMT), target(nullptr), source(nullptr), on_condition(nullptr) {}
};

struct TruncateStmt : SQLNode {
    TableRef* table;
    bool cascade;

    TruncateStmt()
        : SQLNode(SQLNodeKind::TRUNCATE_STMT), table(nullptr), cascade(false) {}
};

/// ============================================================================
/// DDL Statements - Tables & Indexes
/// ============================================================================

struct ColumnDef : SQLNode {
    std::string_view name;
    std::string_view type;          // Data type
    bool not_null;
    bool primary_key;
    bool unique;
    bool auto_increment;
    SQLNode* default_value;
    std::string_view check_constraint;

    ColumnDef()
        : SQLNode(SQLNodeKind::COLUMN_DEF), not_null(false), primary_key(false),
          unique(false), auto_increment(false), default_value(nullptr) {}
};

struct TableConstraint : SQLNode {
    enum class Type { PRIMARY_KEY, FOREIGN_KEY, UNIQUE, CHECK };

    Type constraint_type;
    std::vector<std::string_view> columns;
    TableRef* ref_table;  // For FOREIGN KEY
    std::vector<std::string_view> ref_columns;
    std::string_view on_delete_action;
    std::string_view on_update_action;
    SQLNode* check_expr;  // For CHECK

    TableConstraint()
        : SQLNode(SQLNodeKind::TABLE_CONSTRAINT), ref_table(nullptr), check_expr(nullptr) {}
};

struct CreateTableStmt : SQLNode {
    TableRef* table;
    std::vector<ColumnDef*> columns;
    std::vector<TableConstraint*> constraints;
    bool if_not_exists;
    bool temporary;
    SelectStmt* as_select;  // CREATE TABLE AS SELECT

    CreateTableStmt()
        : SQLNode(SQLNodeKind::CREATE_TABLE_STMT), table(nullptr),
          if_not_exists(false), temporary(false), as_select(nullptr) {}
};

struct DropTableStmt : SQLNode {
    TableRef* table;
    bool if_exists;
    bool cascade;

    DropTableStmt()
        : SQLNode(SQLNodeKind::DROP_TABLE_STMT), table(nullptr),
          if_exists(false), cascade(false) {}
};

enum class AlterOperation { ADD_COLUMN, DROP_COLUMN, MODIFY_COLUMN, RENAME_COLUMN, RENAME_TABLE };

struct AlterTableStmt : SQLNode {
    TableRef* table;
    AlterOperation operation;
    ColumnDef* column_def;        // For ADD/MODIFY
    std::string_view old_name;
    std::string_view new_name;

    AlterTableStmt()
        : SQLNode(SQLNodeKind::ALTER_TABLE_STMT), table(nullptr), column_def(nullptr) {}
};

struct CreateIndexStmt : SQLNode {
    std::string_view index_name;
    TableRef* table;
    std::vector<std::string_view> columns;
    bool unique;
    bool if_not_exists;

    CreateIndexStmt()
        : SQLNode(SQLNodeKind::CREATE_INDEX_STMT), table(nullptr),
          unique(false), if_not_exists(false) {}
};

struct DropIndexStmt : SQLNode {
    std::string_view index_name;
    TableRef* table;  // Optional (dialect-specific)
    bool if_exists;

    DropIndexStmt()
        : SQLNode(SQLNodeKind::DROP_INDEX_STMT), table(nullptr), if_exists(false) {}
};

/// ============================================================================
/// DDL Statements - Views & Schemas
/// ============================================================================

struct CreateViewStmt : SQLNode {
    std::string_view name;
    std::vector<std::string_view> columns;  // Optional
    SelectStmt* query;
    bool or_replace;
    bool if_not_exists;

    CreateViewStmt()
        : SQLNode(SQLNodeKind::CREATE_VIEW_STMT), query(nullptr),
          or_replace(false), if_not_exists(false) {}
};

struct DropViewStmt : SQLNode {
    std::string_view name;
    bool if_exists;
    bool cascade;

    DropViewStmt()
        : SQLNode(SQLNodeKind::DROP_VIEW_STMT), if_exists(false), cascade(false) {}
};

struct CreateSchemaStmt : SQLNode {
    std::string_view name;
    bool if_not_exists;

    CreateSchemaStmt()
        : SQLNode(SQLNodeKind::CREATE_SCHEMA_STMT), if_not_exists(false) {}
};

struct DropSchemaStmt : SQLNode {
    std::string_view name;
    bool if_exists;
    bool cascade;

    DropSchemaStmt()
        : SQLNode(SQLNodeKind::DROP_SCHEMA_STMT), if_exists(false), cascade(false) {}
};

// Note: CREATE DATABASE / DROP DATABASE use the same structs as CREATE/DROP SCHEMA
// No separate types needed - just use CreateSchemaStmt / DropSchemaStmt

/// ============================================================================
/// Transaction Statements
/// ============================================================================

struct BeginStmt : SQLNode {
    std::string_view transaction_type;  // "WORK", "TRANSACTION", or empty

    BeginStmt() : SQLNode(SQLNodeKind::BEGIN_STMT) {}
    explicit BeginStmt(std::string_view type) : SQLNode(SQLNodeKind::BEGIN_STMT), transaction_type(type) {}
};

struct CommitStmt : SQLNode {
    CommitStmt() : SQLNode(SQLNodeKind::COMMIT_STMT) {}
};

struct RollbackStmt : SQLNode {
    std::string_view savepoint_name;  // Optional

    RollbackStmt() : SQLNode(SQLNodeKind::ROLLBACK_STMT) {}
};

struct SavepointStmt : SQLNode {
    std::string_view name;

    SavepointStmt() : SQLNode(SQLNodeKind::SAVEPOINT_STMT) {}
};

/// ============================================================================
/// Utility Statements
/// ============================================================================

struct SetStmt : SQLNode {
    std::vector<std::pair<std::string_view, SQLNode*>> assignments;

    SetStmt() : SQLNode(SQLNodeKind::SET_STMT) {}
};

struct ShowStmt : SQLNode {
    std::string_view what;    // TABLES, DATABASES, etc.
    std::string_view target;  // Optional

    ShowStmt() : SQLNode(SQLNodeKind::SHOW_STMT) {}
};

struct DescribeStmt : SQLNode {
    std::string_view target;

    DescribeStmt() : SQLNode(SQLNodeKind::DESCRIBE_STMT) {}
};

struct ExplainStmt : SQLNode {
    bool analyze;
    SQLNode* statement;

    ExplainStmt()
        : SQLNode(SQLNodeKind::EXPLAIN_STMT), analyze(false), statement(nullptr) {}
};

struct AnalyzeStmt : SQLNode {
    std::vector<TableRef*> tables;             // Tables to analyze (can be multiple)
    std::vector<std::string_view> columns;     // Column specifications for single table
    bool verbose;
    bool local;                                 // MySQL: LOCAL
    bool no_write_to_binlog;                   // MySQL: NO_WRITE_TO_BINLOG
    bool use_table_keyword;                     // MySQL: ANALYZE TABLE vs PostgreSQL: ANALYZE

    AnalyzeStmt() : SQLNode(SQLNodeKind::ANALYZE_STMT), verbose(false), local(false),
                    no_write_to_binlog(false), use_table_keyword(false) {}
};

struct VacuumStmt : SQLNode {
    std::vector<TableRef*> tables;             // Tables to vacuum (can be multiple)
    std::vector<std::string_view> columns;     // Column specifications for single table
    bool full;
    bool freeze;
    bool verbose;
    bool analyze;
    std::vector<std::pair<std::string_view, std::string_view>> paren_options;  // Parenthesized options like (PARALLEL 4)

    VacuumStmt() : SQLNode(SQLNodeKind::VACUUM_STMT), full(false), freeze(false),
                   verbose(false), analyze(false) {}
};

struct GrantStmt : SQLNode {
    std::vector<std::string_view> privileges;
    std::string_view object_type;
    std::string_view object_name;
    std::vector<std::string_view> grantees;
    bool with_grant_option;
    bool with_admin_option;
    bool with_hierarchy_option;

    GrantStmt()
        : SQLNode(SQLNodeKind::GRANT_STMT), with_grant_option(false),
          with_admin_option(false), with_hierarchy_option(false) {}
};

struct RevokeStmt : SQLNode {
    std::vector<std::string_view> privileges;
    std::string_view object_type;
    std::string_view object_name;
    std::vector<std::string_view> grantees;
    bool cascade;
    bool restrict;
    bool grant_option_for;
    bool admin_option_for;

    RevokeStmt()
        : SQLNode(SQLNodeKind::REVOKE_STMT), cascade(false), restrict(false),
          grant_option_for(false), admin_option_for(false) {}
};

/// ============================================================================
/// Stored Procedures & Functions (Simplified for Phase A)
/// ============================================================================

// Procedure/Function parameter (not an AST node, just a data struct)
struct ProcedureParameter {
    std::string_view mode;       // IN, OUT, INOUT (empty = IN)
    std::string_view name;
    std::string_view type;
};

struct CreateProcedureStmt : SQLNode {
    bool is_function;
    std::string_view name;
    std::vector<ProcedureParameter> parameters;
    std::string_view return_type;  // For functions
    std::string_view language;
    std::vector<SQLNode*> body;
    bool or_replace;

    CreateProcedureStmt()
        : SQLNode(SQLNodeKind::CREATE_PROCEDURE_STMT), is_function(false), or_replace(false) {}
};

struct DropProcedureStmt : SQLNode {
    bool is_function;
    std::string_view name;
    bool if_exists;

    DropProcedureStmt()
        : SQLNode(SQLNodeKind::DROP_PROCEDURE_STMT), is_function(false), if_exists(false) {}
};

struct CallProcedureStmt : SQLNode {
    std::string_view name;
    std::vector<SQLNode*> arguments;

    CallProcedureStmt() : SQLNode(SQLNodeKind::CALL_PROCEDURE_STMT) {}
};

struct DeclareVarStmt : SQLNode {
    std::string_view variable_name;
    std::string_view type;
    SQLNode* default_value;

    DeclareVarStmt()
        : SQLNode(SQLNodeKind::DECLARE_VAR_STMT), default_value(nullptr) {}
};

struct DeclareCursorStmt : SQLNode {
    std::string_view cursor_name;
    bool scroll;              // SCROLL cursor (allows backward fetch)
    SelectStmt* query;

    DeclareCursorStmt()
        : SQLNode(SQLNodeKind::DECLARE_CURSOR_STMT), scroll(false), query(nullptr) {}
};

struct AssignmentStmt : SQLNode {
    std::string_view variable_name;
    SQLNode* value;

    AssignmentStmt()
        : SQLNode(SQLNodeKind::ASSIGNMENT_STMT), value(nullptr) {}
};

struct ReturnStmt : SQLNode {
    SQLNode* return_value;

    explicit ReturnStmt(SQLNode* val = nullptr)
        : SQLNode(SQLNodeKind::RETURN_STMT), return_value(val) {}
};

struct IfStmt : SQLNode {
    SQLNode* condition;
    std::vector<SQLNode*> then_stmts;
    std::vector<SQLNode*> else_stmts;

    IfStmt()
        : SQLNode(SQLNodeKind::IF_STMT), condition(nullptr) {}
};

struct WhileLoop : SQLNode {
    SQLNode* condition;
    std::vector<SQLNode*> body;

    WhileLoop()
        : SQLNode(SQLNodeKind::WHILE_LOOP), condition(nullptr) {}
};

struct ForLoop : SQLNode {
    std::string_view variable;
    SQLNode* start_value;
    SQLNode* end_value;
    std::vector<SQLNode*> body;

    ForLoop()
        : SQLNode(SQLNodeKind::FOR_LOOP), start_value(nullptr), end_value(nullptr) {}
};

struct LoopStmt : SQLNode {
    std::vector<SQLNode*> body;

    LoopStmt() : SQLNode(SQLNodeKind::LOOP_STMT) {}
};

struct BreakStmt : SQLNode {
    BreakStmt() : SQLNode(SQLNodeKind::BREAK_STMT) {}
};

struct ContinueStmt : SQLNode {
    ContinueStmt() : SQLNode(SQLNodeKind::CONTINUE_STMT) {}
};

struct BeginEndBlock : SQLNode {
    std::vector<SQLNode*> statements;

    BeginEndBlock() : SQLNode(SQLNodeKind::BEGIN_END_BLOCK) {}
};

struct DoBlock : SQLNode {
    std::string_view language;     // Optional LANGUAGE clause
    std::string_view code_block;    // Raw code block (including delimiters like $$...$$)
    std::vector<SQLNode*> statements;  // Parsed statements (optional, for future use)

    DoBlock() : SQLNode(SQLNodeKind::DO_BLOCK) {}
};

struct ExceptionBlock : SQLNode {
    std::vector<SQLNode*> try_statements;
    std::vector<std::pair<std::string_view, std::vector<SQLNode*>>> handlers;  // (exception_name, statements)

    ExceptionBlock() : SQLNode(SQLNodeKind::EXCEPTION_BLOCK) {}
};

struct RaiseStmt : SQLNode {
    std::string_view level;      // EXCEPTION, NOTICE, WARNING, INFO, LOG, DEBUG (PostgreSQL) or SIGNAL (MySQL)
    std::string_view sqlstate;   // SQLSTATE for SIGNAL (MySQL)
    std::string_view message;

    RaiseStmt() : SQLNode(SQLNodeKind::RAISE_STMT) {}
};

struct OpenCursorStmt : SQLNode {
    std::string_view cursor_name;

    OpenCursorStmt() : SQLNode(SQLNodeKind::OPEN_CURSOR_STMT) {}
};

struct FetchCursorStmt : SQLNode {
    std::string_view cursor_name;
    std::string_view direction;  // NEXT, PRIOR, FIRST, LAST, or empty
    std::vector<std::string_view> into_variables;

    FetchCursorStmt() : SQLNode(SQLNodeKind::FETCH_CURSOR_STMT) {}
};

struct CloseCursorStmt : SQLNode {
    std::string_view cursor_name;

    CloseCursorStmt() : SQLNode(SQLNodeKind::CLOSE_CURSOR_STMT) {}
};

struct DelimiterStmt : SQLNode {
    std::string_view delimiter;

    DelimiterStmt() : SQLNode(SQLNodeKind::DELIMITER_STMT) {}
};

/// ============================================================================
/// Triggers
/// ============================================================================

enum class TriggerTiming { BEFORE, AFTER, INSTEAD_OF };
enum class TriggerEvent { INSERT, UPDATE, DELETE };

struct CreateTriggerStmt : SQLNode {
    std::string_view name;
    std::string_view table;
    TriggerTiming timing;
    TriggerEvent event;
    bool for_each_row;
    std::vector<SQLNode*> body;

    CreateTriggerStmt()
        : SQLNode(SQLNodeKind::CREATE_TRIGGER_STMT), for_each_row(false) {}
};

struct DropTriggerStmt : SQLNode {
    std::string_view name;
    std::string_view table;  // Optional
    bool if_exists;

    DropTriggerStmt()
        : SQLNode(SQLNodeKind::DROP_TRIGGER_STMT), if_exists(false) {}
};

/// ============================================================================
/// Advanced Features
/// ============================================================================

struct PivotClause : SQLNode {
    SQLNode* table_expr;
    FunctionCall* aggregate;
    SQLNode* pivot_column;
    std::vector<SQLNode*> pivot_values;

    PivotClause()
        : SQLNode(SQLNodeKind::PIVOT_CLAUSE), table_expr(nullptr),
          aggregate(nullptr), pivot_column(nullptr) {}
};

struct UnpivotClause : SQLNode {
    SQLNode* table_expr;
    std::string_view value_column;
    std::string_view name_column;
    std::vector<std::string_view> unpivot_columns;

    UnpivotClause()
        : SQLNode(SQLNodeKind::UNPIVOT_CLAUSE), table_expr(nullptr) {}
};

/// ============================================================================
/// BigQuery ML
/// ============================================================================

struct CreateModelStmt : SQLNode {
    std::string_view model_name;
    std::string_view model_type;
    SelectStmt* training_query;
    bool or_replace;

    CreateModelStmt()
        : SQLNode(SQLNodeKind::CREATE_MODEL_STMT), training_query(nullptr), or_replace(false) {}
};

struct DropModelStmt : SQLNode {
    std::string_view model_name;
    bool if_exists;

    DropModelStmt()
        : SQLNode(SQLNodeKind::DROP_MODEL_STMT), if_exists(false) {}
};

struct MLPredictExpr : SQLNode {
    std::string_view model_name;
    SelectStmt* input_query;

    MLPredictExpr()
        : SQLNode(SQLNodeKind::ML_PREDICT_EXPR), input_query(nullptr) {}
};

struct MLEvaluateExpr : SQLNode {
    std::string_view model_name;
    SelectStmt* evaluation_query;

    MLEvaluateExpr()
        : SQLNode(SQLNodeKind::ML_EVALUATE_EXPR), evaluation_query(nullptr) {}
};

struct MLTrainingInfoExpr : SQLNode {
    std::string_view model_name;

    MLTrainingInfoExpr()
        : SQLNode(SQLNodeKind::ML_TRAINING_INFO_EXPR) {}
};

/// ============================================================================
/// Advanced DDL (Simplified for Phase A)
/// ============================================================================

enum class PartitionType { RANGE, LIST, HASH };

struct PartitionSpec : SQLNode {
    PartitionType type;
    std::vector<std::string_view> columns;

    PartitionSpec()
        : SQLNode(SQLNodeKind::PARTITION_SPEC) {}
};

struct CreateTablespaceStmt : SQLNode {
    std::string_view name;
    std::string_view location;

    CreateTablespaceStmt()
        : SQLNode(SQLNodeKind::CREATE_TABLESPACE_STMT) {}
};

struct CreateIndexAdv : SQLNode {
    std::string_view index_name;
    TableRef* table;
    std::vector<SQLNode*> columns;  // Can be expressions
    bool unique;
    bool concurrently;
    SQLNode* where_clause;  // Partial index

    CreateIndexAdv()
        : SQLNode(SQLNodeKind::CREATE_INDEX_ADV), table(nullptr),
          unique(false), concurrently(false), where_clause(nullptr) {}
};

} // namespace libglot::sql
