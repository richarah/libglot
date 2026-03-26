#include <libglot/util/arena.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

TEST_CASE("libglot::Arena - Basic allocation", "[arena]") {
    libglot::Arena arena;

    SECTION("Single allocation") {
        void* ptr = arena.allocate(100);
        REQUIRE(ptr != nullptr);
        REQUIRE(arena.total_allocated() >= 100);
    }

    SECTION("Multiple allocations") {
        void* ptr1 = arena.allocate(50);
        void* ptr2 = arena.allocate(100);
        void* ptr3 = arena.allocate(200);

        REQUIRE(ptr1 != nullptr);
        REQUIRE(ptr2 != nullptr);
        REQUIRE(ptr3 != nullptr);
        REQUIRE(ptr1 != ptr2);
        REQUIRE(ptr2 != ptr3);
    }

    SECTION("Aligned allocation") {
        void* ptr = arena.allocate(100, 64);
        REQUIRE(ptr != nullptr);
        REQUIRE(reinterpret_cast<uintptr_t>(ptr) % 64 == 0);
    }
}

TEST_CASE("libglot::Arena - Object creation", "[arena]") {
    libglot::Arena arena;

    SECTION("Create simple struct") {
        struct TestStruct {
            int x;
            double y;
            TestStruct(int x_, double y_) : x(x_), y(y_) {}
        };

        auto* obj = arena.create<TestStruct>(42, 3.14);
        REQUIRE(obj != nullptr);
        REQUIRE(obj->x == 42);
        REQUIRE(obj->y == Catch::Approx(3.14));
    }

    SECTION("Create multiple objects") {
        auto* i1 = arena.create<int>(10);
        auto* i2 = arena.create<int>(20);
        auto* i3 = arena.create<int>(30);

        REQUIRE(*i1 == 10);
        REQUIRE(*i2 == 20);
        REQUIRE(*i3 == 30);
    }
}

TEST_CASE("libglot::Arena - Array allocation", "[arena]") {
    libglot::Arena arena;

    SECTION("Allocate int array") {
        int* arr = arena.allocate_array<int>(100);
        REQUIRE(arr != nullptr);

        // Write to array
        for (int i = 0; i < 100; ++i) {
            arr[i] = i;
        }

        // Read back
        for (int i = 0; i < 100; ++i) {
            REQUIRE(arr[i] == i);
        }
    }

    SECTION("Allocate struct array") {
        struct Point {
            float x, y;
        };

        Point* points = arena.allocate_array<Point>(50);
        REQUIRE(points != nullptr);

        points[0] = {1.0f, 2.0f};
        points[49] = {99.0f, 100.0f};

        REQUIRE(points[0].x == Catch::Approx(1.0f));
        REQUIRE(points[49].y == Catch::Approx(100.0f));
    }
}

TEST_CASE("libglot::Arena - Large allocation (multiple chunks)", "[arena]") {
    libglot::Arena arena(1024); // Small chunk size to force multiple chunks

    // Allocate more than one chunk
    void* ptr1 = arena.allocate(2000);
    void* ptr2 = arena.allocate(2000);
    void* ptr3 = arena.allocate(2000);

    REQUIRE(ptr1 != nullptr);
    REQUIRE(ptr2 != nullptr);
    REQUIRE(ptr3 != nullptr);
    REQUIRE(arena.chunk_count() >= 3);
}

TEST_CASE("libglot::Arena - Reset", "[arena]") {
    libglot::Arena arena;

    // Allocate some memory
    [[maybe_unused]] void* ptr1 = arena.allocate(1000);
    [[maybe_unused]] void* ptr2 = arena.allocate(1000);

    size_t allocated_before = arena.total_allocated();
    REQUIRE(allocated_before >= 2000);

    // Reset arena
    arena.reset();

    // Should reuse memory
    [[maybe_unused]] void* ptr3 = arena.allocate(500);
    REQUIRE(arena.chunk_count() == 1);
}

TEST_CASE("libglot::Arena - Stress test", "[arena][stress]") {
    libglot::Arena arena;

    // Allocate many small objects
    for (int i = 0; i < 10000; ++i) {
        auto* obj = arena.create<int>(i);
        REQUIRE(*obj == i);
    }

    REQUIRE(arena.total_allocated() >= 10000 * sizeof(int));
}

TEST_CASE("libglot::Arena - Alignment edge cases", "[arena]") {
    libglot::Arena arena(256); // Small chunk

    SECTION("Various alignments") {
        void* ptr1 = arena.allocate(1, 1);
        void* ptr2 = arena.allocate(1, 2);
        void* ptr4 = arena.allocate(1, 4);
        void* ptr8 = arena.allocate(1, 8);
        void* ptr16 = arena.allocate(1, 16);
        void* ptr32 = arena.allocate(1, 32);
        void* ptr64 = arena.allocate(1, 64);

        REQUIRE(reinterpret_cast<uintptr_t>(ptr1) % 1 == 0);
        REQUIRE(reinterpret_cast<uintptr_t>(ptr2) % 2 == 0);
        REQUIRE(reinterpret_cast<uintptr_t>(ptr4) % 4 == 0);
        REQUIRE(reinterpret_cast<uintptr_t>(ptr8) % 8 == 0);
        REQUIRE(reinterpret_cast<uintptr_t>(ptr16) % 16 == 0);
        REQUIRE(reinterpret_cast<uintptr_t>(ptr32) % 32 == 0);
        REQUIRE(reinterpret_cast<uintptr_t>(ptr64) % 64 == 0);
    }
}
