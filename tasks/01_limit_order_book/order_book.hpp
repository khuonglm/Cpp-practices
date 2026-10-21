#pragma once

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <vector>

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

    bool operator==(const Fill& other) const noexcept = default;
};

class OrderBook {
public:
    OrderBook() = default;

    std::vector<Fill> add(Order order) {
        (void)order;
        throw std::logic_error("TODO: OrderBook::add");
    }

    bool cancel(OrderId id) {
        (void)id;
        throw std::logic_error("TODO: OrderBook::cancel");
    }

    bool modify(OrderId id, Qty new_qty) {
        (void)id;
        (void)new_qty;
        throw std::logic_error("TODO: OrderBook::modify");
    }

    std::optional<Price> best_bid() const {
        throw std::logic_error("TODO: OrderBook::best_bid");
    }

    std::optional<Price> best_ask() const {
        throw std::logic_error("TODO: OrderBook::best_ask");
    }

    Qty depth_at(Side side, Price price) const {
        (void)side;
        (void)price;
        throw std::logic_error("TODO: OrderBook::depth_at");
    }
};
