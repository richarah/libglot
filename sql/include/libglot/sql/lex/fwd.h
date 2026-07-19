#pragma once

#include <cstdint>

namespace libglot::sql::lex {

// Forward declarations used across the tokenizer headers. (Vestigial
// declarations for types from the original standalone library - Arena,
// Expression, Parser, Generator - were removed; the tokenizer here is
// self-contained.)
struct Token;                     // Defined in tokens.h
enum class TokenType : uint16_t;  // Defined in tokens.h

} // namespace libglot::sql::lex
