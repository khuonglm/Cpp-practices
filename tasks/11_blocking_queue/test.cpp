#include "blocking_queue.hpp"
#include "test_framework.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

TEST_CASE("01. Non-blocking try_push / try_pop capacity bounds") {
    BlockingQueue<int> q(2);
    REQUIRE(!q.try_pop().has_value());
    REQUIRE(q.try_push(11));
    REQUIRE(q.try_push(22));
    REQUIRE(!q.try_push(33));  // Capacity 2 reached

    REQUIRE_EQ(q.try_pop(), std::optional<int>{11});
    REQUIRE(q.try_push(33));
    REQUIRE_EQ(q.try_pop(), std::optional<int>{22});
    REQUIRE_EQ(q.try_pop(), std::optional<int>{33});
    REQUIRE(!q.try_pop().has_value());
}

TEST_CASE("02. Move-only element transfer (std::unique_ptr)") {
    BlockingQueue<std::unique_ptr<int>> q(4);
    REQUIRE(q.push(std::make_unique<int>(777)));
    auto out = q.pop();
    REQUIRE(out.has_value());
    REQUIRE_EQ(**out, 777);
}

TEST_CASE("03. Backpressure blocks full push until consumer pops") {
    BlockingQueue<int> q(2);
    REQUIRE(q.push(1));
    REQUIRE(q.push(2));

    std::atomic<bool> third_pushed{false};
    std::thread producer([&] {
        REQUIRE(q.push(3));  // Must block until pop() occurs!
        third_pushed.store(true, std::memory_order_release);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    REQUIRE(!third_pushed.load(std::memory_order_acquire));

    REQUIRE_EQ(q.pop(), std::optional<int>{1});
    producer.join();
    REQUIRE(third_pushed.load());
    REQUIRE_EQ(q.pop(), std::optional<int>{2});
    REQUIRE_EQ(q.pop(), std::optional<int>{3});
}

TEST_CASE("04. close() rejects new pushes while draining queued items in FIFO order") {
    BlockingQueue<int> q(8);
    REQUIRE(q.push(10));
    REQUIRE(q.push(20));
    REQUIRE(q.push(30));

    q.close();
    REQUIRE(q.is_closed());
    REQUIRE(!q.push(40));
    REQUIRE(!q.try_push(50));

    // Remaining 3 items MUST drain completely before nullopt is returned!
    REQUIRE_EQ(q.pop(), std::optional<int>{10});
    REQUIRE_EQ(q.pop(), std::optional<int>{20});
    REQUIRE_EQ(q.pop(), std::optional<int>{30});
    REQUIRE_EQ(q.pop(), std::nullopt);
    REQUIRE_EQ(q.try_pop(), std::nullopt);
}

TEST_CASE("05. close() unblocks all waiting consumers and waiting full producers") {
    // Part A: Unblock stalled consumers on empty queue
    BlockingQueue<int> empty_q(4);
    std::vector<std::thread> consumers;
    std::atomic<int> woken_consumers{0};
    for (int i = 0; i < 4; ++i) {
        consumers.emplace_back([&] {
            if (!empty_q.pop().has_value()) {
                woken_consumers.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    empty_q.close();
    for (auto& c : consumers) c.join();
    REQUIRE_EQ(woken_consumers.load(), 4);

    // Part B: Unblock stalled producers on full queue
    BlockingQueue<int> full_q(1);
    REQUIRE(full_q.push(99));
    std::vector<std::thread> producers;
    std::atomic<int> rejected_producers{0};
    for (int i = 0; i < 4; ++i) {
        producers.emplace_back([&] {
            if (!full_q.push(123)) {
                rejected_producers.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    full_q.close();
    for (auto& p : producers) p.join();
    REQUIRE_EQ(rejected_producers.load(), 4);
}

TEST_CASE("06. 4-Producer x 4-Consumer MPMC stress test (80,000 items)") {
    BlockingQueue<uint64_t> q(64);
    constexpr uint64_t kPerProducer = 20'000;
    std::atomic<uint64_t> produced_sum{0};
    std::atomic<uint64_t> consumed_sum{0};
    std::atomic<uint64_t> consumed_count{0};

    std::vector<std::thread> prods;
    for (uint64_t p = 0; p < 4; ++p) {
        prods.emplace_back([&, p] {
            uint64_t local = 0;
            for (uint64_t i = 1; i <= kPerProducer; ++i) {
                const uint64_t v = p * 100'000 + i;
                REQUIRE(q.push(v));
                local += v;
            }
            produced_sum.fetch_add(local, std::memory_order_relaxed);
        });
    }

    std::vector<std::thread> cons;
    for (int c = 0; c < 4; ++c) {
        cons.emplace_back([&] {
            uint64_t local_sum = 0;
            uint64_t local_cnt = 0;
            while (auto item = q.pop()) {
                local_sum += *item;
                ++local_cnt;
            }
            consumed_sum.fetch_add(local_sum, std::memory_order_relaxed);
            consumed_count.fetch_add(local_cnt, std::memory_order_relaxed);
        });
    }

    for (auto& t : prods) t.join();
    q.close();
    for (auto& t : cons) t.join();

    REQUIRE_EQ(consumed_count.load(), 4 * kPerProducer);
    REQUIRE_EQ(consumed_sum.load(), produced_sum.load());
}

RUN_TEST_SUITE("11 — Bounded MPMC Blocking Queue")
