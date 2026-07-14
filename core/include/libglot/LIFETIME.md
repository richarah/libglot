# Lifetime Management in libglot

## The Source Lifetime Problem

libglot AST nodes contain `std::string_view` members that point into the
source text (identifiers, literals, keywords). If the source string is
destroyed before the AST, those views dangle.

## Design Decision: Arena-Owned Source

The parser copies the source into the arena at construction time
(`Arena::copy_source`) and tokenizes the arena-owned copy. Every
`string_view` in tokens and AST nodes therefore points into arena memory
and remains valid exactly as long as the AST itself.

Consequences:

1. **Safety by construction** — callers may pass a temporary string; the
   parser never retains a reference to it.
2. **One copy per parse** — `copy_source` is called once, in the
   tokenize-and-copy helper. Do not copy the source a second time.
3. **AST lifetime == arena lifetime** — AST nodes are created only via
   `Arena::create` and are invalidated by `Arena::reset()` or arena
   destruction. Never wrap an arena pointer in `std::unique_ptr` or call
   `delete` on it.

## Correct Usage

```cpp
libglot::Arena arena;
libglot::sql::SQLParser parser(arena, "SELECT * FROM users");
auto* ast = parser.parse_top_level();
// `ast` (and every string_view inside it) is valid while `arena` lives.
```

The temporary source string passed to the constructor may go out of scope
immediately; the parser already copied it.

## Incorrect Usage

```cpp
libglot::Arena arena;
SQLNode* ast = nullptr;
{
    libglot::Arena inner;
    libglot::sql::SQLParser parser(inner, "SELECT 1");
    ast = parser.parse_top_level();
}   // inner destroyed: every node behind `ast` is gone
// UNDEFINED BEHAVIOUR: ast points into freed arena memory
```

```cpp
// NEVER: arena pointers are not heap pointers
std::unique_ptr<SQLNode> owned(parser.parse_top_level()); // delete on arena memory = UB
```

## Destructors

`Arena::create<T>` registers the destructor of any non-trivially-
destructible `T` and runs it (in reverse construction order) at
`reset()` or arena destruction. Nodes holding `std::vector`/`std::string`
members are therefore cleaned up correctly; trivially destructible nodes
carry no bookkeeping cost.

## Verifying

The ASan CI job exercises parse + generate flows; any dangling-view or
use-after-reset regression fails the build.
