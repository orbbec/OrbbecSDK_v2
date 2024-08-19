#pragma once

#include "frame/Frame.hpp"

#include <queue>

namespace libobsensor {

template <typename T = Frame> class FrameQueue {
public:
    explicit FrameQueue(size_t capacity) : capacity_(capacity), stoped_(true), stopping_(false), callback_(nullptr), flushing_(false) {}

    ~FrameQueue() noexcept {
        reset();
    }

    size_t capacity() const {
        return capacity_;
    }

    void resize(size_t capacity) {
        capacity_ = capacity;
    }

    size_t size() const {
        return queue_.size();
    }

    bool empty() const {
        return queue_.empty();
    }

    bool fulled() const {
        return queue_.size() >= capacity_;
    }

    bool enqueue(std::shared_ptr<T> frame) {  // returns false if queue is full
        std::unique_lock<std::mutex> lock(mutex_);
        if(queue_.size() >= capacity_ || flushing_) {
            return false;
        }
        queue_.push(frame);
        condition_.notify_one();
        return true;
    }

    // blocking methods
    std::shared_ptr<T> dequeue(uint64_t timeoutMsec = 0) {  // returns nullptr if timeout is reached
        std::unique_lock<std::mutex> lock(mutex_);
        if(!queue_.empty()) {
            auto result = queue_.front();
            queue_.pop();
            return result;
        }

        if(timeoutMsec == 0) {
            return nullptr;
        }

        condition_.wait_for(lock, std::chrono::milliseconds(timeoutMsec), [this] { return !queue_.empty(); });
        if(queue_.empty()) {
            return nullptr;
        }
        auto result = queue_.front();
        queue_.pop();
        return result;
    }

    // async methods
    void start(std::function<void(std::shared_ptr<T>)> callback) {  // start async dequeue
        if(isStarted()) {
            throw libobsensor::wrong_api_call_sequence_exception("FrameQueue have already started!");
        }
        callback_      = callback;
        stoped_        = false;
        stopping_      = false;
        flushing_      = false;
        dequeueThread_ = std::thread([&] {
            std::unique_lock<std::mutex> lock(mutex_);
            while(true) {
                condition_.wait(lock, [this] { return !queue_.empty() || stopping_ || flushing_; });
                if(stopping_) {
                    break;
                }
                if(flushing_ && queue_.empty()) {
                    break;
                }
                std::shared_ptr<T> frame = queue_.front();
                queue_.pop();
                if(frame) {
                    callback_(frame);
                }
            }
            stoped_ = true;
        });
    }

    bool isStarted() const {  // returns true if dequeue thread is running
        return !stoped_;
    }

    void flush() {  // stop until all frames are called back
        flushing_ = true;
        condition_.notify_one();
        if(dequeueThread_.joinable()) {
            dequeueThread_.join();
        }
    }

    void stop() {  // stop immediately
        stopping_ = true;
        condition_.notify_one();
        if(dequeueThread_.joinable()) {
            dequeueThread_.join();
        }

        std::unique_lock<std::mutex> lock(mutex_);
        while(!queue_.empty()) {
            queue_.pop();
        }
    }

    // clear all frames in queue, flags, and callback. Stop dequeue thread. reset to initial state.
    void reset() {
        stop();  // try stop if it's running, clear all frames on queue
        callback_ = nullptr;
        stopping_ = false;
        flushing_ = false;
        stoped_   = true;
    }

private:
    std::mutex                     mutex_;
    std::condition_variable        condition_;
    std::queue<std::shared_ptr<T>> queue_;
    size_t                         capacity_;

    std::thread                             dequeueThread_;
    std::atomic<bool>                       stoped_;
    std::atomic<bool>                       stopping_;
    std::function<void(std::shared_ptr<T>)> callback_;
    std::atomic<bool>                       flushing_;
};

}  // namespace libobsensor