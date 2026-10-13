#pragma once
// Tiny zero-dependency test framework + an INDEPENDENT "obviously correct" oracle
// (BruteMatchingEngine) used for differential testing. This file is COMPLETE.
#include "ob/types.hpp"
#include <vector>
#include <optional>
#include <algorithm>
#include <iostream>

namespace ob::test {

struct Ctx { long passed = 0; long failed = 0; };

#define OB_CHECK(ctx, cond) do {                                                    \
    if (cond) { ++(ctx).passed; }                                                   \
    else { ++(ctx).failed;                                                          \
        std::cerr << "  [FAIL] " << __FILE__ << ":" << __LINE__ << "  " << #cond << "\n"; } \
} while (0)

#define OB_CHECK_EQ(ctx, a, b) do {                                                 \
    auto _a = (a); auto _b = (b);                                                   \
    if (_a == _b) { ++(ctx).passed; }                                               \
    else { ++(ctx).failed;                                                          \
        std::cerr << "  [FAIL] " << __FILE__ << ":" << __LINE__ << "  "             \
                  << #a << " != " << #b << "\n"; }                                  \
} while (0)

// -----------------------------------------------------------------------------
// Reference oracle: dead-simple O(n) linear-scan engine. Deliberately a DIFFERENT
// implementation from the map/list engine, so comparing the two catches real bugs.
// Same public API as ob::MatchingEngine.
// -----------------------------------------------------------------------------
class BruteMatchingEngine {
public:
    std::vector<Trade> add_limit(OrderId id, Side side, Price price, Qty qty) {
        std::vector<Trade> out;
        if (qty == 0) return out;
        Qty left = match_(id, side, /*has_limit=*/true, price, qty, out);
        if (left > 0) book_.push_back(R{id, side, price, left, seq_++, true});
        return out;
    }
    std::vector<Trade> add_market(OrderId id, Side side, Qty qty) {
        std::vector<Trade> out;
        if (qty == 0) return out;
        match_(id, side, /*has_limit=*/false, 0, qty, out);   // leftover discarded
        return out;
    }
    bool cancel(OrderId id) {
        for (auto& r : book_) if (r.alive && r.id == id) { r.alive = false; return true; }
        return false;
    }
    std::optional<Price> best_bid() const { return best_(Side::Buy); }
    std::optional<Price> best_ask() const { return best_(Side::Sell); }
    Qty qty_at(Side side, Price price) const {
        Qty s = 0;
        for (const auto& r : book_) if (r.alive && r.side == side && r.price == price) s += r.qty;
        return s;
    }
    bool empty() const {
        for (const auto& r : book_) if (r.alive) return false;
        return true;
    }

private:
    struct R { OrderId id; Side side; Price price; Qty qty; std::uint64_t seq; bool alive; };
    std::vector<R> book_;
    std::uint64_t  seq_ = 0;

    Qty match_(OrderId taker, Side ts, bool has_limit, Price limit, Qty qty, std::vector<Trade>& out) {
        const Side opp = opposite(ts);
        while (qty > 0) {
            int best = -1;
            for (int i = 0; i < static_cast<int>(book_.size()); ++i) {
                const R& r = book_[i];
                if (!r.alive || r.side != opp) continue;
                if (has_limit) {
                    const bool ok = (ts == Side::Buy) ? (r.price <= limit) : (r.price >= limit);
                    if (!ok) continue;
                }
                if (best < 0) { best = i; continue; }
                const R& b = book_[best];
                // Best = best price; tie broken by earliest seq (time priority).
                const bool better = (opp == Side::Sell)
                    ? (r.price <  b.price || (r.price == b.price && r.seq < b.seq))
                    : (r.price >  b.price || (r.price == b.price && r.seq < b.seq));
                if (better) best = i;
            }
            if (best < 0) break;
            R& m = book_[best];
            const Qty t = std::min(qty, m.qty);
            out.push_back(Trade{taker, m.id, m.price, t});
            qty -= t; m.qty -= t;
            if (m.qty == 0) m.alive = false;
        }
        return qty;
    }
    std::optional<Price> best_(Side side) const {
        std::optional<Price> best;
        for (const auto& r : book_) {
            if (!r.alive || r.side != side) continue;
            if (!best)                    best = r.price;
            else if (side == Side::Buy)   best = std::max(*best, r.price);
            else                          best = std::min(*best, r.price);
        }
        return best;
    }
};

} // namespace ob::test
