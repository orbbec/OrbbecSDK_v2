// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include <chrono>
#include <condition_variable>
#include <mutex>

#ifdef __linux__
#include <pthread.h>
#endif

namespace libobsensor {
namespace utils {

/// A condition variable that is immune to system clock adjustments.
///
/// On Linux (glibc < 2.30), std::condition_variable::wait_for and wait_until
/// internally use CLOCK_REALTIME, so adjusting the system clock can cause
/// unexpected early returns or indefinite blocking.
///
/// This class uses pthread_cond_t with CLOCK_MONOTONIC on Linux, guaranteeing
/// correct timeout behavior regardless of system time changes.
/// On Windows and macOS, std::condition_variable is already reliable, so we
/// wrap it directly.
class SteadyCondVar {
public:
    SteadyCondVar();
    ~SteadyCondVar();

    // Non-copyable, non-movable
    SteadyCondVar(const SteadyCondVar &)            = delete;
    SteadyCondVar &operator=(const SteadyCondVar &) = delete;
    SteadyCondVar(SteadyCondVar &&)                 = delete;
    SteadyCondVar &operator=(SteadyCondVar &&)      = delete;

    /// Wake one waiting thread.
    void notify_one();

    /// Wake all waiting threads.
    void notify_all();

    /// Wait indefinitely until notified.
    void wait(std::unique_lock<std::mutex> &lock);

    /// Wait indefinitely until predicate returns true.
    template<typename Predicate>
    void wait(std::unique_lock<std::mutex> &lock, Predicate pred) {
        while(!pred()) {
            wait(lock);
        }
    }

    /// Wait for a duration. Returns std::cv_status::timeout on timeout,
    /// std::cv_status::no_timeout if notified before timeout.
    template<typename Rep, typename Period>
    std::cv_status wait_for(std::unique_lock<std::mutex> &lock, const std::chrono::duration<Rep, Period> &timeout) {
        auto deadline = std::chrono::steady_clock::now() + timeout;
        return wait_until_steady(lock, deadline);
    }

    /// Wait for a duration with predicate. Returns false if predicate is still
    /// false after timeout, true otherwise.
    template<typename Rep, typename Period, typename Predicate>
    bool wait_for(std::unique_lock<std::mutex> &lock, const std::chrono::duration<Rep, Period> &timeout, Predicate pred) {
        auto deadline = std::chrono::steady_clock::now() + timeout;
        while(!pred()) {
            if(wait_until_steady(lock, deadline) == std::cv_status::timeout) {
                return pred();
            }
        }
        return true;
    }

    /// Wait until a steady_clock time point.
    std::cv_status wait_until(std::unique_lock<std::mutex> &lock, const std::chrono::steady_clock::time_point &deadline) {
        return wait_until_steady(lock, deadline);
    }

    /// Wait until a steady_clock time point with predicate.
    template<typename Predicate>
    bool wait_until(std::unique_lock<std::mutex> &lock, const std::chrono::steady_clock::time_point &deadline, Predicate pred) {
        while(!pred()) {
            if(wait_until_steady(lock, deadline) == std::cv_status::timeout) {
                return pred();
            }
        }
        return true;
    }

private:
    std::cv_status wait_until_steady(std::unique_lock<std::mutex> &lock,
                                     const std::chrono::steady_clock::time_point &deadline);

#ifdef __linux__
    pthread_cond_t cond_;
#else
    std::condition_variable cv_;
#endif
};

}  // namespace utils
}  // namespace libobsensor
