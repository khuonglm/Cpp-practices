#pragma once

#include <atomic>
#include <stdexcept>

namespace quant_test {

struct Tracked {
    inline static std::atomic<long> live{0};
    inline static std::atomic<long> default_ctors{0};
    inline static std::atomic<long> value_ctors{0};
    inline static std::atomic<long> copy_ctors{0};
    inline static std::atomic<long> move_ctors{0};
    inline static std::atomic<long> dtors{0};

    int value = 0;

    static void reset_counters() noexcept {
        live.store(0);
        default_ctors.store(0);
        value_ctors.store(0);
        copy_ctors.store(0);
        move_ctors.store(0);
        dtors.store(0);
    }

    Tracked() noexcept : value(0) {
        ++live;
        ++default_ctors;
    }

    explicit Tracked(int v) noexcept : value(v) {
        ++live;
        ++value_ctors;
    }

    Tracked(const Tracked& other) noexcept : value(other.value) {
        ++live;
        ++copy_ctors;
    }

    Tracked(Tracked&& other) noexcept : value(other.value) {
        other.value = -1;
        ++live;
        ++move_ctors;
    }

    Tracked& operator=(const Tracked& other) noexcept {
        value = other.value;
        return *this;
    }

    Tracked& operator=(Tracked&& other) noexcept {
        if (this != &other) {
            value = other.value;
            other.value = -1;
        }
        return *this;
    }

    ~Tracked() {
        value = -999;
        --live;
        ++dtors;
    }
};

// Note: Move constructor is intentionally NOT noexcept so std::move_if_noexcept
// chooses copy construction when strong exception guarantee is required.
struct FragileItem {
    inline static int copies_remaining_before_throw = -1;
    inline static int live = 0;

    int value;

    explicit FragileItem(int v) : value(v) { ++live; }

    FragileItem(const FragileItem& o) : value(o.value) {
        if (copies_remaining_before_throw == 0) {
            throw std::runtime_error("Simulated copy construction failure");
        }
        if (copies_remaining_before_throw > 0) {
            --copies_remaining_before_throw;
        }
        ++live;
    }

    FragileItem(FragileItem&& o) noexcept(false) : value(o.value) {
        ++live;
    }

    FragileItem& operator=(const FragileItem& o) {
        value = o.value;
        return *this;
    }

    ~FragileItem() { --live; }
};

}  // namespace quant_test
