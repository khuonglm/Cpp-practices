#include "function.hpp"
#include "lifetime_tracker.hpp"
#include "test_framework.hpp"

#include <array>
#include <atomic>
#include <cstdlib>
#include <memory>
#include <new>

using quant_test::Tracked;

// Intercept heap allocations when not running under AddressSanitizer so we can
// mathematically prove SBO performs ZERO heap allocations for <= 32-byte callables!
#if !defined(__SANITIZE_ADDRESS__)
#if defined(__has_feature)
#if __has_feature(address_sanitizer)
#define QUANT_ASAN_ACTIVE 1
#endif
#endif
#endif

#ifndef QUANT_ASAN_ACTIVE
namespace {
std::atomic<long> g_alloc_count{0};
}

void* operator new(std::size_t sz) {
    g_alloc_count.fetch_add(1, std::memory_order_relaxed);
    if (void* p = std::malloc(sz ? sz : 1)) return p;
    throw std::bad_alloc();
}

void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
#endif

namespace {
int multiply_add(int a, int b) { return a * b + 5; }
}

TEST_CASE("01. Empty Function state throws std::bad_function_call") {
    Function<int(int)> empty;
    REQUIRE(!empty);
    REQUIRE_THROWS(empty(10));

    int (*null_fp)(int) = nullptr;
    Function<int(int)> from_null_ptr(null_fp);
    // Either empty or handles cleanly; nullptr_t ctor must definitely be empty:
    Function<int(int)> explicit_null(nullptr);
    REQUIRE(!explicit_null);
    (void)from_null_ptr;
}

TEST_CASE("02. Free function pointer, void return, reference & move-only args") {
    Function<int(int, int)> fn(&multiply_add);
    REQUIRE(static_cast<bool>(fn));
    REQUIRE_EQ(fn(6, 7), 47);

    // Reference mutation + void return
    Function<void(int&)> mutator([](int& x) { x += 100; });
    int val = 23;
    mutator(val);
    REQUIRE_EQ(val, 123);

    // Move-only capture (unique_ptr) AND move-only argument (unique_ptr)!
    auto captured = std::make_unique<int>(50);
    Function<int(std::unique_ptr<int>)> move_only_fn(
        [cap = std::move(captured)](std::unique_ptr<int> arg) { return *cap + *arg; });
    REQUIRE_EQ(move_only_fn(std::make_unique<int>(25)), 75);
}

TEST_CASE("03. Small <= 32-byte capture uses inline SBO with ZERO heap allocations") {
    // 4 x uint64_t = exact 32 bytes!
    struct SmallCapture {
        uint64_t a, b, c, d;
        uint64_t operator()(uint64_t x) const noexcept { return a + b + c + d + x; }
    };
    static_assert(sizeof(SmallCapture) == 32);

#ifndef QUANT_ASAN_ACTIVE
    const long before = g_alloc_count.load();
#endif

    Function<uint64_t(uint64_t)> f1(SmallCapture{10, 20, 30, 40});
    Function<uint64_t(uint64_t)> f2(std::move(f1));
    REQUIRE(!f1);
    REQUIRE_EQ(f2(5), uint64_t{105});

#ifndef QUANT_ASAN_ACTIVE
    const long after = g_alloc_count.load();
    REQUIRE_EQ(after - before, 0L);  // Strict proof: 0 mallocs!
#endif
}

TEST_CASE("04. Large > 32-byte functor heap-allocates once and moves with zero extra allocs") {
    struct LargeCallable {
        std::array<uint64_t, 8> weights;  // 64 bytes > kBufferSize (32)
        uint64_t operator()(uint64_t scale) const {
            uint64_t sum = 0;
            for (uint64_t w : weights) sum += w * scale;
            return sum;
        }
    };

#ifndef QUANT_ASAN_ACTIVE
    const long before = g_alloc_count.load();
#endif

    Function<uint64_t(uint64_t)> big1(LargeCallable{{1, 2, 3, 4, 5, 6, 7, 8}});

#ifndef QUANT_ASAN_ACTIVE
    const long after_ctor = g_alloc_count.load();
    REQUIRE_EQ(after_ctor - before, 1L);
#endif

    Function<uint64_t(uint64_t)> big2(std::move(big1));
    REQUIRE(!big1);

#ifndef QUANT_ASAN_ACTIVE
    // Moving heap-backed Function must steal pointer with ZERO extra heap allocations!
    REQUIRE_EQ(g_alloc_count.load() - after_ctor, 0L);
#endif

    REQUIRE_EQ(big2(2), uint64_t{72});
}

TEST_CASE("05. Exact RAII destructor lifecycle across inline & heap move assignments") {
    Tracked::reset_counters();
    {
        Function<int()> inline_fn([t = Tracked(11)] { return t.value; });
        REQUIRE_EQ(Tracked::live.load(), 1L);

        struct BigHolder {
            Tracked t;
            char pad[64]{};
            explicit BigHolder(int v) : t(v) {}
            int operator()() const { return t.value; }
        };

        Function<int()> heap_fn(BigHolder(22));
        REQUIRE_EQ(Tracked::live.load(), 2L);
        REQUIRE_EQ(inline_fn(), 11);
        REQUIRE_EQ(heap_fn(), 22);

        // Overwrite inline Function with heap Function -> old inline Tracked(11) immediately destroyed!
        inline_fn = std::move(heap_fn);
        REQUIRE_EQ(Tracked::live.load(), 1L);
        REQUIRE_EQ(inline_fn(), 22);
    }
    REQUIRE_EQ(Tracked::live.load(), 0L);
}

RUN_TEST_SUITE("13 — Type-Erased SBO Function<R(Args...)>")
