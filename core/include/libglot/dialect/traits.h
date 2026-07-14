#pragma once

#include <concepts>
#include <cstddef>
#include <string_view>
#include <type_traits>

namespace libglot {

/// ============================================================================
/// DialectTraits Concept - Defines contract for dialect-specific behavior
/// ============================================================================
///
/// Every domain defines a set of dialects (e.g., SQL has MySQL, PostgreSQL, etc.)
/// Each dialect may have different:
/// - Syntax preferences (identifier quotes, string literals, etc.)
/// - Feature support (CTEs, window functions, etc.)
/// - Code generation patterns
///
/// This concept enables compile-time dialect selection with zero runtime overhead.
/// ============================================================================

template<typename T>
concept DialectEnum = requires {
    requires std::is_enum_v<T>;
    T::COUNT;  ///< Number of dialects (must exist)
    requires std::convertible_to<std::underlying_type_t<T>, size_t>;  ///< Underlying type must be numeric
};

template<typename T>
concept DialectTraits = requires(typename T::DialectId id) {
    // ========================================================================
    // Required Types
    // ========================================================================

    /// Dialect identifier enum (e.g., enum class SQLDialect { ANSI, MySQL, ... })
    typename T::DialectId;
    requires DialectEnum<typename T::DialectId>;

    /// Feature flags struct (domain-specific)
    typename T::Features;

    // ========================================================================
    // Required Functions
    // ========================================================================

    /// Get feature flags for a dialect
    /// PERFORMANCE: Should use constexpr lookup table for zero overhead
    { T::get_features(id) } -> std::same_as<const typename T::Features&>;

    /// Get human-readable dialect name
    { T::name(id) } -> std::convertible_to<std::string_view>;
};

} // namespace libglot
