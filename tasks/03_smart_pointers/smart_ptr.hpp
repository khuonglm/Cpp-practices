#pragma once

#include <atomic>
#include <cstddef>
#include <stdexcept>
#include <utility>

template <typename T>
struct DefaultDeleter {
    constexpr DefaultDeleter() noexcept = default;
    void operator()(T* p) const noexcept { delete p; }
};

template <typename T, typename Deleter = DefaultDeleter<T>>
class UniquePtr {
public:
    constexpr UniquePtr() noexcept = default;
    constexpr UniquePtr(std::nullptr_t) noexcept {}
    explicit UniquePtr(T* p) noexcept { (void)p; }
    UniquePtr(T* p, Deleter d) noexcept {
        (void)p;
        (void)d;
    }

    ~UniquePtr() {}

    UniquePtr(const UniquePtr&) = delete;
    UniquePtr& operator=(const UniquePtr&) = delete;

    UniquePtr(UniquePtr&& other) noexcept { (void)other; }
    UniquePtr& operator=(UniquePtr&& other) noexcept {
        (void)other;
        return *this;
    }

    T* release() noexcept { return nullptr; }
    void reset(T* p = nullptr) noexcept { (void)p; }

    T* get() const noexcept { return nullptr; }
    T& operator*() const { throw std::logic_error("TODO: UniquePtr::operator*"); }
    T* operator->() const noexcept { return nullptr; }
    explicit operator bool() const noexcept { return false; }
};

template <typename T>
class WeakPtr;

template <typename T>
class SharedPtr {
public:
    constexpr SharedPtr() noexcept = default;
    constexpr SharedPtr(std::nullptr_t) noexcept {}
    explicit SharedPtr(T* p) {
        (void)p;
        throw std::logic_error("TODO: SharedPtr::SharedPtr(T*)");
    }

    SharedPtr(const SharedPtr& other) noexcept { (void)other; }
    SharedPtr(SharedPtr&& other) noexcept { (void)other; }

    SharedPtr& operator=(const SharedPtr& other) noexcept {
        (void)other;
        return *this;
    }
    SharedPtr& operator=(SharedPtr&& other) noexcept {
        (void)other;
        return *this;
    }

    ~SharedPtr() {}

    void reset(T* p = nullptr) {
        (void)p;
        throw std::logic_error("TODO: SharedPtr::reset");
    }

    long use_count() const noexcept { return 0; }
    T* get() const noexcept { return nullptr; }
    T& operator*() const { throw std::logic_error("TODO: SharedPtr::operator*"); }
    T* operator->() const noexcept { return nullptr; }
    explicit operator bool() const noexcept { return false; }

private:
    friend class WeakPtr<T>;
};

template <typename T>
class WeakPtr {
public:
    constexpr WeakPtr() noexcept = default;
    WeakPtr(const SharedPtr<T>& sp) noexcept { (void)sp; }
    WeakPtr(const WeakPtr& other) noexcept { (void)other; }
    WeakPtr(WeakPtr&& other) noexcept { (void)other; }

    WeakPtr& operator=(const SharedPtr<T>& sp) noexcept {
        (void)sp;
        return *this;
    }
    WeakPtr& operator=(const WeakPtr& other) noexcept {
        (void)other;
        return *this;
    }
    WeakPtr& operator=(WeakPtr&& other) noexcept {
        (void)other;
        return *this;
    }

    ~WeakPtr() {}

    void reset() noexcept {}
    long use_count() const noexcept { return 0; }
    bool expired() const noexcept { return use_count() == 0; }

    SharedPtr<T> lock() const noexcept { return SharedPtr<T>{}; }
};
