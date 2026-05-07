// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "SteadyCondVar.hpp"

#include <cassert>
#include <cerrno>

namespace libobsensor {
namespace utils {

#ifdef __linux__

namespace {

constexpr long kNanosecondsPerSecond = 1000000000L;

// RAII guard that releases unique_lock ownership before a pthread_cond wait
// and re-acquires it on destruction (the mutex itself stays locked/unlocked
// by pthread, matching std::condition_variable semantics).
class NativeMutexWaitGuard {
public:
    explicit NativeMutexWaitGuard(std::unique_lock<std::mutex> &lock)
        : lock_(lock), mutex_(*lock.mutex()), nativeHandle_(mutex_.native_handle()) {
        assert(lock_.owns_lock());
        lock_.release();
    }

    ~NativeMutexWaitGuard() {
        if(!lock_.owns_lock()) {
            lock_ = std::unique_lock<std::mutex>(mutex_, std::adopt_lock);
        }
    }

    NativeMutexWaitGuard(const NativeMutexWaitGuard &)            = delete;
    NativeMutexWaitGuard &operator=(const NativeMutexWaitGuard &) = delete;

    pthread_mutex_t *nativeHandle() const {
        return nativeHandle_;
    }

private:
    std::unique_lock<std::mutex> &lock_;
    std::mutex                   &mutex_;
    pthread_mutex_t              *nativeHandle_;
};

// Convert a steady_clock time_point directly to a CLOCK_MONOTONIC timespec.
// On Linux, steady_clock is CLOCK_MONOTONIC, so we can convert without
// calling clock_gettime a second time (avoiding a tiny race window).
timespec toMonotonicTimespec(const std::chrono::steady_clock::time_point &tp) {
    auto totalNs = std::chrono::duration_cast<std::chrono::nanoseconds>(tp.time_since_epoch()).count();
    if(totalNs < 0) {
        totalNs = 0;
    }

    timespec ts{};
    ts.tv_sec  = static_cast<time_t>(totalNs / kNanosecondsPerSecond);
    ts.tv_nsec = static_cast<long>(totalNs % kNanosecondsPerSecond);
    return ts;
}

}  // namespace

SteadyCondVar::SteadyCondVar() {
    pthread_condattr_t attr;
    int                rc = pthread_condattr_init(&attr);
    assert(rc == 0);
    rc = pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
    assert(rc == 0);
    rc = pthread_cond_init(&cond_, &attr);
    assert(rc == 0);
    rc = pthread_condattr_destroy(&attr);
    assert(rc == 0);
    (void)rc;
}

SteadyCondVar::~SteadyCondVar() {
    const int rc = pthread_cond_destroy(&cond_);
    assert(rc == 0);
    (void)rc;
}

void SteadyCondVar::notify_one() {
    const int rc = pthread_cond_signal(&cond_);
    assert(rc == 0);
    (void)rc;
}

void SteadyCondVar::notify_all() {
    const int rc = pthread_cond_broadcast(&cond_);
    assert(rc == 0);
    (void)rc;
}

void SteadyCondVar::wait(std::unique_lock<std::mutex> &lock) {
    NativeMutexWaitGuard guard(lock);

    const int rc = pthread_cond_wait(&cond_, guard.nativeHandle());
    assert(rc == 0);
    (void)rc;
}

std::cv_status SteadyCondVar::wait_until_steady(std::unique_lock<std::mutex> &lock,
                                                 const std::chrono::steady_clock::time_point &deadline) {
    if(deadline <= std::chrono::steady_clock::now()) {
        return std::cv_status::timeout;
    }

    const timespec       ts = toMonotonicTimespec(deadline);
    NativeMutexWaitGuard guard(lock);
    int                  rc = 0;

    do {
        rc = pthread_cond_timedwait(&cond_, guard.nativeHandle(), &ts);
    } while(rc == EINTR);

    return (rc == ETIMEDOUT) ? std::cv_status::timeout : std::cv_status::no_timeout;
}

#else  // Windows, macOS

SteadyCondVar::SteadyCondVar() = default;
SteadyCondVar::~SteadyCondVar() = default;

void SteadyCondVar::notify_one() {
    cv_.notify_one();
}

void SteadyCondVar::notify_all() {
    cv_.notify_all();
}

void SteadyCondVar::wait(std::unique_lock<std::mutex> &lock) {
    cv_.wait(lock);
}

std::cv_status SteadyCondVar::wait_until_steady(std::unique_lock<std::mutex> &lock,
                                                 const std::chrono::steady_clock::time_point &deadline) {
    return cv_.wait_until(lock, deadline);
}

#endif

}  // namespace utils
}  // namespace libobsensor
