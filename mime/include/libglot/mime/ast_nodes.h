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
struct Message;
struct Header;

/// ============================================================================
/// Base Node
/// ============================================================================

struct MimeNode : libglot::AstNodeBase<MimeNode, MimeNodeKind> {
    using libglot::AstNodeBase<MimeNode, MimeNodeKind>::AstNodeBase;
};

/// ============================================================================
/// Header Node (field: value, with optional parameters)
/// ============================================================================

struct Header : MimeNode {
    std::string_view field;
    std::string_view value;

    /// Parameters extracted from header value (e.g., charset=utf-8, boundary=xyz)
    std::vector<std::pair<std::string_view, std::string_view>> parameters;

    explicit Header(std::string_view f, std::string_view v)
        : MimeNode(MimeNodeKind::HEADER)
        , field(f)
        , value(v)
        , parameters()
    {}
};

/// Part is an alias for Message (used in multipart parsing)
using Part = Message;

/// ============================================================================
/// Message Node (headers + body + optional multipart parts)
/// ============================================================================

struct Message : MimeNode {
    std::vector<Header*> headers;
    std::string_view body;

    /// For multipart messages, this contains the individual parts
    std::vector<Message*> parts;

    explicit Message()
        : MimeNode(MimeNodeKind::MESSAGE)
        , headers()
        , body()
        , parts()
    {}

    explicit Message(std::vector<Header*> h, std::string_view b = "")
        : MimeNode(MimeNodeKind::MESSAGE)
        , headers(std::move(h))
        , body(b)
        , parts()
    {}
};

} // namespace libglot::mime
