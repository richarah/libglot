#pragma once
#include <memory>
#include <vector>
#include <cstring>
#include <cassert>
#include <cstdlib>

namespace libglot {

/// Monotonic arena allocator for AST nodes
/// All allocations live until the arena is destroyed
/// O(1) deallocation - just free the entire arena
/// Cache-friendly - nodes allocated contiguously
class Arena {
public:
    static constexpr size_t kDefaultChunkSize = 64 * 1024; // 64KB chunks
    static constexpr size_t kAlignment = alignof(std::max_align_t);

    explicit Arena(size_t chunk_size = kDefaultChunkSize)
        : chunk_size_(chunk_size)
        , current_chunk_(nullptr)
        , current_offset_(0)
        , current_capacity_(0)
    {
        allocate_chunk();
    }

    ~Arena() = default;

    // Non-copyable, movable
    Arena(const Arena&) = delete;
    Arena& operator=(const Arena&) = delete;
    Arena(Arena&&) noexcept = default;
    Arena& operator=(Arena&&) noexcept = default;

    /// Allocate `size` bytes with alignment `align`
    [[nodiscard]] void* allocate(size_t size, size_t align = kAlignment) {
        assert(size > 0);
        assert((align & (align - 1)) == 0); // power of 2

        // Check for integer overflow BEFORE doing arithmetic
        // Max reasonable allocation: 1GB
        constexpr size_t kMaxAllocation = 1024 * 1024 * 1024;
        if (size > kMaxAllocation) {
            throw std::bad_alloc();  // Request too large
        }

        // Check: current_offset_ + align - 1 won't overflow
        if (current_offset_ > SIZE_MAX - align) {
            throw std::bad_alloc();  // Alignment would overflow
        }

        // Align current offset safely
        size_t aligned_offset = (current_offset_ + align - 1) & ~(align - 1);

        // Check: aligned_offset + size won't overflow
        if (aligned_offset > SIZE_MAX - size) {
            throw std::bad_alloc();  // Size would overflow
        }

        // Check if we need a new chunk
        if (aligned_offset + size > current_capacity_) {
            // Check: size + align won't overflow for new chunk
            if (size > SIZE_MAX - align) {
                throw std::bad_alloc();
            }

            // Allocate new chunk (use max of chunk_size and requested size)
            size_t new_chunk_size = std::max(chunk_size_, size + align);
            allocate_chunk(new_chunk_size);
            aligned_offset = 0; // New chunk is already aligned
        }

        void* ptr = current_chunk_ + aligned_offset;
        current_offset_ = aligned_offset + size;
        return ptr;
    }

    /// Allocate and construct object of type T
    template<typename T, typename... Args>
    [[nodiscard]] T* create(Args&&... args) {
        void* mem = allocate(sizeof(T), alignof(T));
        return new (mem) T(std::forward<Args>(args)...);
    }

    /// Allocate array of T (uninitialized)
    template<typename T>
    [[nodiscard]] T* allocate_array(size_t count) {
        return static_cast<T*>(allocate(sizeof(T) * count, alignof(T)));
    }

    /// Total bytes allocated
    [[nodiscard]] size_t total_allocated() const {
        size_t total = 0;
        for (const auto& chunk : chunks_) {
            total += chunk.size;
        }
        return total;
    }

    /// Number of chunks
    [[nodiscard]] size_t chunk_count() const {
        return chunks_.size();
    }

    /// Copy source string into arena and return a string_view to it
    /// This ensures the source outlives all AST nodes allocated from this arena
    [[nodiscard]] std::string_view copy_source(std::string_view source) {
        if (source.empty()) {
            return {};
        }

        // Allocate space for source + null terminator (for safety with C APIs)
        char* buffer = static_cast<char*>(allocate(source.size() + 1, 1));
        std::memcpy(buffer, source.data(), source.size());
        buffer[source.size()] = '\0';

        return std::string_view(buffer, source.size());
    }

    /// Reset arena (reuse memory, invalidates all pointers)
    void reset() {
        current_chunk_ = chunks_.empty() ? nullptr : chunks_[0].data.get();
        current_offset_ = 0;
        current_capacity_ = chunks_.empty() ? 0 : chunks_[0].size;

        // Keep first chunk, discard rest
        if (chunks_.size() > 1) {
            chunks_.resize(1);
        }
    }

private:
    struct Chunk {
        std::unique_ptr<char[]> data;
        size_t size;
    };

    void allocate_chunk(size_t min_size = 0) {
        size_t size = std::max(chunk_size_, min_size);
        // Allocate extra space for alignment (worst case: need 63 extra bytes for 64-byte alignment)
        constexpr size_t max_align = 64;
        auto data = std::make_unique<char[]>(size + max_align);

        // Align the chunk pointer to max_align boundary
        char* raw_ptr = data.get();
        char* aligned_ptr = reinterpret_cast<char*>(
            (reinterpret_cast<uintptr_t>(raw_ptr) + max_align - 1) & ~(max_align - 1)
        );

        current_chunk_ = aligned_ptr;
        current_offset_ = 0;
        current_capacity_ = size;  // Usable capacity after alignment
        chunks_.push_back({std::move(data), size});
    }

    size_t chunk_size_;
    char* current_chunk_;
    size_t current_offset_;
    size_t current_capacity_;
    std::vector<Chunk> chunks_;
};

} // namespace libglot
