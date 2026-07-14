#pragma once

#include "fwd.h"
#include <cstdint>
#include <string_view>

namespace libglot::sql::lex {

/// Token types - every SQL keyword, operator, literal format, delimiter
enum class TokenType : uint16_t {
    // Special
    ERROR,
    EOF_TOKEN,
    WHITESPACE,
    COMMENT,

    // Literals
    NUMBER,           // 123, 123.45, 1.23e10, 0x1F, 0b1010
    STRING,           // 'text', "text", $$text$$
    IDENTIFIER,       // column_name, "quoted id", `backtick`, [bracket]
    PARAMETER,        // ?, $1, :name, @name
    BIT_STRING,       // b'0101', 0b1010
    HEX_STRING,       // x'1F2A', 0x1F2A
    NATIONAL_STRING,  // N'text'

    // Operators
    PLUS,             // +
    MINUS,            // -
    STAR,             // *
    SLASH,            // /
    PERCENT,          // %
    CARET,            // ^ (xor or power depending on dialect)
    AMPERSAND,        // &
    PIPE,             // |
    TILDE,            // ~
    EQ,               // =
    NEQ,              // <>, !=
    LT,               // <
    LTE,              // <=
    GT,               // >
    GTE,              // >=
    CONCAT,           // ||
    ARROW,            // -> (JSON)
    LONG_ARROW,       // ->> (JSON)
    HASH_ARROW,       // #> (JSON path)
    HASH_LONG_ARROW,  // #>> (JSON path)
    AT_GT,            // @> (contains)
    LT_AT,            // <@ (contained by)
    QUESTION,         // ? (JSON exists)
    DOUBLE_COLON,     // :: (Postgres cast)

    // Delimiters
    LPAREN,           // (
    RPAREN,           // )
    LBRACKET,         // [
    RBRACKET,         // ]
    LBRACE,           // {
    RBRACE,           // }
    COMMA,            // ,
    SEMICOLON,        // ;
    DOT,              // .
    COLON,            // :
    DOUBLE_DOT,       // .. (range)

    // Keywords - SQL Standard
    SELECT, INSERT, UPDATE, DELETE, MERGE,
    CREATE, DROP, ALTER, TRUNCATE,
    FROM, WHERE, HAVING, GROUP, ORDER, LIMIT, OFFSET,
    JOIN, INNER, LEFT, RIGHT, FULL, CROSS, OUTER,
    ON, USING, NATURAL,
    UNION, INTERSECT, EXCEPT, MINUS_KW,  // MINUS_KW to avoid conflict with MINUS operator
    AS, DISTINCT, ALL, ANY, SOME,
    AND, OR, NOT, IN, EXISTS, BETWEEN, LIKE, ILIKE,
    IS, NULL_KW, TRUE, FALSE,
    CASE, WHEN, THEN, ELSE, END,
    ASC, DESC, NULLS, FIRST, LAST,
    WITH, RECURSIVE,
    VALUES, DEFAULT,
    SET, RETURNING,

    // Data types
    INT, INTEGER, BIGINT, SMALLINT, TINYINT,
    FLOAT, DOUBLE, REAL, DECIMAL, NUMERIC,
    CHAR, VARCHAR, TEXT, STRING_TYPE,  // STRING_TYPE to avoid conflict with STRING literal
    BOOLEAN, BOOL,
    DATE, TIME, TIMESTAMP, TIMESTAMPTZ, INTERVAL,
    BINARY, VARBINARY, BLOB,
    ARRAY, MAP, STRUCT, JSON, JSONB, UUID,

    // DDL
    TABLE, VIEW, INDEX, SCHEMA, DATABASE, CATALOG,
    COLUMN, CONSTRAINT, PRIMARY, FOREIGN, KEY, REFERENCES,
    UNIQUE, CHECK, DEFAULT_KW,
    TEMPORARY, TEMP, IF_KW, NOT_KW, EXISTS_KW,  // EXISTS_KW to avoid conflict
    RENAME, ADD, MODIFY, CHANGE,

    // Functions - Common
    COUNT, SUM, AVG, MIN, MAX,
    COALESCE, NULLIF, IFNULL, NVL,
    CAST, TRY_CAST, SAFE_CAST, CONVERT,
    EXTRACT, DATE_ADD, DATE_SUB, DATE_DIFF, DATE_TRUNC,
    SUBSTRING, SUBSTR, CONCAT_KW, CONCAT_WS, LENGTH, TRIM,
    UPPER, LOWER, REPLACE, SPLIT,
    ROUND, FLOOR, CEIL, ABS, POWER, SQRT,

