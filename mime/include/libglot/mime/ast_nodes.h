#pragma once

#include <libglot/ast/node.h>
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

// Defined in complete_features.h; attached to nodes by the pipeline
// (parser_extended.h) when the corresponding syntax is present.
struct AddressGroup;
struct ExternalBodyRef;
struct MessagePartialRef;

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

    /// Parameters extracted from header value (e.g., charset=utf-8, boundary=xyz).
    /// RFC 2231 continued parameters (name*0, name*1*, ...) additionally get a
    /// reassembled + percent-decoded entry appended under the base name.
    std::vector<std::pair<std::string_view, std::string_view>> parameters;

    /// RFC 5322 address groups ("Team: a@x, b@y;"), populated by the pipeline
    /// for address headers that use group syntax; nullptr otherwise.
    std::vector<AddressGroup>* address_groups = nullptr;

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

    /// For message/external-body parts (RFC 2046 §5.2.3): the parsed
    /// access-type/name/site/... reference; nullptr otherwise.
    ExternalBodyRef* external_body = nullptr;

    /// For message/partial parts (RFC 2046 §5.2.2): the parsed
    /// id/number/total reference; nullptr otherwise. Reassembly of the
    /// fragments is out of scope -- see MessagePartialParser.
    MessagePartialRef* message_partial = nullptr;

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
