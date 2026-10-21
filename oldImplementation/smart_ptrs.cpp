#include <atomic>
#include <utility>

class ControlBlock {
public:
    ControlBlock(): count_{1} {}
    void increase() {
        count_.fetch_add(1, std::memory_order_relaxed);
    }
    bool decrease() {
        return count_.fetch_sub(1, std::memory_order_acq_rel) == 1;
    }
    int get() {
        return count_.load(std::memory_order_relaxed);
    }
private:
    std::atomic<int> count_;
};

template<typename T>
class SharedPtr {
public:
    explicit SharedPtr(T* p): p_{p} {
        if(p_) {
            try {
                ctr_block_ = new ControlBlock();
            } catch (...) {
                delete p_;
                throw;
            }
        }
    }
    SharedPtr(const SharedPtr& other): p_{other.p_}, ctr_block_{other.ctr_block_} {
        if(ctr_block_) ctr_block_->increase();
    }
    SharedPtr(SharedPtr&& other) noexcept
    : p_{std::exchange(other.p_, nullptr)},
      ctr_block_{std::exchange(other.ctr_block_, nullptr)} {}
    SharedPtr& operator=(const SharedPtr& other) {
        if(this != &other) {
            destroy_();
            SharedPtr tmp(other);
            swap(tmp);
        }
        return *this;
    }
    SharedPtr& operator=(SharedPtr&& other) noexcept {
        if(this != &other) {
            destroy_();
            SharedPtr tmp(std::move(other));
            swap(tmp);
        }
        return *this;
    }
    ~SharedPtr() {
        destroy_();
    }

    void swap(SharedPtr& other) noexcept {
        std::swap(p_, other.p_);
        std::swap(ctr_block_, other.ctr_block_);
    }
    void reset(T* p = nullptr) {
        destroy_();
        if(p) {
            p_ = p;
            ctr_block_ = new ControlBlock();
        }
    }
    T* get() const noexcept { return p_; }
    T& operator*() const noexcept { return *p_; }
    T* operator->() const noexcept { return p_; }
    int use_count() const noexcept { return ctr_block_ ? ctr_block_->get() : 0; }
    explicit operator bool() const noexcept { return p_ != nullptr; }    
private:
    void destroy_() {
        if(ctr_block_ && ctr_block_->decrease()) {
            delete p_;
            delete ctr_block_;
        }
        p_ = nullptr;
        ctr_block_ = nullptr;
    }

    T* p_;
    ControlBlock* ctr_block_;
};

template <typename T>
class UniquePtr {
public:
    UniquePtr() = default;
    explicit UniquePtr(T* p): p_(p) {}
    UniquePtr(const UniquePtr& other) = delete;
    UniquePtr& operator=(const UniquePtr& other) = delete;
    UniquePtr(UniquePtr&& other) noexcept : p_{other.release()} {}
    UniquePtr& operator=(UniquePtr&& other) noexcept {
        if(this != &other) {
            reset(other.release());
        }
        return *this;
    }
    ~UniquePtr() {
        destroy_();
    }

    T* release() { return std::exchange(p_, nullptr); }
    void reset(T* p = nullptr) { destroy_(); p_ = p; }
    T* get() const noexcept { return p_; }
    T& operator*() const noexcept { return *p_; }
    T* operator->() const noexcept { return p_; }
    explicit operator bool() const noexcept { return p_ != nullptr; }
private:
    void destroy_() { delete p_; } // invoke destructor of T
    T* p_ = nullptr;
};

