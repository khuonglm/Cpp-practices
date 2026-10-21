#include "spsc_queue.hpp"
#include "test_framework.hpp"

#include <cstdint>
#include <string>
#include <thread>

TEST_CASE("01. Empty pop leaves output untouched") {
    SpscQueue<int, 4> q;
    int sentinel = 98765;
    REQUIRE(!q.try_pop(sentinel));
    REQUIRE_EQ(sentinel, 98765);
    REQUIRE_EQ(q.capacity(), size_t{4});
}

TEST_CASE("02. Exact full capacity N and immediate slot recovery") {
    SpscQueue<int, 4> q;
    REQUIRE(q.try_push(10));
    REQUIRE(q.try_push(20));
    REQUIRE(q.try_push(30));
    REQUIRE(q.try_push(40));
    REQUIRE(!q.try_push(50));  // Full at exact N=4

    int out = 0;
    REQUIRE(q.try_pop(out));
    REQUIRE_EQ(out, 10);
    REQUIRE(q.try_push(50));   // Slot recovered across wrap boundary

    for (int expected : {20, 30, 40, 50}) {
        REQUIRE(q.try_pop(out));
        REQUIRE_EQ(out, expected);
    }
    REQUIRE(!q.try_pop(out));
}

TEST_CASE("03. Wrap-around stress on small power-of-two capacity") {
    SpscQueue<uint64_t, 8> q;
    uint64_t next_in = 1;
    uint64_t next_out = 1;

    for (int iter = 0; iter < 10000; ++iter) {
        for (int k = 0; k < 5; ++k) {
            REQUIRE(q.try_push(next_in++));
        }
        for (int k = 0; k < 5; ++k) {
            uint64_t val = 0;
            REQUIRE(q.try_pop(val));
            REQUIRE_EQ(val, next_out++);
        }
    }
}

TEST_CASE("04. Cache-line padding check") {
    SpscQueue<int, 4> q;
    (void)q;
    // Head cache line (64B) + Tail cache line (64B) + Buffer cache line (>=64B)
    REQUIRE(sizeof(SpscQueue<int, 4>) >= 128);
}

TEST_CASE("05. Non-trivial std::string elements and rvalue push") {
    SpscQueue<std::string, 8> q;
    std::string heavy(128, 'Q');  // Defeats SSO to exercise heap string buffer
    REQUIRE(q.try_push(heavy));
    REQUIRE(q.try_push(std::string(256, 'Z')));

    std::string out;
    REQUIRE(q.try_pop(out));
    REQUIRE_EQ(out, std::string(128, 'Q'));
    REQUIRE(q.try_pop(out));
    REQUIRE_EQ(out, std::string(256, 'Z'));
}

TEST_CASE("06. Two-thread SPSC 2,000,000-element strict FIFO integrity test") {
    {
        SpscQueue<int, 2> probe;
        int dummy = 0;
        (void)probe.try_push(1);
        (void)probe.try_pop(dummy);
    }
    constexpr uint64_t kCount = 2'000'000;
    SpscQueue<uint64_t, 1024> q;
    bool consumer_ok = true;

    std::thread producer([&] {
        for (uint64_t i = 0; i < kCount; ++i) {
            while (!q.try_push(i)) {
                std::this_thread::yield();
            }
        }
    });

    std::thread consumer([&] {
        uint64_t val = 0;
        for (uint64_t expected = 0; expected < kCount; ++expected) {
            while (!q.try_pop(val)) {
                std::this_thread::yield();
            }
            if (val != expected) {
                consumer_ok = false;
                break;
            }
        }
    });

    producer.join();
    consumer.join();
    REQUIRE(consumer_ok);
}

TEST_CASE("07. Struct payload tear-free cross-thread test") {
    {
        SpscQueue<int, 2> probe;
        int dummy = 0;
        (void)probe.try_push(1);
        (void)probe.try_pop(dummy);
    }
    struct Tick {
        uint64_t seq;
        int64_t  bid;
        int64_t  ask;
        uint64_t checksum;
    };

    constexpr uint64_t kMsgs = 250'000;
    SpscQueue<Tick, 256> q;
    bool intact = true;

    std::thread prod([&] {
        for (uint64_t s = 1; s <= kMsgs; ++s) {
            const int64_t b = static_cast<int64_t>(s * 10);
            const int64_t a = b + 5;
            Tick t{s, b, a, s ^ static_cast<uint64_t>(b) ^ static_cast<uint64_t>(a)};
            while (!q.try_push(t)) {}
        }
    });

    std::thread cons([&] {
        Tick t{};
        for (uint64_t s = 1; s <= kMsgs; ++s) {
            while (!q.try_pop(t)) {}
            const uint64_t expected_cs =
                t.seq ^ static_cast<uint64_t>(t.bid) ^ static_cast<uint64_t>(t.ask);
            if (t.seq != s || t.checksum != expected_cs) {
                intact = false;
                break;
            }
        }
    });

    prod.join();
    cons.join();
    REQUIRE(intact);
}

RUN_TEST_SUITE("02 — Lock-Free SPSC Ring Buffer")
