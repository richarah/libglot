#pragma once

#include "compile_time.h"
#include <array>
#include <string_view>
#include <concepts>
#include <cstdint>

namespace libglot {

/// ============================================================================
/// Perfect Hash Table - Generic keyword lookup with O(1) performance
/// ============================================================================
///
/// Template-based perfect hash table for domain-specific keyword lookup.
/// Used by all domains (SQL, MIME, logs, etc.) for fast token classification.
///
/// PERFORMANCE CRITICAL:
/// - This is on the HOT PATH (called for every identifier during tokenization)
/// - Zero-cost abstraction: All polymorphism resolved at compile-time
/// - Branchless uppercase conversion
/// - Length-first comparison for fast rejection
/// - Cache-friendly data layout
///
/// USAGE:
///   See example at bottom of file for SQL keyword table generation.
///
/// CUSTOMIZATION:
///   - Override TableSize for different keyword set sizes
///   - Override HashFunc for better distribution (FNV-1a, DJB2, etc.)
///   - Override SlotSize if collisions exceed 8 per bucket
/// ============================================================================

// ============================================================================
/// Concept: Token Type Enumeration
/// ============================================================================

template<typename T>
concept TokenTypeEnum = requires {
    requires std::is_enum_v<T>;
    // Must have an IDENTIFIER variant for non-keyword identifiers
    { T::IDENTIFIER } -> std::convertible_to<T>;
};

// ============================================================================
/// Perfect Hash Table Template
/// ============================================================================

template<
    TokenTypeEnum TokenType,
    uint32_t TableSize = 256,
    uint32_t SlotSize = 8,
    uint32_t MaxKeywordLength = 32,
    auto HashFunc = default_hash_function
>
    requires HashFunction<decltype(HashFunc)>
class PerfectHashTable {
public:
    // ========================================================================
    // Entry Structure (one per hash bucket)
    // ========================================================================

    struct Entry {
        /// Keyword strings (nullptr for empty slots)
        const char* keywords[SlotSize];

        /// Lengths of keywords (0 for empty slots)
        uint8_t lengths[SlotSize];

        /// Token types for keywords
        TokenType types[SlotSize];

        /// Default constructor (all slots empty)
        constexpr Entry()
            : keywords{}, lengths{}, types{} {
            for (uint32_t i = 0; i < SlotSize; ++i) {
                keywords[i] = nullptr;
                lengths[i] = 0;
                types[i] = TokenType::IDENTIFIER;
            }
        }
    };

    // ========================================================================
    // Table Storage
    // ========================================================================

    /// The hash table (compile-time constant)
    std::array<Entry, TableSize> table;

    // ========================================================================
    // Lookup Function (HOT PATH)
    // ========================================================================

    /// Look up a keyword in the perfect hash table
    ///
    /// PERFORMANCE: This is the most performance-critical function.
    /// Optimizations:
    /// - Early exit for empty or too-long strings
    /// - Branchless uppercase conversion
    /// - Length comparison before string comparison
    /// - Linear probing with max 8 iterations
    /// - Inline optimization for Release builds
    ///
    /// @param text Input string (case-insensitive)
    /// @return Token type (IDENTIFIER if not found)
    [[nodiscard]] TokenType lookup(std::string_view text) const noexcept {
        // Early exit: empty or too long
        if (text.empty() || text.size() > MaxKeywordLength) {
            return TokenType::IDENTIFIER;
        }

        // Convert to uppercase inline (branchless optimization)
        char upper[MaxKeywordLength + 1];
        const size_t len = text.size();

        for (size_t i = 0; i < len; ++i) {
            upper[i] = to_upper_branchless(text[i]);
        }
        upper[len] = '\0';

        // Compute hash
        const uint32_t hash = HashFunc(std::string_view(upper, len), TableSize);
        const Entry& entry = table[hash];

        // Linear probing for collisions (max SlotSize iterations)
        for (uint32_t i = 0; i < SlotSize; ++i) {
            // Empty slot: keyword not found
            if (entry.keywords[i] == nullptr) {
                break;
            }

            // Length mismatch: fast rejection
            if (len != entry.lengths[i]) {
                continue;
            }

            // Compare strings (already uppercase)
            bool match = true;
            for (size_t j = 0; j < len; ++j) {
                if (upper[j] != entry.keywords[i][j]) {
                    match = false;
                    break;
                }
            }

            if (match) {
                return entry.types[i];
            }
        }

        // Not found: treat as identifier
        return TokenType::IDENTIFIER;
    }

    // ========================================================================
    // Builder API (for compile-time table generation)
    // ========================================================================

