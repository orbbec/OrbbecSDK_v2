// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include <IFrame.hpp>

#include <functional>

namespace libobsensor {
class IFrameTimestampCalculator {
public:
    virtual ~IFrameTimestampCalculator() = default;

    virtual void calculate(std::shared_ptr<Frame> frame) = 0;
    virtual void clear()                                 = 0;
};

// First degree function coefficient y=ax+b
typedef struct {
    double   coefficientA;
    double   constantB;
    uint64_t checkDataX;
    uint64_t checkDataY;
} LinearFuncParam;

// Returns the current host time in microseconds, in whatever clock domain the
// caller wants the fitter to map device timestamps into. Default is the SDK's
// std::chrono::system_clock-based getNowTimesUs(); callers may inject a
// monotonic source (e.g. steady_clock or a host application's clock).
using HostClockFn = std::function<uint64_t()>;

class IGlobalTimestampFitter {
public:
    virtual ~IGlobalTimestampFitter() = default;

    virtual LinearFuncParam getLinearFuncParam() = 0;

    /**
     * @brief re-fitting timestamp linear param
     *
     * @param[in] async true/false
     */
    virtual void reFitting(bool async) = 0;
    virtual void pause()               = 0;
    virtual void resume()              = 0;

    virtual void enable(bool en)   = 0;
    virtual bool isEnabled() const = 0;

    // Replace the host clock source used to pair host time with device time.
    // Safe to call while the fitter is running; the implementation must clear
    // any in-flight samples so the next iteration starts in the new domain.
    virtual void setHostClockFn(HostClockFn fn) = 0;

    // Upper bound on the host-side RTT of a device-time query that the fitter
    // is willing to accept as a sample. Higher RTTs are discarded. In
    // microseconds. Default is the implementation's compile-time constant.
    virtual void setMaxValidRtt(uint64_t maxValidUs) = 0;
};

}  // namespace libobsensor
