// M4 - per-operation latency benchmark for the M1 engine (baseline).
// Copy/adapt to point at MatchingEngineFast to measure your M2 improvement.
#include "ob/matching_engine.hpp"
#include <chrono>
#include <random>
#include <vector>
#include <algorithm>
#include <cstdio>
#include <cstddef>

using namespace ob;

int main() {
    std::mt19937 rng(2024);
    std::uniform_int_distribution<int> op(0, 99), s01(0, 1), off(-15, 15), q(1, 10);
    constexpr int WARM = 100'000, N = 1'000'000;

    MatchingEngine e;
    OrderId next = 1;
    std::vector<OrderId> live;
    std::vector<long long> lat; lat.reserve(N);

    auto step = [&](bool timed) {
        const int o = op(rng);
        const auto t0 = std::chrono::steady_clock::now();
        if (o < 70)      { Price p = 1000 + off(rng); Side s = s01(rng) ? Side::Sell : Side::Buy;
                           OrderId id = next++; e.add_limit(id, s, p, static_cast<Qty>(q(rng))); live.push_back(id); }
        else if (o < 85) { Side s = s01(rng) ? Side::Sell : Side::Buy; e.add_market(next++, s, static_cast<Qty>(q(rng))); }
        else             { if (!live.empty()) e.cancel(live[rng() % live.size()]); }
        const auto t1 = std::chrono::steady_clock::now();
        if (timed) lat.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
    };

    for (int i = 0; i < WARM; ++i) step(false);
    for (int i = 0; i < N;    ++i) step(true);

    std::sort(lat.begin(), lat.end());
    auto pct = [&](double p) { return lat[static_cast<std::size_t>(p * (lat.size() - 1))]; };
    double mean = 0; for (auto x : lat) mean += static_cast<double>(x); mean /= static_cast<double>(lat.size());

    std::printf("MatchingEngine (M1)  ops=%zu\n", lat.size());
    std::printf("  mean=%.1f ns | p50=%lld | p90=%lld | p99=%lld | p99.9=%lld | max=%lld  (ns)\n",
                mean, pct(0.50), pct(0.90), pct(0.99), pct(0.999), lat.back());
    return 0;
}
