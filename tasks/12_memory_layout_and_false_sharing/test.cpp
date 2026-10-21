#include "memory_drill.hpp"
#include "test_framework.hpp"

#include <chrono>
#include <cstddef>
#include <iostream>
#include <vector>

TEST_CASE("01. Verify hand-calculated sizeof and offsetof predictions") {
    if (LayoutPredictions::kSizeofA == 0) {
        throw std::logic_error("TODO: Fill in LayoutPredictions constants in memory_drill.hpp");
    }

    REQUIRE_EQ(LayoutPredictions::kSizeofA, sizeof(StructA));
    REQUIRE_EQ(LayoutPredictions::kSizeofB, sizeof(StructB));
    REQUIRE_EQ(LayoutPredictions::kSizeofC, sizeof(StructC));
    REQUIRE_EQ(LayoutPredictions::kSizeofD, sizeof(StructD));
    REQUIRE_EQ(LayoutPredictions::kSizeofE, sizeof(StructE));
    REQUIRE_EQ(LayoutPredictions::kSizeofF, sizeof(StructF));

    REQUIRE_EQ(LayoutPredictions::kOffsetAb, offsetof(StructA, b));
    REQUIRE_EQ(LayoutPredictions::kOffsetCb, offsetof(StructC, b));
    REQUIRE_EQ(LayoutPredictions::kOffsetCc, offsetof(StructC, c));
}

TEST_CASE("02. PaddedCounters cache-line separation and False Sharing benchmark") {
    REQUIRE(offsetof(PaddedCounters, b) - offsetof(PaddedCounters, a) >= 64);
    REQUIRE(sizeof(PaddedCounters) >= 128);

    const auto timings = run_false_sharing_benchmark(15'000'000ULL);
    REQUIRE(timings.shared_seconds > 0.0);
    REQUIRE(timings.padded_seconds > 0.0);

    std::cout << "         [BENCH] False Sharing: " << timings.shared_seconds * 1000.0
              << " ms | Cache-Padded: " << timings.padded_seconds * 1000.0
              << " ms | Ratio: " << (timings.shared_seconds / timings.padded_seconds) << "x\n";
}

TEST_CASE("03. AoS vs SoA quote bid reduction parity and memory bandwidth benchmark") {
    constexpr size_t N = 2'000'000;
    std::vector<QuoteAoS> aos;
    aos.reserve(N);
    QuotesSoA soa;
    soa.reserve(N);

    for (size_t i = 0; i < N; ++i) {
        const double b = 100.0 + static_cast<double>(i % 1000) * 0.01;
        const double a = b + 0.05;
        const int64_t ts = static_cast<int64_t>(1'700'000'000ULL + i);
        aos.push_back(QuoteAoS{b, a, ts, "AAPL"});
        soa.push_back(b, a, ts);
    }

    const auto t0 = std::chrono::steady_clock::now();
    const double s_aos = sum_bids_aos(aos);
    const auto t1 = std::chrono::steady_clock::now();
    const double s_soa = sum_bids_soa(soa);
    const auto t2 = std::chrono::steady_clock::now();

    REQUIRE_NEAR(s_aos, s_soa, 1e-2);

    const double aos_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    const double soa_ms = std::chrono::duration<double, std::milli>(t2 - t1).count();
    std::cout << "         [BENCH] AoS (32B/elem): " << aos_ms
              << " ms | SoA (8B/elem): " << soa_ms << " ms\n";
}

RUN_TEST_SUITE("12 — Memory Layout, False Sharing & AoS vs SoA")
