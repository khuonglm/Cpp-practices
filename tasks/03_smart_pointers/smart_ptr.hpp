#pragma once

#include <atomic>
#include <cstddef>
#include <stdexcept>
#include <utility>
#include <atomic>

template <typename T>
struct DefaultDeleter {
    constexpr DefaultDeleter() noexcept = default;
    void operator()(T* p) const noexcept { delete p; }
};

template <typename T, typename Deleter = DefaultDeleter<T>>
class UniquePtr: private Deleter {
public:
    constexpr UniquePtr() noexcept = default;
    constexpr UniquePtr(std::nullptr_t) noexcept {}
    explicit UniquePtr(T* p) noexcept : p_{p} {}
    UniquePtr(T* p, Deleter d)
        noexcept (std::is_nothrow_move_constructible_v<Deleter>)
        : Deleter(std::move(d)), p_{p} {}

    ~UniquePtr() {
        if(p_) deleter_()(p_); 
    }

    UniquePtr(const UniquePtr&) = delete;
    UniquePtr& operator=(const UniquePtr&) = delete;

    UniquePtr(UniquePtr&& other)
        noexcept(std::is_nothrow_move_constructible_v<Deleter>)
        : Deleter(std::move(other.deleter_()))
        , p_{std::exchange(other.p_, nullptr)} {}
    UniquePtr& operator=(UniquePtr&& other) noexcept
        requires std::is_nothrow_move_assignable_v<Deleter> 
    {
        if(this != &other) {
            reset(other.release());
            deleter_() = std::move(other.deleter_());
        }
        return *this;
    }

    T* release() noexcept { return std::exchange(p_, nullptr); }
    void reset(T* p = nullptr) noexcept { 
        if(p_) deleter_()(p_);
        p_ = p;
    }

    T* get() const noexcept { return p_; }
    T& operator*() const { return *p_; }
    T* operator->() const noexcept { return p_; }
    explicit operator bool() const noexcept { return p_ != nullptr; }

private:
    T* p_;

    Deleter& deleter_() noexcept {
        return static_cast<Deleter&>(*this);
    }

    const Deleter& deleter_() const noexcept {
        return static_cast<const Deleter&>(*this);
    }
};

template <typename T>
class ControlBlock {
public:
    ControlBlock(T* p) noexcept: p_{p} {}

    long use_count() { return strong_.load(std::memory_order_relaxed); }

    long addStrong(int value) {
        if(value > 0)
            return strong_.fetch_add(value, std::memory_order_relaxed);
        
        return strong_.fetch_sub(-value, std::memory_order_acq_rel);
    }

    long addWeak(int value) {
        if(value > 0)
            return weak_.fetch_add(value, std::memory_order_relaxed);
        
        return weak_.fetch_sub(-value, std::memory_order_acq_rel);
    }

    bool try_add_strong() {
        long strong = strong_.load(std::memory_order_relaxed);
        while(strong > 0) {
            if(strong_.compare_exchange_weak(strong, strong + 1,
                std::memory_order_acq_rel, std::memory_order_relaxed))
                return true;
        }
        return false;
    }

    void destroyObject() { delete p_; }
    void destroySelf() { delete this; }

private:
    std::atomic<long> strong_{1};
    std::atomic<long> weak_{1};
    T* p_ = nullptr;
};

template <typename T>
class WeakPtr;

template <typename T>
class SharedPtr {
public:
    constexpr SharedPtr() noexcept = default;
    constexpr SharedPtr(std::nullptr_t) noexcept {}
    explicit SharedPtr(T* p): p_{p}, cb_{new ControlBlock<T>(p_)} {}

    SharedPtr(const SharedPtr& other) noexcept
        : p_{other.p_}, cb_{other.cb_} { cb_->addStrong(1); }
    SharedPtr(SharedPtr&& other) noexcept
        : p_{std::exchange(other.p_, nullptr)},
          cb_{std::exchange(other.cb_, nullptr)} {}

    SharedPtr& operator=(const SharedPtr& other) noexcept {
        if(this != &other) {
            destroy_();
            p_ = other.p_;
            cb_ = other.cb_;
            cb_->addStrong(1);
        }
        return *this;
    }
    SharedPtr& operator=(SharedPtr&& other) noexcept {
        if(this != &other) {
            destroy_();
            p_ = std::exchange(other.p_, nullptr);
            cb_ = std::exchange(other.cb_, nullptr);
        }
        return *this;
    }

    ~SharedPtr() { destroy_(); }

    void reset(T* p = nullptr) {
        if(p == p_)
            return;
        
        destroy_();
        if(p != nullptr) {
            p_ = p;
            cb_ = new ControlBlock<T>(p_);
        }
    }

    long use_count() const noexcept { return cb_? cb_->use_count(): 0; }
    T* get() const noexcept { return p_; }
    T& operator*() const { throw *p_; }
    T* operator->() const noexcept { return p_; }
    explicit operator bool() const noexcept { return p_ != nullptr; }

private:
    void destroy_() {
        if(cb_ && cb_->addStrong(-1) == 1) {
            cb_->destroyObject();
            if(cb_->addWeak(-1) == 1) {
                cb_->destroySelf();
            }
        }
        p_ = nullptr;
        cb_ = nullptr;
    }

    friend class WeakPtr<T>;
    T* p_ = nullptr;
    ControlBlock<T>* cb_ = nullptr;
};

template <typename T>
class WeakPtr {
public:
    constexpr WeakPtr() noexcept = default;
    WeakPtr(const SharedPtr<T>& sp) noexcept
        : p_{sp.p_}, cb_{sp.cb_} { cb_->addWeak(1); }
    WeakPtr(const WeakPtr& other) noexcept
        : p_{other.p_}, cb_{other.cb_} { cb_->addWeak(1); }
    WeakPtr(WeakPtr&& other) noexcept
        : p_{std::exchange(other.p_, nullptr)},
          cb_{std::exchange(other.cb_, nullptr)} {}

    WeakPtr& operator=(const SharedPtr<T>& sp) noexcept {
        reset();
        p_ = sp.p_;
        cb_ = sp.cb_;
        cb_->addWeak(1);
        return *this;
    }
    WeakPtr& operator=(const WeakPtr& other) noexcept {
        if(this != &other) {
            reset();
            p_ = other.p_;
            cb_ = other.cb_;
            cb_->addWeak(1);
        }
        return *this;
    }
    WeakPtr& operator=(WeakPtr&& other) noexcept {
        if(this != &other) {
            reset();
            p_ = std::exchange(other.p_, nullptr);
            cb_ = std::exchange(other.cb_, nullptr);
        }
        return *this;
    }

    ~WeakPtr() { reset(); }

    void reset() noexcept {
        if(cb_ && cb_->addWeak(-1) == 1) {
            cb_->destroySelf();
        }
        p_ = nullptr;
        cb_ = nullptr;
    }
    long use_count() const noexcept { return cb_? cb_->use_count(): 0; }
    bool expired() const noexcept { return use_count() == 0; }

    SharedPtr<T> lock() const noexcept {
        SharedPtr<T> ptr;
        if(cb_->try_add_strong()) {
            ptr.p_ = p_, ptr.cb_ = cb_;
        }
        return ptr;
    }

private:
    T* p_ = nullptr;
    ControlBlock<T>* cb_ = nullptr;
};
