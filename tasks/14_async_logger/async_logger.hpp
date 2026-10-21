#pragma once

// Tip: Reuse your SpscQueue from Task #02 (#include "02_spsc_ring_buffer/spsc_queue.hpp")
// or embed a dedicated ring buffer per registered thread.

#include <atomic>
#include <bit>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

template <typename T>
concept LogScalar = std::is_integral_v<std::decay_t<T>> || std::is_floating_point_v<std::decay_t<T>>;

class AsyncLogger {
public:
    explicit AsyncLogger(const std::string& file_path) {
        (void)file_path;
        throw std::logic_error("TODO: AsyncLogger constructor");
    }

    AsyncLogger(const AsyncLogger&) = delete;
    AsyncLogger& operator=(const AsyncLogger&) = delete;

    ~AsyncLogger() {}

    // Formats '{}' placeholders asynchronously on background thread:
    // e.g., logger.log("Fill id={} side={} px={} delta={}", 42ULL, 1, 105.25, -3);
    // Output line format: "[<timestamp>] <formatted text>\n"
    template <LogScalar... Args>
    void log(const char* fmt, Args... args) {
        static_assert(sizeof...(Args) <= 4, "Up to 4 scalar arguments supported");
        (void)fmt;
        ((void)args, ...);
        throw std::logic_error("TODO: AsyncLogger::log");
    }

    uint64_t dropped_count() const noexcept {
        return dropped_.load(std::memory_order_relaxed);
    }

private:
    std::atomic<uint64_t> dropped_{0};
};
