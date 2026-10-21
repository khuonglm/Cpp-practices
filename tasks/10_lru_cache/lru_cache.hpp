#pragma once

#include <array>
#include <cstddef>
#include <functional>
#include <list>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <utility>

template <typename K, typename V>
class LruCache {
public:
    explicit LruCache(size_t capacity) : cap_(capacity) {}
    LruCache(const LruCache&) = delete;
    LruCache& operator=(const LruCache&) = delete;

    std::optional<V> get(const K& key) {
        (void)key;
        throw std::logic_error("TODO: LruCache::get");
    }

    void put(const K& key, V value) {
        (void)key;
        (void)value;
        throw std::logic_error("TODO: LruCache::put");
    }

    size_t size() const {
        std::lock_guard<std::mutex> lk(m_);
        return size_unlocked();
    }

private:
    size_t size_unlocked() const noexcept { return 0; }

    size_t             cap_;
    mutable std::mutex m_;
};

template <typename K, typename V, size_t NumShards = 16>
class ShardedLruCache {
    static_assert(NumShards >= 1 && (NumShards & (NumShards - 1)) == 0,
                  "NumShards must be a power of two");

public:
    explicit ShardedLruCache(size_t total_capacity) {
        (void)total_capacity;
        throw std::logic_error("TODO: ShardedLruCache constructor");
    }

    std::optional<V> get(const K& key) {
        (void)key;
        throw std::logic_error("TODO: ShardedLruCache::get");
    }

    void put(const K& key, V value) {
        (void)key;
        (void)value;
        throw std::logic_error("TODO: ShardedLruCache::put");
    }
};
