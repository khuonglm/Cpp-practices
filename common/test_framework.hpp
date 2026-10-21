#pragma once

#include <chrono>
#include <cmath>
#include <cstring>
#include <exception>
#include <functional>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace quant_test {

struct AssertionFailure : std::exception {
    std::string message;
    explicit AssertionFailure(std::string msg) : message(std::move(msg)) {}
    const char* what() const noexcept override { return message.c_str(); }
};

template <typename T>
concept Streamable = requires(std::ostream& os, const T& val) {
    { os << val } -> std::same_as<std::ostream&>;
};

template <typename T>
std::string stringify(const T& value) {
    if constexpr (std::is_same_v<std::decay_t<T>, std::nullptr_t>) {
        return "nullptr";
    } else if constexpr (std::is_same_v<std::decay_t<T>, bool>) {
        return value ? "true" : "false";
    } else if constexpr (Streamable<T>) {
        std::ostringstream oss;
        oss << std::setprecision(10) << value;
        return oss.str();
    } else if constexpr (std::is_enum_v<T>) {
        return "enum(" + std::to_string(static_cast<long long>(value)) + ")";
    } else {
        return "<unprintable>";
    }
}

template <typename T>
std::string stringify(const std::optional<T>& opt) {
    return opt.has_value() ? ("Some(" + stringify(*opt) + ")") : "std::nullopt";
}

inline std::string stringify(std::nullopt_t) { return "std::nullopt"; }

struct TestCase {
    std::string name;
    std::function<void()> fn;
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> cases;
    return cases;
}

struct Registrar {
    Registrar(const char* name, std::function<void()> fn) {
        registry().push_back(TestCase{name, std::move(fn)});
    }
};

inline void fail_at(const char* file, int line, const std::string& detail) {
    std::ostringstream oss;
    oss << file << ":" << line << " -> " << detail;
    throw AssertionFailure(oss.str());
}

template <typename L, typename R>
void require_eq(const L& lhs, const R& rhs, const char* lstr, const char* rstr,
                const char* file, int line) {
    if (!(lhs == rhs)) {
        std::ostringstream oss;
        oss << "Expected (" << lstr << " == " << rstr << "), got ["
            << stringify(lhs) << "] vs [" << stringify(rhs) << "]";
        fail_at(file, line, oss.str());
    }
}

inline void require_near(double lhs, double rhs, double eps, const char* lstr,
                         const char* rstr, const char* file, int line) {
    if (!std::isfinite(lhs) || !std::isfinite(rhs) || std::fabs(lhs - rhs) > eps) {
        std::ostringstream oss;
        oss << "Expected |" << lstr << " - " << rstr << "| <= " << eps
            << ", got |" << std::setprecision(12) << lhs << " - " << rhs
            << "| = " << std::fabs(lhs - rhs);
        fail_at(file, line, oss.str());
    }
}

inline int run_all(const char* suite_name) {
    std::cout << "====================================================================\n";
    std::cout << " Suite: " << suite_name << " (" << registry().size() << " test cases)\n";
    std::cout << "====================================================================\n";

    size_t passed = 0;
    size_t todo_count = 0;
    size_t failed = 0;

    for (const auto& tc : registry()) {
        const auto t0 = std::chrono::steady_clock::now();
        try {
            tc.fn();
            const auto us = std::chrono::duration_cast<std::chrono::microseconds>(
                                std::chrono::steady_clock::now() - t0)
                                .count();
            std::cout << "  [PASS] " << tc.name << " (" << us << " us)\n";
            ++passed;
        } catch (const AssertionFailure& af) {
            std::cout << "  [FAIL] " << tc.name << "\n         " << af.what() << "\n";
            ++failed;
        } catch (const std::logic_error& le) {
            if (std::strncmp(le.what(), "TODO", 4) == 0) {
                std::cout << "  [TODO] " << tc.name << " (" << le.what() << ")\n";
                ++todo_count;
            } else {
                std::cout << "  [FAIL] " << tc.name << " (unexpected logic_error: " << le.what() << ")\n";
                ++failed;
            }
        } catch (const std::exception& ex) {
            std::cout << "  [FAIL] " << tc.name << " (unexpected exception: " << ex.what() << ")\n";
            ++failed;
        }
    }

    std::cout << "--------------------------------------------------------------------\n";
    std::cout << " Summary: " << passed << "/" << registry().size() << " passed";
    if (todo_count > 0) std::cout << ", " << todo_count << " pending implementation";
    if (failed > 0) std::cout << ", " << failed << " failed";
    std::cout << "\n====================================================================\n";

    return (failed == 0 && todo_count == 0) ? 0 : 1;
}

}  // namespace quant_test

#define QUANT_CONCAT_INNER(a, b) a##b
#define QUANT_CONCAT(a, b) QUANT_CONCAT_INNER(a, b)

#define TEST_CASE(name_literal)                                                         \
    static void QUANT_CONCAT(quant_tc_fn_, __LINE__)();                                 \
    static ::quant_test::Registrar QUANT_CONCAT(quant_tc_reg_, __LINE__)(               \
        name_literal, &QUANT_CONCAT(quant_tc_fn_, __LINE__));                           \
    static void QUANT_CONCAT(quant_tc_fn_, __LINE__)()

#define REQUIRE(cond)                                                                   \
    do {                                                                                \
        if (!(cond)) ::quant_test::fail_at(__FILE__, __LINE__, "Condition failed: " #cond); \
    } while (0)

#define REQUIRE_EQ(lhs, rhs)                                                            \
    ::quant_test::require_eq((lhs), (rhs), #lhs, #rhs, __FILE__, __LINE__)

#define REQUIRE_NEAR(lhs, rhs, eps)                                                     \
    ::quant_test::require_near(static_cast<double>(lhs), static_cast<double>(rhs),      \
                               static_cast<double>(eps), #lhs, #rhs, __FILE__, __LINE__)

#define REQUIRE_THROWS(expr)                                                            \
    do {                                                                                \
        bool caught_ = false;                                                           \
        try {                                                                           \
            (void)(expr);                                                               \
        } catch (const std::logic_error& le_) {                                         \
            if (std::strncmp(le_.what(), "TODO", 4) == 0) throw;                        \
            caught_ = true;                                                             \
        } catch (...) {                                                                 \
            caught_ = true;                                                             \
        }                                                                               \
        if (!caught_)                                                                   \
            ::quant_test::fail_at(__FILE__, __LINE__, "Expected exception from: " #expr); \
    } while (0)

#define RUN_TEST_SUITE(name_literal)                                                    \
    int main() { return ::quant_test::run_all(name_literal); }
