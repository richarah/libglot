#pragma once

#include "../../../../core/include/libglot/ast/node.h"
#include <string_view>
#include <vector>

namespace libglot::mime {

/// ============================================================================
/// MIME AST Node Types
/// ============================================================================

enum class MimeNodeKind {
    HEADER,
    MESSAGE
};

/// ============================================================================
/// Forward declaration
/// ============================================================================

struct MimeNode;

/// ============================================================================
/// Base Node
/// ============================================================================

struct MimeNode : libglot::AstNodeBase<MimeNode, MimeNodeKind> {
    using libglot::AstNodeBase<MimeNode, MimeNodeKind>::AstNodeBase;
};

/// ============================================================================
/// Header Node (field: value)
/// ============================================================================

struct Header : MimeNode {
    std::string_view field;
    std::string_view value;

    explicit Header(std::string_view f, std::string_view v)
        : MimeNode(MimeNodeKind::HEADER)
        , field(f)
        , value(v)
    {}
};

/// ============================================================================
/// Message Node (headers + body)
/// ============================================================================

struct Message : MimeNode {
    std::vector<Header*> headers;
    std::string_view body;

    explicit Message()
        : MimeNode(MimeNodeKind::MESSAGE)
        , headers()
        , body()
    {}

    explicit Message(std::vector<Header*> h, std::string_view b = "")
        : MimeNode(MimeNodeKind::MESSAGE)
        , headers(std::move(h))
        , body(b)
    {}
};

} // namespace libglot::mime