    // Window functions
    OVER, PARTITION, BY, ROWS, RANGE,
    PRECEDING, FOLLOWING, UNBOUNDED, CURRENT, ROW,
    RANK, DENSE_RANK, ROW_NUMBER, NTILE,
    LEAD, LAG, FIRST_VALUE, LAST_VALUE, NTH_VALUE,

    // Set operations & clauses
    LATERAL, APPLY, PIVOT, UNPIVOT,
    QUALIFY, TABLESAMPLE,
    FETCH, NEXT, ONLY,
    FOR, UPDATE_LOCK, SHARE, NOWAIT, SKIP, LOCKED,  // UPDATE_LOCK to avoid conflict

    // DML modifiers
    INTO, OVERWRITE, IGNORE, REPLACE_KW,

    // Transaction control
    BEGIN, COMMIT, ROLLBACK, SAVEPOINT,
    TRANSACTION, WORK, ISOLATION, LEVEL,
    READ, WRITE, COMMITTED, UNCOMMITTED, REPEATABLE, SERIALIZABLE,

    // Misc SQL
    EXPLAIN, ANALYZE, VERBOSE,
    DESCRIBE, DESC_KW, SHOW,
    USE, GRANT, REVOKE, PRIVILEGES,
    COPY, LOAD, EXPORT, IMPORT,
    PRAGMA, VACUUM, REINDEX,
    REGEXP, RLIKE, SIMILAR, MATCHED,

    // DuckDB-specific
    HUGEINT, UHUGEINT,
    LIST, STRUCT_KW,
    EXCLUDE, REPLACE_DDB,  // REPLACE_DDB to avoid conflict with REPLACE_KW
    COLUMNS,
    SAMPLE,
    SUMMARIZE,

    // BigQuery-specific
    SAFE, ORDINAL, SAFE_OFFSET,
    UNNEST, FLATTEN,
    OPTIONS, CLUSTER,
    CURRENT_DATE, CURRENT_TIME, CURRENT_TIMESTAMP,
    MODEL, ML, PREDICT, EVALUATE, TRAINING_INFO,  // BigQuery ML keywords

    // Snowflake-specific
    VARIANT, OBJECT,
    FLATTEN_KW,
    CONNECT, NOCYCLE, START_WITH, CONNECT_BY, PRIOR,

    // Postgres-specific
    RETURNING_KW, DO, LANGUAGE,
    PLPGSQL, DECLARE, PERFORM,
    GENERATE_SERIES,
    DELIMITER_KW,  // DELIMITER for MySQL

    // Stored Procedures & Functions
    FUNCTION, PROCEDURE_KW,  // PROCEDURE_KW to distinguish from PROCEDURE already defined
    CALL, RETURN_KW, RETURNS, SETOF,  // RETURN_KW for RETURN statement, RETURNS for function return type, SETOF for set-returning functions
    OUT, INOUT,
    IF, WHILE, LOOP, EACH,
    ELSEIF, ENDIF, ENDWHILE, ENDLOOP,
    BREAK, CONTINUE, EXIT,  // Loop control statements
    EXCEPTION, WHEN_KW, RAISE, SIGNAL,  // Exception handling
    CURSOR, OPEN, CLOSE, SCROLL,  // Cursor operations
    COLON_EQUALS,  // := (assignment operator)

    // Triggers
    TRIGGER, BEFORE, AFTER, INSTEAD, OF,
    EACH_ROW, EACH_STMT,
    OLD, NEW,

    // Advanced DDL
    TABLESPACE, CONCURRENTLY,
    HASH, RANGE_KW, LIST_KW,  // RANGE_KW and LIST_KW to avoid conflicts
    MAXVALUE, MINVALUE,

    // TSQL-specific
    TOP, PERCENT_KW, WITH_TIES,
    OUTPUT, INSERTED, DELETED,
    GO, EXEC, EXECUTE, PROCEDURE,
    IDENTITY, SCOPE_IDENTITY,

    // MySQL-specific
    AUTO_INCREMENT, UNSIGNED, ZEROFILL,
    ENGINE, CHARSET, COLLATE,
    STRAIGHT_JOIN,
    FORCE, IGNORE_MYSQL, USE_INDEX,  // IGNORE_MYSQL to avoid conflict
    LOCAL, NO_WRITE_TO_BINLOG,  // ANALYZE options

    // Oracle-specific
    DUAL, ROWNUM, ROWID,
    CONNECT_BY_ROOT, SYS_CONNECT_BY_PATH,

    // Clickhouse-specific
    ENGINE_KW, PARTITION_BY, ORDER_BY,
    FINAL, PREWHERE,
    SETTINGS,

    // Redshift-specific
    DISTKEY, SORTKEY, SUPER, DISTSTYLE,

    // DuckDB/ClickHouse-specific
    ASOF,

