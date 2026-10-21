#include "rolling_stats.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

namespace {

double ref_mean(const std::vector<double>& xs) {
    double s = 0.0;
    for (double v : xs) s += v;
    return s / static_cast<double>(xs.size());
}

double ref_sample_var(const std::vector<double>& xs) {
    if (xs.size() <= 1) return 0.0;
    const double m = ref_mean(xs);
    double m2 = 0.0;
    for (double v : xs) {
        const double d = v - m;
        m2 += d * d;
    }
    return m2 / static_cast<double>(xs.size() - 1);
}

}  // namespace

TEST_CASE("01. RollingMeanVar matches exact two-pass reference on random stream") {
    constexpr size_t W = 12;
    RollingMeanVar rmv(W);
    std::mt19937_64 rng(42);
    std::normal_distribution<double> dist(250.0, 7.5);

    std::vector<double> history;
    for (size_t i = 0; i < 500; ++i) {
        const double x = dist(rng);
        history.push_back(x);
        rmv.push(x);

        const size_t start = history.size() > W ? history.size() - W : 0;
        std::vector<double> win(history.begin() + static_cast<ptrdiff_t>(start), history.end());

        REQUIRE_EQ(rmv.count(), win.size());
        REQUIRE_NEAR(rmv.mean(), ref_mean(win), 1e-9);
        REQUIRE_NEAR(rmv.variance(), ref_sample_var(win), 1e-8);
    }
}

TEST_CASE("02. Catastrophic cancellation torture test around 1e9 base price") {
    // Naive sum(x^2) - sum(x)^2/W completely fails here because (1e9)^2 = 1e18 > 2^53!
    RollingMeanVar rmv(4);
    const double base = 1'000'000'000.0;
    for (int cycle = 0; cycle < 1000; ++cycle) {
        rmv.push(base + 1.0);
        rmv.push(base + 2.0);
        rmv.push(base + 3.0);
        rmv.push(base + 4.0);
    }
    REQUIRE_NEAR(rmv.mean(), base + 2.5, 1e-5);
    // Sample variance of {1, 2, 3, 4} is ((1.5^2 + 0.5^2)*2) / 3 = 5.0 / 3.0
    REQUIRE_NEAR(rmv.variance(), 5.0 / 3.0, 1e-4);
}

TEST_CASE("03. Huge outlier sliding out leaves non-negative zero variance") {
    RollingMeanVar rmv(4);
    rmv.push(1.0e8);
    rmv.push(10.0);
    rmv.push(10.0);
    rmv.push(10.0);
    rmv.push(10.0);  // Outlier 1e8 just slid out!

    REQUIRE(rmv.variance() >= 0.0);
    REQUIRE(!std::isnan(std::sqrt(rmv.variance())));
    REQUIRE_NEAR(rmv.variance(), 0.0, 1e-7);
}

TEST_CASE("04. EWMA initialization without zero bias and decay accuracy") {
    Ewma ewma(0.25);
    REQUIRE(!ewma.initialized());

    ewma.push(100.0);
    REQUIRE(ewma.initialized());
    REQUIRE_NEAR(ewma.value(), 100.0, 1e-12);

    ewma.push(108.0);  // 0.25 * 108 + 0.75 * 100 = 102.0
    REQUIRE_NEAR(ewma.value(), 102.0, 1e-12);

    ewma.push(98.0);   // 0.25 * 98 + 0.75 * 102 = 101.0
    REQUIRE_NEAR(ewma.value(), 101.0, 1e-12);
}

TEST_CASE("05. RollingMax and RollingMin match window scans on adversarial patterns") {
    constexpr size_t W = 7;
    RollingMax rmax(W);
    RollingMin rmin(W);

    std::vector<double> inputs = {
        // Strictly decreasing then plateau then sharp spike
        50, 49, 48, 47, 46, 45, 44, 43, 42,
        10, 10, 10, 10, 10, 10, 10,
        99, 5, 6, 7, 8, 9, 10, 11, 12
    };

    for (size_t i = 0; i < inputs.size(); ++i) {
        rmax.push(inputs[i]);
        rmin.push(inputs[i]);

        const size_t start = (i + 1 > W) ? (i + 1 - W) : 0;
        const double expected_max = *std::max_element(inputs.begin() + static_cast<ptrdiff_t>(start),
                                                      inputs.begin() + static_cast<ptrdiff_t>(i + 1));
        const double expected_min = *std::min_element(inputs.begin() + static_cast<ptrdiff_t>(start),
                                                      inputs.begin() + static_cast<ptrdiff_t>(i + 1));
        REQUIRE_NEAR(rmax.value(), expected_max, 1e-12);
        REQUIRE_NEAR(rmin.value(), expected_min, 1e-12);
    }
}

TEST_CASE("06. RunningMedian matches exact sorted median across odd and even counts") {
    RunningMedian med;
    std::mt19937_64 rng(999);
    std::uniform_real_distribution<double> dist(-500.0, 500.0);

    std::vector<double> seen;
    for (size_t i = 0; i < 400; ++i) {
        const double v = (i % 9 == 0) ? 42.0 : dist(rng);  // Mix exact duplicates too
        seen.push_back(v);
        med.push(v);

        std::vector<double> sorted = seen;
        std::sort(sorted.begin(), sorted.end());
        const size_t n = sorted.size();
        const double expected =
            (n % 2 == 1) ? sorted[n / 2] : 0.5 * (sorted[n / 2 - 1] + sorted[n / 2]);

        REQUIRE_EQ(med.count(), n);
        REQUIRE_NEAR(med.median(), expected, 1e-12);
    }
}

RUN_TEST_SUITE("05 — Rolling Statistics on a Tick Stream")
