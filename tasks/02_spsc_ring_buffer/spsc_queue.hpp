#pragma once

#include <atomic>
#include <cstddef>
#include <stdexcept>
#include <memory>
#include <atomic>

template <typename T, size_t N>
class SpscQueue {
    static_assert(N >= 2 && (N & (N - 1)) == 0, "Capacity N must be a power of two >= 2");

public:
    SpscQueue(): buf_{std::make_unique<T[]>(N)}, head_{0}, tail_{0} {}
    SpscQueue(const SpscQueue<T, N>& other) = delete;
    SpscQueue& operator=(const SpscQueue<T, N>& other) = delete;
    ~SpscQueue() = default;

    bool try_push(const T& value) {
        return push_(value);
    }

    // first overload will copy and create a temporary for value
    bool try_push(T&& value) {
        return push_(std::move(value));
    }

    bool try_pop(T& out) {
        // no other thread write it except itself -> relaxed
        auto headIndex = head_.load(std::memory_order_relaxed);
        // empty
        if(tail_.load(std::memory_order_acquire) - headIndex == 0)
            return false;
        
        out = buf_[head_ & (N - 1)];
        head_.fetch_add(1, std::memory_order_release);
        return true;
    }

    std::size_t size_approx() const noexcept { return 0; }
    static constexpr std::size_t capacity() noexcept { return N; }

private:
    static constexpr std::size_t kLine = 128;
    alignas(kLine) std::unique_ptr<T[]> buf_;
    alignas(kLine) std::atomic<std::size_t> head_;
    alignas(kLine) std::atomic<std::size_t> tail_;

    template<typename U>
    bool push_(U&& value) {
        // no other thread write it except itself -> relaxed
        auto tailIndex = tail_.load(std::memory_order_relaxed);
        // full, use the property of unsigned number, wrap around
        if(tailIndex - head_.load(std::memory_order_acquire) == N)
            return false;
        
        buf_[tail_ & (N - 1)] = std::forward<U>(value);
        tail_.fetch_add(1, std::memory_order_release);
        return true;
    }
};
