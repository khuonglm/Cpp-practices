#pragma once
// ============================================================================
//  M1 SKELETON - YOU fill in the TODOs.
//  Build/run against this file: ./build.sh  (target: test_engine_skel).
//  Tests fail until you implement it. Peek at reference/ob/matching_engine.hpp
//  ONLY after an honest attempt.
//
//  Rules (price-time priority):
//   * A BUY crosses an ask priced <= its limit; a SELL crosses a bid priced >= its limit.
//   * Match the BEST price first; within a price, the OLDEST order first (FIFO).
//   * Trade price = the RESTING (maker) order's price.
//   * LIMIT: unmatched remainder rests. MARKET: remainder is discarded.
//  Precondition: OrderId is unique among live orders (assert it in rest_).
// ============================================================================
#include "ob/types.hpp"
#include <map>
#include <list>
#include <unordered_map>
#include <functional>
#include <algorithm>
#include <cassert>
#include <optional>

namespace ob {

class MatchingEngine {
public:
    // Rule of 5: this class will store iterators into its own lists, so a copy would
    // dangle. Make it non-copyable but movable (an interview-favourite ownership call).
    MatchingEngine() = default;
    MatchingEngine(const MatchingEngine&)            = delete;
    MatchingEngine& operator=(const MatchingEngine&) = delete;
    MatchingEngine(MatchingEngine&&)                 = default;
    MatchingEngine& operator=(MatchingEngine&&)      = default;

    std::vector<Trade> add_limit(OrderId id, Side side, Price price, Qty qty) {
        std::vector<Trade> out;
        if(qty == 0) return out;
        if(side == Side::Buy)
            match_(id, side, asks_, true, price, qty, out);
        else
            match_(id, side, bids_, true, price, qty, out);
        if(qty > 0) rest_(id, side, price, qty);
        return out;
    }

    std::vector<Trade> add_market(OrderId id, Side side, Qty qty) {
        std::vector<Trade> out;
        if(qty == 0) return out;
        if(side == Side::Buy)
            match_(id, side, asks_, false, 0, qty, out);
        else
            match_(id, side, bids_, false, 0, qty, out);
        return out;
    }

    bool cancel(OrderId id) {
        auto loc = index_.find(id);
        if(loc == index_.end())
            return false;
        
        auto [side, price, level_it] = loc->second;
        index_.erase(loc);
        if(side == Side::Buy) erase_order_(bids_, price, level_it);
        else erase_order_(asks_, price, level_it);
        return true;
    }

    std::optional<Price> best_bid() const noexcept {
        if (bids_.empty()) return std::nullopt;
        return bids_.begin()->first;
    }

    std::optional<Price> best_ask() const noexcept {
        if (asks_.empty()) return std::nullopt;
        return asks_.begin()->first;
    }

    Qty qty_at(Side side, Price price) const noexcept {
        return side == Side::Buy ? level_qty_(bids_, price) : level_qty_(asks_, price);
    }

    bool empty() const noexcept { return bids_.empty() && asks_.empty(); }

private:
    struct Order { OrderId id; Qty qty; };
    using Level = std::list<Order>;
    using Bids  = std::map<Price, Level, std::greater<Price>>; // best (highest) first
    using Asks  = std::map<Price, Level, std::less<Price>>;    // best (lowest)  first
    struct Loc { Side side; Price price; Level::iterator it; };

    Bids bids_;
    Asks asks_;
    std::unordered_map<OrderId, Loc> index_;

    static bool crosses_(Side taker, Price limit, Price book_px) noexcept {
        return taker == Side::Buy ? book_px <= limit : book_px >= limit;
    }

    template <class Book>
    void match_(OrderId taker, Side taker_side, Book& opp,
                bool has_limit, Price limit, Qty& qty, std::vector<Trade>& out) {
        while(!opp.empty()) {
            auto level = opp.begin();
            const Price& book_px = level->first;
            if(qty == 0 || (has_limit && !crosses_(taker_side, limit, book_px)))
                return;
            
            Level& orders = level->second;
            while(qty > 0 && !orders.empty()) {
                auto& maker = orders.front();
                Qty consume = std::min(maker.qty, qty);
                maker.qty -= consume;
                qty -= consume;
                out.emplace_back(taker, maker.id, book_px, consume);
                if(maker.qty == 0) {
                    index_.erase(maker.id);
                    orders.pop_front();
                }
            }
            if(orders.empty())
                opp.erase(level);
        }
    }

    void rest_(OrderId id, Side side, Price price, Qty qty) {
        assert(index_.find(id) == index_.end());
        if(side == Side::Buy) {
            auto& orders = bids_[price];
            orders.emplace_back(id, qty);
            index_[id] = Loc{side, price, std::prev(orders.end())};
        } else {
            auto& orders = asks_[price];
            orders.emplace_back(id, qty);
            index_[id] = Loc{side, price, std::prev(orders.end())};
        }
    }

    template <class Book>
    void erase_order_(Book& book, Price price, Level::iterator it) {
        auto& orders = book[price];
        orders.erase(it);
        if(orders.empty())
            book.erase(price);
    }

    template <class Book>
    static Qty level_qty_(const Book& book, Price price) noexcept {
        auto lvl = book.find(price);
        if (lvl == book.end()) return 0;
        Qty s = 0;
        for (const auto& o : lvl->second) s += o.qty;
        return s;
    }
};

} // namespace ob
