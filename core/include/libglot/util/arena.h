#pragma once
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <string_view>
#include <type_traits>
#include <vector>

namespace libglot {

/// Monotonic arena allocator for AST nodes.
///
/// All allocations live until the arena is destroyed or reset(). Freeing is
/// O(chunks) plus one destructor call per non-trivially-destructible object
/// created via create<T>(); trivially destructible objects (the common case
/// for hot-path nodes) carry no bookkeeping at all.
class Arena {
public:
    static constexpr size_t kDefaultChunkSize = size_t{64} * 1024; // 64KB chunks
    static constexpr size_t kAlignment = alignof(std::max_align_t);
    /// Chunks are over-allocated and aligned to this boundary; it is also the
    /// maximum alignment allocate() supports.
    static constexpr size_t kMaxAlignment = 64;

    explicit Arena(size_t chunk_size = kDefaultChunkSize)
        : chunk_size_(chunk_size), current_chunk_(nullptr), current_offset_(0),
          current_capacity_(0) {
        allocate_chunk();
    }

    ~Arena() { run_finalizers(); }

    // Non-copyable. Movable: the moved-from arena is left empty and unusable
    // until reassigned (its chunk pointer is nulled so it cannot corrupt the
    // destination's memory).
    Arena(const Arena&) = delete;
    Arena& operator=(const Arena&) = delete;

    Arena(Arena&& other) noexcept
        : chunk_size_(other.chunk_size_), current_chunk_(other.current_chunk_),
          current_offset_(other.current_offset_), current_capacity_(other.current_capacity_),
          chunks_(std::move(other.chunks_)), finalizers_(std::move(other.finalizers_)) {
        other.current_chunk_ = nullptr;
        other.current_offset_ = 0;
        other.current_capacity_ = 0;
        other.chunks_.clear();
        other.finalizers_.clear();
    }

    Arena& operator=(Arena&& other) noexcept {
        if (this != &other) {
            run_finalizers();
            chunk_size_ = other.chunk_size_;
            current_chunk_ = other.current_chunk_;
            current_offset_ = other.current_offset_;
            current_capacity_ = other.current_capacity_;
            chunks_ = std::move(other.chunks_);
            finalizers_ = std::move(other.finalizers_);
            other.current_chunk_ = nullptr;
            other.current_offset_ = 0;
            other.current_capacity_ = 0;
            other.chunks_.clear();
            other.finalizers_.clear();
        }
        return *this;
    }

    /// Allocate `size` bytes with alignment `align` (align <= kMaxAlignment)
    [[nodiscard]] void* allocate(size_t size, size_t align = kAlignment) {
        assert(size > 0);
        assert((align & (align - 1)) == 0); // power of 2
        assert(align <= kMaxAlignment);

        // Check for integer overflow BEFORE doing arithmetic
        // Max reasonable allocation: 1GB
        constexpr size_t kMaxAllocation = size_t{1024} * 1024 * 1024;
        if (size > kMaxAllocation || align > kMaxAlignment) {
            throw std::bad_alloc();
        }

        // Align current offset (overflow-safe: offset and align are both
        // bounded well below SIZE_MAX by the checks above and chunk sizes)
        size_t aligned_offset = (current_offset_ + align - 1) & ~(align - 1);

        // Check if we need a new chunk
        if (aligned_offset + size > current_capacity_) {
            size_t new_chunk_size = std::max(chunk_size_, size);
            allocate_chunk(new_chunk_size);
            aligned_offset = 0; // Fresh chunks are aligned to kMaxAlignment
        }

        void* ptr = current_chunk_ + aligned_offset;
        current_offset_ = aligned_offset + size;
        return ptr;
    }

    /// Allocate and construct object of type T.
    ///
    /// Non-trivially-destructible objects are registered so their destructors
    /// run at reset()/destruction; trivially destructible ones are not
    /// tracked (zero overhead).
    template<typename T, typename... Args>
    [[nodiscard]] T* create(Args&&... args) {
        void* mem = allocate(sizeof(T), alignof(T));
        T* obj = new (mem) T(std::forward<Args>(args)...);
        if constexpr (!std::is_trivially_destructible_v<T>) {
            try {
                finalizers_.push_back(Finalizer{[](void* p) { static_cast<T*>(p)->~T(); }, obj});
            } catch (...) {
                obj->~T();
                throw;
            }
        }
        return obj;
    }

    /// Allocate array of trivially destructible T (uninitialized)
    template<typename T>
    [[nodiscard]] T* allocate_array(size_t count) {
        static_assert(std::is_trivially_destructible_v<T>,
                      "allocate_array does not run destructors");
        if (count != 0 && sizeof(T) > SIZE_MAX / count) {
            throw std::bad_alloc(); // count * sizeof(T) would overflow
        }
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
    [[nodiscard]] size_t chunk_count() const { return chunks_.size(); }

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

    /// Reset arena (reuse memory, runs destructors, invalidates all pointers)
    void reset() {
        run_finalizers();

        // Keep first chunk, discard rest
        if (chunks_.size() > 1) {
            chunks_.resize(1);
        }

        if (chunks_.empty()) {
            current_chunk_ = nullptr;
            current_offset_ = 0;
            current_capacity_ = 0;
        } else {
            // Restore the *aligned* base established at allocation time, not
            // the raw pointer -- otherwise the alignment invariant silently
            // breaks after reset.
            current_chunk_ = aligned_base(chunks_[0].data.get());
            current_offset_ = 0;
            current_capacity_ = chunks_[0].size;
        }
    }

private:
    struct Chunk {
        std::unique_ptr<char[]> data;
        size_t size;
    };

    struct Finalizer {
        void (*destroy)(void*);
        void* object;
    };

    static char* aligned_base(char* raw_ptr) noexcept {
        return reinterpret_cast<char*>((reinterpret_cast<uintptr_t>(raw_ptr) + kMaxAlignment - 1) &
                                       ~(kMaxAlignment - 1));
    }

    void run_finalizers() noexcept {
        // Reverse order of construction, matching normal destruction order.
        for (auto it = finalizers_.rbegin(); it != finalizers_.rend(); ++it) {
            it->destroy(it->object);
        }
        finalizers_.clear();
    }

    void allocate_chunk(size_t min_size = 0) {
        size_t size = std::max(chunk_size_, min_size);
        // Over-allocate so the usable region can be aligned to kMaxAlignment.
        auto data = std::make_unique<char[]>(size + kMaxAlignment);

        current_chunk_ = aligned_base(data.get());
        current_offset_ = 0;
        current_capacity_ = size; // Usable capacity after alignment
        chunks_.push_back({std::move(data), size});
    }

    size_t chunk_size_;
    char* current_chunk_;
    size_t current_offset_;
    size_t current_capacity_;
    std::vector<Chunk> chunks_;
    std::vector<Finalizer> finalizers_;
};

} // namespace libglot
