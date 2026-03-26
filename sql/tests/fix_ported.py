#!/usr/bin/env python3
"""
fix_ported.py - Post-process ported test files to fix common issues

Fixes:
1. Lines like "std::string output = libglot::Arena arena;" → proper multi-line format
2. Duplicate variable declarations
3. Variable name conflicts in same scope
4. Transpiler::parse() calls that weren't replaced
"""

import re
import sys
from pathlib import Path


def fix_malformed_assignments(content: str) -> str:
    """
    Fix lines like:
      std::string output = libglot::Arena arena;

    Into:
      libglot::Arena arena;
      Parser parser(arena, sql);
      auto ast = parser.parse_top_level();
      Generator gen(Dialect::MySQL);
      std::string output = gen.generate(ast);
    """
    # Pattern: assignment followed immediately by arena declaration on same line
    pattern = r'(\s*)std::string\s+(\w+)\s*=\s*(libglot::Arena arena;[^\n]+)'

    def replace_match(m):
        indent = m.group(1)
        var_name = m.group(2)
        rest = m.group(3)

        # Extract the generator and generate parts
        # The rest should look like: "libglot::Arena arena;\n    Parser parser(arena, sql);\n    ..."
        lines = rest.split(';')

        # Build proper multi-line format
        result_lines = []
        for line in lines:
            line = line.strip()
            if line:
                if 'std::string' in line and '=' in line:
                    # This is the final assignment - use the var_name
                    line = re.sub(r'std::string\s+\w+\s*=', f'std::string {var_name} =', line)
                result_lines.append(f"{indent}{line};")

        return '\n'.join(result_lines)

    return re.sub(pattern, replace_match, content, flags=re.MULTILINE)


def fix_duplicate_declarations(content: str) -> str:
    """
    Fix duplicate variable declarations on same line:
      std::string output = ...; std::string output = ...;;

    Remove the first declaration.
    """
    pattern = r'std::string\s+(\w+)\s*=\s*([^;]+);\s*std::string\s+\1\s*='
    content = re.sub(pattern, r'std::string \1 =', content)

    # Fix double semicolons
    content = content.replace(';;', ';')

    return content


def fix_variable_conflicts(content: str) -> str:
    """
    Fix variable name conflicts in same scope by adding numeric suffixes.

    Detects multiple declarations of:
    - Arena arena
    - Parser parser
    - auto ast
    - Generator gen

    Within the same TEST_CASE block.
    """
    # Split content into TEST_CASE blocks
    test_cases = re.split(r'(TEST_CASE\([^)]+\)[^{]*\{)', content)

    fixed_parts = []
    for i, part in enumerate(test_cases):
        if i % 2 == 0:
            # Not a TEST_CASE header
            fixed_parts.append(part)
        else:
            # TEST_CASE header
            fixed_parts.append(part)
            # Next part is the body
            if i + 1 < len(test_cases):
                body = test_cases[i + 1]

                # Count occurrences of key variables
                arena_count = len(re.findall(r'\bArena\s+arena\b', body))
                parser_count = len(re.findall(r'\bParser\s+parser\b', body))
                ast_count = len(re.findall(r'\bauto\s+ast\b', body))
                gen_count = len(re.findall(r'\bGenerator\s+gen\b', body))

                # If duplicates found, add numeric suffixes
                if arena_count > 1:
                    counter = [1]
                    def replace_arena(m):
                        if counter[0] == 1:
                            counter[0] += 1
                            return m.group(0)
                        result = m.group(0).replace('arena', f'arena{counter[0]}')
                        # Also need to update references to this arena in same statement group
                        counter[0] += 1
                        return result
                    body = re.sub(r'\bArena\s+arena\b', replace_arena, body)

                if parser_count > 1:
                    counter = [1]
                    def replace_parser(m):
                        if counter[0] == 1:
                            counter[0] += 1
                            return m.group(0)
                        result = m.group(0).replace('parser', f'parser{counter[0]}')
                        counter[0] += 1
                        return result
                    body = re.sub(r'\bParser\s+parser\b', replace_parser, body)

                if ast_count > 1:
                    counter = [1]
                    def replace_ast(m):
                        if counter[0] == 1:
                            counter[0] += 1
                            return m.group(0)
                        result = m.group(0).replace('ast', f'ast{counter[0]}')
                        counter[0] += 1
                        return result
                    body = re.sub(r'\bauto\s+ast\b', replace_ast, body)

                if gen_count > 1:
                    counter = [1]
                    def replace_gen(m):
                        if counter[0] == 1:
                            counter[0] += 1
                            return m.group(0)
                        result = m.group(0).replace('gen', f'gen{counter[0]}')
                        counter[0] += 1
                        return result
                    body = re.sub(r'\bGenerator\s+gen\b', replace_gen, body)

                fixed_parts.append(body)

    return ''.join(fixed_parts)


def fix_transpiler_parse(content: str) -> str:
    """
    Fix remaining Transpiler::parse() calls.
    Replace with: Parser parser(arena, sql); auto ast = parser.parse_top_level();
    """
    pattern = r'auto\s+(\w+)\s*=\s*Transpiler::parse\s*\(\s*(\w+)\s*,\s*([^)]+)\s*\);'

    def replace_match(m):
        var_name = m.group(1)
        arena_var = m.group(2)
        sql_var = m.group(3)
        return f'Parser parser({arena_var}, {sql_var});\n    auto {var_name} = parser.parse_top_level();'

    return re.sub(pattern, replace_match, content, flags=re.MULTILINE)


def main():
    if len(sys.argv) != 3:
        print("Usage: fix_ported.py <input_file> <output_file>")
        sys.exit(1)

    input_path = Path(sys.argv[1])
    output_path = Path(sys.argv[2])

    with open(input_path, 'r') as f:
        content = f.read()

    print(f"Fixing {input_path}...")

    # Apply fixes in order
    content = fix_malformed_assignments(content)
    content = fix_duplicate_declarations(content)
    content = fix_transpiler_parse(content)
    content = fix_variable_conflicts(content)

    with open(output_path, 'w') as f:
        f.write(content)

    print(f"✓ Fixed file written to {output_path}")


if __name__ == "__main__":
    main()
