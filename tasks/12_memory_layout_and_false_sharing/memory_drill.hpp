#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

struct StructA { char a; double b; char c; };
struct StructB { double b; char a; char c; };
struct StructC { char a; int b; short c; };
struct StructD { int b; short c; char a; };
struct alignas(64) StructE { int x; };

#pragma pack(push, 1)
struct StructF { char a; int b; };
#pragma pack(pop)

// Step 1: Fill in your hand-calculated values BEFORE running the compiler!
struct LayoutPredictions {
    static constexpr size_t kSizeofA  = 0;  // TODO
    static constexpr size_t kSizeofB  = 0;  // TODO
    static constexpr size_t kSizeofC  = 0;  // TODO
    static constexpr size_t kSizeofD  = 0;  // TODO
    static constexpr size_t kSizeofE  = 0;  // TODO
    static constexpr size_t kSizeofF  = 0;  // TODO

    static constexpr size_t kOffsetAb = 0;  // TODO: offsetof(StructA, b)
    static constexpr size_t kOffsetCb = 0;  // TODO: offsetof(StructC, b)
    static constexpr size_t kOffsetCc = 0;  // TODO: offsetof(StructC, c)
};

// Step 2: False Sharing vs Cache-Line Padded Counters
struct SharedCounters {
    std::atomic<uint64_t> a{0};
    std::atomic<uint64_t> b{0};
};

struct PaddedCounters {
    // TODO: isolate 'a' and 'b' onto separate 64-byte cache lines!
    std::atomic<uint64_t> a{0};
    std::atomic<uint64_t> b{0};
};

struct FalseSharingTimings {
    double shared_seconds = 0.0;
    double padded_seconds = 0.0;
};

inline FalseSharingTimings run_false_sharing_benchmark(uint64_t iterations) {
    (void)iterations;
    throw std::logic_error("TODO: run_false_sharing_benchmark");
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
    (void)quotes;
    throw std::logic_error("TODO: sum_bids_aos");
}

inline double sum_bids_soa(const QuotesSoA& quotes) {
    (void)quotes;
    throw std::logic_error("TODO: sum_bids_soa");
}
