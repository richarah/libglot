#!/usr/bin/env python3
"""Generate complete SQL AST nodes from libsqlglot's ExprType enum."""

import re

# Read libsqlglot's expression.h
with open('libsqlglot/include/libsqlglot/expression.h', 'r') as f:
    content = f.read()

# Extract ExprType enum
enum_match = re.search(r'enum class ExprType[^{]*{([^}]+)}', content, re.DOTALL)
if not enum_match:
    print("ERROR: Could not find ExprType enum")
    exit(1)

enum_content = enum_match.group(1)

# Parse enum values
entries = []
for line in enum_content.split('\n'):
    line = line.strip()
    if not line or line.startswith('//'):
        continue
    # Match: NAME, or NAME  // comment
    match = re.match(r'([A-Z_]+),?\s*(//.*)?', line)
    if match:
        name = match.group(1)
        if name != '':
            entries.append(name)

print(f"// Found {len(entries)} ExprType entries")
for e in entries[:10]:
    print(f"//   {e}")
print("// ...")
