#include "allocators.hpp"
#include "lifetime_tracker.hpp"
#include "test_framework.hpp"

#include <cstdint>
#include <cstring>
#include <set>
#include <stdexcept>
#include <vector>

using quant_test::Tracked;

TEST_CASE("01. ObjectPool exact capacity exhaustion and slot recycling") {
    ObjectPool<uint64_t, 8> pool;
    std::vector<uint64_t*> ptrs;
    std::set<uint64_t*> unique_addrs;

    for (size_t i = 0; i < 8; ++i) {
        uint64_t* p = pool.allocate();
        REQUIRE(p != nullptr);
        REQUIRE_EQ(reinterpret_cast<uintptr_t>(p) % alignof(uint64_t), uintptr_t{0});
        *p = i * 100;
        ptrs.push_back(p);
        unique_addrs.insert(p);
    }

    REQUIRE_EQ(unique_addrs.size(), size_t{8});
    REQUIRE_EQ(pool.allocate(), nullptr);  // Exhausted

    // Free odd indices then reallocate
    pool.deallocate(ptrs[1]);
    pool.deallocate(ptrs[5]);
    pool.deallocate(ptrs[3]);

    REQUIRE(pool.allocate() != nullptr);
    REQUIRE(pool.allocate() != nullptr);
    REQUIRE(pool.allocate() != nullptr);
    REQUIRE_EQ(pool.allocate(), nullptr);
}

TEST_CASE("02. ObjectPool handles tiny types sizeof(T) < sizeof(void*)") {
    ObjectPool<char, 16> char_pool;
    std::vector<char*> chars;
    for (int i = 0; i < 16; ++i) {
        char* c = char_pool.allocate();
        REQUIRE(c != nullptr);
        *c = static_cast<char>('A' + i);
        chars.push_back(c);
    }
    REQUIRE_EQ(char_pool.allocate(), nullptr);
    for (int i = 0; i < 16; ++i) {
        REQUIRE_EQ(*chars[static_cast<size_t>(i)], static_cast<char>('A' + i));
        char_pool.deallocate(chars[static_cast<size_t>(i)]);
    }
}

TEST_CASE("03. ObjectPool over-aligned 64B and non-default-constructible types") {
    struct alignas(64) AlignedOrder {
        int id;
        double px;
        AlignedOrder(int i, double p) : id(i), px(p) {}  // No default constructor!
    };

    ObjectPool<AlignedOrder, 4> pool;
    AlignedOrder* o1 = pool.construct(10, 99.5);
    AlignedOrder* o2 = pool.construct(11, 100.25);
    REQUIRE(o1 != nullptr);
    REQUIRE(o2 != nullptr);
    REQUIRE_EQ(reinterpret_cast<uintptr_t>(o1) % 64, uintptr_t{0});
    REQUIRE_EQ(reinterpret_cast<uintptr_t>(o2) % 64, uintptr_t{0});
    REQUIRE_EQ(o1->id, 10);
    REQUIRE_NEAR(o2->px, 100.25, 1e-12);

    pool.destroy(o1);
    pool.destroy(o2);
}

TEST_CASE("04. ObjectPool RAII construct/destroy and throwing constructor slot recovery") {
    Tracked::reset_counters();
    {
        ObjectPool<Tracked, 4> pool;
        // Raw pool construction must construct 0 Tracked objects upfront
        REQUIRE_EQ(Tracked::live.load(), 0L);

        Tracked* a = pool.construct(7);
        Tracked* b = pool.construct(9);
        REQUIRE_EQ(Tracked::live.load(), 2L);
        REQUIRE_EQ(a->value, 7);
        REQUIRE_EQ(b->value, 9);

        pool.destroy(a);
        REQUIRE_EQ(Tracked::live.load(), 1L);
        pool.destroy(b);
        REQUIRE_EQ(Tracked::live.load(), 0L);
    }

    struct ThrowOnOdd {
        explicit ThrowOnOdd(int x) {
            if (x % 2 != 0) throw std::runtime_error("odd");
        }
    };
    ObjectPool<ThrowOnOdd, 2> fragile_pool;
    REQUIRE_THROWS(fragile_pool.construct(1));
    // Slot must not be leaked when constructor throws!
    ThrowOnOdd* ok1 = fragile_pool.construct(2);
    ThrowOnOdd* ok2 = fragile_pool.construct(4);
    REQUIRE(ok1 != nullptr);
    REQUIRE(ok2 != nullptr);
    fragile_pool.destroy(ok1);
    fragile_pool.destroy(ok2);
}

TEST_CASE("05. Arena aligns true hardware addresses even when base pointer is misaligned") {
    alignas(64) uint8_t raw[512];
    // Intentionally pass base + 3 so naive offset-only alignment bugs fail immediately!
    Arena arena(raw + 3, 400);

    void* p1 = arena.alloc(5, 1);
    void* p16 = arena.alloc(24, 16);
    void* p64 = arena.alloc(64, 64);
    void* p8 = arena.alloc(16, 8);

    REQUIRE(p1 != nullptr);
    REQUIRE(p16 != nullptr);
    REQUIRE(p64 != nullptr);
    REQUIRE(p8 != nullptr);

    REQUIRE_EQ(reinterpret_cast<uintptr_t>(p16) % 16, uintptr_t{0});
    REQUIRE_EQ(reinterpret_cast<uintptr_t>(p64) % 64, uintptr_t{0});
    REQUIRE_EQ(reinterpret_cast<uintptr_t>(p8) % 8, uintptr_t{0});

    // Write distinct bit patterns to prove non-overlapping memory ranges
    std::memset(p1, 0x11, 5);
    std::memset(p16, 0x22, 24);
    std::memset(p64, 0x33, 64);
    std::memset(p8, 0x44, 16);

    REQUIRE_EQ(static_cast<uint8_t*>(p1)[4], uint8_t{0x11});
    REQUIRE_EQ(static_cast<uint8_t*>(p16)[23], uint8_t{0x22});
    REQUIRE_EQ(static_cast<uint8_t*>(p64)[63], uint8_t{0x33});
    REQUIRE_EQ(static_cast<uint8_t*>(p8)[15], uint8_t{0x44});
}

TEST_CASE("06. Arena overflow protection, exhaustion non-corruption, and reset()") {
    alignas(16) uint8_t buf[64];
    Arena arena(buf, sizeof(buf));

    REQUIRE_EQ(arena.alloc(SIZE_MAX, 8), nullptr);
    REQUIRE_EQ(arena.used(), size_t{0});

    void* a = arena.alloc(48, 8);
    REQUIRE(a != nullptr);
    REQUIRE_EQ(arena.alloc(32, 8), nullptr);  // Only 16 bytes remain; state must stay valid
    void* b = arena.alloc(16, 8);             // Exact remaining 16 bytes still succeed!
    REQUIRE(b != nullptr);
    REQUIRE_EQ(arena.alloc(1, 1), nullptr);

    arena.reset();
    REQUIRE_EQ(arena.used(), size_t{0});
    REQUIRE_EQ(arena.alloc(64, 16), static_cast<void*>(buf));
}

RUN_TEST_SUITE("04 — Object Pool & Arena Allocator")
