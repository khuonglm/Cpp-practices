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
    }

    BlockingQueue(const BlockingQueue&) = delete;
    BlockingQueue& operator=(const BlockingQueue&) = delete;

    bool push(T value) {
        std::unique_lock<std::mutex> lk(m_);
        not_full_.wait(lk, [this] {
            return q_.size() < cap_ || closed_;
        });
        if(closed_)
            return false;
        
        q_.push(std::move(value));
        lk.unlock();

        not_empty_.notify_one();
        return true;
    }

    bool try_push(T value) {
        std::unique_lock<std::mutex> lk(m_);
        if(closed_ || q_.size() >= cap_)
            return false;
        
        q_.push(std::move(value));
        lk.unlock();

        not_empty_.notify_one();
        return true;
    }

    std::optional<T> pop() {
        std::unique_lock<std::mutex> lk(m_);
        not_empty_.wait(lk, [this] {
            return !q_.empty() || closed_;
        });
        if(q_.empty() && closed_)
            return std::nullopt;
        
        T value = std::move(q_.front());
        q_.pop();
        lk.unlock();

        not_full_.notify_one();
        return value;
    }

    std::optional<T> try_pop() {
        std::unique_lock<std::mutex> lk(m_);
        if(q_.empty())
            return std::nullopt;
        
        T value = std::move(q_.front());
        q_.pop();
        lk.unlock();

        not_full_.notify_one();
        return value;
    }

    void close() noexcept {
        {
            std::lock_guard<std::mutex> lk(m_);
            if(closed_)
                return;
            closed_ = true;
        }
        not_empty_.notify_all();
        not_full_.notify_all();
    }

    bool is_closed() const noexcept {
        std::lock_guard<std::mutex> lk(m_);
        return closed_;
    }

private:
    size_t cap_;
    mutable std::mutex m_;
    std::condition_variable not_empty_;
    std::condition_variable not_full_;
    std::queue<T> q_;
    bool closed_ = false;
};
