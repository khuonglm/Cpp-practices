#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <optional>
#include <random>
#include <stdexcept>
#include <thread>
#include <vector>

inline double norm_cdf(double x) noexcept {
    return 0.5 * std::erfc(-x / std::numbers::sqrt2);
}

inline double norm_pdf(double x) noexcept {
    return std::exp(-0.5 * x * x) / std::sqrt(2.0 * std::numbers::pi);
}

struct Greeks {
    double price = 0.0;
    double delta = 0.0;
    double gamma = 0.0;
    double vega  = 0.0;  // Unscaled partial derivative dV / d(sigma)
};

struct McResult {
    double price   = 0.0;
    double std_err = 0.0;
};

inline Greeks bs(double S, double K, double T, double r, double sigma, bool is_call) {
    (void)S; (void)K; (void)T; (void)r; (void)sigma; (void)is_call;
    throw std::logic_error("TODO: bs()");
}

inline std::optional<double> implied_vol(double mkt_price, double S, double K,
                                         double T, double r, bool is_call) {
    (void)mkt_price; (void)S; (void)K; (void)T; (void)r; (void)is_call;
    throw std::logic_error("TODO: implied_vol()");
}

inline McResult mc_call(double S, double K, double T, double r, double sigma,
                        size_t n_paths, uint64_t seed = 42) {
    (void)S; (void)K; (void)T; (void)r; (void)sigma; (void)n_paths; (void)seed;
    throw std::logic_error("TODO: mc_call()");
}

inline McResult mc_call_parallel(double S, double K, double T, double r, double sigma,
                                 size_t n_paths, size_t num_threads, uint64_t seed = 42) {
    (void)S; (void)K; (void)T; (void)r; (void)sigma; (void)n_paths;
    (void)num_threads; (void)seed;
    throw std::logic_error("TODO: mc_call_parallel()");
}