    /// Insert a keyword into the hash table (constexpr for compile-time generation)
    ///
    /// @param keyword Keyword string (must be uppercase)
    /// @param type Token type for this keyword
    /// @return true if inserted successfully, false if slot is full
    constexpr bool insert(const char* keyword, TokenType type) noexcept {
        const size_t len = std::string_view(keyword).size();
        if (len == 0 || len > MaxKeywordLength) {
            return false;
        }

        // Compute hash
        const uint32_t hash = HashFunc(std::string_view(keyword, len), TableSize);
        Entry& entry = table[hash];

        // Find empty slot in bucket
        for (uint32_t i = 0; i < SlotSize; ++i) {
            if (entry.keywords[i] == nullptr) {
                entry.keywords[i] = keyword;
                entry.lengths[i] = static_cast<uint8_t>(len);
                entry.types[i] = type;
                return true;
            }
        }

        // Slot full: collision overflow
        return false;
    }

    /// Get collision statistics (for table quality analysis)
    struct CollisionStats {
        uint32_t total_slots;           ///< Total number of hash slots
        uint32_t used_slots;             ///< Slots with at least one entry
        uint32_t collision_slots;        ///< Slots with >1 entry
        uint32_t max_collisions;         ///< Max entries in a single slot
        uint32_t total_keywords;         ///< Total keywords in table
    };

    [[nodiscard]] constexpr CollisionStats get_stats() const noexcept {
        CollisionStats stats{};
        stats.total_slots = TableSize;

        for (const auto& entry : table) {
            uint32_t slot_count = 0;
            for (uint32_t i = 0; i < SlotSize; ++i) {
                if (entry.keywords[i] != nullptr) {
                    ++slot_count;
                }
            }

            if (slot_count > 0) {
                ++stats.used_slots;
                stats.total_keywords += slot_count;

                if (slot_count > 1) {
                    ++stats.collision_slots;
                }

                if (slot_count > stats.max_collisions) {
                    stats.max_collisions = slot_count;
                }
            }
        }

        return stats;
    }
};

// ============================================================================
/// Convenience Type Alias
/// ============================================================================

/// Standard perfect hash table with default parameters
template<TokenTypeEnum TokenType>
using StandardHashTable = PerfectHashTable<TokenType, 256, 8, 32>;

// ============================================================================
/// Example: SQL Keyword Table (for documentation)
/// ============================================================================

#if 0  // Example only, not compiled

enum class ExampleTokenType : uint16_t {
    IDENTIFIER = 0,
    SELECT,
    FROM,
    WHERE,
    INSERT,
    UPDATE,
    DELETE,
    // ... more SQL keywords
};

// Compile-time keyword table generation
constexpr auto make_example_keyword_table() {
    PerfectHashTable<ExampleTokenType> table{};

    // Insert keywords (must be uppercase)
    table.insert("SELECT", ExampleTokenType::SELECT);
    table.insert("FROM", ExampleTokenType::FROM);
    table.insert("WHERE", ExampleTokenType::WHERE);
    table.insert("INSERT", ExampleTokenType::INSERT);
    table.insert("UPDATE", ExampleTokenType::UPDATE);
    table.insert("DELETE", ExampleTokenType::DELETE);
    // ... more keywords

    return table;
}

// Global constant table (zero runtime cost)
static constexpr auto KEYWORD_TABLE = make_example_keyword_table();

// Usage in tokenizer
void example_usage() {
    std::string_view text = "select";  // Lowercase input
    auto token_type = KEYWORD_TABLE.lookup(text);  // Returns ExampleTokenType::SELECT

    std::string_view ident = "my_column";
    auto ident_type = KEYWORD_TABLE.lookup(ident);  // Returns ExampleTokenType::IDENTIFIER
}

// Verify table quality at compile-time
static_assert(
    make_example_keyword_table().get_stats().max_collisions <= 8,
    "Hash table has too many collisions - increase SlotSize or change HashFunc"
);

#endif  // Example

// ============================================================================
/// Legacy API Wrapper (for libsqlglot compatibility)
/// ============================================================================

/// Wrapper class that matches libsqlglot's KeywordLookup API
///
/// This allows existing libsqlglot code to use the new generic perfect hash
/// implementation with minimal changes.
///
/// @tparam Table The PerfectHashTable instance (constexpr global)
template<typename TokenType, const auto& Table>
    requires std::same_as<TokenType, std::remove_cvref_t<decltype(Table.lookup(std::string_view{}))>>
class KeywordLookupWrapper {
public:
    [[nodiscard]] static TokenType lookup(std::string_view text) noexcept {
        return Table.lookup(text);
    }
};

} // namespace libglot
