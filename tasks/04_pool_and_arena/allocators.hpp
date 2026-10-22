#pragma once

#include <cstddef>
#include <cstdint>
#include <new>
#include <stdexcept>
#include <utility>

template <typename T, size_t N>
class ObjectPool {
    static_assert(N > 0, "Pool capacity N must be > 0");

public:
    ObjectPool() = default;
    ObjectPool(const ObjectPool&) = delete;
    ObjectPool& operator=(const ObjectPool&) = delete;

    T* allocate() {
        throw std::logic_error("TODO: ObjectPool::allocate");
    }

    void deallocate(T* p) noexcept {
        (void)p;
    }

    template <typename... Args>
    T* construct(Args&&... args) {
        (void)sizeof...(args);
        throw std::logic_error("TODO: ObjectPool::construct");
    }

    void destroy(T* p) noexcept {
        if (p) {
            p->~T();
            deallocate(p);
        }
    }
};

class Arena {
public:
    Arena(void* buf, size_t cap) noexcept : base_(static_cast<uint8_t*>(buf)), cap_(cap) {}
    Arena(const Arena&) = delete;
    Arena& operator=(const Arena&) = delete;

    void* alloc(size_t size, size_t align = alignof(std::max_align_t)) {
        (void)size;
        (void)align;
        (void)base_;
        throw std::logic_error("TODO: Arena::alloc");
    }

    void reset() noexcept { off_ = 0; }
    size_t used() const noexcept { return off_; }
    size_t capacity() const noexcept { return cap_; }

private:
    uint8_t* base_;
    size_t   cap_;
    size_t   off_ = 0;
};
