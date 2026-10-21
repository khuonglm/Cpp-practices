#pragma once

#include <atomic>
#include <cstddef>
#include <stdexcept>

template <typename T, size_t N>
class SpscQueue {
    static_assert(N >= 2 && (N & (N - 1)) == 0, "Capacity N must be a power of two >= 2");

public:
    SpscQueue() = default;
    SpscQueue(const SpscQueue&) = delete;
    SpscQueue& operator=(const SpscQueue&) = delete;

    bool try_push(const T& value) {
        (void)value;
        throw std::logic_error("TODO: SpscQueue::try_push(const T&)");
    }

    bool try_push(T&& value) {
        (void)value;
        throw std::logic_error("TODO: SpscQueue::try_push(T&&)");
    }

    bool try_pop(T& out) {
        (void)out;
        throw std::logic_error("TODO: SpscQueue::try_pop");
    }

    size_t size_approx() const noexcept { return 0; }
    static constexpr size_t capacity() noexcept { return N; }
};
