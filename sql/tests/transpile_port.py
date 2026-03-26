#!/usr/bin/env python3
"""
transpile_port.py - Port libsqlglot tests to libglot-sql

Handles complex multi-line API translations that sed can't do:
1. Transpiler::transpile(sql, from, to) → parse + generate pattern
2. Transpiler::parse() + Transpiler::generate() → SQLParser + SQLGenerator
3. Proper arena allocation and variable scoping
"""

import re
import sys
from pathlib import Path
from typing import List, Tuple


def find_transpile_calls(content: str) -> List[Tuple[int, int, str, str, str, str]]:
    """
    Find all Transpiler::transpile() calls and extract arguments.

    Returns list of (start_pos, end_pos, sql_var, from_dialect, to_dialect, full_match)
    """
    # Pattern: Transpiler::transpile(sql_expr, Dialect::From, Dialect::To)
    # Handles newlines, whitespace, and optional 4th parameter (optimize flag)
    pattern = r'Transpiler::transpile\s*\(\s*([^,]+?)\s*,\s*Dialect::(\w+)\s*,\s*Dialect::(\w+)\s*(?:,\s*[^)]+)?\s*\)'

    matches = []
    for m in re.finditer(pattern, content, re.MULTILINE | re.DOTALL):
        sql_var = m.group(1).strip()
        from_dialect = m.group(2)
        to_dialect = m.group(3)
        matches.append((m.start(), m.end(), sql_var, from_dialect, to_dialect, m.group(0)))

    return matches


def find_transpile_single_arg(content: str) -> List[Tuple[int, int, str, str]]:
    """
    Find Transpiler::transpile(sql) calls with no dialect specified.

    Returns list of (start_pos, end_pos, sql_var, full_match)
    """
    # Pattern: Transpiler::transpile(sql_expr) - no dialect
    pattern = r'Transpiler::transpile\s*\(\s*([^,)]+?)\s*\)'

    matches = []
    for m in re.finditer(pattern, content, re.MULTILINE):
        sql_var = m.group(1).strip()
        # Skip if this is actually a 3-arg version (avoid false positives)
        if 'Dialect::' not in m.group(0):
            matches.append((m.start(), m.end(), sql_var, m.group(0)))

    return matches


def find_transpiler_parse(content: str) -> List[Tuple[int, int, str, str, str]]:
    """
    Find Transpiler::parse() calls.

    Returns list of (start_pos, end_pos, arena_var, sql_var, full_match)
    """
    pattern = r'Transpiler::parse\s*\(\s*(\w+)\s*,\s*([^)]+?)\s*\)'

    matches = []
    for m in re.finditer(pattern, content, re.MULTILINE):
        arena_var = m.group(1).strip()
        sql_var = m.group(2).strip()
        matches.append((m.start(), m.end(), arena_var, sql_var, m.group(0)))

    return matches


def find_transpiler_generate(content: str) -> List[Tuple[int, int, str, str]]:
    """
    Find Transpiler::generate() calls.

    Returns list of (start_pos, end_pos, expr_var, full_match)
    """
    # Pattern: Transpiler::generate(expr) or Transpiler::generate(expr, dialect)
    pattern = r'Transpiler::generate\s*\(\s*([^,)]+?)(?:\s*,\s*Dialect::(\w+))?\s*\)'

    matches = []
    for m in re.finditer(pattern, content, re.MULTILINE):
        expr_var = m.group(1).strip()
        dialect = m.group(2) if m.group(2) else "ANSI"
        matches.append((m.start(), m.end(), expr_var, dialect, m.group(0)))

    return matches


def find_generator_generate(content: str) -> List[Tuple[int, int, str, str, str]]:
    """
    Find Generator::generate(stmt, Dialect::X) calls.

    Returns list of (start_pos, end_pos, stmt_var, dialect, full_match)
    """
    pattern = r'Generator::generate\s*\(\s*(\w+)\s*,\s*Dialect::(\w+)\s*\)'

    matches = []
    for m in re.finditer(pattern, content, re.MULTILINE):
        stmt_var = m.group(1).strip()
        dialect = m.group(2)
        matches.append((m.start(), m.end(), stmt_var, dialect, m.group(0)))

    return matches


def generate_parse_generate_code(sql_var: str, to_dialect: str, indent: str = "    ", var_name: str = "output") -> str:
    """
    Generate the parse + generate replacement code.

    Args:
        sql_var: Variable containing SQL string
        to_dialect: Target dialect (e.g., "MySQL")
        indent: Indentation for generated code
        var_name: Name of output variable

    Returns:
        Multi-line string with parse + generate code
    """
    lines = [
        f"libglot::Arena arena;",
        f"Parser parser(arena, {sql_var});",
        f"auto ast = parser.parse_top_level();",
        f"Generator gen(Dialect::{to_dialect});",
        f"std::string {var_name} = gen.generate(ast);"
    ]
    return '\n'.join(indent + line for line in lines)


