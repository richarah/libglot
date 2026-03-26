# Sed script to port libsqlglot tests to libglot
# Usage: sed -f port_tests.sed input.cpp > output.cpp

# Include paths - libglot
s|#include <libsqlglot/parser\.h>|#include <libglot/sql/parser.h>|g
s|#include <libsqlglot/generator\.h>|#include <libglot/sql/generator.h>|g
s|#include <libsqlglot/ast_nodes\.h>|#include <libglot/sql/ast_nodes.h>|g
s|#include <libsqlglot/arena\.h>|#include <libglot/util/arena.h>|g
s|#include <libsqlglot/dialect_traits\.h>|#include <libglot/sql/dialect_traits.h>|g
s|#include <libsqlglot/dialects\.h>|#include <libglot/sql/dialect_traits.h>|g
s|#include <libsqlglot/transpiler\.h>|#include <libglot/sql/parser.h>\n#include <libglot/sql/generator.h>\n#include <libglot/util/arena.h>|g

# Include paths - keep libsqlglot tokenizer for now
s|#include <libsqlglot/tokenizer\.h>|#include <libsqlglot/tokenizer.h>|g
s|#include <libsqlglot/tokens\.h>|#include <libsqlglot/tokens.h>|g

# Namespace declarations
s|using namespace libsqlglot;|using namespace libglot::sql;|g

# Namespace prefixes in code
s|libsqlglot::Arena|libglot::Arena|g
s|libsqlglot::ExprType::|SQLNodeKind::|g
s|libsqlglot::NodeType::|SQLNodeKind::|g
s|ExprType::|SQLNodeKind::|g
s|NodeType::|SQLNodeKind::|g

# Parser and Generator classes (avoid double-replacement)
s|\bParser\b|SQLParser|g
s|\bGenerator\b|SQLGenerator|g

# Arena API
s|\.alloc<|.create<|g
s|->alloc<|->create<|g

# AST field renames
s|->window_spec|->over|g
s|\.window_spec|.over|g

# Dialect API
s|DialectConfig::get_features|SQLDialectTraits::get_features|g
s|DialectConfig::get_name|SQLDialectTraits::name|g
s|DialectConfig::|SQLDialectTraits::|g
s|DialectFeatures::|SQLFeatures::|g
s|Dialect::|SQLDialect::|g

# Token types (keep libsqlglot:: prefix for these)
# These are intentionally not changed as they still use libsqlglot tokenizer

# Fix any double namespace issues
s|libglot::sql::sql::|libglot::sql::|g
s|SQLNodeKind::NodeKind::|SQLNodeKind::|g
s|SQLDialect::SQLDialect::|SQLDialect::|g
