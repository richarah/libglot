# Lifetime Management in libglot

## The Source Lifetime Problem

All libglot AST nodes contain `std::string_view` references that point into the original source text. These string views provide zero-copy access to identifiers, literals, and keywords.

**Critical Issue**: If the source string is destroyed before the AST, all string_view references become dangling pointers, causing undefined behaviour.

Example of the problem:
```cpp
libglot::Arena arena;
std::unique_ptr<SQLNode> ast;

{
    std::string source = "SELECT * FROM users";
    SQLParser parser(arena, source);
    ast = parser.parse();
    // source destroyed here
}

// UNDEFINED BEHAVIOUR: ast contains string_views pointing to freed memory
```

## Design Decision: Arena-Owned Source

**Decision**: The arena owns the source string. Parsers copy the source into arena memory at construction time.

**Rationale**:
1. **Safety by construction**: Impossible to create dangling string_view references
2. **Simplicity**: Callers don't need to manage source lifetime
3. **Performance**: Single allocation in arena, minimal overhead
4. **Consistency**: Same lifetime model as AST nodes

## Implementation

### Arena::copy_source()

The `Arena` class provides `copy_source()` to copy source text into arena memory:

```cpp
std::string_view Arena::copy_source(std::string_view source);
```

**Behaviour**:
- Allocates `source.size() + 1` bytes in arena (includes null terminator)
- Copies source data into arena memory
- Returns `string_view` pointing to arena-owned copy
- Returned `string_view` is valid until arena is destroyed or reset

**Null Terminator**: The copy includes a null terminator for safety when interoperating with C APIs (e.g., error messages, debugging).

### Parser Requirements

**All parsers MUST**:
1. Accept `std::string_view source` parameter
2. Call `arena.copy_source(source)` in constructor
3. Store the returned `string_view` as `source_` member
4. Pass `source_` to tokeniser, NOT the original parameter

**Correct Pattern**:
```cpp
class SQLParser : public ParserBase<SQLParser, SQLTokenSpec, SQLNode> {
public:
    explicit SQLParser(Arena& arena, std::string_view source)
        : Base(arena, tokenize(arena.copy_source(source)))
        , source_(arena.copy_source(source))
    {}

private:
    std::string_view source_;  // Arena-owned copy
};
```

**Incorrect Pattern (DO NOT USE)**:
```cpp
class SQLParser : public ParserBase<SQLParser, SQLTokenSpec, SQLNode> {
public:
    explicit SQLParser(Arena& arena, std::string_view source)
        : Base(arena, tokenize(source))  // ❌ WRONG: source may be freed
        , source_(source)                // ❌ WRONG: dangling reference
    {}

private:
    std::string_view source_;  // ❌ WRONG: not arena-owned
};
```

## Usage Examples

### Safe Usage
```cpp
libglot::Arena arena;

// Temporary source (destroyed after parse)
{
    std::string source = "SELECT * FROM users";
    SQLParser parser(arena, source);
    auto ast = parser.parse();
    // source destroyed here - BUT AST is safe because arena owns copy
}

// AST remains valid
SQLGenerator gen(Dialect::PostgreSQL);
std::string sql = gen.generate(ast);  // ✅ Safe
```

### Arena Reset
```cpp
libglot::Arena arena;
std::string_view source_ref;

{
    std::string source = "SELECT 1";
    source_ref = arena.copy_source(source);
    // source destroyed
}

std::cout << source_ref;  // ✅ Safe: points to arena memory

arena.reset();  // ❌ Invalidates source_ref

std::cout << source_ref;  // ❌ UNDEFINED BEHAVIOUR after reset
```

## Testing Lifetime Safety

To validate lifetime safety:

1. **AddressSanitizer (ASan)**: Detects use-after-free bugs
   ```bash
   cmake --preset fast-debug-asan
   cmake --build build/fast-debug-asan
   ctest --test-dir build/fast-debug-asan
   ```

2. **Explicit Lifetime Tests**: Create tests where source is destroyed before AST usage
   ```cpp
   TEST_CASE("Source lifetime: arena-owned") {
       Arena arena;
       std::unique_ptr<SQLNode> ast;

       {
           std::string source = "SELECT * FROM users";
           SQLParser parser(arena, source);
           ast = parser.parse();
           // source destroyed here
       }

       // AST usage must work (source is arena-owned)
       REQUIRE(ast != nullptr);
       SQLGenerator gen(Dialect::ANSI);
       std::string sql = gen.generate(ast);
       REQUIRE(sql == "SELECT * FROM users");
   }
   ```

## Alternatives Considered (Rejected)

### Caller-Owned Source
**Rejected**: Requires callers to manage lifetime, error-prone, defeats arena allocation benefits.

**Would require**:
- Concept check: `requires std::is_lvalue_reference_v<decltype(source)>`
- Static assertions preventing temporaries
- Documentation burden on all callers
- Easy to misuse

### Reference-Counted Source
**Rejected**: Adds runtime overhead (atomic refcount), incompatible with arena allocation philosophy.

### std::string Copies in AST Nodes
**Rejected**: Breaks zero-copy design, heap allocates every string, defeats arena performance benefits.

## Summary

- ✅ **Arena owns source**: Call `arena.copy_source(source)` in parser constructor
- ✅ **Zero-copy tokens**: Tokens are `string_view` into arena-owned source
- ✅ **Safe by construction**: Impossible to create dangling references
- ✅ **Validate with ASan**: All tests must pass under AddressSanitizer

**This decision is final and must not be revisited.**
