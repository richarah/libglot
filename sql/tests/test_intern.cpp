#include <libglot/util/intern.h>
#include <catch2/catch_test_macros.hpp>
#include <thread>
#include <vector>

using namespace libglot;

TEST_CASE("StringPool - Basic interning", "[intern]") {
    StringPool pool;

    SECTION("Intern single string") {
        const char* s1 = pool.intern("hello");
        REQUIRE(s1 != nullptr);
        REQUIRE(std::string_view(s1) == "hello");
    }

    SECTION("Intern same string twice returns same pointer") {
        const char* s1 = pool.intern("world");
        const char* s2 = pool.intern("world");
        REQUIRE(s1 == s2);  // Pointer equality!
    }

    SECTION("Different strings have different pointers") {
        const char* s1 = pool.intern("foo");
        const char* s2 = pool.intern("bar");
        REQUIRE(s1 != s2);
    }
}

TEST_CASE("StringPool - Pointer equality comparison", "[intern]") {
    StringPool pool;

    const char* s1 = pool.intern("test");
    const char* s2 = pool.intern("test");
    const char* s3 = pool.intern("different");

    REQUIRE(StringPool::equals(s1, s2));
    REQUIRE_FALSE(StringPool::equals(s1, s3));
}

TEST_CASE("StringPool - Size tracking", "[intern]") {
    StringPool pool;

    REQUIRE(pool.size() == 0);

    (void)pool.intern("one");
    REQUIRE(pool.size() == 1);

    (void)pool.intern("two");
    REQUIRE(pool.size() == 2);

    (void)pool.intern("one");  // Duplicate
    REQUIRE(pool.size() == 2);  // Size unchanged
}

TEST_CASE("StringPool - Clear", "[intern]") {
    StringPool pool;

    (void)pool.intern("a");
    (void)pool.intern("b");
    (void)pool.intern("c");

    REQUIRE(pool.size() == 3);

    pool.clear();
    REQUIRE(pool.size() == 0);
}

TEST_CASE("StringPool - Thread safety", "[intern][threading]") {
    StringPool pool;

    constexpr int num_threads = 10;
    constexpr int iterations = 1000;

    std::vector<std::thread> threads;

    // Each thread interns the same strings concurrently
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&pool]() {
            for (int i = 0; i < iterations; ++i) {
                (void)pool.intern("concurrent");
                (void)pool.intern("thread");
                (void)pool.intern("safe");
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Should only have 3 unique strings despite many threads
    REQUIRE(pool.size() == 3);
}

TEST_CASE("StringPool - Global singleton", "[intern]") {
    auto& pool1 = StringPool::instance();
    auto& pool2 = StringPool::instance();

    REQUIRE(&pool1 == &pool2);  // Same instance
}

TEST_CASE("LocalStringPool - Basic interning", "[intern][local]") {
    LocalStringPool pool;

    SECTION("Intern single string") {
        const char* s1 = pool.intern("hello");
        REQUIRE(s1 != nullptr);
        REQUIRE(std::string_view(s1) == "hello");
    }

    SECTION("Intern same string twice returns same pointer") {
        const char* s1 = pool.intern("world");
        const char* s2 = pool.intern("world");
        REQUIRE(s1 == s2);
    }

    SECTION("Size tracking") {
        REQUIRE(pool.size() == 0);
        (void)pool.intern("one");
        REQUIRE(pool.size() == 1);
        (void)pool.intern("two");
        REQUIRE(pool.size() == 2);
        (void)pool.intern("one");
        REQUIRE(pool.size() == 2);
    }
}

TEST_CASE("LocalStringPool - Performance (no locking)", "[intern][local][perf]") {
    LocalStringPool pool;

    // Intern many strings
    for (int i = 0; i < 10000; ++i) {
        std::string s = "string_" + std::to_string(i);
        (void)pool.intern(s);
    }

    REQUIRE(pool.size() == 10000);
}

TEST_CASE("LocalStringPool - Move semantics", "[intern][local]") {
    LocalStringPool pool1;
    (void)pool1.intern("test");
    REQUIRE(pool1.size() == 1);

    LocalStringPool pool2 = std::move(pool1);
    REQUIRE(pool2.size() == 1);
}
