# libglot

Generic parser and transpiler framework using CRTP, with SQL parser implementation.

## What it does

Parses SQL across 26 dialects (PostgreSQL, MySQL, SQL Server, BigQuery, Snowflake, Oracle, DuckDB, SQLite, and others) into a common AST. Generates SQL back out in any supported dialect. Built on a generic parser framework that can be extended to other languages.

SQL parser supports SELECT, INSERT, UPDATE, DELETE, CREATE, DROP, ALTER, MERGE, and procedural SQL (IF/WHILE/FOR/procedures/functions/cursors/exceptions). 100 AST node types. JSON operators for PostgreSQL and Snowflake. Dialect-specific transformations (LIMIT→TOP for SQL Server, ILIKE→LOWER+LIKE for MySQL).

51 passing test suites covering core SQL, procedural SQL, dialect transpilation, JSON operations, GRANT/REVOKE, CTEs, window functions, and joins.

## Building

```
git clone https://github.com/richarah/libglot
cd libglot
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
```

Requires C++20 or later.

## Usage

```cpp
#include <libglot/sql/parser.h>
#include <libglot/sql/generator.h>
#include <libglot/util/arena.h>

libglot::Arena arena;
libglot::sql::SQLParser parser(arena, "SELECT * FROM users LIMIT 10");
auto ast = parser.parse_top_level();

libglot::sql::SQLGenerator gen(libglot::sql::SQLDialect::SQLServer);
std::string output = gen.generate(ast);  // SELECT TOP 10 * FROM [users]
```

## Project structure

```
core/           Generic parser framework (CRTP-based ParserBase, GeneratorBase)
sql/            SQL parser and generator implementation
libsqlglot/     Tokenizer (from Python sqlglot project)
```

## Current status

SQL parser handles standard SQL and procedural SQL. Dialect-specific transformations work for common cases (LIMIT/TOP, boolean literals, ILIKE polyfill). Optimizer not implemented. MIME parser defined but not complete.

## License

MIT
