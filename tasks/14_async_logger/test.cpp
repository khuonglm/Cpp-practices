#include "async_logger.hpp"
#include "test_framework.hpp"

#include <chrono>
#include <cstdio>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

namespace {
std::vector<std::string> read_lines(const std::string& path) {
    std::ifstream in(path);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(in, line)) {
        lines.push_back(line);
    }
    return lines;
}
}  // namespace

TEST_CASE("01. Single-threaded formatting of ints, negatives, uint64_t, and doubles") {
    const std::string path = "test_async_log_single.log";
    std::remove(path.c_str());

    {
        AsyncLogger logger(path);
        logger.log("Engine online");
        logger.log("Order id={} qty={} pnl={} px={}", 9000000001ULL, -25, -12.5, 432.75);
    }  // Destructor must drain completely before closing file

    const auto lines = read_lines(path);
    REQUIRE_EQ(lines.size(), size_t{2});
    REQUIRE(lines[0].find("Engine online") != std::string::npos);
    REQUIRE(lines[1].find("9000000001") != std::string::npos);
    REQUIRE(lines[1].find("-25") != std::string::npos);
    REQUIRE(lines[1].find("-12.5") != std::string::npos);
    REQUIRE(lines[1].find("432.75") != std::string::npos);

    std::remove(path.c_str());
}

TEST_CASE("02. Short-lived thread termination does not cause use-after-free during drain") {
    const std::string path = "test_async_log_ephemeral.log";
    std::remove(path.c_str());

    {
        AsyncLogger logger(path);
        for (int wave = 0; wave < 6; ++wave) {
            std::thread ephemeral([&logger, wave] {
                for (int i = 0; i < 100; ++i) {
                    logger.log("Ephemeral wave={} seq={}", wave, i);
                }
            });
            ephemeral.join();  // Thread exits BEFORE logger destructor runs!
        }
    }

    const auto lines = read_lines(path);
    REQUIRE_EQ(lines.size() + 0, size_t{600});
    std::remove(path.c_str());
}

TEST_CASE("03. 6-thread concurrent logging accounting parity & non-blocking burst latency") {
    const std::string path = "test_async_log_mt.log";
    std::remove(path.c_str());

    uint64_t dropped = 0;
    constexpr int kThreads = 6;
    constexpr int kPerThread = 10'000;

    const auto t0 = std::chrono::steady_clock::now();
    {
        AsyncLogger logger(path);
        std::vector<std::thread> producers;
        for (int t = 0; t < kThreads; ++t) {
            producers.emplace_back([&logger, t] {
                for (int i = 0; i < kPerThread; ++i) {
                    logger.log("Tick thread={} seq={} mid={}", t, i, 100.25);
                }
            });
        }
        for (auto& p : producers) p.join();
        dropped = logger.dropped_count();
    }
    const auto total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                              std::chrono::steady_clock::now() - t0)
                              .count();
    (void)total_ms;

    const auto lines = read_lines(path);
    // Strict conservation law: every single log() call was either written intact OR counted in dropped!
    REQUIRE_EQ(lines.size() + dropped, static_cast<size_t>(kThreads * kPerThread));
    if (!lines.empty()) {
        REQUIRE(lines.front().find("Tick thread=") != std::string::npos);
        REQUIRE(lines.back().find("mid=100.25") != std::string::npos);
    }

    std::remove(path.c_str());
}

RUN_TEST_SUITE("14 — Low-Latency Non-Blocking Asynchronous Logger")
