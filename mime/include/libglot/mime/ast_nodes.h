#pragma once

#include <libglot/ast/node.h>
#include <string_view>
#include <vector>

namespace libglot::mime {

/// ============================================================================
/// MIME AST Node Types
/// ============================================================================

enum class MimeNodeKind { HEADER, MESSAGE };

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
struct DeliveryStatusRef;
struct MessageId;
struct ParsedDateTime;

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
        : MimeNode(MimeNodeKind::HEADER), field(f), value(v), parameters() {}
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

    /// For message/rfc822 parts (RFC 2046 §5.2.1): the recursively parsed
    /// encapsulated message (its own headers + body, run through the same
    /// pipeline); nullptr for every other content type. The same
    /// nesting-depth/part-count limits as multipart apply -- see
    /// MimeParserExtended::parse_encapsulated_message.
    Message* encapsulated = nullptr;

    /// Parsed Date header (RFC 5322 §3.3), when present and syntactically
    /// valid; nullptr when the header is absent or fails to parse (see
    /// AnomalyKind::InvalidDateFormat).
    ParsedDateTime* date = nullptr;

    /// Message-ID (RFC 5322 §3.6.4); nullptr when the header is absent.
    MessageId* message_id = nullptr;

    /// In-Reply-To (RFC 5322 §3.6.4); nullptr when the header is absent
    /// (an empty, non-null vector means the header was present but carried
    /// no recognizable msg-id).
    std::vector<MessageId>* in_reply_to = nullptr;

    /// References (RFC 5322 §3.6.4); nullptr when the header is absent.
    std::vector<MessageId>* references = nullptr;

    /// For message/delivery-status parts (RFC 3464, transported inside a
    /// multipart/report per RFC 6522): the parsed per-message and
    /// per-recipient field groups; nullptr otherwise.
    DeliveryStatusRef* delivery_status = nullptr;

    /// For multipart/related (RFC 2387 §3.4): the part resolved from the
    /// "start" Content-ID parameter, or the first part when "start" is
    /// absent or does not resolve; nullptr when this message has no parts.
    Message* related_root = nullptr;

    /// The exact bytes of this part (headers + body) as they appeared
    /// between multipart boundary delimiters -- before header unfolding,
    /// transfer-decoding, or charset conversion. Populated for every
    /// multipart child part (see MimeParserExtended::parse_part); empty for
    /// the top-level message. This is the byte-exact view a multipart/signed
    /// (RFC 1847) signature would be computed over; libglot does not verify
    /// signatures (no crypto dependency -- out of scope), it only guarantees
    /// this span is never normalized, unfolded, or re-encoded.
    std::string_view raw_source;

    /// RFC 2046 §5.1.1: content before the first boundary delimiter
    /// ("preamble") and after the final close delimiter ("epilogue") of a
    /// multipart body. Both are defined as material a conforming reader
    /// "should" ignore for content purposes, but they are still part of
    /// the message -- captured here rather than silently discarded so a
    /// caller can inspect them if it needs to (e.g. detecting a
    /// non-MIME-aware relay's banner text). Empty when this message is
    /// not multipart, or when no boundary delimiter was found at all (an
    /// isolated close-delimiter with no preceding opening one is not a
    /// valid multipart-body per the RFC 2046 grammar -- msg->body holds
    /// the untouched raw content in that case, not preamble/epilogue).
    std::string_view preamble;
    std::string_view epilogue;

    explicit Message() : MimeNode(MimeNodeKind::MESSAGE), headers(), body(), parts() {}

    explicit Message(std::vector<Header*> h, std::string_view b = "")
        : MimeNode(MimeNodeKind::MESSAGE), headers(std::move(h)), body(b), parts() {}
};

} // namespace libglot::mime
