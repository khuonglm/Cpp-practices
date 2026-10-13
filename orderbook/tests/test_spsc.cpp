// M3 tests: single-thread sanity, threaded ordering/throughput, and an integration
// test that pushes an order stream through the queue to an engine and checks the
// result matches a single-threaded run. Build a TSan variant to catch data races.
#include "ob/spsc_queue.hpp"
#include "ob/types.hpp"
#include "ob/testkit.hpp"
#include <thread>
#include <atomic>
#include <vector>
#include <random>
#include <cstdint>

using namespace ob;
using namespace ob::test;

// If push/pop are not implemented yet, the spinning loops below would hang forever.
// This probe lets an unimplemented queue FAIL FAST instead. Returns false => bail.
static bool queue_works(Ctx& ctx) {
    SpscQueue<int, 8> probe; int tmp = 0;
    if (!probe.push(1) || !probe.pop(tmp) || tmp != 1) { OB_CHECK(ctx, false); return false; }
    return true;
}

static void test_single_thread(Ctx& ctx) {
    SpscQueue<int, 8> q;
    int x = -1;
    OB_CHECK(ctx, !q.pop(x));                        // empty
    for (int i = 0; i < 7; ++i) OB_CHECK(ctx, q.push(i));  // 7 usable slots (one reserved)
    OB_CHECK(ctx, !q.push(999));                     // full
    for (int i = 0; i < 7; ++i) { OB_CHECK(ctx, q.pop(x)); OB_CHECK_EQ(ctx, x, i); }
    OB_CHECK(ctx, !q.pop(x));                         // empty again
}

static void test_threaded(Ctx& ctx) {
    if (!queue_works(ctx)) return;
    constexpr int N = 1'000'000;
    SpscQueue<int, 1024> q;
    std::atomic<bool> in_order{true};
    long long sum = 0;
    std::thread consumer([&] {
        int expect = 0, v = 0;
        for (int got = 0; got < N; ) {
            if (q.pop(v)) { if (v != expect) in_order = false; sum += v; ++expect; ++got; }
        }
    });
    for (int i = 0; i < N; ++i) { while (!q.push(i)) { /* spin */ } }
    consumer.join();
    OB_CHECK(ctx, in_order.load());
    OB_CHECK_EQ(ctx, sum, static_cast<long long>(N) * (N - 1) / 2);
}

struct Msg { std::uint8_t type; OrderId id; Side side; Price price; Qty qty; }; // 0=limit,1=market,2=cancel

static void apply(BruteMatchingEngine& e, const Msg& m) {
    if      (m.type == 0) e.add_limit(m.id, m.side, m.price, m.qty);
    else if (m.type == 1) e.add_market(m.id, m.side, m.qty);
    else                  e.cancel(m.id);
}

static void test_integration(Ctx& ctx) {
    if (!queue_works(ctx)) return;
    std::mt19937 rng(777);
    std::uniform_int_distribution<int> op(0, 99), s01(0, 1), px(95, 105), q(1, 6), mq(1, 9);
    std::vector<Msg> msgs; OrderId next = 1; std::vector<OrderId> issued;
    const int N = 200'000;
    for (int i = 0; i < N; ++i) {
        Msg m{};
        const int o = op(rng);
        if (o < 60)      { m = {0, next++, s01(rng) ? Side::Sell : Side::Buy, static_cast<Price>(px(rng)), static_cast<Qty>(q(rng))}; issued.push_back(m.id); }
        else if (o < 85) { m = {1, next++, s01(rng) ? Side::Sell : Side::Buy, 0, static_cast<Qty>(mq(rng))}; }
        else             { OrderId id = issued.empty() ? 999999u : issued[rng() % issued.size()]; m = {2, id, Side::Buy, 0, 0}; }
        msgs.push_back(m);
    }
    BruteMatchingEngine single; for (const auto& m : msgs) apply(single, m);

    BruteMatchingEngine viaq;
    SpscQueue<Msg, 1024> queue;
    std::thread consumer([&] {
        Msg m{};
        for (int got = 0; got < N; ) { if (queue.pop(m)) { apply(viaq, m); ++got; } }
    });
    for (const auto& m : msgs) { while (!queue.push(m)) { /* spin */ } }
    consumer.join();

    for (Price p = 95; p <= 105; ++p) {
        OB_CHECK_EQ(ctx, single.qty_at(Side::Buy,  p), viaq.qty_at(Side::Buy,  p));
        OB_CHECK_EQ(ctx, single.qty_at(Side::Sell, p), viaq.qty_at(Side::Sell, p));
    }
    OB_CHECK_EQ(ctx, single.best_bid(), viaq.best_bid());
    OB_CHECK_EQ(ctx, single.best_ask(), viaq.best_ask());
}

int main() {
    Ctx ctx;
    std::cerr << "== SpscQueue  (M3) ==\n";
    test_single_thread(ctx);
    test_threaded(ctx);
    test_integration(ctx);
    std::cerr << "  passed=" << ctx.passed << "  failed=" << ctx.failed
              << (ctx.failed == 0 ? "   OK\n" : "   *** FAILURES ***\n");
    return ctx.failed == 0 ? 0 : 1;
}
