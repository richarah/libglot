# Contributing

## Building and testing

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
```

Run specific test:
```
./build/sql/tests/test_parser
```

Run with sanitizers:
```
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined"
cmake --build build
./build/sql/tests/test_parser
```

## Adding a new parser domain

To add a parser for a new language (e.g. log parsing):

1. **Create TokenSpec** (`your_domain/tokens.h`):
```cpp
enum class YourTokenType { IDENTIFIER, NUMBER, /* ... */ };

struct YourTokenSpec {
    using TokenKind = YourTokenType;
    // Define token properties
};
```

2. **Create AST nodes** (`your_domain/ast_nodes.h`):
```cpp
enum class YourNodeKind { EXPRESSION, STATEMENT, /* ... */ };

struct YourNode {
    YourNodeKind type;
    // Base node
};

struct YourExpression : YourNode {
    // Specific node fields
};
```

3. **Create GrammarSpec** (`your_domain/grammar.h`):
```cpp
struct YourGrammarSpec {
    using TokenSpec = YourTokenSpec;
    using AstNodeType = YourNode;
    using NodeKind = YourNodeKind;

    static constexpr OperatorInfo operators[] = {
        {YourTokenType::PLUS, 10, Associativity::LEFT},
        // Define operator precedence
    };
};
```

4. **Derive parser** (`your_domain/parser.h`):
```cpp
class YourParser : public libglot::ParserBase<YourGrammarSpec, YourParser> {
    using Base = libglot::ParserBase<YourGrammarSpec, YourParser>;

    YourNode* parse_expression();
    YourNode* parse_statement();
    // Implement parsing methods
};
```

5. **Derive generator** (`your_domain/generator.h`):
```cpp
class YourGenerator : public libglot::GeneratorBase<YourGeneratorSpec, YourGenerator> {
    void visit(YourNode* node);
    void visit_expression(YourExpression* expr);
    // Implement generation methods
};
```

6. **Write tests** (`your_domain/tests/test_parser.cpp`):
```cpp
TEST_CASE("Parse expression") {
    libglot::Arena arena;
    YourParser parser(arena, "1 + 2");
    auto ast = parser.parse_expression();
    REQUIRE(ast != nullptr);
}
```

7. **Verify zero vtables**:
```
nm build/your_test | grep vtable
```
Should produce no output (CRTP eliminates virtual dispatch).

Reference implementations:
- TokenSpec: `sql/include/libglot/sql/tokens.h`
- AST nodes: `sql/include/libglot/sql/ast_nodes.h`
- GrammarSpec: `sql/include/libglot/sql/grammar.h`
- Parser: `sql/include/libglot/sql/parser.h`
- Generator: `sql/include/libglot/sql/generator.h`

## Code style

C++20 minimum. Concepts over SFINAE. CRTP over virtual on hot paths. Header-only for templated code, .cpp for non-templated. Arena allocator for AST nodes. string_view over string where lifetime permits.

## PR expectations

Tests must pass. ASan must be clean. No performance regressions on existing benchmarks.
