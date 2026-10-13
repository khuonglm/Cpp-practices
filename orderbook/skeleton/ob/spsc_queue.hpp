#pragma once
// ============================================================================
//  M3 SKELETON - implement a bounded lock-free SPSC ring buffer.
//  Exactly ONE producer thread calls push(); exactly ONE consumer calls pop().
//
//  Hints:
//   * Ring buffer of size Capacity (power of two); index wrap = (i + 1) & (Capacity-1).
//   * Reserve one empty slot so "full" (next(tail)==head) is distinct from
//     "empty" (head==tail).
//   * push: relaxed-load tail; if next==acquire-load(head) -> full; write slot;
//           release-store tail.
//   * pop:  relaxed-load head; if head==acquire-load(tail) -> empty; read slot;
//           release-store head.
//   The cache-line padding (alignas kLine on head_/tail_/buf_) is ALREADY GIVEN below -
//   you only implement push/pop. Build the TSan variant (test_spsc_skel_tsan) to prove
//   your memory_order is correct: on arm64 a wrong order is a real bug a plain run may miss.
// ============================================================================
#include <atomic>
#include <array>
#include <cstddef>

namespace ob {

template <class T, std::size_t Capacity>
class SpscQueue {
    static_assert(Capacity >= 2 && (Capacity & (Capacity - 1)) == 0,
                  "Capacity must be a power of two >= 2");
public:
    bool push(const T& v) {
        auto cur_tail = tail_.load(std::memory_order_relaxed);
        if(next_(cur_tail) == head_.load(std::memory_order_acquire)) { // full
            return false;
        }
        buf_[cur_tail] = v;
        tail_.store(next_(cur_tail), std::memory_order_release);
        return true;
    }
    bool pop(T& out) {
        auto cur_head = head_.load(std::memory_order_relaxed);
        if(tail_.load(std::memory_order_acquire) == cur_head) { // empty
            return false;
        }
        out = buf_[cur_head];
        head_.store(next_(cur_head), std::memory_order_release);
        return true;
    }

private:
    size_t next_(const size_t& ind) {
        return (ind + 1) & (Capacity - 1);
    }

    // arm64 / Apple Silicon cache line = 128 bytes (64 would leave head_ & tail_ sharing a line).
    static constexpr std::size_t kLine = 128;
    alignas(kLine) std::atomic<std::size_t> head_{0};
    alignas(kLine) std::atomic<std::size_t> tail_{0};
    alignas(kLine) std::array<T, Capacity> buf_{};
};

} // namespace ob
