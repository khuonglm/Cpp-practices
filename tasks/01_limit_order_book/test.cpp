#include "order_book.hpp"
#include "test_framework.hpp"

#include <random>

TEST_CASE("01. Empty book invariants") {
    OrderBook ob;
    REQUIRE_EQ(ob.best_bid(), std::nullopt);
    REQUIRE_EQ(ob.best_ask(), std::nullopt);
    REQUIRE_EQ(ob.depth_at(Side::Buy, 100), 0u);
    REQUIRE_EQ(ob.depth_at(Side::Sell, 100), 0u);
    REQUIRE(!ob.cancel(42));
    REQUIRE(!ob.modify(42, 10));
}

TEST_CASE("02. Resting non-crossing orders and multi-level depth") {
    OrderBook ob;
    REQUIRE(ob.add(Order{1, Side::Buy, 99, 50}).empty());
    REQUIRE(ob.add(Order{2, Side::Buy, 100, 30}).empty());
    REQUIRE(ob.add(Order{3, Side::Buy, 100, 70}).empty());
    REQUIRE(ob.add(Order{4, Side::Sell, 102, 40}).empty());
    REQUIRE(ob.add(Order{5, Side::Sell, 101, 25}).empty());

    REQUIRE_EQ(ob.best_bid(), std::optional<Price>{100});
    REQUIRE_EQ(ob.best_ask(), std::optional<Price>{101});
    REQUIRE_EQ(ob.depth_at(Side::Buy, 100), 100u);
    REQUIRE_EQ(ob.depth_at(Side::Buy, 99), 50u);
    REQUIRE_EQ(ob.depth_at(Side::Sell, 101), 25u);
    REQUIRE_EQ(ob.depth_at(Side::Sell, 102), 40u);
}

TEST_CASE("03. Partial fill at exact touch price") {
    OrderBook ob;
    REQUIRE(ob.add(Order{1, Side::Buy, 10, 100}).empty());

    auto fills = ob.add(Order{2, Side::Sell, 10, 60});
    REQUIRE_EQ(fills.size(), size_t{1});
    REQUIRE_EQ(fills[0], (Fill{1, 2, 10, 60}));

    REQUIRE_EQ(ob.best_bid(), std::optional<Price>{10});
    REQUIRE_EQ(ob.best_ask(), std::nullopt);
    REQUIRE_EQ(ob.depth_at(Side::Buy, 10), 40u);
    REQUIRE(!ob.cancel(2));  // Incoming order completely filled; must not rest
}

TEST_CASE("04. Price improvement executes at resting maker price and rests remainder") {
    OrderBook ob;
    REQUIRE(ob.add(Order{1, Side::Buy, 10, 40}).empty());

    auto fills = ob.add(Order{2, Side::Sell, 9, 50});
    REQUIRE_EQ(fills.size(), size_t{1});
    REQUIRE_EQ(fills[0], (Fill{1, 2, 10, 40}));  // Must fill at 10, not 9

    REQUIRE_EQ(ob.best_bid(), std::nullopt);
    REQUIRE_EQ(ob.best_ask(), std::optional<Price>{9});
    REQUIRE_EQ(ob.depth_at(Side::Sell, 9), 10u);
    REQUIRE(!ob.cancel(1));  // Resting order was fully filled
}

TEST_CASE("05. Multi-level sweep ordering") {
    OrderBook ob;
    ob.add(Order{1, Side::Sell, 103, 50});
    ob.add(Order{2, Side::Sell, 101, 20});
    ob.add(Order{3, Side::Sell, 102, 30});

    auto fills = ob.add(Order{10, Side::Buy, 103, 75});
    REQUIRE_EQ(fills.size(), size_t{3});
    REQUIRE_EQ(fills[0], (Fill{2, 10, 101, 20}));
    REQUIRE_EQ(fills[1], (Fill{3, 10, 102, 30}));
    REQUIRE_EQ(fills[2], (Fill{1, 10, 103, 25}));

    REQUIRE_EQ(ob.best_ask(), std::optional<Price>{103});
    REQUIRE_EQ(ob.depth_at(Side::Sell, 103), 25u);
    REQUIRE_EQ(ob.best_bid(), std::nullopt);
}

