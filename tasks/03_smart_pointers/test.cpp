#include "smart_ptr.hpp"
#include "lifetime_tracker.hpp"
#include "test_framework.hpp"

#include <atomic>
#include <thread>
#include <vector>

using quant_test::Tracked;

TEST_CASE("01. UniquePtr RAII lifecycle and stateless deleter size") {
    Tracked::reset_counters();
    {
        UniquePtr<Tracked> p(new Tracked(42));
        REQUIRE(static_cast<bool>(p));
        REQUIRE(p.get() != nullptr);
        REQUIRE_EQ((*p).value, 42);
        REQUIRE_EQ(p->value, 42);
        REQUIRE_EQ(Tracked::live.load(), 1L);
    }
    REQUIRE_EQ(Tracked::live.load(), 0L);
    REQUIRE_EQ(sizeof(UniquePtr<Tracked>), sizeof(Tracked*));
}

TEST_CASE("02. UniquePtr move semantics and self-move safety") {
    Tracked::reset_counters();
    UniquePtr<Tracked> a(new Tracked(10));
    UniquePtr<Tracked> b(std::move(a));
    REQUIRE(!a);
    REQUIRE(static_cast<bool>(b));
    REQUIRE_EQ(b->value, 10);

    UniquePtr<Tracked> c(new Tracked(20));
    c = std::move(b);
    REQUIRE(!b);
    REQUIRE_EQ(Tracked::live.load(), 1L);  // Old object(20) immediately deleted
    REQUIRE_EQ(c->value, 10);

    // Self-move assignment must not destroy owned object prematurely
    UniquePtr<Tracked>* alias = &c;
    c = std::move(*alias);
    REQUIRE(static_cast<bool>(c));
    REQUIRE_EQ(c->value, 10);
}

TEST_CASE("03. UniquePtr release, reset, and custom stateful deleter") {
    int custom_deletes = 0;
    struct CountingDeleter {
        int* counter;
        void operator()(int* p) const noexcept {
            ++(*counter);
            delete p;
        }
    };

    {
        UniquePtr<int, CountingDeleter> up(new int(99), CountingDeleter{&custom_deletes});
        REQUIRE_EQ(*up, 99);
        up.reset(new int(105));
        REQUIRE_EQ(custom_deletes, 1);
        REQUIRE_EQ(*up, 105);

        int* raw = up.release();
        REQUIRE(!up);
        REQUIRE_EQ(custom_deletes, 1);
        delete raw;
    }
    REQUIRE_EQ(custom_deletes, 1);
}

TEST_CASE("04. SharedPtr refcounting, copy, move, and self-assign") {
    Tracked::reset_counters();
    SharedPtr<Tracked> empty;
    REQUIRE_EQ(empty.use_count(), 0L);
    REQUIRE(!empty);

    {
        SharedPtr<Tracked> s1(new Tracked(77));
        REQUIRE_EQ(s1.use_count(), 1L);
        REQUIRE_EQ(s1->value, 77);

        {
            SharedPtr<Tracked> s2(s1);
            SharedPtr<Tracked> s3;
            s3 = s2;
            REQUIRE_EQ(s1.use_count(), 3L);
            REQUIRE_EQ(s3->value, 77);

            SharedPtr<Tracked> s4(std::move(s3));
            REQUIRE(!s3);
            REQUIRE_EQ(s1.use_count(), 3L);
            REQUIRE_EQ(s4->value, 77);
        }
        REQUIRE_EQ(s1.use_count(), 1L);
        REQUIRE_EQ(Tracked::live.load(), 1L);

        SharedPtr<Tracked>* alias = &s1;
        s1 = *alias;
        s1 = std::move(*alias);
        REQUIRE_EQ(s1.use_count(), 1L);
        REQUIRE_EQ(s1->value, 77);
    }
    REQUIRE_EQ(Tracked::live.load(), 0L);
}

TEST_CASE("05. WeakPtr object teardown vs control block lifetime") {
    Tracked::reset_counters();
    WeakPtr<Tracked> wp;
    {
        SharedPtr<Tracked> sp(new Tracked(314));
        wp = sp;
        REQUIRE(!wp.expired());
        REQUIRE_EQ(wp.use_count(), 1L);

        SharedPtr<Tracked> locked = wp.lock();
        REQUIRE(static_cast<bool>(locked));
        REQUIRE_EQ(locked->value, 314);
        REQUIRE_EQ(sp.use_count(), 2L);
    }
    // Object MUST be destroyed as soon as strong hits 0, even while wp is alive!
    REQUIRE_EQ(Tracked::live.load(), 0L);
    REQUIRE(wp.expired());
    REQUIRE_EQ(wp.use_count(), 0L);
    REQUIRE(!wp.lock());
}

TEST_CASE("06. Breaking cyclic graphs using WeakPtr") {
    struct Node {
        Tracked payload;
        SharedPtr<Node> next;
        WeakPtr<Node> prev;
        explicit Node(int v) : payload(v) {}
    };

    Tracked::reset_counters();
    {
        SharedPtr<Node> a(new Node(1));
        SharedPtr<Node> b(new Node(2));
        a->next = b;
        b->prev = a;
        REQUIRE_EQ(Tracked::live.load(), 2L);
        REQUIRE_EQ(b->prev.lock()->payload.value, 1);
    }
    REQUIRE_EQ(Tracked::live.load(), 0L);
}

TEST_CASE("07. Multithreaded SharedPtr churn + concurrent WeakPtr::lock() CAS stress") {
    Tracked::reset_counters();
    for (int round = 0; round < 200; ++round) {
        SharedPtr<Tracked> owner(new Tracked(12345));
        WeakPtr<Tracked> observer(owner);
        std::atomic<bool> start{false};
        std::atomic<long> successful_locks{0};

        std::vector<std::thread> threads;
        for (int t = 0; t < 6; ++t) {
            threads.emplace_back([&] {
                while (!start.load(std::memory_order_acquire)) {}
                for (int i = 0; i < 500; ++i) {
                    if (auto sp = observer.lock()) {
                        if (sp->value == 12345) {
                            successful_locks.fetch_add(1, std::memory_order_relaxed);
                        }
                    }
                }
            });
        }

        threads.emplace_back([&] {
            while (!start.load(std::memory_order_acquire)) {}
            for (int i = 0; i < 100; ++i) {
                SharedPtr<Tracked> copy(owner);
                (void)copy;
            }
            owner.reset();  // Drops final strong reference concurrently with lock()!
        });

        start.store(true, std::memory_order_release);
        for (auto& th : threads) th.join();
    }
    REQUIRE_EQ(Tracked::live.load(), 0L);
}

RUN_TEST_SUITE("03 — UniquePtr, SharedPtr & WeakPtr")
