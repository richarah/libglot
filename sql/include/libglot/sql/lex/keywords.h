#pragma once

#include "tokens.h"
#include <cstdint>
#include <string_view>

namespace libglot::sql::lex {

/// Fast keyword lookup using perfect hash function
///
/// Hash table configuration:
/// - Table size: 256
/// - Slot size: 8
/// - Max keyword length: 19
/// - Hash function: (first * 31 + last + length) & 255
///
// Hash collision report (89 slots with collisions):
// Slot 4 (2 entries): PERFORM, HASH
// Slot 7 (3 entries): POWER, PARTITION, PRIOR
// Slot 34 (2 entries): IGNORE, INSTEAD
// Slot 38 (2 entries): AND, ADD
// Slot 41 (3 entries): AVG, IFNULL, ASOF
// Slot 43 (2 entries): INTERVAL, ANALYZE
// Slot 46 (4 entries): INNER, ALL, INT, ISOLATION
// Slot 48 (2 entries): INTEGER, INOUT
// Slot 49 (2 entries): INSERT, IMPORT
// Slot 52 (3 entries): INTERSECT, AS, INDEX
// Slot 54 (3 entries): ALTER, READ, AFTER
// Slot 55 (3 entries): ROUND, ROWID, AUTO_RANDOM
// Slot 56 (4 entries): RANGE, RLIKE, RAISE, IDENTITY
// Slot 57 (2 entries): RENAME, REVOKE
// Slot 58 (2 entries): REPLACE, ZEROFILL
// Slot 61 (5 entries): ARRAY, JSONB, RANK, APPLY, REPEATABLE
// Slot 62 (3 entries): RETURNING, REAL, ZORDER
// Slot 65 (3 entries): ROLLBACK, AUTO_INCREMENT, ROWNUM
// Slot 68 (2 entries): BLOB, REGEXP
// Slot 72 (4 entries): JOIN, JSON, ROW, RETURNS
// Slot 78 (2 entries): BOOL, BREAK
// Slot 83 (2 entries): BETWEEN, BOOLEAN
// Slot 86 (2 entries): SOME, SAFE
// Slot 88 (4 entries): BIGINT, SAMPLE, SETOF, BUCKETS
// Slot 90 (2 entries): STRING, SEGMENTED
// Slot 93 (3 entries): BINARY, SUM, SUBSTRING
// Slot 95 (3 entries): START_WITH, SIGNAL, SCROLL
// Slot 100 (2 entries): SET, SUPER
// Slot 101 (3 entries): SUBSTR, SQRT, STATS
// Slot 102 (3 entries): CASE, SPLIT, SIMILAR
// Slot 103 (3 entries): SELECT, STRUCT, CLOSE
// Slot 104 (6 entries): CREATE, CHANGE, SHOW, STRAIGHT_JOIN, SYS_CONNECT_BY_PATH, SETTINGS
// Slot 105 (3 entries): SMALLINT, COLLATE, COMPUTE
// Slot 106 (5 entries): COALESCE, SAFE_CAST, SAVEPOINT, COMMITTED, CONTINUE
// Slot 109 (4 entries): CHECK, CEIL, CALL, SORTKEY
// Slot 110 (2 entries): CURRENT_DATE, CURRENT_TIME
// Slot 113 (2 entries): COLUMN, KEY
// Slot 117 (5 entries): CROSS, TRUE, TIME, CAST, CURSOR
// Slot 118 (3 entries): TABLE, COUNT, CLUSTER
// Slot 119 (3 entries): CONCAT, COMMIT, COLUMNS
// Slot 120 (4 entries): CONVERT, CURRENT, CONNECT, CHARSET
// Slot 121 (2 entries): TRUNCATE, CONCAT_WS
// Slot 123 (2 entries): CONSTRAINT, TABLESPACE
// Slot 124 (4 entries): LEAD, TABLESAMPLE, LOAD, TAIL
// Slot 125 (2 entries): LIKE, TRIM
// Slot 126 (4 entries): THEN, LAG, LOCKED, CURRENT_TIMESTAMP
// Slot 128 (3 entries): TEMP, CONNECT_BY, CONNECT_BY_ROOT
// Slot 130 (2 entries): LENGTH, CONCURRENTLY
// Slot 131 (2 entries): DESC, LAST_VALUE
// Slot 133 (6 entries): DATE, TIMESTAMP, TRANSACTION, LEVEL, TRIGGER, LOCAL
// Slot 135 (5 entries): DELETE, TINYINT, DOUBLE, LATERAL, DELETED
// Slot 136 (4 entries): TRY_CAST, DATE_ADD, DECLARE, LOOP
// Slot 137 (3 entries): DATABASE, DATE_TRUNC, DESCRIBE
// Slot 138 (2 entries): DISTSTYLE, DUPLICATE
// Slot 139 (3 entries): DATE_DIFF, LOWER, DISTRIBUTED
// Slot 140 (4 entries): LEFT, LAST, LIST, DUAL
// Slot 141 (2 entries): LIMIT, DO
// Slot 145 (2 entries): TIMESTAMPTZ, DENSE_RANK
// Slot 147 (2 entries): UUID, USE
// Slot 150 (2 entries): UPDATE, UNIQUE
// Slot 151 (4 entries): USING, DEFAULT, DELIMITER, UNSIGNED
// Slot 152 (2 entries): DISTINCT, UNBOUNDED
// Slot 158 (2 entries): UNION, MATCHED
// Slot 160 (2 entries): MAXVALUE, MINVALUE
// Slot 162 (3 entries): END, UPPER, EXEC
// Slot 164 (2 entries): ELSE, MIN
// Slot 165 (2 entries): UNNEST, UPSERT
// Slot 166 (4 entries): MAP, UNPIVOT, ENDIF, ENGINE
// Slot 167 (5 entries): UHUGEINT, EXCLUDE, EACH, ELSEIF, EXECUTE
// Slot 178 (3 entries): MODIFY, ENDLOOP, EXCEPTION
// Slot 181 (2 entries): EXCEPT, EXPORT
// Slot 182 (2 entries): EXTRACT, VERBOSE
// Slot 188 (2 entries): NUMERIC, NTILE
// Slot 190 (2 entries): NULLIF, NOCYCLE
// Slot 194 (2 entries): NULL, VECTOR
// Slot 195 (2 entries): VALUES, VARCHAR
// Slot 196 (2 entries): FALSE, FORCE
// Slot 197 (3 entries): NATURAL, VIEW, VARIANT
// Slot 202 (5 entries): FULL, NULLS, FOLLOWING, FIRST_VALUE, NEXT
// Slot 203 (3 entries): FROM, NO_WRITE_TO_BINLOG, FINAL
// Slot 204 (3 entries): VARBINARY, NOWAIT, NEW
// Slot 207 (3 entries): FOREIGN, FOR, FLATTEN
// Slot 211 (5 entries): WHERE, FIRST, FLOAT, WRITE, WHILE
// Slot 216 (2 entries): WORK, OLD
// Slot 219 (2 entries): WHEN, WHEN
// Slot 232 (3 entries): ORDER, OUTER, OUT
// Slot 235 (4 entries): OFFSET, OPTIONS, OBJECT, OUTPUT
// Slot 238 (2 entries): GROUP, ONLY
// Slot 242 (2 entries): GRANT, ORDER_BY

class KeywordLookup {
public:
    [[nodiscard]] static TokenType lookup(std::string_view text) noexcept {
        if (text.empty() || text.size() > 19) {
            return TokenType::IDENTIFIER;
        }

        // Convert to uppercase inline (branchless optimization)
        char upper[20];
        size_t len = text.size();
        for (size_t i = 0; i < len; ++i) {
            char c = text[i];
            // Branchless: subtract 32 if lowercase (avoids branch misprediction)
            // (c >= 'a') & (c <= 'z') evaluates to 0 or 1, shift left 5 bits = 0 or 32
            upper[i] = static_cast<char>(c - (((c >= 'a') & (c <= 'z')) << 5));
        }
        upper[len] = '\0';

        // Perfect hash: (first * 31 + last + length) & 255
        uint32_t hash = (upper[0] * 31 + upper[len - 1] + len) & 255;
        const KeywordEntry& entry = keyword_table[hash];

        // Linear probing for collisions
        for (int i = 0; i < 8; ++i) {
            if (entry.keywords[i] == nullptr)
                break;
            if (len == entry.lengths[i]) {
                bool match = true;
                for (size_t j = 0; j < len; ++j) {
                    if (upper[j] != entry.keywords[i][j]) {
                        match = false;
                        break;
                    }
                }
                if (match)
                    return entry.types[i];
            }
        }
        return TokenType::IDENTIFIER;
    }

private:
    struct KeywordEntry {
        const char* keywords[8];
        uint8_t lengths[8];
        TokenType types[8];
    };

