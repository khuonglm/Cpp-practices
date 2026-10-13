#pragma once
// Reusable, engine-agnostic test suite. Instantiated for M1 (map/list) and M2 (fast),
// both of which must satisfy the same public contract. COMPLETE - do not edit.
#include "ob/types.hpp"
#include "ob/testkit.hpp"
#include <random>
#include <vector>
#include <cstddef>

namespace ob::test {

template <class Engine>
void test_worked_example(Ctx& ctx) {
    Engine e;
    OB_CHECK(ctx, e.add_limit(1, Side::Buy, 100, 10).empty());
    OB_CHECK_EQ(ctx, e.best_bid(), std::optional<Price>(100));
    OB_CHECK(ctx, e.add_limit(2, Side::Buy, 99, 5).empty());
    OB_CHECK(ctx, e.add_limit(3, Side::Sell, 101, 8).empty());
    OB_CHECK_EQ(ctx, e.best_ask(), std::optional<Price>(101));

    auto t4 = e.add_limit(4, Side::Sell, 100, 6);
    OB_CHECK(ctx, (t4 == std::vector<Trade>{ {4,1,100,6} }));
    OB_CHECK_EQ(ctx, e.qty_at(Side::Buy, 100), Qty(4));

    auto t5 = e.add_limit(5, Side::Sell, 99, 7);
    OB_CHECK(ctx, (t5 == std::vector<Trade>{ {5,1,100,4}, {5,2,99,3} }));
    OB_CHECK_EQ(ctx, e.qty_at(Side::Buy, 100), Qty(0));
    OB_CHECK_EQ(ctx, e.qty_at(Side::Buy, 99),  Qty(2));
    OB_CHECK_EQ(ctx, e.best_bid(), std::optional<Price>(99));

    auto t6 = e.add_market(6, Side::Buy, 10);
    OB_CHECK(ctx, (t6 == std::vector<Trade>{ {6,3,101,8} }));
    OB_CHECK_EQ(ctx, e.best_ask(), std::optional<Price>());   // empty
    OB_CHECK_EQ(ctx, e.qty_at(Side::Buy, 99), Qty(2));
}

template <class Engine>
void test_edges(Ctx& ctx) {
    { Engine e; OB_CHECK(ctx, e.add_market(1, Side::Buy, 5).empty()); OB_CHECK(ctx, e.empty()); }
    { Engine e; OB_CHECK(ctx, !e.cancel(42)); }
    { Engine e; e.add_limit(1, Side::Sell, 100, 5);
      OB_CHECK(ctx, e.cancel(1)); OB_CHECK(ctx, !e.cancel(1));
      OB_CHECK(ctx, e.add_market(2, Side::Buy, 3).empty()); OB_CHECK(ctx, e.empty()); }
    { Engine e; e.add_limit(1, Side::Sell, 100, 5);
      auto t = e.add_limit(2, Side::Buy, 100, 5);
      OB_CHECK(ctx, (t == std::vector<Trade>{ {2,1,100,5} })); OB_CHECK(ctx, e.empty()); }
    { Engine e; OB_CHECK(ctx, e.add_limit(1, Side::Buy, 100, 0).empty()); OB_CHECK(ctx, e.empty()); }
    { Engine e; e.add_limit(1, Side::Sell, 101, 5);
      OB_CHECK(ctx, e.add_limit(2, Side::Buy, 100, 5).empty());
      OB_CHECK_EQ(ctx, e.qty_at(Side::Buy, 100), Qty(5)); }
    { // partial fill: taker bigger than one maker, walks price levels
      Engine e; e.add_limit(1, Side::Sell, 100, 3); e.add_limit(2, Side::Sell, 101, 3);
      auto t = e.add_limit(3, Side::Buy, 101, 4);
      OB_CHECK(ctx, (t == std::vector<Trade>{ {3,1,100,3}, {3,2,101,1} }));
      OB_CHECK_EQ(ctx, e.qty_at(Side::Sell, 101), Qty(2)); }
}

// Differential test: run identical random command streams through the engine under
// test and the independent oracle; the whole book state (via public API) plus the
// trade stream must match after EVERY command.
template <class Engine>
void test_differential(Ctx& ctx, int steps = 30000, unsigned seed = 12345) {
    Engine eng; BruteMatchingEngine ref;
    std::mt19937 rng(seed);
    const Price LO = 95, HI = 105;
    std::uniform_int_distribution<int> op(0, 99), side01(0, 1),
        px(static_cast<int>(LO), static_cast<int>(HI)), q(1, 6), mq(1, 9);
    OrderId next = 1;
    std::vector<OrderId> issued;

    for (int step = 0; step < steps; ++step) {
        std::vector<Trade> ta, tb;
        const int o = op(rng);
        if (o < 60) {
            OrderId id = next++; Side s = side01(rng) ? Side::Sell : Side::Buy;
            Price p = px(rng); Qty n = static_cast<Qty>(q(rng));
            ta = eng.add_limit(id, s, p, n); tb = ref.add_limit(id, s, p, n); issued.push_back(id);
        } else if (o < 85) {
            OrderId id = next++; Side s = side01(rng) ? Side::Sell : Side::Buy; Qty n = static_cast<Qty>(mq(rng));
            ta = eng.add_market(id, s, n); tb = ref.add_market(id, s, n);
        } else {
            OrderId id = issued.empty() ? 999999u : issued[rng() % issued.size()];
            OB_CHECK_EQ(ctx, eng.cancel(id), ref.cancel(id));
        }
        OB_CHECK(ctx, ta == tb);
        for (Price p = LO; p <= HI; ++p) {
            OB_CHECK_EQ(ctx, eng.qty_at(Side::Buy,  p), ref.qty_at(Side::Buy,  p));
            OB_CHECK_EQ(ctx, eng.qty_at(Side::Sell, p), ref.qty_at(Side::Sell, p));
        }
        OB_CHECK_EQ(ctx, eng.best_bid(), ref.best_bid());
        OB_CHECK_EQ(ctx, eng.best_ask(), ref.best_ask());
        if (ctx.failed > 0) { std::cerr << "  (differential stopped at step " << step << ")\n"; return; }
    }
}

template <class Engine>
int run_all(const char* name) {
    Ctx ctx;
    std::cerr << "== " << name << " ==\n";
    test_worked_example<Engine>(ctx);
    test_edges<Engine>(ctx);
    test_differential<Engine>(ctx);
    std::cerr << "  passed=" << ctx.passed << "  failed=" << ctx.failed
              << (ctx.failed == 0 ? "   OK\n" : "   *** FAILURES ***\n");
    return ctx.failed == 0 ? 0 : 1;
}

} // namespace ob::test
