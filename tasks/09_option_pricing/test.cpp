#include "option_pricing.hpp"
#include "test_framework.hpp"

#include <cmath>

TEST_CASE("01. Canonical Black-Scholes price and Greeks values") {
    const Greeks call = bs(100.0, 100.0, 1.0, 0.05, 0.20, true);
    const Greeks put  = bs(100.0, 100.0, 1.0, 0.05, 0.20, false);

    REQUIRE_NEAR(call.price, 10.45058357, 1e-7);
    REQUIRE_NEAR(put.price,   5.57352602, 1e-7);
    REQUIRE_NEAR(call.delta,  0.63683065, 1e-7);
    REQUIRE_NEAR(put.delta,  -0.36316935, 1e-7);
    REQUIRE_NEAR(call.gamma,  0.01876203, 1e-7);
    REQUIRE_NEAR(put.gamma,   0.01876203, 1e-7);
    REQUIRE_NEAR(call.vega,  37.52403469, 1e-6);
    REQUIRE_NEAR(put.vega,   37.52403469, 1e-6);
}

TEST_CASE("02. Put-Call Parity across strikes, rates, and maturities") {
    for (double K : {60.0, 85.0, 100.0, 120.0, 155.0}) {
        for (double T : {0.05, 0.5, 2.0}) {
            const double S = 100.0;
            const double r = 0.035;
            const double sigma = 0.32;
            const Greeks c = bs(S, K, T, r, sigma, true);
            const Greeks p = bs(S, K, T, r, sigma, false);
            REQUIRE_NEAR(c.price - p.price, S - K * std::exp(-r * T), 1e-10);
            REQUIRE_NEAR(c.delta - p.delta, 1.0, 1e-11);
        }
    }
}

TEST_CASE("03. Analytical Greeks match central finite differences") {
    const double S = 105.0, K = 100.0, T = 0.75, r = 0.04, sigma = 0.25;
    for (bool is_call : {true, false}) {
        const Greeks g = bs(S, K, T, r, sigma, is_call);

        const double h_s = 1e-4;
        const double v_up   = bs(S + h_s, K, T, r, sigma, is_call).price;
        const double v_mid  = g.price;
        const double v_down = bs(S - h_s, K, T, r, sigma, is_call).price;
        const double fd_delta = (v_up - v_down) / (2.0 * h_s);
        const double fd_gamma = (v_up - 2.0 * v_mid + v_down) / (h_s * h_s);

        const double h_vol = 1e-5;
        const double fd_vega = (bs(S, K, T, r, sigma + h_vol, is_call).price -
                                bs(S, K, T, r, sigma - h_vol, is_call).price) /
                               (2.0 * h_vol);

        REQUIRE_NEAR(g.delta, fd_delta, 1e-6);
        REQUIRE_NEAR(g.gamma, fd_gamma, 1e-5);
        REQUIRE_NEAR(g.vega,  fd_vega,  1e-6);
    }
}

TEST_CASE("04. Safeguarded implied volatility across surface including deep OTM wings") {
    for (bool is_call : {true, false}) {
        for (double K : {70.0, 100.0, 145.0}) {  // K=145 exercises tiny-vega wing bisection fallback!
            for (double true_vol : {0.05, 0.22, 0.65, 1.75}) {
                const double mkt = bs(100.0, K, 0.5, 0.03, true_vol, is_call).price;
                const auto solved = implied_vol(mkt, 100.0, K, 0.5, 0.03, is_call);
                REQUIRE(solved.has_value());
                REQUIRE_NEAR(*solved, true_vol, 1e-6);
            }
        }
    }
}

TEST_CASE("05. Implied volatility rejects arbitrage-violating market prices") {
    const double S = 100.0, K = 90.0, T = 1.0, r = 0.05;
    const double intrinsic_call = S - K * std::exp(-r * T);  // ~14.39

    REQUIRE(!implied_vol(-1.0, S, K, T, r, true).has_value());
    REQUIRE(!implied_vol(intrinsic_call - 0.50, S, K, T, r, true).has_value());
    REQUIRE(!implied_vol(S + 0.01, S, K, T, r, true).has_value());
    REQUIRE(!implied_vol(K * std::exp(-r * T) + 1.0, S, K, T, r, false).has_value());
}

TEST_CASE("06. Single-threaded and parallel antithetic Monte Carlo match Black-Scholes") {
    const double S = 100.0, K = 102.0, T = 1.0, r = 0.05, sigma = 0.25;
    const double exact = bs(S, K, T, r, sigma, true).price;

    const McResult single = mc_call(S, K, T, r, sigma, 120'000, 12345);
    REQUIRE(single.std_err > 0.0);
    REQUIRE(single.std_err < 0.08);
    REQUIRE_NEAR(single.price, exact, 3.5 * single.std_err);

    const McResult par = mc_call_parallel(S, K, T, r, sigma, 200'000, 4, 98765);
    REQUIRE(par.std_err > 0.0);
    REQUIRE(par.std_err < 0.06);
    REQUIRE_NEAR(par.price, exact, 3.5 * par.std_err);
}

RUN_TEST_SUITE("09 — Option Pricing: BS, Implied Vol & Parallel Monte Carlo")
