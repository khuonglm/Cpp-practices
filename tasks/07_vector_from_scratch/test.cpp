#include "vector.hpp"
#include "lifetime_tracker.hpp"
#include "test_framework.hpp"

#include <cstdint>
#include <string>

using quant_test::FragileItem;
using quant_test::Tracked;

TEST_CASE("01. Non-default-constructible type support") {
    struct Quote {
        int id;
        double px;
        Quote(int i, double p) : id(i), px(p) {}  // Deleted implicit default ctor!
    };

    Vector<Quote> v;
    v.reserve(2);
    REQUIRE_EQ(v.size(), size_t{0});
    REQUIRE_EQ(v.capacity(), size_t{2});

    v.emplace_back(1, 101.25);
    v.emplace_back(2, 101.50);
    v.emplace_back(3, 101.75);  // Triggers reallocation of non-default-constructible type

    REQUIRE_EQ(v.size(), size_t{3});
    REQUIRE_EQ(v[0].id, 1);
    REQUIRE_NEAR(v[2].px, 101.75, 1e-12);
}

TEST_CASE("02. Exact destructor count and zero leaks with Tracked") {
    Tracked::reset_counters();
    {
        Vector<Tracked> v;
        for (int i = 0; i < 65; ++i) {
            v.emplace_back(i * 10);
        }
        REQUIRE_EQ(v.size(), size_t{65});
        REQUIRE_EQ(Tracked::live.load(), 65L);
        // Tracked has noexcept move constructor -> growth relocations must MOVE, never copy!
        REQUIRE_EQ(Tracked::copy_ctors.load(), 0L);

        v.pop_back();
        REQUIRE_EQ(v.size(), size_t{64});
        REQUIRE_EQ(Tracked::live.load(), 64L);

        const size_t saved_cap = v.capacity();
        v.clear();
        REQUIRE_EQ(v.size(), size_t{0});
        REQUIRE_EQ(v.capacity(), saved_cap);
        REQUIRE_EQ(Tracked::live.load(), 0L);
    }
    REQUIRE_EQ(Tracked::live.load(), 0L);
}

TEST_CASE("03. Self-aliasing push_back(v[0]) across reallocations") {
    Vector<std::string> v;
    // Non-SSO 128-byte heap string exposes use-after-move immediately under ASan!
    const std::string expected(128, 'H');
    v.push_back(expected);

    for (int i = 0; i < 40; ++i) {
        v.push_back(v[0]);
    }

    REQUIRE_EQ(v.size(), size_t{41});
    for (size_t i = 0; i < v.size(); ++i) {
        REQUIRE_EQ(v[i], expected);
    }
}

TEST_CASE("04. Rule of Five: copy, O(1) move, and self-assignment") {
    Vector<std::string> a;
    a.push_back("alpha");
    a.push_back("beta");
    a.push_back("gamma");

    Vector<std::string> copy(a);
    REQUIRE_EQ(copy.size(), size_t{3});
    REQUIRE_EQ(copy[1], std::string("beta"));
    copy[1] = "mutated";
    REQUIRE_EQ(a[1], std::string("beta"));  // Deep copy verified

    const std::string* raw_ptr = a.data();
    Vector<std::string> moved(std::move(a));
    REQUIRE_EQ(moved.data(), raw_ptr);      // O(1) pointer steal verified
    REQUIRE_EQ(a.size(), size_t{0});

    Vector<std::string>* alias = &moved;
    moved = *alias;
    moved = std::move(*alias);
    REQUIRE_EQ(moved.size(), size_t{3});
    REQUIRE_EQ(moved[2], std::string("gamma"));
}

TEST_CASE("05. Strong exception guarantee on throwing copy during reallocation") {
    FragileItem::live = 0;
    FragileItem::copies_remaining_before_throw = -1;

    {
        Vector<FragileItem> v;
        v.reserve(4);
        v.emplace_back(10);
        v.emplace_back(20);
        v.emplace_back(30);
        v.emplace_back(40);
        REQUIRE_EQ(FragileItem::live, 4);

        // Allow 2 copies during grow(), throw on the 3rd copy!
        FragileItem::copies_remaining_before_throw = 2;
        REQUIRE_THROWS(v.emplace_back(50));

        // Strong guarantee: original size, elements, and exact live object count preserved!
        FragileItem::copies_remaining_before_throw = -1;
        REQUIRE_EQ(v.size(), size_t{4});
        REQUIRE_EQ(v[0].value, 10);
        REQUIRE_EQ(v[1].value, 20);
        REQUIRE_EQ(v[2].value, 30);
        REQUIRE_EQ(v[3].value, 40);
        REQUIRE_EQ(FragileItem::live, 4);
    }
    REQUIRE_EQ(FragileItem::live, 0);
}

TEST_CASE("06. resize() grow/shrink and 64-byte over-aligned type support") {
    struct alignas(64) AlignedTick {
        int x = 7;
    };

    Vector<AlignedTick> v;
    v.resize(10);
    REQUIRE_EQ(v.size(), size_t{10});
    REQUIRE_EQ(reinterpret_cast<uintptr_t>(v.data()) % 64, uintptr_t{0});
    REQUIRE_EQ(v[9].x, 7);

    v.resize(3);
    REQUIRE_EQ(v.size(), size_t{3});
}

RUN_TEST_SUITE("07 — std::vector<T> from Scratch")
