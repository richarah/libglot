#pragma once

#include <concepts>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <functional>

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

// ============================================================================
/// Registry for runtime dialect selection (type-erased, small overhead)
/// Use this when dialect must be selected at runtime (e.g., CLI tool)
/// ============================================================================

template<DialectEnum D, typename Features>
class DialectRegistry {
public:
    using DialectId = D;
    using FeatureSet = Features;

    /// Register a dialect with its features
    void register_dialect(DialectId id, Features features) {
        registry_[id] = std::move(features);
    }

    /// Get features for a dialect (runtime lookup, small overhead)
    [[nodiscard]] const Features& get_features(DialectId id) const {
        auto it = registry_.find(id);
        if (it == registry_.end()) {
            throw std::out_of_range("Unknown dialect");
        }
        return it->second;
    }

    /// Get human-readable name
    [[nodiscard]] std::string_view name(DialectId id) const {
        return dialect_name(id);
    }

    /// Check if dialect is registered
    [[nodiscard]] bool has_dialect(DialectId id) const {
        return registry_.contains(id);
    }

    /// Get all registered dialects
    [[nodiscard]] std::vector<DialectId> get_all_dialects() const {
        std::vector<DialectId> result;
        result.reserve(registry_.size());
        for (const auto& [id, _] : registry_) {
            result.push_back(id);
        }
        return result;
    }

private:
    std::unordered_map<DialectId, Features> registry_;

    /// Override this to provide names
    virtual std::string_view dialect_name(DialectId id) const = 0;
};

// ============================================================================
/// Example DialectTraits Implementation (for documentation)
/// ============================================================================

#if 0  // Example only, not compiled

enum class ExampleDialect : uint8_t {
    STANDARD,
    EXTENDED_V1,
    EXTENDED_V2,
    COUNT
};

struct ExampleFeatures {
    bool supports_comments = true;
    bool supports_nested_blocks = false;
    char identifier_quote = '"';
    char string_quote = '\'';
};

struct ExampleDialectTraits {
    using DialectId = ExampleDialect;
    using Features = ExampleFeatures;

    static constexpr const Features& get_features(DialectId id) noexcept {
        // Compile-time lookup table (zero runtime overhead)
        constexpr Features features[] = {
            // STANDARD
            {
                .supports_comments = true,
                .supports_nested_blocks = false,
                .identifier_quote = '"',
                .string_quote = '\''
            },
            // EXTENDED_V1
            {
                .supports_comments = true,
                .supports_nested_blocks = true,
                .identifier_quote = '`',
                .string_quote = '\''
            },
            // EXTENDED_V2
            {
                .supports_comments = true,
                .supports_nested_blocks = true,
                .identifier_quote = '[',
                .string_quote = '"'
            }
        };

        return features[static_cast<size_t>(id)];
    }

    static constexpr std::string_view name(DialectId id) noexcept {
        constexpr std::string_view names[] = {
            "Standard",
            "Extended v1",
            "Extended v2"
        };
        return names[static_cast<size_t>(id)];
    }
};

static_assert(DialectTraits<ExampleDialectTraits>, "ExampleDialectTraits must satisfy DialectTraits");

#endif  // Example

} // namespace libglot

// ============================================================================
/// Hash specialization for dialect enums (enables use in unordered_map)
/// ============================================================================

namespace std {
    template<libglot::DialectEnum D>
    struct hash<D> {
        size_t operator()(D d) const noexcept {
            return hash<std::underlying_type_t<D>>{}(static_cast<std::underlying_type_t<D>>(d));
        }
    };
}
