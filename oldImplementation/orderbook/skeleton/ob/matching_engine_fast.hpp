#pragma once
// ============================================================================
//  M2 SKELETON - reimplement the engine with an intrusive list + object pool,
//  KEEPING the exact same public API (so tests/test_engine_m2.cpp still grades you:
//  ./build.sh target test_engine_m2_skel). Reference: reference/ob/matching_engine_fast.hpp.
//
//  Goal of M2: same behaviour as M1, but NO per-order allocation on the hot path and
//  O(1) unlink. You implement the pool (alloc_/release_) and the intrusive list
//  (push_back_/unlink_) plus match_/rest_/cancel. The map still orders price levels.
// ============================================================================
#include "ob/types.hpp"
#include <map>
#include <deque>
#include <vector>
#include <unordered_map>
#include <functional>
#include <algorithm>
#include <cassert>

namespace ob {

class MatchingEngineFast {
public:
    MatchingEngineFast() = default;
    MatchingEngineFast(const MatchingEngineFast&)            = delete;   // owns raw Node*
    MatchingEngineFast& operator=(const MatchingEngineFast&) = delete;
    MatchingEngineFast(MatchingEngineFast&&)                 = default;
    MatchingEngineFast& operator=(MatchingEngineFast&&)      = default;

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
        
        auto [side, price, node] = loc->second;
        index_.erase(loc);
        if(side == Side::Buy) {
            auto& lvl = bids_[price];
            unlink_(lvl, node);
            if(lvl.head == nullptr)
                bids_.erase(price);
        } else {
            auto& lvl = asks_[price];
            unlink_(lvl, node);
            if(lvl.head == nullptr)
                asks_.erase(price);
        }
        release_(node);
        return true;
    }

    std::optional<Price> best_bid() const noexcept {
        if (bids_.empty()) return std::nullopt; return bids_.begin()->first;
    }

    std::optional<Price> best_ask() const noexcept {
        if (asks_.empty()) return std::nullopt; return asks_.begin()->first;
    }

    Qty qty_at(Side side, Price price) const noexcept {
        const Level* L = nullptr;
        if (side == Side::Buy) { auto it = bids_.find(price); if (it != bids_.end()) L = &it->second; }
        else                   { auto it = asks_.find(price); if (it != asks_.end()) L = &it->second; }
        Qty s = 0;
        for (const Node* n = L ? L->head : nullptr; n; n = n->next) s += n->qty;
        return s;
    }

    bool empty() const noexcept { return bids_.empty() && asks_.empty(); }

private:
    struct Node { OrderId id; Qty qty; Node* prev = nullptr; Node* next = nullptr; };
    struct Level { Node* head = nullptr; Node* tail = nullptr; };
    using Bids = std::map<Price, Level, std::greater<Price>>;
    using Asks = std::map<Price, Level, std::less<Price>>;
    struct Loc { Side side; Price price; Node* node; };

    Bids bids_;
    Asks asks_;
    std::unordered_map<OrderId, Loc> index_;
    std::deque<Node>  storage_;
    std::vector<Node*> free_;

    Node* alloc_(OrderId id, Qty q) {
        Node* n;
        if(free_.empty()) {
            storage_.push_back(Node());
            n = &storage_.back();
        } else {
            n = free_.back();
            free_.pop_back();
        }
        n->id = id, n->qty = q;
        n->prev = n->next = nullptr;
        return n;
    }

    void release_(Node* n) {
        free_.push_back(n);
    }

    static void push_back_(Level& lvl, Node* n) noexcept {
        if(lvl.head == nullptr) {
            lvl.head = lvl.tail = n;
            n->next = n->prev = nullptr;
        } else {
            lvl.tail->next = n;
            n->prev = lvl.tail;
            n->next = nullptr;
            lvl.tail = n;
        }
    }

    static void unlink_(Level& lvl, Node* n) noexcept {
        if(n->prev != nullptr) {
            n->prev->next = n->next;
        } else {
            lvl.head = n->next;
        }
        if(n->next != nullptr) {
            n->next->prev = n->prev;
        } else {
            lvl.tail = n->prev;
        }
        
        n->prev = n->next = nullptr;
    }

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
            while(qty > 0 && orders.head != nullptr) {
                auto& maker = *orders.head;
                Qty consume = std::min(maker.qty, qty);
                maker.qty -= consume;
                qty -= consume;
                out.emplace_back(taker, maker.id, book_px, consume);
                if(maker.qty == 0) {
                    index_.erase(maker.id);
                    Node* done = orders.head;
                    unlink_(orders, orders.head);
                    release_(done);
                }
            }
            if(orders.head == nullptr)
                opp.erase(level);
        }
    }

    void rest_(OrderId id, Side side, Price price, Qty qty) {
        assert(index_.find(id) == index_.end());
        Node* n = alloc_(id, qty);
        index_[id] = Loc{side, price, n};
        if(side == Side::Buy) {
            push_back_(bids_[price], n);
        } else {
            push_back_(asks_[price], n);
        }
    }
};

} // namespace ob
