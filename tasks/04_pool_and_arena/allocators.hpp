#pragma once

#include <cstddef>
#include <cstdint>
#include <new>
#include <stdexcept>
#include <utility>
#include <vector>
#include <memory>

template <typename T, std::size_t N>
class ObjectPool {
    static_assert(N > 0, "Pool capacity N must be > 0");

public:
    ObjectPool(): slots_{std::vector<Slot>(N)}, free_{nullptr} {
        free_ = &slots_[0];
        for(std::size_t i = 0; i < N - 1; ++i) {
            slots_[i].nextFree = &slots_[i + 1];
        }
        slots_[N - 1].nextFree = nullptr;
    }
    ObjectPool(const ObjectPool<T,N>&) = delete;
    ObjectPool& operator=(const ObjectPool<T,N>&) = delete;
    ObjectPool(ObjectPool<T,N>&&) = delete;
    ObjectPool& operator=(ObjectPool<T,N>&&) = delete;
    ~ObjectPool() {
        // caller must deallocate/destroy everything before
    }

    T* allocate() noexcept {
        if(free_ == nullptr)
            return nullptr;
        Slot* slot = free_;
        free_ = free_->nextFree;
        return reinterpret_cast<T*>(&slot->storage);
    }

    void deallocate(T* p) noexcept {
        if(p == nullptr)
            return;
        Slot* slot = reinterpret_cast<Slot*>(p);
        slot->nextFree = free_;
        free_ = slot;
    }

    template <typename... Args>
    T* construct(Args&&... args) {
        T* object = allocate();
        if(object == nullptr)
            return nullptr;
        
        try{
            return std::construct_at(object, std::forward<Args>(args)...);
        } catch (...) {
            deallocate(object);
            throw;
        }
    }

    void destroy(T* p) noexcept {
        if (p) {
            p->~T();
            deallocate(p);
        }
    }
private:
    union Slot {
        // ensure that the address is suitable for accessing a T
        alignas(T) unsigned char storage[sizeof(T)];
        Slot* nextFree;

        Slot() noexcept : nextFree(nullptr) {}
        ~Slot() noexcept {}
    };
    std::vector<Slot> slots_;
    Slot* free_;
};

class Arena {
public:
    Arena(void* buf, size_t cap) noexcept : base_(static_cast<uint8_t*>(buf)), cap_(cap) {}
    Arena(const Arena&) = delete;
    Arena& operator=(const Arena&) = delete;

    void* alloc(size_t size, size_t align = alignof(std::max_align_t)) {
        if(align == 0 || (align & (align - 1)))
            return nullptr;
        
        std::uintptr_t cur = reinterpret_cast<uintptr_t>(base_) + off_;
        std::uintptr_t curPadding = (cur + align - 1) & (~(align - 1));
        std::uintptr_t padding = curPadding - cur;
        if(cap_ - off_ < padding || cap_ - off_ - padding < size)
            return nullptr;
        off_ += padding + size;
        return reinterpret_cast<void*>(curPadding);
    }

    void reset() noexcept { off_ = 0; }
    std::size_t used() const noexcept { return off_; }
    std::size_t capacity() const noexcept { return cap_; }

private:
    uint8_t* base_;
    std::size_t cap_;
    std::size_t off_ = 0;
};
