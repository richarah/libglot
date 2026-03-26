#pragma once
#include <string>
#include <string_view>
#include <unordered_set>
#include <shared_mutex>
#include <mutex>
#include <memory>

namespace libglot {

/// Thread-safe string interning pool
/// All identifiers, keywords, and frequently-used strings are interned
/// Enables O(1) string comparison via pointer equality
/// Strings live forever (until pool is destroyed)
class StringPool {
public:
    StringPool() = default;
    ~StringPool() = default;

    // Non-copyable, non-movable (global singleton pattern)
    StringPool(const StringPool&) = delete;
    StringPool& operator=(const StringPool&) = delete;
    StringPool(StringPool&&) = delete;
    StringPool& operator=(StringPool&&) = delete;

    /// Intern a string - returns pointer to canonical copy
    /// Thread-safe, multiple threads can intern concurrently
    [[nodiscard]] const char* intern(std::string_view str) {
        // Fast path: check if already interned (shared lock)
        {
            std::shared_lock<std::shared_mutex> lock(mutex_);
            auto it = pool_.find(str);
            if (it != pool_.end()) {
                return it->c_str();
            }
        }

        // Slow path: insert new string (exclusive lock)
        {
            std::unique_lock<std::shared_mutex> lock(mutex_);
            // Double-check after acquiring exclusive lock (another thread may have inserted)
            auto it = pool_.find(str);
            if (it != pool_.end()) {
                return it->c_str();
            }

            // Insert and return pointer to the newly interned string
            auto [inserted_it, success] = pool_.emplace(str);
            return inserted_it->c_str();
        }
    }

    /// Check if two interned strings are equal (pointer comparison)
    [[nodiscard]] static bool equals(const char* a, const char* b) {
        return a == b;
    }

    /// Number of unique strings in pool
    [[nodiscard]] size_t size() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return pool_.size();
    }

    /// Clear the pool (invalidates all previously interned pointers!)
    void clear() {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        pool_.clear();
    }

    /// Get global singleton instance
    static StringPool& instance() {
        static StringPool pool;
        return pool;
    }

private:
    // Custom hash for string_view to avoid allocation
    struct StringViewHash {
        using is_transparent = void;

        size_t operator()(std::string_view sv) const {
            return std::hash<std::string_view>{}(sv);
        }

        size_t operator()(const std::string& s) const {
            return std::hash<std::string>{}(s);
        }
    };

    // Custom equality
    struct StringViewEqual {
        using is_transparent = void;

        bool operator()(const std::string& a, const std::string& b) const {
            return a == b;
        }

        bool operator()(const std::string& a, std::string_view b) const {
            return a == b;
        }

        bool operator()(std::string_view a, const std::string& b) const {
            return a == b;
        }

        bool operator()(std::string_view a, std::string_view b) const {
            return a == b;
        }
    };

    mutable std::shared_mutex mutex_;
    std::unordered_set<std::string, StringViewHash, StringViewEqual> pool_;
};

/// RAII wrapper for local string pool (for single-threaded use cases)
/// Faster than global pool - no locking overhead
class LocalStringPool {
public:
    LocalStringPool() = default;
    ~LocalStringPool() = default;

    LocalStringPool(const LocalStringPool&) = delete;
    LocalStringPool& operator=(const LocalStringPool&) = delete;
    LocalStringPool(LocalStringPool&&) noexcept = default;
    LocalStringPool& operator=(LocalStringPool&&) noexcept = default;

    /// Intern a string - no locking, single-threaded only
    [[nodiscard]] const char* intern(std::string_view str) {
        auto it = pool_.find(str);
        if (it != pool_.end()) {
            return it->c_str();
        }

        auto [inserted_it, success] = pool_.emplace(str);
        return inserted_it->c_str();
    }

    /// Number of unique strings
    [[nodiscard]] size_t size() const {
        return pool_.size();
    }

    /// Clear the pool
    void clear() {
        pool_.clear();
    }

private:
    struct StringViewHash {
        using is_transparent = void;
        size_t operator()(std::string_view sv) const {
            return std::hash<std::string_view>{}(sv);
        }
        size_t operator()(const std::string& s) const {
            return std::hash<std::string>{}(s);
        }
    };

    struct StringViewEqual {
        using is_transparent = void;
        bool operator()(const std::string& a, const std::string& b) const { return a == b; }
        bool operator()(const std::string& a, std::string_view b) const { return a == b; }
        bool operator()(std::string_view a, const std::string& b) const { return a == b; }
        bool operator()(std::string_view a, std::string_view b) const { return a == b; }
    };

    std::unordered_set<std::string, StringViewHash, StringViewEqual> pool_;
};

} // namespace libglot
