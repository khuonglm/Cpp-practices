#pragma once

#include <cstddef>
#include <memory>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>

template <typename T>
class Vector {
public:
    Vector() noexcept = default;

    Vector(const Vector& other) {
        (void)other;
        throw std::logic_error("TODO: Vector copy constructor");
    }

    Vector(Vector&& other) noexcept { (void)other; }

    Vector& operator=(const Vector& other) {
        (void)other;
        throw std::logic_error("TODO: Vector copy assignment");
    }

    Vector& operator=(Vector&& other) noexcept {
        (void)other;
        return *this;
    }

    ~Vector() {}

    void push_back(const T& value) {
        (void)value;
        throw std::logic_error("TODO: Vector::push_back(const T&)");
    }

    void push_back(T&& value) {
        (void)value;
        throw std::logic_error("TODO: Vector::push_back(T&&)");
    }

    template <typename... Args>
    T& emplace_back(Args&&... args) {
        (void)sizeof...(args);
        throw std::logic_error("TODO: Vector::emplace_back");
    }

    void pop_back() noexcept {}

    void reserve(size_t new_cap) {
        (void)new_cap;
        throw std::logic_error("TODO: Vector::reserve");
    }

    void resize(size_t new_size) {
        (void)new_size;
        throw std::logic_error("TODO: Vector::resize");
    }

    void clear() noexcept {}

    T& operator[](size_t i) noexcept { return data_[i]; }
    const T& operator[](size_t i) const noexcept { return data_[i]; }

    size_t size() const noexcept { return size_; }
    size_t capacity() const noexcept { return cap_; }
    bool empty() const noexcept { return size_ == 0; }

    T* data() noexcept { return data_; }
    const T* data() const noexcept { return data_; }
    T* begin() noexcept { return data_; }
    T* end() noexcept { return data_ + size_; }
    const T* begin() const noexcept { return data_; }
    const T* end() const noexcept { return data_ + size_; }

private:
    T*     data_ = nullptr;
    size_t size_ = 0;
    size_t cap_  = 0;
};
