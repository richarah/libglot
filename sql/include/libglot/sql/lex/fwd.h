#pragma once

#include <cstddef>
#include <cstdint>

namespace libglot::sql::lex {

// Forward declarations
class Arena;
class StringPool;
struct Expression;  // Defined as struct in expression.h
struct Token;  // Defined as struct in tokens.h
class Tokenizer;
class Parser;
class Generator;

// Forward declare enum
enum class TokenType : uint16_t;

// Type aliases
using NodeID = uint32_t;

} // namespace libglot::sql::lex
