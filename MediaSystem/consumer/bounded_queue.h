#pragma once
#include <deque>
#include <mutex>
#include <condition_variable>
#include <optional>

template<typename T>
class BoundedQueue {
public:
    explicit BoundedQueue(size_t capacity): capacity_(capacity) {}

    // Try to push; returns true if enqueued, false if dropped (full).
    bool try_push(T&& item) {
        std::lock_guard<std::mutex> lk(mtx_);
        if (q_.size() >= capacity_) return false;
        q_.emplace_back(std::move(item));
        cv_.notify_one();
        return true;
    }

    // Blocks until item available.
    T pop_blocking() {
        std::unique_lock<std::mutex> lk(mtx_);
        cv_.wait(lk, [&]{ return !q_.empty(); });
        T it = std::move(q_.front());
        q_.pop_front();
        return it;
    }

    size_t size() {
        std::lock_guard<std::mutex> lk(mtx_);
        return q_.size();
    }

private:
    std::deque<T> q_;
    size_t capacity_;
    std::mutex mtx_;
    std::condition_variable cv_;
};
