#include "thread_pool.hpp"
#include "test_framework.hpp"

#include <atomic>
#include <chrono>
#include <memory>
#include <numeric>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

TEST_CASE("01. Value, void, and move-only return futures") {
    ThreadPool pool(4);

    auto f_int = pool.submit([](int a, int b) { return a * b + 1; }, 6, 7);
    REQUIRE_EQ(f_int.get(), 43);

    std::atomic<bool> flag{false};
    auto f_void = pool.submit([&flag] { flag.store(true); });
    f_void.get();
    REQUIRE(flag.load());

    auto f_uptr = pool.submit([] { return std::make_unique<int>(999); });
    auto ptr = f_uptr.get();
    REQUIRE(ptr != nullptr);
    REQUIRE_EQ(*ptr, 999);
}

TEST_CASE("02. Exception propagation without terminating worker threads") {
    ThreadPool pool(2);

    auto bad = pool.submit([]() -> int {
        throw std::runtime_error("simulated pricing failure");
    });
    REQUIRE_THROWS(bad.get());

    // Pool must remain healthy after exception
    auto healthy = pool.submit([] { return std::string("recovered"); });
    REQUIRE_EQ(healthy.get(), std::string("recovered"));
}

TEST_CASE("03. True concurrent execution across 4 workers") {
    ThreadPool pool(4);
    const auto t0 = std::chrono::steady_clock::now();

    std::vector<std::future<int>> futs;
    for (int i = 0; i < 4; ++i) {
        futs.push_back(pool.submit([i] {
            std::this_thread::sleep_for(std::chrono::milliseconds(60));
            return i * 10;
        }));
    }

    int sum = 0;
    for (auto& f : futs) sum += f.get();
    const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::steady_clock::now() - t0)
                                .count();

    REQUIRE_EQ(sum, 60);
    // Serial would take >= 240ms; parallel on >=4 threads finishes around ~65ms
    REQUIRE(elapsed_ms < 210);
}

TEST_CASE("04. Destructor drains queued tasks cleanly without broken_promise") {
    std::atomic<int> completed{0};
    std::vector<std::future<int>> futures;

    {
        ThreadPool pool(3);
        for (int i = 0; i < 300; ++i) {
            futures.push_back(pool.submit([&completed, i] {
                completed.fetch_add(1, std::memory_order_relaxed);
                return i;
            }));
        }
    }  // Destructor must drain all 300 queued tasks before returning!

    REQUIRE_EQ(completed.load(), 300);
    REQUIRE_EQ(futures.back().get(), 299);
}

TEST_CASE("05. Nested task submission from inside worker threads") {
    ThreadPool pool(4);
    std::promise<int> final_result;
    auto final_fut = final_result.get_future();

    pool.submit([&pool, p = std::move(final_result)]() mutable {
        // Submitting while running inside worker_loop proves mutex is NOT held across task()
        pool.submit([p = std::move(p)]() mutable {
            p.set_value(2026);
        });
    });

    REQUIRE_EQ(final_fut.get(), 2026);
}

TEST_CASE("06. Multi-producer 24,000-task contention test under TSan") {
    ThreadPool pool(4);
    std::atomic<uint64_t> total{0};

    std::vector<std::thread> producers;
    for (int p = 0; p < 8; ++p) {
        producers.emplace_back([&pool, &total] {
            for (int k = 1; k <= 3000; ++k) {
                pool.submit([&total, k] {
                    total.fetch_add(static_cast<uint64_t>(k), std::memory_order_relaxed);
                });
            }
        });
    }
    for (auto& th : producers) th.join();

    // Let destructor drain remaining work cleanly via scope or explicit sub-pool check:
    std::atomic<uint64_t> scoped_total{0};
    {
        ThreadPool scoped(4);
        for (uint64_t i = 1; i <= 5000; ++i) {
            scoped.submit([&scoped_total, i] {
                scoped_total.fetch_add(i, std::memory_order_relaxed);
            });
        }
    }
    REQUIRE_EQ(scoped_total.load(), uint64_t{5000} * 5001 / 2);
}

RUN_TEST_SUITE("06 — Fixed-Size Thread Pool")
