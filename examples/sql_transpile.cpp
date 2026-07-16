// Transpile a SQL statement between dialects.
//
//   ./sql_transpile "SELECT * FROM users LIMIT 10" sqlserver
//
// Reads the statement from argv[1] (or stdin if omitted), parses it with the
// PostgreSQL-flavored default parser, and prints it re-generated for the
// requested target dialect.

#include <libglot/sql/generator.h>
#include <libglot/sql/parser.h>
#include <libglot/util/arena.h>

#include <iostream>
#include <map>
#include <string>
#include <string_view>

using libglot::sql::SQLDialect;

namespace {

SQLDialect dialect_from_name(std::string_view name) {
    static const std::map<std::string_view, SQLDialect> known = {
        {"ansi", SQLDialect::ANSI},           {"postgresql", SQLDialect::PostgreSQL},
        {"postgres", SQLDialect::PostgreSQL}, {"mysql", SQLDialect::MySQL},
        {"sqlite", SQLDialect::SQLite},       {"sqlserver", SQLDialect::SQLServer},
        {"tsql", SQLDialect::SQLServer},      {"oracle", SQLDialect::Oracle},
        {"snowflake", SQLDialect::Snowflake}, {"bigquery", SQLDialect::BigQuery},
        {"duckdb", SQLDialect::DuckDB},
    };
    auto it = known.find(name);
    if (it == known.end()) {
        throw std::runtime_error("unknown dialect: " + std::string(name));
    }
    return it->second;
}

} // namespace

int main(int argc, char** argv) {
    try {
        std::string sql;
        if (argc > 1) {
            sql = argv[1];
        } else {
            std::getline(std::cin, sql);
        }
        const SQLDialect target = argc > 2 ? dialect_from_name(argv[2]) : SQLDialect::PostgreSQL;

        libglot::Arena arena;
        libglot::sql::SQLParser parser(arena, sql);
        auto* ast = parser.parse_top_level();

        libglot::sql::SQLGenerator generator(target);
        std::cout << generator.generate(ast) << '\n';
        return 0;
    } catch (const libglot::ParseError& e) {
        std::cerr << "parse error: " << e.what() << '\n';
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << '\n';
        return 1;
    }
}
