
#include <memory>
#include <algorithm>
#include <cassert>
#include <utility>
#include <cstddef>

template<typename T>
class Vector {
public:
    Vector(): size_{0}, capacity_{128}, buf_{std::make_unique<T[]>(capacity_)} {}
    Vector(const Vector& other)
    : size_{other.size_}, capacity_{other.capacity_}, buf_{std::make_unique<T[]>(other.capacity_)}{
        std::copy(other.buf_.get(), other.buf_.get() + other.size_, buf_.get());
    }
    Vector& operator=(const Vector& other) {
        // if(this != &other) {
        //     size_ = other.size_;
        //     capacity_ = other.capacity_;
        //     buf_ = std::make_unique<T[]>(capacity_); // can throw
        //     std::copy(other.buf_.get(), other.buf_.get() + other.size_, buf_.get());
        //     // can throw -> use copy-and-swap
        //     // this could be half-modified if throws
        // }
        // return *this;
        Vector tmp(other); // phase 1: allocate and copy, throwable here
        swap(tmp); // phase 2: non-throwable
        return *this;
    }

    Vector(Vector&& other) noexcept
    : capacity_{std::exchange(other.capacity_, 0)},
      size_{std::exchange(other.size_, 0)},
      buf_{std::move(other.buf_)} {}
    Vector& operator=(Vector&& other) noexcept {
        if(this != &other) {
            size_ = std::exchange(other.size_, 0);
            capacity_ = std::exchange(other.capacity_, 0);
            buf_ = std::move(other.buf_);
        }
        return *this;
    }

    void swap(Vector& other) noexcept {
        std::swap(size_, other.size_);
        std::swap(capacity_, other.capacity_);
        std::swap(buf_, other.buf_);
    }

    ~Vector() = default;

    T& operator[](std::size_t index) {
        assert(index < size_ && "out of bound");
        return buf_[index];
    }
    const T& operator[](std::size_t index) const {
        assert(index < size_ && "out of bound");
        return buf_[index];
    }

    void push_back(T value) {
        if (size_ == capacity_)
            grow_();
        buf_[size_++] = std::move(value);
    }
    
    void pop_back() {
        assert(size_ > 0 && "cannot pop_back an empty vector");
        buf_[--size_] = T{}; // release resources held by the popped slot
    }

    std::size_t size() const { return size_; }
    std::size_t capacity() const { return capacity_; }
    bool empty() const { return size_ == 0; }
private:
    static constexpr std::size_t kInitialCapacity = 128;
    void grow_() {
        std::size_t new_cap = capacity == 0? kInitialCapacity: capacity_ << 1;
        auto new_buf = std::make_unique<T[]>(new_cap);
        std::move(buf_.get(), buf_.get() + size_, new_buf.get());
        buf_ = std::move(new_buf);
        capacity_ = new_cap; // only when allocation succeeded
    }
    std::size_t capacity_;
    std::size_t size_;
    std::unique_ptr<T[]> buf_;
};