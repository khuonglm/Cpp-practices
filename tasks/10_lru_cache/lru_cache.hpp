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
#include <memory>

template<typename K>
struct Hash {
    std::size_t operator()(const K& k) const noexcept {
        return std::hash<K>{}(k);
    }
};

template <typename K, typename V>
class LruCache {
public:
    explicit LruCache(size_t capacity) : cap_(capacity) {}
    LruCache(const LruCache&) = delete;
    LruCache& operator=(const LruCache&) = delete;

    std::optional<V> get(const K& key) {
        std::lock_guard<std::mutex> lk(m_);
        auto it = data_.find(key);
        if(it == data_.end())
            return std::nullopt;
        touch_(key);
        return it->second;
    }

    void put(const K& key, V value) {
        std::lock_guard<std::mutex> lk(m_);
        auto [it, inserted] = data_.try_emplace(key, value);
        if(!inserted) it->second = value;
        touch_(key);
        if(data_.size() > cap_) {
            auto evictIt = mru_.back();
            auto evictKey = evictIt->first;
            index_.erase(evictKey);
            mru_.pop_back();
            data_.erase(evictIt);
        }
    }

    size_t size() const {
        std::lock_guard<std::mutex> lk(m_);
        return size_unlocked();
    }

private:
    std::size_t size_unlocked() const noexcept { return data_.size(); }
    void touch_(const K& key) {
        auto indexIt = index_.find(key);
        if(indexIt == index_.end()) { // first time inserted
            mru_.push_front(data_.find(key));
            index_[key] = mru_.begin();
            return;
        }
        
        auto index = indexIt->second;
        mru_.splice(mru_.begin(), mru_, index);
    }

    std::size_t cap_;
    // cannot be shared_mutex to allow multiple readers as
    // we modify the FIFO list
    mutable std::mutex m_;
    using Cache = std::unordered_map<K, V, Hash<K>>;
    using CacheIter = typename Cache::iterator;
    Cache data_;
    std::list<CacheIter> mru_;
    std::unordered_map<K, typename std::list<CacheIter>::iterator> index_;
};

template <typename K, typename V, size_t NumShards = 16>
class ShardedLruCache {
    static_assert(NumShards >= 1 && (NumShards & (NumShards - 1)) == 0,
                  "NumShards must be a power of two");

public:
    explicit ShardedLruCache(size_t total_capacity) {
        std::size_t capacity = (total_capacity + NumShards - 1) / NumShards;
        caches_.reserve(NumShards);
        for(std::size_t i = 0; i < NumShards; ++i) {
            caches_.push_back(std::make_unique<LruCache<K,V>>(capacity));
        }
    }

    std::optional<V> get(const K& key) {
        auto shardIdx = Hash<K>{}(key) % NumShards;
        return caches_[shardIdx]->get(key);
    }

    void put(const K& key, V value) {
        auto shardIdx = Hash<K>{}(key) % NumShards;
        return caches_[shardIdx]->put(key, value);
    }

private:
    std::vector<std::unique_ptr<LruCache<K, V>>> caches_;
};
