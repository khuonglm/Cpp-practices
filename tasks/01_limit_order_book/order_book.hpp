#pragma once

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <vector>
#include <list>
#include <map>
#include <unordered_map>
#include <functional>
#include <variant>

using OrderId = uint64_t;
using Price   = int64_t;
using Qty     = uint32_t;

enum class Side : uint8_t { Buy, Sell };

struct Order {
    OrderId id;
    Side    side;
    Price   price;
    Qty     qty;
};

struct Fill {
    OrderId resting_id;
    OrderId incoming_id;
    Price   price;
    Qty     qty;

    bool operator==(const Fill& other) const {
        return std::tie(resting_id, incoming_id, price, qty) == std::tie(other.resting_id, other.incoming_id, other.price, other.qty);
    }
};

/* 
    for a match, bid (buy price) >= ask (sell price)
    at resting state of order book, best_ask (min sell) > best_bid (max buy)
    lowest ask for incoming buys, and highest bid for incoming sells
    order in the same level is ordered by time order (FIFO)
    price of Fill is resting price
    no dangling levels/orders
*/
class OrderBook {
public:
    OrderBook() = default;

    std::vector<Fill> add(Order order) {
        if(order.qty == 0)
            return {};
        
        if(order.side == Side::Sell) {
            return match_(bids_, asks_, order);
        } else {
            return match_(asks_, bids_, order);
        }
    }

    bool cancel(OrderId id) {
        auto indexIt = index_.find(id);
        if(indexIt == index_.end())
            return false;

        /* pass pointer and receive back a pointer, nullptr if not AskLocation */
        if(auto *askLoc = std::get_if<AskLocation>(&indexIt->second)) {
            removeOrder_(asks_, *askLoc, indexIt);
        } else {
            auto& bidLoc = std::get<BidLocation>(indexIt->second);
            removeOrder_(bids_, bidLoc, indexIt);
        }
        return true;
    }

    /*
        this is a simplified version. 
        it does not consider the case where:
        the remainging qty <= new qty <= original qty &&
        remaining qty < original qty (partially used)
    */
    bool modify(OrderId id, Qty newQty) {
        auto indexIt = index_.find(id);
        if(indexIt == index_.end())
            return false;

        /* pass pointer and receive back a pointer, nullptr if not AskLocation */
        if(auto* askLoc = std::get_if<AskLocation>(&indexIt->second)) {
            changeQty_(asks_, *askLoc, newQty, indexIt);
        } else {
            auto& bidLoc = std::get<BidLocation>(indexIt->second);
            changeQty_(bids_, bidLoc, newQty, indexIt);
        }
        return true;
    }

    std::optional<Price> best_bid() const {
        if(bids_.empty())
            return std::nullopt;
        return bids_.begin()->first;
    }

    std::optional<Price> best_ask() const {
        if(asks_.empty())
            return std::nullopt;
        return asks_.begin()->first;
    }

    Qty depth_at(Side side, Price price) const {
        auto getQty = [](auto& book, Price price) {
            auto it = book.find(price);
            if(it == book.end())
                return static_cast<Qty>(0);
            return it->second.second;
        };
        if(side == Side::Buy) {
            return getQty(bids_, price);
        }
        return getQty(asks_, price);
    }

private:
    using Level = std::list<Order>;
    using AskBook = std::map<Price, std::pair<std::list<Order>, Qty>, std::less<Price>>;
    using BidBook = std::map<Price, std::pair<std::list<Order>, Qty>, std::greater<Price>>;
    struct AskLocation {
        Level::iterator orderIt;
        AskBook::iterator levelIt;
    };
    struct BidLocation {
        Level::iterator orderIt;
        BidBook::iterator levelIt;
    };
    using Location = std::variant<AskLocation, BidLocation>;
    using IndexBook = std::unordered_map<OrderId, Location>;

    AskBook asks_;
    BidBook bids_;
    IndexBook index_;

    bool cross_(Side incomingSide, Price incomingPrice, Price restingPrice) {
        return incomingSide == Side::Buy? incomingPrice >= restingPrice: incomingPrice <= restingPrice;
    }

    template<typename Book, typename RestingBook>
    std::vector<Fill> match_(Book& book, RestingBook& restingBook, Order& order) {
        std::vector<Fill> fills;
        while(!book.empty() && order.qty > 0) {
            auto best = book.begin();
            if(!cross_(order.side, order.price, best->first))
                break;
            
            auto& [orders, totalQty] = best->second;
            while(!orders.empty() && order.qty > 0) {
                auto& restingOrder = orders.front();
                auto amount = std::min(restingOrder.qty, order.qty);
                order.qty -= amount;
                restingOrder.qty -= amount;
                totalQty -= amount;
                fills.emplace_back(restingOrder.id, order.id, restingOrder.price, amount);
                if(restingOrder.qty == 0) {
                    index_.erase(restingOrder.id);
                    orders.pop_front();
                }
            }
            if(orders.empty())
                book.erase(best);
        }

        if(order.qty > 0) {
            auto levelIt = restingBook.try_emplace(order.price).first;
            auto& [orders, totalQty] = levelIt->second;
            orders.push_back(order);
            totalQty += order.qty;
            if(order.side == Side::Buy) {
                index_[order.id] = BidLocation{std::prev(orders.end()), levelIt};
            } else {
                index_[order.id] = AskLocation{std::prev(orders.end()), levelIt};
            }
        }
        return fills;
    }

    template<typename Book, typename ExplicitLocation>
    void removeOrder_(Book& book, ExplicitLocation& loc, IndexBook::iterator indexIt) {
        auto& [orders, totalQty] = loc.levelIt->second;
        totalQty -= loc.orderIt->qty;
        orders.erase(loc.orderIt);
        if(orders.empty())
            book.erase(loc.levelIt);
        
        index_.erase(indexIt);
    }

    template<typename Book, typename ExplicitLocation>
    void changeQty_(Book& book, ExplicitLocation& loc, Qty newQty, IndexBook::iterator indexIt) {
        if(newQty == 0) {
            removeOrder_(book, loc, indexIt);
            return;
        }

        Order& order = *loc.orderIt;
        if(newQty < order.qty) {
            loc.levelIt->second.second -= (order.qty - newQty);
            order.qty = newQty;
        } else {
            auto& [orders, totalQty] = loc.levelIt->second;
            totalQty += (newQty - order.qty);
            order.qty = newQty;
            /* iterator is still valid after this, no change */
            orders.splice(orders.end(), orders, loc.orderIt);
        }
    }
};
