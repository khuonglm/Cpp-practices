#pragma once

#include <cstddef>
#include <deque>
#include <functional>
#include <queue>
#include <stdexcept>
#include <utility>
#include <vector>

class RollingMeanVar {
public:
    explicit RollingMeanVar(size_t window) {
        if (window == 0) throw std::invalid_argument("window must be > 0");
        (void)window;
    }

    void push(double x) {
        (void)x;
        throw std::logic_error("TODO: RollingMeanVar::push");
    }

    double mean() const noexcept { return 0.0; }
    double variance() const noexcept { return 0.0; }
    size_t count() const noexcept { return 0; }
};

class Ewma {
public:
    explicit Ewma(double alpha) {
        if (!(alpha > 0.0 && alpha <= 1.0)) {
            throw std::invalid_argument("alpha must be in (0, 1]");
        }
        (void)alpha;
    }

    void push(double x) {
        (void)x;
        throw std::logic_error("TODO: Ewma::push");
    }

    double value() const noexcept { return 0.0; }
    bool initialized() const noexcept { return false; }
};

class RollingMax {
public:
    explicit RollingMax(size_t window) {
        if (window == 0) throw std::invalid_argument("window must be > 0");
        (void)window;
    }

    void push(double x) {
        (void)x;
        throw std::logic_error("TODO: RollingMax::push");
    }

    double value() const {
        throw std::logic_error("TODO: RollingMax::value");
    }
};

class RollingMin {
public:
    explicit RollingMin(size_t window) {
        if (window == 0) throw std::invalid_argument("window must be > 0");
        (void)window;
    }

    void push(double x) {
        (void)x;
        throw std::logic_error("TODO: RollingMin::push");
    }

    double value() const {
        throw std::logic_error("TODO: RollingMin::value");
    }
};

class RunningMedian {
public:
    RunningMedian() = default;

    void push(double x) {
        (void)x;
        throw std::logic_error("TODO: RunningMedian::push");
    }

    double median() const {
        throw std::logic_error("TODO: RunningMedian::median");
    }

    size_t count() const noexcept { return 0; }
};
