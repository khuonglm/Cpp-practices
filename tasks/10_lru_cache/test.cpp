#include "lru_cache.hpp"
#include "test_framework.hpp"

#include <string>
#include <thread>
#include <vector>

TEST_CASE("01. Basic hit, miss, and strict LRU eviction order") {
    LruCache<int, std::string> cache(3);
    REQUIRE(!cache.get(1).has_value());

    cache.put(1, "one");
    cache.put(2, "two");
    cache.put(3, "three");
    REQUIRE_EQ(cache.size(), size_t{3});

    cache.put(4, "four");  // Evicts key 1
    REQUIRE(!cache.get(1).has_value());
    REQUIRE_EQ(cache.get(2), std::optional<std::string>{"two"});
    REQUIRE_EQ(cache.get(3), std::optional<std::string>{"three"});
    REQUIRE_EQ(cache.get(4), std::optional<std::string>{"four"});
}

TEST_CASE("02. get() promotes key to MRU and put() update never falsely evicts") {
    LruCache<int, int> cache(3);
    cache.put(10, 100);
    cache.put(20, 200);
    cache.put(30, 300);

    // Promote 10 -> order is now [10, 30, 20] (20 is LRU)
    REQUIRE_EQ(cache.get(10), std::optional<int>{100});

    // Updating existing key 30 at full capacity MUST NOT evict 20!
    cache.put(30, 333);
    REQUIRE_EQ(cache.size(), size_t{3});
    REQUIRE_EQ(cache.get(20), std::optional<int>{200});  // Now 20 promoted -> [20, 30, 10]

    // Inserting 40 now evicts 10!
    cache.put(40, 400);
    REQUIRE(!cache.get(10).has_value());
    REQUIRE_EQ(cache.get(20), std::optional<int>{200});
    REQUIRE_EQ(cache.get(30), std::optional<int>{333});
    REQUIRE_EQ(cache.get(40), std::optional<int>{400});
}

TEST_CASE("03. Capacity 1 boundary behavior") {
    LruCache<std::string, int> tiny(1);
    tiny.put("ES", 5200);
    REQUIRE_EQ(tiny.get("ES"), std::optional<int>{5200});

    tiny.put("NQ", 18100);
    REQUIRE(!tiny.get("ES").has_value());
    REQUIRE_EQ(tiny.get("NQ"), std::optional<int>{18100});
}

TEST_CASE("04. 8-thread concurrent LruCache stress test under TSan") {
    LruCache<int, int> cache(32);
    cache.put(0, 0);
    std::vector<std::thread> threads;

    for (int t = 0; t < 8; ++t) {
        threads.emplace_back([&cache, t] {
            for (int i = 0; i < 15'000; ++i) {
                const int key = (i + t * 7) % 64;
                if (i % 3 == 0) {
                    cache.put(key, key * 10);
                } else {
                    auto res = cache.get(key);
                    if (res.has_value()) {
                        // Value for key k is always k * 10 (proves zero torn/corrupt reads)
                        if (*res != key * 10) std::abort();
                    }
                }
            }
        });
    }
    for (auto& th : threads) th.join();
    REQUIRE(cache.size() <= size_t{32});
}

TEST_CASE("05. ShardedLruCache multi-threaded consistency and sharding") {
    ShardedLruCache<int, int, 16> sharded(256);

    for (int k = 0; k < 64; ++k) {
        sharded.put(k, k + 1000);
    }
    for (int k = 0; k < 64; ++k) {
        REQUIRE_EQ(sharded.get(k), std::optional<int>{k + 1000});
    }

    std::vector<std::thread> workers;
    for (int t = 0; t < 8; ++t) {
        workers.emplace_back([&sharded, t] {
            for (int i = 0; i < 20'000; ++i) {
                const int k = (i * 13 + t) % 512;
                sharded.put(k, k * 2);
                auto v = sharded.get(k);
                if (v.has_value() && *v != k * 2) std::abort();
            }
        });
    }
    for (auto& w : workers) w.join();
}

RUN_TEST_SUITE("10 — Thread-Safe & Sharded LRU Cache")
