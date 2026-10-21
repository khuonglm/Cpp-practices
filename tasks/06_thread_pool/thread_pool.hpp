#pragma once

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <vector>

class ThreadPool {
public:
    explicit ThreadPool(size_t num_threads) {
        if (num_threads == 0) {
            throw std::invalid_argument("ThreadPool requires at least 1 thread");
        }
        (void)num_threads;
        throw std::logic_error("TODO: ThreadPool::ThreadPool");
    }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    ~ThreadPool() {}

    template <class F, class... Args>
    auto submit(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>> {
        (void)f;
        (void)sizeof...(args);
        throw std::logic_error("TODO: ThreadPool::submit");
    }
};
