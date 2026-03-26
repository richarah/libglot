#pragma once

#include "../util/arena.h"
#include <concepts>
#include <string_view>
#include <type_traits>

namespace libglot {

/// ============================================================================
/// AstNode Concept - Defines contract for domain-specific AST nodes
/// ============================================================================
///
/// Every domain must implement an AST hierarchy that:
/// 1. Has a node type enumeration (NodeKind)
/// 2. Provides factory methods for arena allocation
/// 3. Supports visitor pattern traversal
/// 4. Is efficiently copyable/movable
///
/// Zero-cost abstraction: All dispatch happens at compile-time via templates/CRTP.
/// No virtual dispatch on hot paths (code generation, optimization).
/// ============================================================================

template<typename T>
concept AstNodeKind = requires {
    requires std::is_enum_v<T>;
};

template<typename T>
concept AstNode = requires(T node, const T const_node) {
    // ========================================================================
    // Required Types
    // ========================================================================

    /// Node type enumeration (e.g., enum class NodeKind { SELECT, INSERT, ... })
    typename T::NodeKind;
    requires AstNodeKind<typename T::NodeKind>;

    // ========================================================================
    // Required Members
    // ========================================================================

    /// Every node must have a type field for runtime dispatch
    { node.type } -> std::convertible_to<typename T::NodeKind>;

    // ========================================================================
    // Destructibility
    // ========================================================================

    /// Nodes must be destructible (for arena cleanup)
    /// Note: Destructor does NOT need to be virtual - arena destroys all at once
    { node.~T() } noexcept;
};

// ============================================================================
/// Base class for AST nodes using CRTP for zero-cost polymorphism
/// Domains derive from this to get common functionality without virtual calls
// ============================================================================

template<typename Derived, AstNodeKind Kind>
struct AstNodeBase {
    using NodeKind = Kind;  // Expose NodeKind for AstNode concept

    Kind type;

    explicit constexpr AstNodeBase(Kind t) noexcept : type(t) {}

    /// CRTP: Cast to derived type (zero-cost, compile-time checked)
    [[nodiscard]] constexpr Derived& as_derived() noexcept {
        return static_cast<Derived&>(*this);
    }

    [[nodiscard]] constexpr const Derived& as_derived() const noexcept {
        return static_cast<const Derived&>(*this);
    }

    /// Factory method for arena allocation (type-safe, zero-overhead)
    template<typename NodeType, typename... Args>
    [[nodiscard]] static NodeType* create(Arena& arena, Args&&... args) {
        return arena.create<NodeType>(std::forward<Args>(args)...);
    }

    /// Non-copyable (use arena allocation only)
    AstNodeBase(const AstNodeBase&) = delete;
    AstNodeBase& operator=(const AstNodeBase&) = delete;

    /// Movable (for arena reallocation)
    AstNodeBase(AstNodeBase&&) noexcept = default;
    AstNodeBase& operator=(AstNodeBase&&) noexcept = default;

    /// Virtual destructor NOT needed - arena destroys all at once
    ~AstNodeBase() = default;
};

// ============================================================================
/// Visitor Concept - Defines contract for AST traversal
/// ============================================================================

template<typename V, typename Node>
concept AstVisitor = AstNode<Node> && requires(V visitor, Node* node, const Node* const_node) {
    /// Visit mutable node
    { visitor.visit(node) } -> std::same_as<void>;

    /// Visit const node (optional, for read-only traversal)
    // { visitor.visit(const_node) } -> std::same_as<void>;
};

// ============================================================================
/// Walker Concept - Defines contract for recursive tree traversal
/// ============================================================================

template<typename W, typename Node>
concept AstWalker = AstNode<Node> && requires(W walker, Node* node) {
    /// Pre-order visit (before children)
    { walker.pre_visit(node) } -> std::same_as<bool>;  // Return false to skip subtree

    /// Post-order visit (after children)
    { walker.post_visit(node) } -> std::same_as<void>;

    /// Get children of node (for traversal)
    { walker.get_children(node) } -> std::convertible_to<std::vector<Node*>>;
};

// ============================================================================
/// Generic tree walker using depth-first traversal
/// ============================================================================

template<AstNode Node, AstWalker<Node> Walker>
class GenericWalker {
public:
    explicit GenericWalker(Walker& walker) : walker_(walker) {}

    void walk(Node* root) {
        if (!root) return;

        // Pre-order visit
        if (!walker_.pre_visit(root)) {
            return;  // Skip subtree
        }

        // Recursively visit children
        for (auto* child : walker_.get_children(root)) {
            walk(child);
        }

        // Post-order visit
        walker_.post_visit(root);
    }

private:
    Walker& walker_;
};

// ============================================================================
/// Source location tracking (optional mixin for AST nodes)
/// ============================================================================

struct SourceLocation {
    uint32_t start_offset;    ///< Byte offset in source (0-indexed)
    uint32_t end_offset;      ///< Byte offset (exclusive)
    uint16_t start_line;      ///< Line number (1-indexed)
    uint16_t start_col;       ///< Column number (1-indexed)

    [[nodiscard]] constexpr size_t length() const noexcept {
        return end_offset - start_offset;
    }

    [[nodiscard]] constexpr std::string_view extract(std::string_view source) const noexcept {
        if (start_offset >= source.size()) return "";
        size_t len = std::min<size_t>(length(), source.size() - start_offset);
        return source.substr(start_offset, len);
    }
};

// ============================================================================
/// Example AST Node Implementation (for documentation)
/// ============================================================================

#if 0  // Example only, not compiled

enum class ExampleNodeKind : uint16_t {
    LITERAL,
    BINARY_OP,
    FUNCTION_CALL,
    // ... domain-specific node types
};

struct ExampleNode : AstNodeBase<ExampleNode, ExampleNodeKind> {
    using Base = AstNodeBase<ExampleNode, ExampleNodeKind>;
    using Base::Base;  // Inherit constructor

    SourceLocation loc;  // Optional source location
};

struct Literal : ExampleNode {
    std::string value;

    explicit Literal(std::string v)
        : ExampleNode(ExampleNodeKind::LITERAL), value(std::move(v)) {}
};

struct BinaryOp : ExampleNode {
    ExampleNode* left;
    ExampleNode* right;

    BinaryOp(ExampleNodeKind op, ExampleNode* l, ExampleNode* r)
        : ExampleNode(op), left(l), right(r) {}
};

// Verify concept satisfaction
static_assert(AstNode<ExampleNode>, "ExampleNode must satisfy AstNode concept");
static_assert(AstNode<Literal>, "Literal must satisfy AstNode concept");
static_assert(AstNode<BinaryOp>, "BinaryOp must satisfy AstNode concept");

// Example visitor
struct ExampleVisitor {
    void visit(ExampleNode* node) {
        switch (node->type) {
            case ExampleNodeKind::LITERAL:
                visit_literal(static_cast<Literal*>(node));
                break;
            case ExampleNodeKind::BINARY_OP:
                visit_binary_op(static_cast<BinaryOp*>(node));
                break;
            // ... other cases
        }
    }

    void visit_literal(Literal* lit) {
        // ... process literal
    }

    void visit_binary_op(BinaryOp* op) {
        // ... process binary operation
    }
};

static_assert(AstVisitor<ExampleVisitor, ExampleNode>, "ExampleVisitor must satisfy AstVisitor");

#endif  // Example

} // namespace libglot