    static constexpr KeywordEntry keyword_table[256] = {
        {{"PRECEDING", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {9, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::PRECEDING, TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // PRECEDING (hash=0)
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{"PLPGSQL", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {7, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::PLPGSQL, TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // PLPGSQL (hash=3)
        {{"PERFORM", "HASH", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {7, 4, 0, 0, 0, 0, 0, 0},
         {TokenType::PERFORM, TokenType::HASH, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // PERFORM (hash=4), HASH (hash=4)
        {{"HAVING", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {6, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::HAVING, TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // HAVING (hash=5)
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{"POWER", "PARTITION", "PRIOR", nullptr, nullptr, nullptr, nullptr, nullptr},
         {5, 9, 5, 0, 0, 0, 0, 0},
         {TokenType::POWER, TokenType::PARTITION, TokenType::PRIOR, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // POWER (hash=7), PARTITION (hash=7), PRIOR (hash=7)
        {{"PROJECTION", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {10, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::PROJECTION, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}}, // PROJECTION (hash=8)
        {{"PIVOT", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {5, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::PIVOT, TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // PIVOT (hash=9)
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{"PRIVILEGES", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {10, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::PRIVILEGES, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}}, // PRIVILEGES (hash=13)
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{"PRIMARY", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {7, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::PRIMARY, TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // PRIMARY (hash=16)
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{"HUGEINT", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {7, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::HUGEINT, TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // HUGEINT (hash=19)
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{"PARTITION_BY", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {12, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::PARTITION_BY, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}}, // PARTITION_BY (hash=21)
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{"IF", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {2, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IF_KW, TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // IF (hash=31)
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{"ILIKE", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {5, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::ILIKE, TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // ILIKE (hash=33)
        {{"IGNORE", "INSTEAD", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {6, 7, 0, 0, 0, 0, 0, 0},
         {TokenType::IGNORE, TokenType::INSTEAD, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // IGNORE (hash=34), INSTEAD (hash=34)
        {{"INSERTED", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {8, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::INSERTED, TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // INSERTED (hash=35)
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{"ASC", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {3, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::ASC, TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // ASC (hash=37)
        {{"AND", "ADD", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {3, 3, 0, 0, 0, 0, 0, 0},
         {TokenType::AND, TokenType::ADD, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // AND (hash=38), ADD (hash=38)
        {{"IN", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {2, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IN, TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // IN (hash=39)
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{"AVG", "IFNULL", "ASOF", nullptr, nullptr, nullptr, nullptr, nullptr},
         {3, 6, 4, 0, 0, 0, 0, 0},
         {TokenType::AVG, TokenType::IFNULL, TokenType::ASOF, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // AVG (hash=41), IFNULL (hash=41), ASOF (hash=41)
        {{"INTO", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {4, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::INTO, TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // INTO (hash=42)
        {{"INTERVAL", "ANALYZE", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {8, 7, 0, 0, 0, 0, 0, 0},
         {TokenType::INTERVAL, TokenType::ANALYZE, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // INTERVAL (hash=43), ANALYZE (hash=43)
        {{"IS", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {2, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IS, TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // IS (hash=44)
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{"INNER", "ALL", "INT", "ISOLATION", nullptr, nullptr, nullptr, nullptr},
         {5, 3, 3, 9, 0, 0, 0, 0},
         {TokenType::INNER, TokenType::ALL, TokenType::INT, TokenType::ISOLATION,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // INNER (hash=46), ALL (hash=46), INT (hash=46), ISOLATION
                                   // (hash=46)
        {{"QUALIFY", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {7, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::QUALIFY, TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // QUALIFY (hash=47)
        {{"INTEGER", "INOUT", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {7, 5, 0, 0, 0, 0, 0, 0},
         {TokenType::INTEGER, TokenType::INOUT, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // INTEGER (hash=48), INOUT (hash=48)
        {{"INSERT", "IMPORT", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {6, 6, 0, 0, 0, 0, 0, 0},
         {TokenType::INSERT, TokenType::IMPORT, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // INSERT (hash=49), IMPORT (hash=49)
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{"INTERSECT", "AS", "INDEX", nullptr, nullptr, nullptr, nullptr, nullptr},
         {9, 2, 5, 0, 0, 0, 0, 0},
         {TokenType::INTERSECT, TokenType::AS, TokenType::INDEX, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // INTERSECT (hash=52), AS (hash=52), INDEX (hash=52)
        {{"ABS", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {3, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::ABS, TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // ABS (hash=53)
        {{"ALTER", "READ", "AFTER", nullptr, nullptr, nullptr, nullptr, nullptr},
         {5, 4, 5, 0, 0, 0, 0, 0},
         {TokenType::ALTER, TokenType::READ, TokenType::AFTER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // ALTER (hash=54), READ (hash=54), AFTER (hash=54)
        {{"ROUND", "ROWID", "AUTO_RANDOM", nullptr, nullptr, nullptr, nullptr, nullptr},
         {5, 5, 11, 0, 0, 0, 0, 0},
         {TokenType::ROUND, TokenType::ROWID, TokenType::AUTO_RANDOM, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // ROUND (hash=55), ROWID (hash=55), AUTO_RANDOM (hash=55)
        {{"RANGE", "RLIKE", "RAISE", "IDENTITY", nullptr, nullptr, nullptr, nullptr},
         {5, 5, 5, 8, 0, 0, 0, 0},
         {TokenType::RANGE, TokenType::RLIKE, TokenType::RAISE, TokenType::IDENTITY,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // RANGE (hash=56), RLIKE (hash=56), RAISE (hash=56), IDENTITY
                                   // (hash=56)
        {{"RENAME", "REVOKE", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {6, 6, 0, 0, 0, 0, 0, 0},
         {TokenType::RENAME, TokenType::REVOKE, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // RENAME (hash=57), REVOKE (hash=57)
        {{"REPLACE", "ZEROFILL", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {7, 8, 0, 0, 0, 0, 0, 0},
         {TokenType::REPLACE_KW, TokenType::ZEROFILL, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // REPLACE (hash=58), ZEROFILL (hash=58)
        {{"ANY", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {3, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::ANY, TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // ANY (hash=59)
        {{"RECURSIVE", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {9, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::RECURSIVE, TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // RECURSIVE (hash=60)
        {{"ARRAY", "JSONB", "RANK", "APPLY", "REPEATABLE", nullptr, nullptr, nullptr},
         {5, 5, 4, 5, 10, 0, 0, 0},
         {TokenType::ARRAY, TokenType::JSONB, TokenType::RANK, TokenType::APPLY,
          TokenType::REPEATABLE, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // ARRAY (hash=61), JSONB (hash=61), RANK (hash=61), APPLY
                                   // (hash=61), REPEATABLE (hash=61)
        {{"RETURNING", "REAL", "ZORDER", nullptr, nullptr, nullptr, nullptr, nullptr},
         {9, 4, 6, 0, 0, 0, 0, 0},
         {TokenType::RETURNING, TokenType::REAL, TokenType::ZORDER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // RETURNING (hash=62), REAL (hash=62), ZORDER (hash=62)
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{"ROLLBACK", "AUTO_INCREMENT", "ROWNUM", nullptr, nullptr, nullptr, nullptr, nullptr},
         {8, 14, 6, 0, 0, 0, 0, 0},
         {TokenType::ROLLBACK, TokenType::AUTO_INCREMENT, TokenType::ROWNUM, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // ROLLBACK (hash=65), AUTO_INCREMENT (hash=65), ROWNUM (hash=65)
        {{"RETURN", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {6, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::RETURN_KW, TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // RETURN (hash=66)
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{"BLOB", "REGEXP", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {4, 6, 0, 0, 0, 0, 0, 0},
         {TokenType::BLOB, TokenType::REGEXP, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // BLOB (hash=68), REGEXP (hash=68)
        {{"ROWS", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {4, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::ROWS, TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // ROWS (hash=69)
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{"RIGHT", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {5, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::RIGHT, TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // RIGHT (hash=71)
        {{"JOIN", "JSON", "ROW", "RETURNS", nullptr, nullptr, nullptr, nullptr},
         {4, 4, 3, 7, 0, 0, 0, 0},
         {TokenType::JOIN, TokenType::JSON, TokenType::ROW, TokenType::RETURNS,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // JOIN (hash=72), JSON (hash=72), ROW (hash=72), RETURNS
                                   // (hash=72)
        {{"BEFORE", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {6, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::BEFORE, TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // BEFORE (hash=73)
        {{"ROW_NUMBER", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {10, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::ROW_NUMBER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}}, // ROW_NUMBER (hash=74)
        {{"REFERENCES", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {10, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::REFERENCES, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}}, // REFERENCES (hash=75)
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{"REINDEX", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {7, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::REINDEX, TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // REINDEX (hash=77)
        {{"BOOL", "BREAK", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {4, 5, 0, 0, 0, 0, 0, 0},
         {TokenType::BOOL, TokenType::BREAK, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // BOOL (hash=78), BREAK (hash=78)
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{"BEGIN", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {5, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::BEGIN, TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // BEGIN (hash=81)
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{"BETWEEN", "BOOLEAN", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {7, 7, 0, 0, 0, 0, 0, 0},
         {TokenType::BETWEEN, TokenType::BOOLEAN, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // BETWEEN (hash=83), BOOLEAN (hash=83)
        {{"SCHEMA", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {6, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::SCHEMA, TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // SCHEMA (hash=84)
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{"SOME", "SAFE", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {4, 4, 0, 0, 0, 0, 0, 0},
         {TokenType::SOME, TokenType::SAFE, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // SOME (hash=86), SAFE (hash=86)
        {{"SHARE", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {5, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::SHARE, TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // SHARE (hash=87)
        {{"BIGINT", "SAMPLE", "SETOF", "BUCKETS", nullptr, nullptr, nullptr, nullptr},
         {6, 6, 5, 7, 0, 0, 0, 0},
         {TokenType::BIGINT, TokenType::SAMPLE, TokenType::SETOF, TokenType::BUCKETS,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // BIGINT (hash=88), SAMPLE (hash=88), SETOF (hash=88), BUCKETS
                                   // (hash=88)
        {{"BY", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {2, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::BY, TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // BY (hash=89)
        {{"STRING", "SEGMENTED", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {6, 9, 0, 0, 0, 0, 0, 0},
         {TokenType::STRING_TYPE, TokenType::SEGMENTED, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}}, // STRING (hash=90), SEGMENTED (hash=90)
        {{"SUMMARIZE", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {9, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::SUMMARIZE, TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // SUMMARIZE (hash=91)
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{"BINARY", "SUM", "SUBSTRING", nullptr, nullptr, nullptr, nullptr, nullptr},
         {6, 3, 9, 0, 0, 0, 0, 0},
         {TokenType::BINARY, TokenType::SUM, TokenType::SUBSTRING, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // BINARY (hash=93), SUM (hash=93), SUBSTRING (hash=93)
        {{"SERIALIZABLE", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {12, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::SERIALIZABLE, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}}, // SERIALIZABLE (hash=94)
        {{"START_WITH", "SIGNAL", "SCROLL", nullptr, nullptr, nullptr, nullptr, nullptr},
         {10, 6, 6, 0, 0, 0, 0, 0},
         {TokenType::START_WITH, TokenType::SIGNAL, TokenType::SCROLL, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // START_WITH (hash=95), SIGNAL (hash=95), SCROLL (hash=95)
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{"SKIP", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {4, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::SKIP, TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // SKIP (hash=97)
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{"SET", "SUPER", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {3, 5, 0, 0, 0, 0, 0, 0},
         {TokenType::SET, TokenType::SUPER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // SET (hash=100), SUPER (hash=100)
        {{"SUBSTR", "SQRT", "STATS", nullptr, nullptr, nullptr, nullptr, nullptr},
         {6, 4, 5, 0, 0, 0, 0, 0},
         {TokenType::SUBSTR, TokenType::SQRT, TokenType::STATS, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // SUBSTR (hash=101), SQRT (hash=101), STATS (hash=101)
        {{"CASE", "SPLIT", "SIMILAR", nullptr, nullptr, nullptr, nullptr, nullptr},
         {4, 5, 7, 0, 0, 0, 0, 0},
         {TokenType::CASE, TokenType::SPLIT, TokenType::SIMILAR, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // CASE (hash=102), SPLIT (hash=102), SIMILAR (hash=102)
        {{"SELECT", "STRUCT", "CLOSE", nullptr, nullptr, nullptr, nullptr, nullptr},
         {6, 6, 5, 0, 0, 0, 0, 0},
         {TokenType::SELECT, TokenType::STRUCT_KW, TokenType::CLOSE, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // SELECT (hash=103), STRUCT (hash=103), CLOSE (hash=103)
        {{"CREATE", "CHANGE", "SHOW", "STRAIGHT_JOIN", "SYS_CONNECT_BY_PATH", "SETTINGS", nullptr,
          nullptr},
         {6, 6, 4, 13, 19, 8, 0, 0},
         {TokenType::CREATE, TokenType::CHANGE, TokenType::SHOW, TokenType::STRAIGHT_JOIN,
          TokenType::SYS_CONNECT_BY_PATH, TokenType::SETTINGS, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // CREATE (hash=104), CHANGE (hash=104), SHOW (hash=104),
                                   // STRAIGHT_JOIN (hash=104), SYS_CONNECT_BY_PATH (hash=104),
                                   // SETTINGS (hash=104)
        {{"SMALLINT", "COLLATE", "COMPUTE", nullptr, nullptr, nullptr, nullptr, nullptr},
         {8, 7, 7, 0, 0, 0, 0, 0},
         {TokenType::SMALLINT, TokenType::COLLATE, TokenType::COMPUTE, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // SMALLINT (hash=105), COLLATE (hash=105), COMPUTE (hash=105)
        {{"COALESCE", "SAFE_CAST", "SAVEPOINT", "COMMITTED", "CONTINUE", nullptr, nullptr, nullptr},
         {8, 9, 9, 9, 8, 0, 0, 0},
         {TokenType::COALESCE, TokenType::SAFE_CAST, TokenType::SAVEPOINT, TokenType::COMMITTED,
          TokenType::CONTINUE, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // COALESCE (hash=106), SAFE_CAST (hash=106), SAVEPOINT
                                   // (hash=106), COMMITTED (hash=106), CONTINUE (hash=106)
        {{"CATALOG", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {7, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::CATALOG, TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // CATALOG (hash=107)
        {{"SAFE_OFFSET", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {11, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::SAFE_OFFSET, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}}, // SAFE_OFFSET (hash=108)
        {{"CHECK", "CEIL", "CALL", "SORTKEY", nullptr, nullptr, nullptr, nullptr},
         {5, 4, 4, 7, 0, 0, 0, 0},
         {TokenType::CHECK, TokenType::CEIL, TokenType::CALL, TokenType::SORTKEY,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // CHECK (hash=109), CEIL (hash=109), CALL (hash=109), SORTKEY
                                   // (hash=109)
        {{"CURRENT_DATE", "CURRENT_TIME", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {12, 12, 0, 0, 0, 0, 0, 0},
         {TokenType::CURRENT_DATE, TokenType::CURRENT_TIME, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // CURRENT_DATE (hash=110), CURRENT_TIME (hash=110)
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{"COLUMN", "KEY", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {6, 3, 0, 0, 0, 0, 0, 0},
         {TokenType::COLUMN, TokenType::KEY, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // COLUMN (hash=113), KEY (hash=113)
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{"CHAR", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {4, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::CHAR, TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // CHAR (hash=115)
        {{"SCOPE_IDENTITY", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {14, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::SCOPE_IDENTITY, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}}, // SCOPE_IDENTITY (hash=116)
        {{"CROSS", "TRUE", "TIME", "CAST", "CURSOR", nullptr, nullptr, nullptr},
         {5, 4, 4, 4, 6, 0, 0, 0},
         {TokenType::CROSS, TokenType::TRUE, TokenType::TIME, TokenType::CAST, TokenType::CURSOR,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // CROSS (hash=117), TRUE (hash=117), TIME (hash=117), CAST
                                   // (hash=117), CURSOR (hash=117)
        {{"TABLE", "COUNT", "CLUSTER", nullptr, nullptr, nullptr, nullptr, nullptr},
         {5, 5, 7, 0, 0, 0, 0, 0},
         {TokenType::TABLE, TokenType::COUNT, TokenType::CLUSTER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // TABLE (hash=118), COUNT (hash=118), CLUSTER (hash=118)
        {{"CONCAT", "COMMIT", "COLUMNS", nullptr, nullptr, nullptr, nullptr, nullptr},
         {6, 6, 7, 0, 0, 0, 0, 0},
         {TokenType::CONCAT_KW, TokenType::COMMIT, TokenType::COLUMNS, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // CONCAT (hash=119), COMMIT (hash=119), COLUMNS (hash=119)
        {{"CONVERT", "CURRENT", "CONNECT", "CHARSET", nullptr, nullptr, nullptr, nullptr},
         {7, 7, 7, 7, 0, 0, 0, 0},
         {TokenType::CONVERT, TokenType::CURRENT, TokenType::CONNECT, TokenType::CHARSET,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // CONVERT (hash=120), CURRENT (hash=120), CONNECT (hash=120),
                                   // CHARSET (hash=120)
        {{"TRUNCATE", "CONCAT_WS", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {8, 9, 0, 0, 0, 0, 0, 0},
         {TokenType::TRUNCATE, TokenType::CONCAT_WS, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // TRUNCATE (hash=121), CONCAT_WS (hash=121)
        {{"COPY", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {4, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::COPY, TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // COPY (hash=122)
        {{"CONSTRAINT", "TABLESPACE", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {10, 10, 0, 0, 0, 0, 0, 0},
         {TokenType::CONSTRAINT, TokenType::TABLESPACE, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // CONSTRAINT (hash=123), TABLESPACE (hash=123)
        {{"LEAD", "TABLESAMPLE", "LOAD", "TAIL", nullptr, nullptr, nullptr, nullptr},
         {4, 11, 4, 4, 0, 0, 0, 0},
         {TokenType::LEAD, TokenType::TABLESAMPLE, TokenType::LOAD, TokenType::TAIL,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // LEAD (hash=124), TABLESAMPLE (hash=124), LOAD (hash=124), TAIL
                                   // (hash=124)
        {{"LIKE", "TRIM", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {4, 4, 0, 0, 0, 0, 0, 0},
         {TokenType::LIKE, TokenType::TRIM, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // LIKE (hash=125), TRIM (hash=125)
        {{"THEN", "LAG", "LOCKED", "CURRENT_TIMESTAMP", nullptr, nullptr, nullptr, nullptr},
         {4, 3, 6, 17, 0, 0, 0, 0},
         {TokenType::THEN, TokenType::LAG, TokenType::LOCKED, TokenType::CURRENT_TIMESTAMP,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // THEN (hash=126), LAG (hash=126), LOCKED (hash=126),
                                   // CURRENT_TIMESTAMP (hash=126)
        {{"TOP", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {3, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::TOP, TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // TOP (hash=127)
        {{"TEMP", "CONNECT_BY", "CONNECT_BY_ROOT", nullptr, nullptr, nullptr, nullptr, nullptr},
         {4, 10, 15, 0, 0, 0, 0, 0},
         {TokenType::TEMP, TokenType::CONNECT_BY, TokenType::CONNECT_BY_ROOT, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // TEMP (hash=128), CONNECT_BY (hash=128), CONNECT_BY_ROOT
                                   // (hash=128)
        {{"LANGUAGE", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {8, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::LANGUAGE, TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // LANGUAGE (hash=129)
        {{"LENGTH", "CONCURRENTLY", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {6, 12, 0, 0, 0, 0, 0, 0},
         {TokenType::LENGTH, TokenType::CONCURRENTLY, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // LENGTH (hash=130), CONCURRENTLY (hash=130)
        {{"DESC", "LAST_VALUE", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {4, 10, 0, 0, 0, 0, 0, 0},
         {TokenType::DESC, TokenType::LAST_VALUE, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // DESC (hash=131), LAST_VALUE (hash=131)
        {{"TEXT", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {4, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::TEXT, TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // TEXT (hash=132)
        {{"DATE", "TIMESTAMP", "TRANSACTION", "LEVEL", "TRIGGER", "LOCAL", nullptr, nullptr},
         {4, 9, 11, 5, 7, 5, 0, 0},
         {TokenType::DATE, TokenType::TIMESTAMP, TokenType::TRANSACTION, TokenType::LEVEL,
          TokenType::TRIGGER, TokenType::LOCAL, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // DATE (hash=133), TIMESTAMP (hash=133), TRANSACTION (hash=133),
                                   // LEVEL (hash=133), TRIGGER (hash=133), LOCAL (hash=133)
        {{"DATE_SUB", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {8, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::DATE_SUB, TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // DATE_SUB (hash=134)
        {{"DELETE", "TINYINT", "DOUBLE", "LATERAL", "DELETED", nullptr, nullptr, nullptr},
         {6, 7, 6, 7, 7, 0, 0, 0},
         {TokenType::DELETE, TokenType::TINYINT, TokenType::DOUBLE, TokenType::LATERAL,
          TokenType::DELETED, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // DELETE (hash=135), TINYINT (hash=135), DOUBLE (hash=135),
                                   // LATERAL (hash=135), DELETED (hash=135)
        {{"TRY_CAST", "DATE_ADD", "DECLARE", "LOOP", nullptr, nullptr, nullptr, nullptr},
         {8, 8, 7, 4, 0, 0, 0, 0},
         {TokenType::TRY_CAST, TokenType::DATE_ADD, TokenType::DECLARE, TokenType::LOOP,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // TRY_CAST (hash=136), DATE_ADD (hash=136), DECLARE (hash=136),
                                   // LOOP (hash=136)
        {{"DATABASE", "DATE_TRUNC", "DESCRIBE", nullptr, nullptr, nullptr, nullptr, nullptr},
         {8, 10, 8, 0, 0, 0, 0, 0},
         {TokenType::DATABASE, TokenType::DATE_TRUNC, TokenType::DESCRIBE, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // DATABASE (hash=137), DATE_TRUNC (hash=137), DESCRIBE
                                   // (hash=137)
        {{"DISTSTYLE", "DUPLICATE", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {9, 9, 0, 0, 0, 0, 0, 0},
         {TokenType::DISTSTYLE, TokenType::DUPLICATE, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // DISTSTYLE (hash=138), DUPLICATE (hash=138)
        {{"DATE_DIFF", "LOWER", "DISTRIBUTED", nullptr, nullptr, nullptr, nullptr, nullptr},
         {9, 5, 11, 0, 0, 0, 0, 0},
         {TokenType::DATE_DIFF, TokenType::LOWER, TokenType::DISTRIBUTED, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // DATE_DIFF (hash=139), LOWER (hash=139), DISTRIBUTED (hash=139)
        {{"LEFT", "LAST", "LIST", "DUAL", nullptr, nullptr, nullptr, nullptr},
         {4, 4, 4, 4, 0, 0, 0, 0},
         {TokenType::LEFT, TokenType::LAST, TokenType::LIST, TokenType::DUAL, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // LEFT (hash=140), LAST (hash=140), LIST (hash=140), DUAL
                                   // (hash=140)
        {{"LIMIT", "DO", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {5, 2, 0, 0, 0, 0, 0, 0},
         {TokenType::LIMIT, TokenType::DO, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // LIMIT (hash=141), DO (hash=141)
        {{"TEMPORARY", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {9, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::TEMPORARY, TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // TEMPORARY (hash=142)
        {{"DECIMAL", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {7, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::DECIMAL, TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // DECIMAL (hash=143)
        {{"DROP", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {4, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::DROP, TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // DROP (hash=144)
        {{"TIMESTAMPTZ", "DENSE_RANK", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {11, 10, 0, 0, 0, 0, 0, 0},
         {TokenType::TIMESTAMPTZ, TokenType::DENSE_RANK, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // TIMESTAMPTZ (hash=145), DENSE_RANK (hash=145)
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{"UUID", "USE", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {4, 3, 0, 0, 0, 0, 0, 0},
         {TokenType::UUID, TokenType::USE, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // UUID (hash=147), USE (hash=147)
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{"UPDATE", "UNIQUE", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {6, 6, 0, 0, 0, 0, 0, 0},
         {TokenType::UPDATE, TokenType::UNIQUE, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // UPDATE (hash=150), UNIQUE (hash=150)
        {{"USING", "DEFAULT", "DELIMITER", "UNSIGNED", nullptr, nullptr, nullptr, nullptr},
         {5, 7, 9, 8, 0, 0, 0, 0},
         {TokenType::USING, TokenType::DEFAULT, TokenType::DELIMITER_KW, TokenType::UNSIGNED,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // USING (hash=151), DEFAULT (hash=151), DELIMITER (hash=151),
                                   // UNSIGNED (hash=151)
        {{"DISTINCT", "UNBOUNDED", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {8, 9, 0, 0, 0, 0, 0, 0},
         {TokenType::DISTINCT, TokenType::UNBOUNDED, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // DISTINCT (hash=152), UNBOUNDED (hash=152)
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{"UNCOMMITTED", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {11, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::UNCOMMITTED, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}}, // UNCOMMITTED (hash=154)
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{"DISTKEY", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {7, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::DISTKEY, TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // DISTKEY (hash=156)
        {{"MERGE", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {5, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::MERGE, TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // MERGE (hash=157)
        {{"UNION", "MATCHED", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {5, 7, 0, 0, 0, 0, 0, 0},
         {TokenType::UNION, TokenType::MATCHED, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // UNION (hash=158), MATCHED (hash=158)
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{"MAXVALUE", "MINVALUE", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {8, 8, 0, 0, 0, 0, 0, 0},
         {TokenType::MAXVALUE, TokenType::MINVALUE, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // MAXVALUE (hash=160), MINVALUE (hash=160)
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{"END", "UPPER", "EXEC", nullptr, nullptr, nullptr, nullptr, nullptr},
         {3, 5, 4, 0, 0, 0, 0, 0},
         {TokenType::END, TokenType::UPPER, TokenType::EXEC, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // END (hash=162), UPPER (hash=162), EXEC (hash=162)
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{"ELSE", "MIN", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {4, 3, 0, 0, 0, 0, 0, 0},
         {TokenType::ELSE, TokenType::MIN, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // ELSE (hash=164), MIN (hash=164)
        {{"UNNEST", "UPSERT", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {6, 6, 0, 0, 0, 0, 0, 0},
         {TokenType::UNNEST, TokenType::UPSERT, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // UNNEST (hash=165), UPSERT (hash=165)
        {{"MAP", "UNPIVOT", "ENDIF", "ENGINE", nullptr, nullptr, nullptr, nullptr},
         {3, 7, 5, 6, 0, 0, 0, 0},
         {TokenType::MAP, TokenType::UNPIVOT, TokenType::ENDIF, TokenType::ENGINE,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // MAP (hash=166), UNPIVOT (hash=166), ENDIF (hash=166), ENGINE
                                   // (hash=166)
        {{"UHUGEINT", "EXCLUDE", "EACH", "ELSEIF", "EXECUTE", nullptr, nullptr, nullptr},
         {8, 7, 4, 6, 7, 0, 0, 0},
         {TokenType::UHUGEINT, TokenType::EXCLUDE, TokenType::EACH, TokenType::ELSEIF,
          TokenType::EXECUTE, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // UHUGEINT (hash=167), EXCLUDE (hash=167), EACH (hash=167),
                                   // ELSEIF (hash=167), EXECUTE (hash=167)
        {{"ENDWHILE", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {8, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::ENDWHILE, TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // ENDWHILE (hash=168)
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{"MINUS", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {5, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::MINUS_KW, TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // MINUS (hash=171)
        {{"USE_INDEX", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {9, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::USE_INDEX, TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // USE_INDEX (hash=172)
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{"MAX", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {3, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::MAX, TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // MAX (hash=174)
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{"EXPLAIN", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {7, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::EXPLAIN, TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // EXPLAIN (hash=176)
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{"MODIFY", "ENDLOOP", "EXCEPTION", nullptr, nullptr, nullptr, nullptr, nullptr},
         {6, 7, 9, 0, 0, 0, 0, 0},
         {TokenType::MODIFY, TokenType::ENDLOOP, TokenType::EXCEPTION, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // MODIFY (hash=178), ENDLOOP (hash=178), EXCEPTION (hash=178)
        {{"EXIT", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {4, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::EXIT, TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // EXIT (hash=179)
        {{"EXISTS", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {6, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::EXISTS, TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // EXISTS (hash=180)
        {{"EXCEPT", "EXPORT", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {6, 6, 0, 0, 0, 0, 0, 0},
         {TokenType::EXCEPT, TokenType::EXPORT, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // EXCEPT (hash=181), EXPORT (hash=181)
        {{"EXTRACT", "VERBOSE", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {7, 7, 0, 0, 0, 0, 0, 0},
         {TokenType::EXTRACT, TokenType::VERBOSE, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // EXTRACT (hash=182), VERBOSE (hash=182)
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{"EACH_STMT", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {9, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::EACH_STMT, TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // EACH_STMT (hash=184)
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{"EACH_ROW", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {8, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::EACH_ROW, TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // EACH_ROW (hash=186)
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{"NUMERIC", "NTILE", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {7, 5, 0, 0, 0, 0, 0, 0},
         {TokenType::NUMERIC, TokenType::NTILE, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // NUMERIC (hash=188), NTILE (hash=188)
        {{"VACUUM", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {6, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::VACUUM, TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // VACUUM (hash=189)
        {{"NULLIF", "NOCYCLE", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {6, 7, 0, 0, 0, 0, 0, 0},
         {TokenType::NULLIF, TokenType::NOCYCLE, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // NULLIF (hash=190), NOCYCLE (hash=190)
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{"NTH_VALUE", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {9, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::NTH_VALUE, TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // NTH_VALUE (hash=192)
        {{"NVL", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {3, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::NVL, TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // NVL (hash=193)
        {{"NULL", "VECTOR", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {4, 6, 0, 0, 0, 0, 0, 0},
         {TokenType::NULL_KW, TokenType::VECTOR, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // NULL (hash=194), VECTOR (hash=194)
        {{"VALUES", "VARCHAR", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {6, 7, 0, 0, 0, 0, 0, 0},
         {TokenType::VALUES, TokenType::VARCHAR, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // VALUES (hash=195), VARCHAR (hash=195)
        {{"FALSE", "FORCE", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {5, 5, 0, 0, 0, 0, 0, 0},
         {TokenType::FALSE, TokenType::FORCE, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // FALSE (hash=196), FORCE (hash=196)
        {{"NATURAL", "VIEW", "VARIANT", nullptr, nullptr, nullptr, nullptr, nullptr},
         {7, 4, 7, 0, 0, 0, 0, 0},
         {TokenType::NATURAL, TokenType::VIEW, TokenType::VARIANT, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // NATURAL (hash=197), VIEW (hash=197), VARIANT (hash=197)
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{"FETCH", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {5, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::FETCH, TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // FETCH (hash=199)
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{"NOT", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {3, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::NOT, TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // NOT (hash=201)
        {{"FULL", "NULLS", "FOLLOWING", "FIRST_VALUE", "NEXT", nullptr, nullptr, nullptr},
         {4, 5, 9, 11, 4, 0, 0, 0},
         {TokenType::FULL, TokenType::NULLS, TokenType::FOLLOWING, TokenType::FIRST_VALUE,
          TokenType::NEXT, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // FULL (hash=202), NULLS (hash=202), FOLLOWING (hash=202),
                                   // FIRST_VALUE (hash=202), NEXT (hash=202)
        {{"FROM", "NO_WRITE_TO_BINLOG", "FINAL", nullptr, nullptr, nullptr, nullptr, nullptr},
         {4, 18, 5, 0, 0, 0, 0, 0},
         {TokenType::FROM, TokenType::NO_WRITE_TO_BINLOG, TokenType::FINAL, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // FROM (hash=203), NO_WRITE_TO_BINLOG (hash=203), FINAL
                                   // (hash=203)
        {{"VARBINARY", "NOWAIT", "NEW", nullptr, nullptr, nullptr, nullptr, nullptr},
         {9, 6, 3, 0, 0, 0, 0, 0},
         {TokenType::VARBINARY, TokenType::NOWAIT, TokenType::NEW, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // VARBINARY (hash=204), NOWAIT (hash=204), NEW (hash=204)
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{"FOREIGN", "FOR", "FLATTEN", nullptr, nullptr, nullptr, nullptr, nullptr},
         {7, 3, 7, 0, 0, 0, 0, 0},
         {TokenType::FOREIGN, TokenType::FOR, TokenType::FLATTEN, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // FOREIGN (hash=207), FOR (hash=207), FLATTEN (hash=207)
        {{"FUNCTION", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {8, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::FUNCTION, TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // FUNCTION (hash=208)
        {{"FLOOR", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {5, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::FLOOR, TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // FLOOR (hash=209)
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{"WHERE", "FIRST", "FLOAT", "WRITE", "WHILE", nullptr, nullptr, nullptr},
         {5, 5, 5, 5, 5, 0, 0, 0},
         {TokenType::WHERE, TokenType::FIRST, TokenType::FLOAT, TokenType::WRITE, TokenType::WHILE,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // WHERE (hash=211), FIRST (hash=211), FLOAT (hash=211), WRITE
                                   // (hash=211), WHILE (hash=211)
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{"WITH", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {4, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::WITH, TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // WITH (hash=213)
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{"WORK", "OLD", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {4, 3, 0, 0, 0, 0, 0, 0},
         {TokenType::WORK, TokenType::OLD, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // WORK (hash=216), OLD (hash=216)
        {{"OF", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {2, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::OF, TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // OF (hash=217)
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{"WHEN", "WHEN", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {4, 4, 0, 0, 0, 0, 0, 0},
         {TokenType::WHEN, TokenType::WHEN_KW, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // WHEN (hash=219), WHEN (hash=219)
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{"OPTIMIZE", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {8, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::OPTIMIZE, TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // OPTIMIZE (hash=222)
        {{"OVERWRITE", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {9, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::OVERWRITE, TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // OVERWRITE (hash=223)
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{"ON", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {2, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::ON, TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // ON (hash=225)
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{"OPEN", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {4, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::OPEN, TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // OPEN (hash=227)
        {{"ORDINAL", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {7, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::ORDINAL, TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // ORDINAL (hash=228)
        {{"OR", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {2, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::OR, TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // OR (hash=229)
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{"OVER", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {4, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::OVER, TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // OVER (hash=231)
        {{"ORDER", "OUTER", "OUT", nullptr, nullptr, nullptr, nullptr, nullptr},
         {5, 5, 3, 0, 0, 0, 0, 0},
         {TokenType::ORDER, TokenType::OUTER, TokenType::OUT, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // ORDER (hash=232), OUTER (hash=232), OUT (hash=232)
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{"GO", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {2, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::GO, TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // GO (hash=234)
        {{"OFFSET", "OPTIONS", "OBJECT", "OUTPUT", nullptr, nullptr, nullptr, nullptr},
         {6, 7, 6, 6, 0, 0, 0, 0},
         {TokenType::OFFSET, TokenType::OPTIONS, TokenType::OBJECT, TokenType::OUTPUT,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // OFFSET (hash=235), OPTIONS (hash=235), OBJECT (hash=235),
                                   // OUTPUT (hash=235)
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{"GROUP", "ONLY", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {5, 4, 0, 0, 0, 0, 0, 0},
         {TokenType::GROUP, TokenType::ONLY, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // GROUP (hash=238), ONLY (hash=238)
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{"GRANT", "ORDER_BY", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {5, 8, 0, 0, 0, 0, 0, 0},
         {TokenType::GRANT, TokenType::ORDER_BY, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // GRANT (hash=242), ORDER_BY (hash=242)
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{"PRAGMA", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {6, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::PRAGMA, TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // PRAGMA (hash=247)
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{"GENERATE_SERIES", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {15, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::GENERATE_SERIES, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}}, // GENERATE_SERIES (hash=251)
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
        {{"PREWHERE", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {8, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::PREWHERE, TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER}}, // PREWHERE (hash=253)
        {{"PROCEDURE", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {9, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::PROCEDURE_KW, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}}, // PROCEDURE (hash=254)
        {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
         {0, 0, 0, 0, 0, 0, 0, 0},
         {TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER, TokenType::IDENTIFIER,
          TokenType::IDENTIFIER, TokenType::IDENTIFIER}},
    };
};

} // namespace libglot::sql::lex
