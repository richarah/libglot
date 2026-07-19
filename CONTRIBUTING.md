# Contributing

## Building and testing

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
```

Or use the presets: `cmake --preset debug && cmake --build --preset debug && ctest --preset debug`.

Run a specific test binary:
```
./build/sql/tests/test_parser
```

Run with sanitizers (ASan+UBSan):
```
cmake --preset debug-asan && cmake --build --preset debug-asan && ctest --preset debug-asan
```

Fuzzers (requires Clang):
```
cmake -B build-fuzz -DCMAKE_CXX_COMPILER=clang++ -DLIBGLOT_BUILD_FUZZERS=ON -DBUILD_TESTING=OFF
cmake --build build-fuzz
./build-fuzz/fuzz/fuzz_sql_parser -max_total_time=60
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
- TokenSpec: `sql/include/libglot/sql/token_spec.h` (tokenizer: `sql/include/libglot/sql/lex/`)
- AST nodes: `sql/include/libglot/sql/ast_nodes.h`
- GrammarSpec: `sql/include/libglot/sql/grammar.h`
- Parser: `sql/include/libglot/sql/parser.h`
- Generator: `sql/include/libglot/sql/generator.h`

## Code style

C++20. Concepts over SFINAE. CRTP over virtual on hot paths. Header-only for
templated code. Arena allocator for AST nodes; every `string_view` stored in
a token or node must point into arena-owned memory (see
`core/include/libglot/LIFETIME.md`). `clang-format` config is committed —
format your changes.

## Test expectations

- Assertions are exact strings or AST-shape checks, never substring `find()`.
- New parser/generator behavior needs a roundtrip test; consider adding the
  construct to `sql/tests/test_roundtrip_property.cpp`'s corpus.
- Never commit a placeholder (`REQUIRE(true)`) test.

## PR expectations

CI must be green: all tests on GCC and Clang, ASan/UBSan clean,
warnings-as-errors, fuzz smoke. No performance regressions on existing
benchmarks.
