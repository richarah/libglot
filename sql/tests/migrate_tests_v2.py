#!/usr/bin/env python3
"""
V2: Automated migration script to port libsqlglot tests to libglot-sql.
This version properly uses the libglot-sql API (SQLParser, SQLGenerator).
"""

import re
import sys
from pathlib import Path

def migrate_file(content: str) -> str:
    """Apply all transformations to migrate a test file."""

    # 1. Fix includes
    content = re.sub(r'#include <libsqlglot/transpiler\.h>',
                     '#include <libglot/sql/parser.h>\n#include <libglot/sql/generator.h>',
                     content)
    content = re.sub(r'#include <libsqlglot/([^>]+)>',
                     r'#include <libglot/sql/\1>',
                     content)

    # 2. Fix namespace
    content = content.replace('using namespace libsqlglot;', 'using namespace libglot::sql;')
    content = content.replace('libsqlglot::', 'libglot::sql::')

    # 3. Fix type names (Parser → SQLParser, etc.)
    content = re.sub(r'\bParser\b', 'SQLParser', content)
    content = re.sub(r'\bGenerator\b', 'SQLGenerator', content)
    content = re.sub(r'\bDialect::', 'SQLDialect::', content)

    # 4. Fix Transpiler::transpile() calls - most complex transformation
    # Pattern: Transpiler::transpile(sql, from_dialect, to_dialect)
    def replace_transpile(match):
        sql_arg = match.group(1).strip()
        from_dialect = match.group(2).strip() if match.group(2) else 'SQLDialect::ANSI'
        to_dialect = match.group(3).strip() if match.group(3) else from_dialect

        return f'''[&]() {{
        Arena arena;
        SQLParser parser(arena, {sql_arg});
        auto ast = parser.parse_top_level();
        SQLGenerator gen({to_dialect});
        return gen.generate(ast);
    }}()'''

    # Multi-arg version
    content = re.sub(
        r'Transpiler::transpile\(\s*([^,]+),\s*([^,]+),\s*([^,)]+)(?:,\s*(?:true|false))?\s*\)',
        replace_transpile,
        content
    )

    # Single-arg version (defaults to ANSI)
    content = re.sub(
        r'Transpiler::transpile\(\s*([^)]+)\s*\)',
        lambda m: f'''[&]() {{
        Arena arena;
        SQLParser parser(arena, {m.group(1).strip()});
        auto ast = parser.parse_top_level();
        SQLGenerator gen(SQLDialect::ANSI);
        return gen.generate(ast);
    }}()''',
        content
    )

    # 5. Fix Transpiler::parse() calls
    content = re.sub(
        r'Transpiler::parse\(\s*([^,]+),\s*([^)]+)\s*\)',
        lambda m: f'{m.group(1)}.create<SQLParser>({m.group(1)}, {m.group(2)}).parse_top_level()',
        content
    )

    # 6. Fix Transpiler::generate() calls
    def replace_generate(match):
        expr = match.group(1).strip()
        dialect = match.group(2).strip() if match.group(2) else 'SQLDialect::ANSI'
        return f'[&]() {{ SQLGenerator gen({dialect}); return gen.generate({expr}); }}()'

    content = re.sub(
        r'Transpiler::generate\(\s*([^,)]+)(?:,\s*([^)]+))?\s*\)',
        replace_generate,
        content
    )

    # 7. Comment out Optimizer calls (not yet implemented)
    lines = content.split('\n')
    result = []
    for line in lines:
        if 'Optimizer::' in line and not line.strip().startswith('//'):
            result.append('    // ' + line.lstrip() + '  // TODO: Optimizer not yet ported')
        else:
            result.append(line)
    content = '\n'.join(result)

    # 8. Fix DialectConfig references
    content = content.replace('DialectConfig::', 'SQLDialectConfig::')

    # 9. Fix SelectStmt, InsertStmt, etc. type names (they're the same in libglot-sql)
    # No changes needed - AST node names are the same

    return content

def main():
    script_dir = Path(__file__).parent
    libsqlglot_tests = script_dir.parent.parent / 'libsqlglot' / 'tests'
    dest_dir = script_dir

    if not libsqlglot_tests.exists():
        print(f"Error: {libsqlglot_tests} not found")
        sys.exit(1)

    # Get all test files
    test_files = sorted(libsqlglot_tests.glob('test_*.cpp'))

    print(f"Found {len(test_files)} test files in libsqlglot/tests\n")

    # Remove old migrated files
    print("Removing old migrated files...")
    for old_file in dest_dir.glob('test_*.cpp'):
        if old_file.name not in ['test_roundtrip.cpp', 'test_dml_statements.cpp',
                                   'test_advanced_sql.cpp', 'test_cte_windows_subqueries.cpp',
                                   'test_scalar_functions.cpp', 'test_parser.cpp',
                                   'test_end_to_end.cpp', 'test_tokenizer.cpp',
                                   'test_dialect_transpilation.cpp', 'test_transpiler.cpp']:
            old_file.unlink()
            print(f"  Removed {old_file.name}")

    print()

    # Migrate all test files
    for test_file in test_files:
        # Skip files we want to keep as-is
        if test_file.name in ['test_roundtrip.cpp', 'test_dml_statements.cpp']:
            print(f"Skipping {test_file.name} (manually curated)")
            continue

        print(f"Migrating {test_file.name}...")
        content = test_file.read_text()
        migrated = migrate_file(content)

        dest_path = dest_dir / test_file.name
        dest_path.write_text(migrated)
        print(f"  → {dest_path.name}")

    print(f"\nMigration complete! Migrated {len(test_files) - 2} files")
    print("\nNext steps:")
    print("1. Update sql/tests/CMakeLists.txt to build all test files")
    print("2. Run: cmake --build build")
    print("3. Fix remaining compilation errors")
    print("4. Run tests and fix failures")

if __name__ == '__main__':
    main()