    // CockroachDB/SQLite-specific
    UPSERT,

    // Materialize-specific
    TAIL,

    // Vertica-specific
    PROJECTION, SEGMENTED,

    // Greenplum/Doris-specific
    DISTRIBUTED,

    // SingleStore/PGVector-specific
    VECTOR,

    // Doris-specific
    DUPLICATE, BUCKETS,

    // TiDB-specific
    AUTO_RANDOM,

    // MySQL/Spark-specific operators
    NULL_SAFE_EQ,  // <=> operator

    // Databricks-specific
    OPTIMIZE, ZORDER,

    // Hive/Impala-specific
    COMPUTE, STATS,

    // Keep this last - for iteration
    TOKEN_TYPE_COUNT
};

/// Token - compact representation referencing original source
struct Token {
    TokenType type;
    uint32_t start;       // Byte offset in source
    uint32_t end;         // Byte offset (exclusive)
    uint16_t line;        // Line number (1-indexed)
    uint16_t col;         // Column number (1-indexed)
    const char* text;     // Interned text (for identifiers, keywords, literals)

    [[nodiscard]] size_t length() const { return end - start; }
    [[nodiscard]] std::string_view view(std::string_view source) const {
        return source.substr(start, length());
    }
};

/// Get string representation of token type
[[nodiscard]] constexpr std::string_view token_type_name(TokenType type) {
    switch (type) {
        case TokenType::ERROR: return "ERROR";
        case TokenType::EOF_TOKEN: return "EOF";
        case TokenType::WHITESPACE: return "WHITESPACE";
        case TokenType::COMMENT: return "COMMENT";
        case TokenType::NUMBER: return "NUMBER";
        case TokenType::STRING: return "STRING";
        case TokenType::IDENTIFIER: return "IDENTIFIER";
        case TokenType::PARAMETER: return "PARAMETER";
        case TokenType::SELECT: return "SELECT";
        case TokenType::FROM: return "FROM";
        case TokenType::WHERE: return "WHERE";
        case TokenType::JOIN: return "JOIN";
        // ... (all token types - abbreviated for space)
        default: return "UNKNOWN";
    }
}

/// Check if token is a keyword
[[nodiscard]] constexpr bool is_keyword(TokenType type) {
    return type >= TokenType::SELECT && type < TokenType::TOKEN_TYPE_COUNT;
}

/// Check if token is an operator
[[nodiscard]] constexpr bool is_operator(TokenType type) {
    return type >= TokenType::PLUS && type <= TokenType::QUESTION;
}

/// Check if token is a literal
[[nodiscard]] constexpr bool is_literal(TokenType type) {
    return type >= TokenType::NUMBER && type <= TokenType::NATIONAL_STRING;
}

/// Get the text representation of a token type (for operators/delimiters)
[[nodiscard]] constexpr const char* token_type_text(TokenType type) {
    switch (type) {
        case TokenType::PLUS: return "+";
        case TokenType::MINUS: return "-";
        case TokenType::STAR: return "*";
        case TokenType::SLASH: return "/";
        case TokenType::PERCENT: return "%";
        case TokenType::CARET: return "^";
        case TokenType::AMPERSAND: return "&";
        case TokenType::PIPE: return "|";
        case TokenType::TILDE: return "~";
        case TokenType::EQ: return "=";
        case TokenType::NEQ: return "<>";
        case TokenType::LT: return "<";
        case TokenType::LTE: return "<=";
        case TokenType::GT: return ">";
        case TokenType::GTE: return ">=";
        case TokenType::CONCAT: return "||";
        case TokenType::ARROW: return "->";
        case TokenType::LONG_ARROW: return "->>";
        case TokenType::DOUBLE_COLON: return "::";
        case TokenType::LPAREN: return "(";
        case TokenType::RPAREN: return ")";
        case TokenType::LBRACKET: return "[";
        case TokenType::RBRACKET: return "]";
        case TokenType::LBRACE: return "{";
        case TokenType::RBRACE: return "}";
        case TokenType::COMMA: return ",";
        case TokenType::SEMICOLON: return ";";
        case TokenType::DOT: return ".";
        case TokenType::COLON: return ":";
        case TokenType::QUESTION: return "?";
        case TokenType::DOUBLE_DOT: return "..";
        case TokenType::COLON_EQUALS: return ":=";
        default: return nullptr;
    }
}

/// Get text from a token - uses interned text if available, otherwise reconstructs from type
[[nodiscard]] inline const char* get_token_text(const Token& tok) {
    if (tok.text) {
        return tok.text;
    }
    // Fallback: reconstruct from token type (for operators/delimiters)
    return token_type_text(tok.type);
}

} // namespace libglot::sql::lex
