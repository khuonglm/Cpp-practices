#pragma once
// Shared vocabulary types for the whole project. This file is COMPLETE - do not edit.
#include <cstdint>
#include <optional>
#include <vector>
#include <ostream>

namespace ob {

using OrderId = std::uint64_t;
using Price   = std::int64_t;   // integer ticks - NEVER use float/double for price
using Qty     = std::uint64_t;

enum class Side : std::uint8_t { Buy, Sell };

constexpr const char* to_string(Side s) noexcept { return s == Side::Buy ? "BUY" : "SELL"; }
constexpr Side        opposite (Side s) noexcept { return s == Side::Buy ? Side::Sell : Side::Buy; }

// One execution. price is always the RESTING (maker) order's price.
struct Trade {
    OrderId taker;   // incoming / aggressor order id
    OrderId maker;   // resting order id
    Price   price;   // execution price == maker's resting price
    Qty     qty;     // matched quantity

    friend bool operator==(const Trade& a, const Trade& b) noexcept {
        return a.taker == b.taker && a.maker == b.maker &&
               a.price == b.price && a.qty == b.qty;
    }
    friend std::ostream& operator<<(std::ostream& os, const Trade& t) {
        return os << "Trade{taker=" << t.taker << ",maker=" << t.maker
                  << ",px=" << t.price << ",qty=" << t.qty << "}";
    }
};

} // namespace ob