def port_file(input_path: Path, output_path: Path) -> Tuple[int, int]:
    """
    Port a single test file from libsqlglot to libglot-sql.

    Returns: (num_transpile_replacements, num_other_replacements)
    """
    with open(input_path, 'r', encoding='utf-8') as f:
        content = f.read()

    original_content = content
    transpile_count = 0
    other_count = 0

    # Find all matches (sorted by position, reverse order for replacement)
    transpile_3arg = find_transpile_calls(content)
    transpile_1arg = find_transpile_single_arg(content)
    generator_calls = find_generator_generate(content)

    # Combine and sort by position (reverse to replace from end to start)
    all_replacements = []

    # Process 3-arg Transpiler::transpile() calls
    for start, end, sql_var, from_d, to_d, match in transpile_3arg:
        # Determine indentation from the line
        line_start = content.rfind('\n', 0, start) + 1
        line_content = content[line_start:start]
        indent = re.match(r'^(\s*)', line_content).group(1)

        # Check if this is an assignment (output = Transpiler::transpile(...))
        before = content[max(0, start-100):start]
        var_name = "output"  # default
        if '=' in before.split('\n')[-1]:
            # It's an assignment - find the variable name
            assignment_match = re.search(r'(\w+)\s*=\s*$', before)
            if assignment_match:
                var_name = assignment_match.group(1)

        replacement = generate_parse_generate_code(sql_var, to_d, indent, var_name)
        all_replacements.append((start, end, replacement))
        transpile_count += 1

    # Process 1-arg Transpiler::transpile() calls (use default ANSI dialect)
    for start, end, sql_var, match in transpile_1arg:
        line_start = content.rfind('\n', 0, start) + 1
        line_content = content[line_start:start]
        indent = re.match(r'^(\s*)', line_content).group(1)

        before = content[max(0, start-100):start]
        var_name = "output"  # default
        if '=' in before.split('\n')[-1]:
            assignment_match = re.search(r'(\w+)\s*=\s*$', before)
            if assignment_match:
                var_name = assignment_match.group(1)

        replacement = generate_parse_generate_code(sql_var, "ANSI", indent, var_name)
        all_replacements.append((start, end, replacement))
        transpile_count += 1

    # Process Generator::generate(stmt, Dialect::X) → Generator + generate
    for start, end, stmt_var, dialect, match in generator_calls:
        line_start = content.rfind('\n', 0, start) + 1
        line_content = content[line_start:start]
        indent = re.match(r'^(\s*)', line_content).group(1)

        # Check if this is part of an assignment
        before = content[max(0, start-100):start]
        var_name = "output"
        if '=' in before.split('\n')[-1]:
            # Find variable name
            assignment_match = re.search(r'(\w+)\s*=\s*$', before)
            if assignment_match:
                var_name = assignment_match.group(1)
                replacement = f"{indent}Generator gen(Dialect::{dialect});\n{indent}std::string {var_name} = gen.generate({stmt_var});"
            else:
                replacement = f"{indent}Generator gen(Dialect::{dialect});\n{indent}gen.generate({stmt_var});"
        else:
            replacement = f"{indent}Generator gen(Dialect::{dialect});\n{indent}gen.generate({stmt_var});"

        all_replacements.append((start, end, replacement))
        other_count += 1

    # Apply replacements in reverse order (end to start)
    all_replacements.sort(key=lambda x: x[0], reverse=True)

    for start, end, replacement in all_replacements:
        content = content[:start] + replacement + content[end:]

    # Write output
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write(content)

    return transpile_count, other_count


def main():
    if len(sys.argv) != 3:
        print("Usage: transpile_port.py <input_file> <output_file>")
        print("Example: transpile_port.py test_dialect_transpilation.cpp test_dialect_transpilation_ported.cpp")
        sys.exit(1)

    input_path = Path(sys.argv[1])
    output_path = Path(sys.argv[2])

    if not input_path.exists():
        print(f"Error: Input file not found: {input_path}")
        sys.exit(1)

    print(f"Porting {input_path} → {output_path}")

    transpile_count, other_count = port_file(input_path, output_path)

    print(f"✓ Replaced {transpile_count} Transpiler::transpile() calls")
    print(f"✓ Replaced {other_count} Generator::generate() calls")
    print(f"✓ Output written to {output_path}")
    print()
    print("Next step: Run port_tests.sed for mechanical replacements:")
    print(f"  sed -f port_tests.sed {output_path} > temp && mv temp {output_path}")


if __name__ == "__main__":
    main()
