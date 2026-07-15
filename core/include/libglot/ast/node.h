#pragma once

#include "../util/arena.h"
#include <algorithm>
#include <concepts>
#include <cstdint>
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
/// 3. Is efficiently movable
///
/// Zero-cost abstraction: All dispatch happens at compile-time via templates/CRTP.
/// No virtual dispatch on hot paths (code generation, optimization).
/// ============================================================================

template<typename T>
concept AstNodeKind = requires { requires std::is_enum_v<T>; };

template<typename T>
concept AstNode = requires(T node) {
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

    /// Nodes must be destructible. Arena::create registers non-trivial
    /// destructors and runs them at arena reset/destruction.
    { node.~T() } noexcept;
};

// ============================================================================
/// Base class for AST nodes using CRTP for zero-cost polymorphism
/// Domains derive from this to get common functionality without virtual calls
// ============================================================================

template<typename Derived, AstNodeKind Kind>
struct AstNodeBase {
    using NodeKind = Kind; // Expose NodeKind for AstNode concept

    Kind type;

    explicit constexpr AstNodeBase(Kind t) noexcept : type(t) {}

    /// CRTP: Cast to derived type (zero-cost, compile-time checked)
    [[nodiscard]] constexpr Derived& as_derived() noexcept { return static_cast<Derived&>(*this); }

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

    /// Virtual destructor NOT needed - arena runs registered destructors
    ~AstNodeBase() = default;
};

// ============================================================================
/// Source location tracking (optional mixin for AST nodes)
/// ============================================================================

struct SourceLocation {
    uint32_t start_offset; ///< Byte offset in source (0-indexed)
    uint32_t end_offset;   ///< Byte offset (exclusive)
    uint32_t start_line;   ///< Line number (1-indexed)
    uint32_t start_col;    ///< Column number (1-indexed)

    [[nodiscard]] constexpr size_t length() const noexcept {
        return end_offset >= start_offset ? end_offset - start_offset : 0;
    }

    [[nodiscard]] constexpr std::string_view extract(std::string_view source) const noexcept {
        if (start_offset >= source.size())
            return "";
        size_t len = std::min<size_t>(length(), source.size() - start_offset);
        return source.substr(start_offset, len);
    }
};

} // namespace libglot
