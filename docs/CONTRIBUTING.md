# Contributing to libglot

**Welcome!** We're excited that you're interested in contributing to libglot. This guide will help you add new parser domains, follow code style conventions, and submit high-quality PRs.

---

## Table of Contents

1. [Project Philosophy](#project-philosophy)
2. [Adding a New Parser Domain](#adding-a-new-parser-domain)
3. [Code Style Guidelines](#code-style-guidelines)
4. [Testing Requirements](#testing-requirements)
5. [Build System Integration](#build-system-integration)
6. [Performance Validation](#performance-validation)
7. [Documentation Requirements](#documentation-requirements)
8. [PR Submission Process](#pr-submission-process)
9. [FAQ](#faq)

---

## Project Philosophy

libglot is built on three core principles:

### 1. Zero-Cost Abstraction

Every abstraction should compile down to the same code you'd write by hand. We achieve this via:

- **CRTP** (Curiously Recurring Template Pattern) instead of virtual dispatch
- **Constexpr/consteval** for compile-time configuration
- **Perfect hashing** for keyword lookup (O(1), branchless)
- **Arena allocation** for AST nodes (no per-node overhead)

**Verification**: Run `nm --demangle <binary> | grep "vtable"` — should return nothing on hot paths.

### 2. C++26 First

We leverage modern C++ features aggressively:

- **Concepts** instead of SFINAE (better error messages, clearer interfaces)
- **Constexpr algorithms** (perfect hash tables computed at compile time)
- **std::expected** for error handling (when available)
- **Ranges** for iterator chains

**Compiler requirements**: GCC 14.2+ or Clang 18+ with `-std=c++2c`.

### 3. Performance as a Feature

Parser performance isn't just a bonus — it's a core feature. We aim for:

- **Sub-10µs parse times** for typical inputs
- **10-50 million lines/sec** for log parsers
- **Zero allocations** on hot paths (arena bump allocator)
- **Single-pass parsing** (no multi-pass analysis unless necessary)

**Validation**: All new parsers must include benchmarks vs. reference implementations.

---

## Adding a New Parser Domain

Want to add `libglot-foo` for parsing format "foo"? Follow these steps:

### Step 1: Create Directory Structure

```bash
mkdir -p foo/include/libglot/foo
mkdir -p foo/tests
mkdir -p foo/benchmarks
touch foo/CMakeLists.txt
```

### Step 2: Define TokenSpec

Create `foo/include/libglot/foo/token_spec.h`:

```cpp
#pragma once

#include <libglot/lex/token_spec_concept.h>
#include <string_view>

namespace libglot::foo {

enum class TokenKind : uint16_t {
    // Structural
    END_OF_FILE,
    NEWLINE,
    WHITESPACE,

    // Literals
    STRING,
    NUMBER,

    // Keywords
    KEYWORD_BEGIN,  // First keyword
    FOO,
    BAR,
    BAZ,
    KEYWORD_END,    // Last keyword

    // Operators
    EQUALS,
    PLUS,
    MINUS,

    // Special
    IDENTIFIER,
    UNKNOWN
};

class FooTokenSpec {
public:
    using TokenKind = foo::TokenKind;

    // Character classification
    static constexpr bool is_identifier_start(char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
    }

    static constexpr bool is_identifier_continue(char c) {
        return is_identifier_start(c) || (c >= '0' && c <= '9');
    }

    static constexpr bool is_whitespace(char c) {
        return c == ' ' || c == '\t' || c == '\r';
    }

    static constexpr bool is_digit(char c) {
        return c >= '0' && c <= '9';
    }

    // Keyword lookup (perfect hash)
    static constexpr TokenKind lookup_keyword(std::string_view text) {
        // Simple perfect hash: (first * 31 + last + length) & 0xFF
        if (text.empty()) return TokenKind::IDENTIFIER;

        size_t hash = (text[0] * 31 + text.back() + text.size()) & 0xFF;

        // Keyword table (compile-time generated)
        switch (hash) {
            case 0xA1: return (text == "foo") ? TokenKind::FOO : TokenKind::IDENTIFIER;
            case 0xB2: return (text == "bar") ? TokenKind::BAR : TokenKind::IDENTIFIER;
            case 0xC3: return (text == "baz") ? TokenKind::BAZ : TokenKind::IDENTIFIER;
            default: return TokenKind::IDENTIFIER;
        }
    }

    // Token kind queries
    static constexpr bool is_keyword(TokenKind kind) {
        return kind > TokenKind::KEYWORD_BEGIN && kind < TokenKind::KEYWORD_END;
    }

    static constexpr bool is_eof(TokenKind kind) {
        return kind == TokenKind::END_OF_FILE;
    }

    // String representation (for debugging)
    static constexpr std::string_view token_kind_to_string(TokenKind kind) {
        switch (kind) {
            case TokenKind::END_OF_FILE: return "END_OF_FILE";
            case TokenKind::NEWLINE: return "NEWLINE";
            case TokenKind::FOO: return "FOO";
            case TokenKind::BAR: return "BAR";
            case TokenKind::BAZ: return "BAZ";
            case TokenKind::IDENTIFIER: return "IDENTIFIER";
            default: return "UNKNOWN";
        }
    }
};

// Verify concept compliance at compile time
static_assert(libglot::TokenSpec<FooTokenSpec>, "FooTokenSpec must satisfy TokenSpec concept");

} // namespace libglot::foo
```

**Key points**:
- Use `constexpr` everywhere possible (keyword lookup happens at compile time)
- Implement perfect hash for keywords (no `std::unordered_map` on hot path)
- Include `static_assert` to verify concept compliance

### Step 3: Define AST Nodes

Create `foo/include/libglot/foo/ast_nodes.h`:

```cpp
#pragma once

#include <libglot/ast/ast_node_concept.h>
#include <libglot/ast/source_location.h>
#include <libglot/util/arena.h>
#include <vector>
#include <string_view>

namespace libglot::foo {

// Base node for all Foo AST nodes
struct FooNode {
    SourceLocation location;

    virtual ~FooNode() = default;  // OK for base class (not on hot path)

    // Type-safe downcasting (RTTI-free)
    enum class Kind {
        FooStmt,
        BarExpr,
        BazExpr,
        Identifier
    };

    Kind kind;

protected:
    explicit FooNode(Kind k) : kind(k) {}
};

// Verify concept compliance
static_assert(libglot::AstNode<FooNode>, "FooNode must satisfy AstNode concept");

// Statement nodes
struct FooStmt : FooNode {
    std::string_view name;
    std::vector<FooNode*> children;

    FooStmt() : FooNode(Kind::FooStmt) {}
};

// Expression nodes
struct BarExpr : FooNode {
    FooNode* left;
    FooNode* right;

    BarExpr() : FooNode(Kind::BarExpr) {}
};

struct BazExpr : FooNode {
    int value;

    BazExpr() : FooNode(Kind::BazExpr) {}
};

struct Identifier : FooNode {
    std::string_view name;

    Identifier() : FooNode(Kind::Identifier) {}
};

} // namespace libglot::foo
```

**Key points**:
- Inherit from base node type with `SourceLocation`
- Use `std::string_view` for strings (backed by interned strings or arena)
- Use `std::vector` for child lists (allocated via arena)
- Implement type-safe downcasting via `Kind` enum (no RTTI overhead)

### Step 4: Implement Parser

Create `foo/include/libglot/foo/parser.h`:

```cpp
#pragma once

#include <libglot/parse/parser_base.h>
#include <libglot/lex/lexer.h>
#include "token_spec.h"
#include "ast_nodes.h"

namespace libglot::foo {

class FooParser : public ParserBase<FooTokenSpec, FooParser> {
public:
    using Base = ParserBase<FooTokenSpec, FooParser>;
    using TokenKind = FooTokenSpec::TokenKind;

    FooParser(Arena& arena, std::string_view source)
        : Base(arena, source) {}

    // Top-level entry point
    FooNode* parse_top_level() {
        return parse_statement();
    }

    // CRTP callbacks (called by ParserBase)
    FooNode* parse_prefix() {
        switch (current_token().kind) {
            case TokenKind::FOO:
                return parse_foo_stmt();
            case TokenKind::BAR:
                return parse_bar_expr();
            case TokenKind::IDENTIFIER:
                return parse_identifier();
            default:
                report_error("Unexpected token");
                return nullptr;
        }
    }

    FooNode* parse_infix(FooNode* left, int precedence) {
        // Handle binary operators
        if (current_token().kind == TokenKind::PLUS) {
            advance();
            auto* expr = arena_.create<BarExpr>();
            expr->left = left;
            expr->right = parse_expression(precedence);
            return expr;
        }
        return left;
    }

    int get_precedence(TokenKind kind) const {
        switch (kind) {
            case TokenKind::PLUS: return 10;
            case TokenKind::MINUS: return 10;
            default: return -1;
        }
    }

private:
    FooStmt* parse_foo_stmt() {
        expect(TokenKind::FOO);
        auto* stmt = arena_.create<FooStmt>();
        stmt->name = current_token().text;
        advance();
        return stmt;
    }

    BarExpr* parse_bar_expr() {
        expect(TokenKind::BAR);
        auto* expr = arena_.create<BarExpr>();
        // ... parse logic
        return expr;
    }

    Identifier* parse_identifier() {
        auto* ident = arena_.create<Identifier>();
        ident->name = current_token().text;
        advance();
        return ident;
    }

    FooNode* parse_statement() {
        return parse_prefix();
    }

    FooNode* parse_expression(int min_precedence = 0) {
        auto* left = parse_prefix();

        while (get_precedence(current_token().kind) >= min_precedence) {
            int prec = get_precedence(current_token().kind);
            left = parse_infix(left, prec);
        }

        return left;
    }
};

} // namespace libglot::foo
```

**Key points**:
- Derive from `ParserBase` via CRTP: `ParserBase<Spec, Derived>`
- Implement `parse_prefix()`, `parse_infix()`, `get_precedence()` for Pratt parsing
- Use `arena_.create<T>()` for AST node allocation (no `new`/`delete`)
- Use `expect()`, `advance()`, `current_token()` from base class
- All parsing functions should be `private` except `parse_top_level()`

### Step 5: Implement Generator

Create `foo/include/libglot/foo/generator.h`:

```cpp
#pragma once

#include <libglot/gen/generator_base.h>
#include "ast_nodes.h"
#include <string>
#include <sstream>

namespace libglot::foo {

enum class FooDialect {
    Standard,
    Variant1,
    Variant2
};

class FooGenerator : public GeneratorBase<FooNode, FooGenerator> {
public:
    explicit FooGenerator(FooDialect dialect = FooDialect::Standard)
        : dialect_(dialect) {}

    std::string generate(FooNode* node) {
        if (!node) return "";

        output_.clear();
        visit_node(node);
        return output_.str();
    }

private:
    FooDialect dialect_;
    std::ostringstream output_;

    // CRTP callbacks
    void visit_node(FooNode* node) {
        switch (node->kind) {
            case FooNode::Kind::FooStmt:
                visit_foo_stmt(static_cast<FooStmt*>(node));
                break;
            case FooNode::Kind::BarExpr:
                visit_bar_expr(static_cast<BarExpr*>(node));
                break;
            case FooNode::Kind::Identifier:
                visit_identifier(static_cast<Identifier*>(node));
                break;
            default:
                break;
        }
    }

    void visit_foo_stmt(FooStmt* stmt) {
        output_ << "FOO " << stmt->name;

        // Dialect-specific formatting
        if (dialect_ == FooDialect::Variant1) {
            output_ << ";";  // Variant1 requires semicolons
        }

        for (auto* child : stmt->children) {
            output_ << " ";
            visit_node(child);
        }
    }

    void visit_bar_expr(BarExpr* expr) {
        visit_node(expr->left);
        output_ << " + ";
        visit_node(expr->right);
    }

    void visit_identifier(Identifier* ident) {
        output_ << ident->name;
    }
};

} // namespace libglot::foo
```

**Key points**:
- Derive from `GeneratorBase` via CRTP
- Use visitor pattern for AST traversal
- Support multiple dialects via enum + conditional logic
- Use `std::ostringstream` for output buffering
- Implement `generate()` as public entry point

### Step 6: Write Tests

Create `foo/tests/test_foo.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include <libglot/foo/parser.h>
#include <libglot/foo/generator.h>
#include <libglot/util/arena.h>

using namespace libglot::foo;

TEST_CASE("FooParser - basic statement", "[foo][parser]") {
    libglot::Arena arena;
    FooParser parser(arena, "foo hello");

    auto* stmt = parser.parse_top_level();
    REQUIRE(stmt != nullptr);
    REQUIRE(stmt->kind == FooNode::Kind::FooStmt);

    auto* foo_stmt = static_cast<FooStmt*>(stmt);
    REQUIRE(foo_stmt->name == "hello");
}

TEST_CASE("FooParser - expression", "[foo][parser]") {
    libglot::Arena arena;
    FooParser parser(arena, "bar + baz");

    auto* expr = parser.parse_top_level();
    REQUIRE(expr != nullptr);
    REQUIRE(expr->kind == FooNode::Kind::BarExpr);
}

TEST_CASE("FooGenerator - roundtrip", "[foo][generator]") {
    libglot::Arena arena;

    // Parse
    FooParser parser(arena, "foo test");
    auto* stmt = parser.parse_top_level();
    REQUIRE(stmt != nullptr);

    // Generate (Standard dialect)
    FooGenerator gen(FooDialect::Standard);
    std::string output = gen.generate(stmt);
    REQUIRE(output == "FOO test");

    // Roundtrip
    FooParser parser2(arena, output);
    auto* stmt2 = parser2.parse_top_level();
    REQUIRE(stmt2 != nullptr);

    std::string output2 = gen.generate(stmt2);
    REQUIRE(output == output2);
}

TEST_CASE("FooGenerator - dialect variations", "[foo][generator][dialects]") {
    libglot::Arena arena;
    FooParser parser(arena, "foo test");
    auto* stmt = parser.parse_top_level();

    // Standard dialect
    FooGenerator gen_std(FooDialect::Standard);
    REQUIRE(gen_std.generate(stmt) == "FOO test");

    // Variant1 dialect (adds semicolons)
    FooGenerator gen_v1(FooDialect::Variant1);
    REQUIRE(gen_v1.generate(stmt) == "FOO test;");
}
```

**Key points**:
- Use Catch2 `TEST_CASE` macros
- Tag tests with `[domain][component]` for filtering
- Test parsing, generation, and roundtrip separately
- Test all supported dialects
- Verify AST structure with `REQUIRE` assertions

### Step 7: Write Benchmarks

Create `foo/benchmarks/benchmark_foo.cpp`:

```cpp
#include <benchmark/benchmark.h>
#include <libglot/foo/parser.h>
#include <libglot/foo/generator.h>
#include <libglot/util/arena.h>

using namespace libglot::foo;

static void BM_FooParser_Simple(benchmark::State& state) {
    std::string_view input = "foo hello bar baz";

    for (auto _ : state) {
        libglot::Arena arena;
        FooParser parser(arena, input);
        auto* stmt = parser.parse_top_level();
        benchmark::DoNotOptimize(stmt);
    }

    state.SetBytesProcessed(state.iterations() * input.size());
}
BENCHMARK(BM_FooParser_Simple);

static void BM_FooRoundtrip(benchmark::State& state) {
    std::string_view input = "foo hello bar baz";

    for (auto _ : state) {
        libglot::Arena arena;
        FooParser parser(arena, input);
        auto* stmt = parser.parse_top_level();

        FooGenerator gen;
        std::string output = gen.generate(stmt);
        benchmark::DoNotOptimize(output);
    }

    state.SetBytesProcessed(state.iterations() * input.size());
}
BENCHMARK(BM_FooRoundtrip);

BENCHMARK_MAIN();
```

**Key points**:
- Use Google Benchmark library
- Measure both parsing and roundtrip performance
- Use `benchmark::DoNotOptimize()` to prevent dead code elimination
- Report throughput with `SetBytesProcessed()`

### Step 8: Integrate with Build System

Update `foo/CMakeLists.txt`:

```cmake
# libglot-foo: Foo format parser/generator

# Header-only library
add_library(libglot-foo INTERFACE)
target_include_directories(libglot-foo INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(libglot-foo INTERFACE libglot-core)

# Tests
if(LIBGLOT_BUILD_TESTS)
    add_executable(test_foo tests/test_foo.cpp)
    target_link_libraries(test_foo PRIVATE libglot-foo Catch2::Catch2WithMain)
    add_test(NAME FooTests COMMAND test_foo)
endif()

# Benchmarks
if(LIBGLOT_BUILD_BENCHMARKS)
    add_executable(benchmark_foo benchmarks/benchmark_foo.cpp)
    target_link_libraries(benchmark_foo PRIVATE libglot-foo benchmark::benchmark)
endif()
```

Update root `CMakeLists.txt`:

```cmake
# Add new subdirectory
add_subdirectory(foo)
```

### Step 9: Document Your Parser

Create `foo/README.md`:

```markdown
# libglot-foo: Foo Format Parser

High-performance parser for Foo format files.

## Usage

```cpp
#include <libglot/foo/parser.h>
#include <libglot/foo/generator.h>

libglot::Arena arena;
libglot::foo::FooParser parser(arena, "foo hello bar baz");
auto* stmt = parser.parse_top_level();

libglot::foo::FooGenerator gen(libglot::foo::FooDialect::Standard);
std::string output = gen.generate(stmt);
```

## Performance

| Input Size | Parse Time | Throughput |
|------------|------------|------------|
| 100 bytes  | 1.2µs      | 83 MB/s    |
| 1 KB       | 12µs       | 83 MB/s    |
| 10 KB      | 120µs      | 83 MB/s    |

## Supported Dialects

- **Standard**: RFC XXXX compliant
- **Variant1**: Custom extension with semicolons
- **Variant2**: Legacy format

## Test Coverage

- 25 test cases
- 100% statement coverage
- Fuzz testing: 1M inputs, 0 crashes
```

---

## Code Style Guidelines

### C++26 Features

**Use modern C++ features consistently**:

```cpp
// ✅ GOOD: Use concepts instead of SFINAE
template<TokenSpec Spec>
class Lexer { /* ... */ };

// ❌ BAD: SFINAE is verbose and less clear
template<typename Spec, typename = std::enable_if_t</* ... */>>
class Lexer { /* ... */ };

// ✅ GOOD: Use constexpr for compile-time computation
constexpr TokenKind lookup(std::string_view text) {
    return keyword_table[(text[0] * 31) & 0xFF];
}

// ❌ BAD: Runtime hash map lookup
TokenKind lookup(std::string_view text) {
    static std::unordered_map<std::string_view, TokenKind> keywords = { /* ... */ };
    return keywords[text];
}
```

### Naming Conventions

- **Classes/Structs**: `PascalCase` (e.g., `SQLParser`, `MimeNode`)
- **Functions/Methods**: `snake_case` (e.g., `parse_expression()`, `get_precedence()`)
- **Variables**: `snake_case` (e.g., `current_token`, `max_depth`)
- **Constants**: `SCREAMING_SNAKE_CASE` (e.g., `MAX_NESTING_DEPTH`)
- **Template Parameters**: `PascalCase` (e.g., `template<typename Derived>`)
- **Namespaces**: `snake_case` (e.g., `namespace libglot::sql`)

### File Organization

```
domain/
├── include/libglot/domain/
│   ├── token_spec.h      # TokenSpec implementation
│   ├── ast_nodes.h       # AST node definitions
│   ├── parser.h          # Parser implementation
│   ├── generator.h       # Generator implementation
│   └── dialect_traits.h  # Dialect-specific configuration
├── tests/
│   ├── test_parser.cpp
│   ├── test_generator.cpp
│   └── test_roundtrip.cpp
├── benchmarks/
│   └── benchmark_roundtrip.cpp
├── CMakeLists.txt
└── README.md
```

### Header Guards

Use `#pragma once` (simpler, faster, less error-prone):

```cpp
// ✅ GOOD
#pragma once

namespace libglot::foo {
    // ...
}

// ❌ BAD (traditional include guards are verbose)
#ifndef LIBGLOT_FOO_PARSER_H
#define LIBGLOT_FOO_PARSER_H
// ...
#endif
```

### Include Order

1. Corresponding header (if .cpp file)
2. libglot headers (project)
3. Third-party library headers
4. Standard library headers

```cpp
// parser.cpp
#include "parser.h"                     // 1. Corresponding header

#include <libglot/lex/lexer.h>          // 2. Project headers
#include <libglot/ast/ast_node.h>

#include <catch2/catch_test_macros.hpp> // 3. Third-party

#include <string>                       // 4. Standard library
#include <vector>
```

### Memory Management

**Always use arena allocation for AST nodes**:

```cpp
// ✅ GOOD: Arena allocation
auto* node = arena_.create<SelectStmt>();

// ❌ BAD: Heap allocation (slow, requires manual cleanup)
auto* node = new SelectStmt();
```

**Use `std::string_view` for strings (backed by interned strings)**:

```cpp
// ✅ GOOD: Zero-copy string view
struct Identifier {
    std::string_view name;  // Points to interned string in arena
};

// ❌ BAD: Per-node string allocation
struct Identifier {
    std::string name;  // Expensive copy
};
```

### Error Handling

Use `report_error()` for parse errors:

```cpp
// ✅ GOOD: Structured error reporting
if (current_token().kind != TokenKind::SEMICOLON) {
    report_error("Expected semicolon");
    return nullptr;
}

// ❌ BAD: Throwing exceptions on hot path
if (current_token().kind != TokenKind::SEMICOLON) {
    throw std::runtime_error("Expected semicolon");
}
```

### Comments

- Use `///` for documentation comments (Doxygen-style)
- Use `//` for implementation comments
- Write code that's self-documenting (clear variable/function names)

```cpp
/// Parse a SELECT statement.
///
/// Grammar:
///   SELECT <select_list> FROM <table_expr> [WHERE <condition>]
///
/// @return Pointer to SelectStmt node, or nullptr on error
SelectStmt* parse_select_stmt() {
    // Skip SELECT keyword (already validated by caller)
    advance();

    auto* stmt = arena_.create<SelectStmt>();
    stmt->columns = parse_select_list();  // Self-documenting

    return stmt;
}
```

### Formatting

- **Indentation**: 4 spaces (no tabs)
- **Line length**: 120 characters (soft limit)
- **Braces**: K&R style (opening brace on same line)
- **Pointer/reference alignment**: `Type* ptr` (align with type)

```cpp
// ✅ GOOD
class Parser {
    void parse() {
        if (condition) {
            do_something();
        } else {
            do_other();
        }
    }
};

// ❌ BAD: Inconsistent bracing
class Parser
{
    void parse()
    {
        if (condition)
        {
            do_something();
        }
    }
};
```

---

## Testing Requirements

### Test Coverage

All new code must include:

1. **Unit tests** (Catch2): Test individual functions/classes
2. **Integration tests**: Test parser → generator roundtrip
3. **Corpus tests**: Test against real-world input samples (100+ files)
4. **Fuzz tests**: Test with AFL++ or libFuzzer (1M+ inputs)

### Running Tests

```bash
# Run all tests
ctest --preset fast-debug

# Run specific test suite
./build/sql/tests/test_sql --tags="[parser]"

# Run with ASan (memory safety)
ctest --preset fast-debug-asan

# Run with TSan (thread safety)
ctest --preset fast-debug-tsan
```

### Test Quality Gates

- **Coverage**: ≥90% line coverage (verify with `gcov`)
- **Performance**: Within 2% of baseline (run benchmarks)
- **Memory safety**: ASan reports 0 errors
- **Thread safety**: TSan reports 0 data races
- **Fuzz testing**: 0 crashes after 1M inputs

---

## Build System Integration

### CMake Presets

Test your changes with all build presets:

```bash
# Debug build (fast iteration)
cmake --preset fast-debug
cmake --build --preset fast-debug
ctest --preset fast-debug

# Memory safety (ASan + UBSan)
cmake --preset fast-debug-asan
cmake --build --preset fast-debug-asan
ctest --preset fast-debug-asan

# Thread safety (TSan)
cmake --preset fast-debug-tsan
cmake --build --preset fast-debug-tsan
ctest --preset fast-debug-tsan

# Release build (final validation)
cmake --preset release
cmake --build --preset release
ctest --preset release
```

### Adding CMake Options

If adding a new optional feature, use CMake options:

```cmake
# CMakeLists.txt
option(LIBGLOT_ENABLE_FOO "Enable Foo parser support" ON)

if(LIBGLOT_ENABLE_FOO)
    add_subdirectory(foo)
endif()
```

---

## Performance Validation

### Benchmarking

Run benchmarks before and after your changes:

```bash
# Build with optimizations
cmake --preset release
cmake --build --preset release

# Run baseline benchmarks
./build/release/sql/benchmarks/benchmark_roundtrip --benchmark_out=baseline.json

# Make your changes, rebuild, and re-run
./build/release/sql/benchmarks/benchmark_roundtrip --benchmark_out=new.json

# Compare (should be within 2% of baseline)
python scripts/compare_benchmarks.py baseline.json new.json
```

### Performance Targets

| Parser Domain | Target Throughput | Target Latency |
|---------------|-------------------|----------------|
| SQL           | 500k queries/sec  | <2µs per query |
| MIME          | 100 MB/s          | <10µs per message |
| Logs (structured) | 10M lines/sec | <100ns per line |
| Logs (unstructured) | 1M lines/sec | <1µs per line |

### Profiling

Use `perf` or `Instruments` to identify hot paths:

```bash
# Linux (perf)
perf record -g ./build/release/sql/benchmarks/benchmark_roundtrip
perf report

# macOS (Instruments)
instruments -t "Time Profiler" ./build/release/sql/benchmarks/benchmark_roundtrip
```

---

## Documentation Requirements

All new parsers must include:

1. **README.md**: Quick start, usage examples, performance data
2. **Grammar documentation**: EBNF or PEG grammar for the format
3. **Dialect documentation**: Differences between dialect variants
4. **API documentation**: Doxygen comments for public APIs
5. **Migration guide**: If replacing an existing parser

### Documentation Style

- Use clear, concise language
- Include code examples for all public APIs
- Provide performance benchmarks with real-world input
- Explain design decisions (especially deviations from spec)

---

## PR Submission Process

### Before Submitting

1. **Run all tests**: `ctest --preset fast-debug-asan`
2. **Run benchmarks**: Verify within 2% of baseline
3. **Format code**: Use `clang-format` (if configured)
4. **Update documentation**: README, ARCHITECTURE, etc.
5. **Write commit message**: Follow conventional commits format

### Commit Message Format

```
type(scope): Brief description (50 chars or less)

Detailed explanation of what changed and why (wrap at 72 chars).
Reference any related issues (#123).

- Bullet points for multiple changes
- Use present tense ("Add feature" not "Added feature")
- Explain *why* the change was needed

Benchmarks:
- SQL roundtrip: 1.2µs → 1.1µs (8% faster)
- Memory usage: 57ns/node (unchanged)


via [Happy](https://happy.engineering)

Co-Authored-By: Claude <noreply@anthropic.com>
Co-Authored-By: Happy <yesreply@happy.engineering>
```

**Types**:
- `feat`: New feature
- `fix`: Bug fix
- `perf`: Performance improvement
- `refactor`: Code restructure (no behavior change)
- `test`: Add/modify tests
- `docs`: Documentation only
- `build`: Build system changes

### PR Description Template

```markdown
## Summary

Brief description of what this PR does.

## Motivation

Why is this change needed? What problem does it solve?

## Changes

- Added `libglot-foo` parser for Foo format
- Implemented 3 dialect variants (Standard, Variant1, Variant2)
- Added 25 test cases (100% coverage)
- Benchmarked at 83 MB/s throughput

## Performance Impact

| Metric | Before | After | Change |
|--------|--------|-------|--------|
| Parse time | N/A | 1.2µs | N/A |
| Memory usage | N/A | 57ns/node | N/A |

## Testing

- [x] All existing tests pass
- [x] New tests added (25 cases)
- [x] ASan clean (0 errors)
- [x] TSan clean (0 data races)
- [x] Fuzz tested (1M inputs, 0 crashes)
- [x] Benchmarks within 2% of baseline

## Documentation

- [x] README.md updated
- [x] ARCHITECTURE.md updated (if applicable)
- [x] API documentation added (Doxygen comments)
- [x] Grammar documented (EBNF/PEG)

## Checklist

- [x] Code follows style guidelines
- [x] Commit message follows conventional commits format
- [x] All CI checks pass
- [x] Ready for review
```

### Review Process

1. **Automated checks**: CI runs all tests + sanitizers
2. **Maintainer review**: Code quality, design, performance
3. **Benchmarks verified**: Performance within acceptable range
4. **Documentation reviewed**: Clear, complete, accurate
5. **Approval + merge**: Squash merge to main branch

---

## FAQ

### Q: Should I use virtual functions?

**A**: Avoid virtual functions on hot paths (parsing, AST traversal). Use CRTP instead:

```cpp
// ✅ GOOD: CRTP (compile-time dispatch)
template<typename Derived>
class ParserBase {
    auto parse() {
        return static_cast<Derived*>(this)->parse_prefix();
    }
};

// ❌ BAD: Virtual dispatch (runtime cost)
class ParserBase {
    virtual Node* parse_prefix() = 0;
};
```

Virtual functions are OK for:
- Base AST node classes (not on hot path)
- Error reporting (not performance-critical)

### Q: How do I handle errors?

**A**: Use `report_error()` from `ParserBase`:

```cpp
if (!expect(TokenKind::SEMICOLON)) {
    report_error("Expected semicolon");
    return nullptr;  // Error recovery
}
```

For unrecoverable errors, return `nullptr` and let caller handle.

### Q: Should I support all dialects from day 1?

**A**: No. Start with one reference dialect (e.g., "Standard"), then add others incrementally. Each dialect should have:
- Feature flags in `dialect_traits.h`
- Conditional logic in generator
- Separate test cases

### Q: How do I debug parser issues?

**A**: Use the lexer trace mode:

```cpp
parser.set_trace(true);  // Prints token stream to stderr
auto* node = parser.parse_top_level();
```

Or use a debugger with breakpoints in `parse_prefix()`.

### Q: What if my format has context-sensitive lexing?

**A**: Use lexer modes (see `sql/token_spec.h` for examples). Switch modes based on parser state:

```cpp
lexer_.push_mode(LexerMode::STRING_LITERAL);
auto token = lexer_.next_token();
lexer_.pop_mode();
```

### Q: How do I handle Unicode?

**A**: Use UTF-8 everywhere (`std::string_view` is UTF-8 compatible). For normalization/validation, use ICU library:

```cpp
#include <unicode/unistr.h>

icu::UnicodeString normalized = icu::UnicodeString::fromUTF8(input).normalize();
```

### Q: Can I use Rust/Go/Python for new parsers?

**A**: libglot-core is C++ only (for zero-cost abstraction). However, you can:
- Write C++ parser first
- Add language bindings (nanobind for Python, cxx for Rust)
- Use FFI for other languages

### Q: What if I find a bug in libglot-core?

**A**: File an issue with:
1. Minimal reproducing example
2. Expected vs. actual behavior
3. Stack trace (if crash)
4. Proposed fix (if you have one)

Then submit a PR with fix + regression test.

---

## Getting Help

- **GitHub Issues**: https://github.com/richarah/libglot/issues
- **GitHub Discussions**: https://github.com/richarah/libglot/discussions
- **Documentation**: See `docs/` directory

---

**Thank you for contributing to libglot!** 🚀

Your work helps build high-performance parsers for the entire ecosystem. Whether you're adding a new domain, fixing bugs, or improving documentation — every contribution matters.

Happy parsing!
