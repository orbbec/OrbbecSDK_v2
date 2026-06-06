// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "frame/Frame.hpp"
#include "exception/ObException.hpp"
#include "utils/SteadyCondVar.hpp"

#include <algorithm>
#include <atomic>
#include <vector>
#include <thread>
#include <functional>
#include <mutex>
#include <memory>

namespace libobsensor {

template <size_t N> struct SpscFrameQueuePadding {
    char data_[N];
};
template <> struct SpscFrameQueuePadding<0> {};

// SPSC (Single Producer Single Consumer) bounded frame queue with FrameQueue-compatible API.
//
// Design notes:
//   - Logical capacity is the requested capacity; physical buffer size is rounded up to next power of 2.
//     Slot indexing uses bitwise AND (no division).
//   - One extra slot is reserved in the buffer so producer and consumer never touch the same
//     physical slot when the queue is logically full.
//   - An atomic pointer barrier (slotPtrs_) guarantees that producer and consumer never
//     concurrently access the same std::shared_ptr instance, enabling immediate release of
//     evicted frames in enforceEnqueue without any lock.
//   - enqueue / enforceEnqueue are fully lock-free.
//   - Consumer async thread and dequeue use a lock-free fast path; condition_variable is used
//     only for thread sleeping / waking.
template <typename T = Frame> class SpscFrameQueue {
    static size_t nextPow2(size_t n) {
        // Precondition: normal frame queue capacity is small (< 1M).
        // Overflow is not a concern for typical usage.
        size_t p = 1;
        while(p < n) {
            p <<= 1;
        }
        return p;
    }

    static constexpr size_t CACHE_LINE_SIZE    = 64;
    static constexpr size_t ATOMIC_SIZE        = sizeof(std::atomic<size_t>);
    static constexpr size_t PADDING_SIZE       = (ATOMIC_SIZE >= CACHE_LINE_SIZE) ? 0 : (CACHE_LINE_SIZE - ATOMIC_SIZE);
    static constexpr size_t STATE_SIZE         = sizeof(std::atomic<int>) + sizeof(std::atomic<bool>) * 3;
    static constexpr size_t STATE_PADDING_SIZE = (STATE_SIZE >= CACHE_LINE_SIZE) ? 0 : (CACHE_LINE_SIZE - STATE_SIZE);

    struct CacheLineAtomicSizeT {
        std::atomic<size_t>                 value;
        SpscFrameQueuePadding<PADDING_SIZE> pad;
    };

    struct CacheLineQueueState {
        std::atomic<int>                          waiterCount;
        std::atomic<bool>                         stopped;
        std::atomic<bool>                         stopping;
        std::atomic<bool>                         flushing;
        SpscFrameQueuePadding<STATE_PADDING_SIZE> pad;
    };

public:
    explicit SpscFrameQueue(size_t capacity)
        : capacity_((std::max)(capacity, size_t(2))),
          bufferSize_(nextPow2(capacity_ + 1)),
          mask_(bufferSize_ - 1),
          slots_(bufferSize_),
          slotPtrs_(new std::atomic<std::shared_ptr<T> *>[bufferSize_]),
          writeIdx_(),
          readIdx_(),
          state_{},
          callback_(nullptr) {
        state_.waiterCount.store(0, std::memory_order_relaxed);
        state_.stopped.store(true, std::memory_order_relaxed);
        state_.stopping.store(false, std::memory_order_relaxed);
        state_.flushing.store(false, std::memory_order_relaxed);
        for(size_t i = 0; i < bufferSize_; ++i) {
            slotPtrs_[i].store(nullptr, std::memory_order_relaxed);
        }
    }

    ~SpscFrameQueue() noexcept {
        reset();
    }

    size_t capacity() const {
        return capacity_;
    }

    size_t size() const {
        return writeIdx_.value.load(std::memory_order_acquire) - readIdx_.value.load(std::memory_order_acquire);
    }

    bool empty() const {
        return size() == 0;
    }

    bool full() const {
        const size_t w = writeIdx_.value.load(std::memory_order_acquire);
        const size_t r = readIdx_.value.load(std::memory_order_acquire);
        return (w - r) >= capacity_;
    }

    // Lock-free in all paths. Returns false when stopping, flushing, or queue full.
    bool enqueue(std::shared_ptr<T> frame) {
        if(state_.stopping.load(std::memory_order_acquire) || state_.flushing.load(std::memory_order_acquire)) {
            return false;
        }
        const size_t w = writeIdx_.value.load(std::memory_order_relaxed);
        const size_t r = readIdx_.value.load(std::memory_order_acquire);
        if(w - r >= capacity_) {
            return false;
        }
        slots_[w & mask_] = std::move(frame);
        slotPtrs_[w & mask_].store(&slots_[w & mask_], std::memory_order_release);
        writeIdx_.value.store(w + 1, std::memory_order_release);
        if(state_.waiterCount.load(std::memory_order_relaxed) > 0) {
            signal_.notify_one();
        }
        return true;
    }

    // Enqueue a frame, evicting the oldest frame if the queue is full.
    // Returns false only when stopping or flushing.
    // Sets dropped = true if an existing frame was evicted.
    // This path is fully lock-free; the evicted frame is released immediately.
    bool enforceEnqueue(std::shared_ptr<T> frame, bool &dropped) {
        dropped = false;
        if(state_.stopping.load(std::memory_order_acquire) || state_.flushing.load(std::memory_order_acquire)) {
            return false;
        }

        size_t       r = readIdx_.value.load(std::memory_order_acquire);
        const size_t w = writeIdx_.value.load(std::memory_order_relaxed);

        if(w - r >= capacity_) {
            // Try to evict the oldest frame immediately.
            auto *ptr = slotPtrs_[r & mask_].exchange(nullptr, std::memory_order_acq_rel);
            if(ptr != nullptr) {
                ptr->reset();
                dropped = true;
            }

            // Advance readIdx so the producer can proceed.
            if(!readIdx_.value.compare_exchange_strong(r, r + 1, std::memory_order_release, std::memory_order_acquire)) {
                // Consumer already advanced readIdx; reload for the notify check.
                r = readIdx_.value.load(std::memory_order_acquire);
            }
            else {
                ++r;
            }
        }

        slots_[w & mask_] = std::move(frame);
        slotPtrs_[w & mask_].store(&slots_[w & mask_], std::memory_order_release);
        writeIdx_.value.store(w + 1, std::memory_order_release);
        if(state_.waiterCount.load(std::memory_order_relaxed) > 0) {
            signal_.notify_one();
        }
        return true;
    }

    // Blocking dequeue for manual consumption.
    // Returns nullptr on timeout, when no frame is available, or when async dequeue is running.
    // Manual dequeue and async dequeue are mutually exclusive.
    std::shared_ptr<T> dequeue(uint64_t timeoutMsec = 0) {
        if(isStarted()) {
            return nullptr;
        }

        // Fast path: skip slots that were discarded by enforceEnqueue (nullptr barrier).
        auto frame = tryDequeueFrame();
        if(frame) {
            return frame;
        }

        if(timeoutMsec == 0) {
            return nullptr;
        }

        // Slow path: wait for producer notification
        std::unique_lock<std::mutex> lock(signalMutex_);
        state_.waiterCount.fetch_add(1, std::memory_order_relaxed);
        bool hasData =
            signal_.wait_for(lock, std::chrono::milliseconds(timeoutMsec), [this] { return !empty() || state_.stopping.load(std::memory_order_acquire); });
        state_.waiterCount.fetch_sub(1, std::memory_order_relaxed);

        if(!hasData) {
            return nullptr;
        }

        return tryDequeueFrame();
    }

    // Start async dequeue thread with callback.
    // Manual dequeue and async dequeue are mutually exclusive.
    void start(std::function<void(std::shared_ptr<T>)> callback) {
        if(isStarted()) {
            THROW_WRONG_API_CALL_SEQUENCE_EXCEPTION("SpscFrameQueue already started!");
        }
        callback_ = callback;
        state_.stopped.store(false, std::memory_order_release);
        state_.stopping.store(false, std::memory_order_release);
        state_.flushing.store(false, std::memory_order_release);
        dequeueThread_ = std::thread([this] {
            while(true) {
                std::shared_ptr<T> frame;

                frame = tryDequeueFrame();

                if(!frame) {
                    // Slow path: wait for signal
                    std::unique_lock<std::mutex> lock(signalMutex_);

                    // Re-check before sleeping
                    frame = tryDequeueFrame();

                    if(frame) {
                        // frame acquired under lock, no need to wait
                    }
                    else {
                        state_.waiterCount.fetch_add(1, std::memory_order_relaxed);
                        signal_.wait_for(lock, std::chrono::milliseconds(1000), [this] {
                            return !empty() || state_.stopping.load(std::memory_order_acquire) || state_.flushing.load(std::memory_order_acquire);
                        });
                        state_.waiterCount.fetch_sub(1, std::memory_order_relaxed);

                        if(state_.stopping.load(std::memory_order_acquire)) {
                            break;
                        }
                        if(state_.flushing.load(std::memory_order_acquire) && empty()) {
                            break;
                        }

                        frame = tryDequeueFrame();
                    }
                }

                if(frame) {
                    try {
                        callback_(frame);
                    }
                    catch(...) {
                    }
                }
            }
            state_.stopped.store(true, std::memory_order_release);
        });
    }

    bool isStarted() const {
        return !state_.stopped.load(std::memory_order_acquire);
    }

    // Wait until all queued frames have been delivered to the async callback, then stop async dequeue.
    void flush() {
        {
            std::unique_lock<std::mutex> lock(signalMutex_);
            state_.flushing.store(true, std::memory_order_release);
            signal_.notify_all();
        }
        std::thread t;
        {
            std::lock_guard<std::mutex> lock(threadMutex_);
            t = std::move(dequeueThread_);
        }
        if(t.joinable()) {
            t.join();
        }
        state_.stopped.store(true, std::memory_order_release);
        state_.flushing.store(false, std::memory_order_release);
    }

    // Stop async dequeue immediately and discard currently queued frames.
    // The queue remains reusable and can accept frames after stop() returns.
    void stop() {
        {
            std::unique_lock<std::mutex> lock(signalMutex_);
            state_.stopping.store(true, std::memory_order_release);
            signal_.notify_all();
        }
        std::thread t;
        {
            std::lock_guard<std::mutex> lock(threadMutex_);
            t = std::move(dequeueThread_);
        }
        if(t.joinable()) {
            t.join();
        }
        clearQueue();
        state_.stopped.store(true, std::memory_order_release);
        state_.stopping.store(false, std::memory_order_release);
    }

    // Stop and reset to initial state.
    void reset() {
        stop();
        callback_ = nullptr;
        state_.stopping.store(false, std::memory_order_release);
        state_.flushing.store(false, std::memory_order_release);
        state_.stopped.store(true, std::memory_order_release);
    }

private:
    std::shared_ptr<T> tryDequeueFrame() {
        while(true) {
            const size_t r = readIdx_.value.load(std::memory_order_relaxed);
            const size_t w = writeIdx_.value.load(std::memory_order_acquire);
            if(w == r) {
                return nullptr;
            }

            auto *ptr = slotPtrs_[r & mask_].exchange(nullptr, std::memory_order_acq_rel);
            if(ptr != nullptr) {
                auto frame = std::move(*ptr);
                ptr->reset();
                readIdx_.value.store(r + 1, std::memory_order_release);
                return frame;
            }

            readIdx_.value.store(r + 1, std::memory_order_release);
        }
    }

    void clearQueue() {
        const size_t w = writeIdx_.value.load(std::memory_order_acquire);
        size_t       r = readIdx_.value.load(std::memory_order_acquire);
        while(r != w) {
            auto *ptr = slotPtrs_[r & mask_].exchange(nullptr, std::memory_order_acquire);
            if(ptr != nullptr) {
                ptr->reset();
            }
            ++r;
        }
        readIdx_.value.store(w, std::memory_order_release);
    }

    const size_t                                         capacity_;    // logical capacity
    const size_t                                         bufferSize_;  // physical buffer size (next power of 2 of capacity+1)
    const size_t                                         mask_;
    std::vector<std::shared_ptr<T>>                      slots_;
    std::unique_ptr<std::atomic<std::shared_ptr<T> *>[]> slotPtrs_;

    CacheLineAtomicSizeT writeIdx_;
    CacheLineAtomicSizeT readIdx_;

    mutable std::mutex      signalMutex_;
    utils::SteadyCondVar    signal_;
    CacheLineQueueState     state_;

    std::mutex                              threadMutex_;
    std::thread                             dequeueThread_;
    std::function<void(std::shared_ptr<T>)> callback_;
};

}  // namespace libobsensor
