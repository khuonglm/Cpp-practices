#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>
#include <chrono>

struct StructA { char a; double b; char c; };
struct StructB { double b; char a; char c; };
struct StructC { char a; int b; short c; };
struct StructD { int b; short c; char a; };
struct alignas(64) StructE { int x; };

#pragma pack(push, 1)
// pack(push, k)
// eff alignment = min(natural alignment, k)
// start positions: 0, 1 -> size = 5
struct StructF { char a; int b; };
#pragma pack(pop)

// Step 1: Fill in your hand-calculated values BEFORE running the compiler!
struct LayoutPredictions {
    static constexpr size_t kSizeofA  = 24;
    static constexpr size_t kSizeofB  = 16;
    static constexpr size_t kSizeofC  = 12;
    static constexpr size_t kSizeofD  = 8;
    static constexpr size_t kSizeofE  = 64;
    static constexpr size_t kSizeofF  = 5;

    static constexpr size_t kOffsetAb = 8;  // offsetof(StructA, b)
    static constexpr size_t kOffsetCb = 4;  // offsetof(StructC, b)
    static constexpr size_t kOffsetCc = 8;  // offsetof(StructC, c)
};

// Step 2: False Sharing vs Cache-Line Padded Counters
struct SharedCounters {
    std::atomic<uint64_t> a{0};
    std::atomic<uint64_t> b{0};
};

struct PaddedCounters {
    // isolate 'a' and 'b' onto separate 64-byte cache lines!
    alignas(128) std::atomic<uint64_t> a{0};
    alignas(128) std::atomic<uint64_t> b{0};
};

struct FalseSharingTimings {
    double shared_seconds = 0.0;
    double padded_seconds = 0.0;
};

inline FalseSharingTimings run_false_sharing_benchmark(uint64_t iterations) {
    using Clock = std::chrono::steady_clock;
    FalseSharingTimings t;
    SharedCounters sharedCounters;
    PaddedCounters paddedCounters;
    for(std::size_t i = 0; i < iterations; ++i) {
        auto start = Clock::now();
        sharedCounters.a.fetch_add(1);
        sharedCounters.b.fetch_add(1);
        auto dur = Clock::now() - start;
        t.shared_seconds += std::chrono::duration<double>(dur).count();

        start = Clock::now();
        paddedCounters.a.fetch_add(1);
        paddedCounters.b.fetch_add(1);
        dur = Clock::now() - start;
        t.padded_seconds += std::chrono::duration<double>(dur).count();
    }
    return t;
}

// Step 3: Array-of-Structs (AoS) vs Struct-of-Arrays (SoA)
struct QuoteAoS {
    double  bid;
    double  ask;
    int64_t ts;
    char    sym[8];
};

struct QuotesSoA {
    std::vector<double>  bid;
    std::vector<double>  ask;
    std::vector<int64_t> ts;

    void reserve(size_t n) {
        bid.reserve(n);
        ask.reserve(n);
        ts.reserve(n);
    }

    void push_back(double b, double a, int64_t t) {
        bid.push_back(b);
        ask.push_back(a);
        ts.push_back(t);
    }
};

inline double sum_bids_aos(const std::vector<QuoteAoS>& quotes) {
    double sum = 0;
    for(auto& q: quotes) {
        sum += q.bid;
    }
    return sum;
}

inline double sum_bids_soa(const QuotesSoA& quotes) {
    double sum = 0;
    for(auto& v: quotes.bid) {
        sum += v;
    }
    return sum;
}