TEST_CASE("06. Strict FIFO queue priority with middle cancellation") {
    OrderBook ob;
    ob.add(Order{101, Side::Buy, 500, 10});
    ob.add(Order{102, Side::Buy, 500, 15});
    ob.add(Order{103, Side::Buy, 500, 20});

    REQUIRE(ob.cancel(102));
    REQUIRE(!ob.cancel(102));
    REQUIRE_EQ(ob.depth_at(Side::Buy, 500), 30u);

    auto fills = ob.add(Order{200, Side::Sell, 500, 25});
    REQUIRE_EQ(fills.size(), size_t{2});
    REQUIRE_EQ(fills[0], (Fill{101, 200, 500, 10}));
    REQUIRE_EQ(fills[1], (Fill{103, 200, 500, 15}));
    REQUIRE_EQ(ob.depth_at(Side::Buy, 500), 5u);
}

TEST_CASE("07. Cancel updates best_bid and best_ask cleanly") {
    OrderBook ob;
    ob.add(Order{1, Side::Buy, 98, 10});
    ob.add(Order{2, Side::Buy, 100, 10});
    REQUIRE_EQ(ob.best_bid(), std::optional<Price>{100});

    REQUIRE(ob.cancel(2));
    REQUIRE_EQ(ob.best_bid(), std::optional<Price>{98});

    REQUIRE(ob.cancel(1));
    REQUIRE_EQ(ob.best_bid(), std::nullopt);
}

TEST_CASE("08. Modify downward preserves FIFO queue priority and zero cancels") {
    OrderBook ob;
    ob.add(Order{1, Side::Buy, 100, 50});
    ob.add(Order{2, Side::Buy, 100, 50});

    // Reduce order 1 from 50 -> 20: must remain ahead of order 2
    REQUIRE(ob.modify(1, 20));
    REQUIRE_EQ(ob.depth_at(Side::Buy, 100), 70u);

    auto fills = ob.add(Order{3, Side::Sell, 100, 30});
    REQUIRE_EQ(fills.size(), size_t{2});
    REQUIRE_EQ(fills[0], (Fill{1, 3, 100, 20}));
    REQUIRE_EQ(fills[1], (Fill{2, 3, 100, 10}));

    // Modify remaining order 2 to 0 should cancel and clear level
    REQUIRE(ob.modify(2, 0));
    REQUIRE_EQ(ob.best_bid(), std::nullopt);
    REQUIRE_EQ(ob.depth_at(Side::Buy, 100), 0u);
}

TEST_CASE("09. Modify upward loses queue priority") {
    OrderBook ob;
    ob.add(Order{1, Side::Buy, 100, 20});
    ob.add(Order{2, Side::Buy, 100, 30});

    // Increase order 1 from 20 -> 40: loses priority behind order 2
    REQUIRE(ob.modify(1, 40));
    REQUIRE_EQ(ob.depth_at(Side::Buy, 100), 70u);

    auto fills = ob.add(Order{3, Side::Sell, 100, 35});
    REQUIRE_EQ(fills.size(), size_t{2});
    REQUIRE_EQ(fills[0], (Fill{2, 3, 100, 30}));
    REQUIRE_EQ(fills[1], (Fill{1, 3, 100, 5}));
    REQUIRE_EQ(ob.depth_at(Side::Buy, 100), 35u);
}

TEST_CASE("10. Randomized fuzz: uncrossed book invariant across 20,000 ops") {
    OrderBook ob;
    std::mt19937_64 rng(20260801);
    std::uniform_int_distribution<Price> px_dist(90, 110);
    std::uniform_int_distribution<Qty> qty_dist(1, 100);
    std::uniform_int_distribution<int> op_dist(0, 9);

    OrderId next_id = 1;
    for (int step = 0; step < 20000; ++step) {
        const int op = op_dist(rng);
        if (op < 6) {
            const Side side = (op % 2 == 0) ? Side::Buy : Side::Sell;
            ob.add(Order{next_id++, side, px_dist(rng), qty_dist(rng)});
        } else if (op < 8 && next_id > 1) {
            const OrderId target = 1 + (rng() % (next_id - 1));
            ob.cancel(target);
        } else if (next_id > 1) {
            const OrderId target = 1 + (rng() % (next_id - 1));
            ob.modify(target, qty_dist(rng));
        }

        const auto bb = ob.best_bid();
        const auto ba = ob.best_ask();
        if (bb.has_value() && ba.has_value()) {
            REQUIRE(*bb < *ba);
        }
    }
}

RUN_TEST_SUITE("01 — Limit Order Book")
