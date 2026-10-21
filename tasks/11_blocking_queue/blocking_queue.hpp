#pragma once

#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <optional>
#include <queue>
#include <stdexcept>

template <typename T>
class BlockingQueue {
public:
    explicit BlockingQueue(size_t capacity) : cap_(capacity) {
        if (capacity == 0) {
            throw std::invalid_argument("capacity must be > 0");
        }
        throw std::logic_error("TODO: BlockingQueue::BlockingQueue");
    }

    BlockingQueue(const BlockingQueue&) = delete;
    BlockingQueue& operator=(const BlockingQueue&) = delete;

    bool push(T value) {
        (void)value;
        throw std::logic_error("TODO: BlockingQueue::push");
    }

    bool try_push(T value) {
        (void)value;
        throw std::logic_error("TODO: BlockingQueue::try_push");
    }

    std::optional<T> pop() {
        throw std::logic_error("TODO: BlockingQueue::pop");
    }

    std::optional<T> try_pop() {
        throw std::logic_error("TODO: BlockingQueue::try_pop");
    }

    void close() noexcept {}

    bool is_closed() const noexcept {
        std::lock_guard<std::mutex> lk(m_);
        return closed_;
    }

private:
    size_t                        cap_;
    mutable std::mutex            m_;
    std::condition_variable       not_empty_;
    std::condition_variable       not_full_;
    std::queue<T>                 q_;
    bool                          closed_ = false;
};
