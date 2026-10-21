#pragma once

#include <cstddef>
#include <functional>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>

template <typename Signature>
class Function;

template <typename R, typename... Args>
class Function<R(Args...)> {
public:
    static constexpr size_t kBufferSize = 32;

    Function() noexcept = default;
    Function(std::nullptr_t) noexcept {}

    Function(const Function&) = delete;
    Function& operator=(const Function&) = delete;

    template <typename F>
        requires (!std::is_same_v<std::decay_t<F>, Function>)
    Function(F&& f) {
        (void)f;
        throw std::logic_error("TODO: Function(F&&)");
    }

    Function(Function&& other) noexcept { (void)other; }

    Function& operator=(Function&& other) noexcept {
        (void)other;
        return *this;
    }

    ~Function() {}

    R operator()(Args... args) {
        (void)sizeof...(args);
        if (!vt_) throw std::bad_function_call();
        throw std::logic_error("TODO: Function::operator()");
    }

    explicit operator bool() const noexcept { return vt_ != nullptr; }

private:
    struct VTable {
        R    (*invoke)(void* storage, Args&&...);
        void (*move_to)(void* src, void* dst) noexcept;
        void (*destroy)(void* storage) noexcept;
    };

    alignas(std::max_align_t) unsigned char buf_[kBufferSize]{};
    const VTable*                           vt_ = nullptr;
};
