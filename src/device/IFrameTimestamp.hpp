// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include <IFrame.hpp>

#include <cstdint>
#include <memory>

namespace libobsensor {
class IFrameTimestampCalculator {
public:
    virtual ~IFrameTimestampCalculator() = default;

    virtual void calculate(std::shared_ptr<Frame> frame) = 0;
    virtual void clear()                                 = 0;
};

/**
 * @brief Point-slope parameters for global timestamp linear regression
 * Formula: system_ts(us) = coefficientA * (device_ts - refDevTime) + refSysTime
 */
typedef struct {
    double   coefficientA;  // Host microseconds per device clock tick
    uint64_t refDevTime;    // Reference device timestamp in device clock ticks
    uint64_t refSysTime;    // Reference host timestamp in microseconds
    uint64_t devTime;       // Latest device timestamp in device clock ticks
    uint64_t sysTime;       // Latest host timestamp in microseconds
} LinearFuncParam;

typedef struct {
    uint64_t timestampUs;
    uint64_t generation;
    double   remainingCorrectionUs;
    double   estimatedErrorUs;
    bool     valid;
    bool     stable;
} GlobalTimestampMapResult;

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

    virtual GlobalTimestampMapResult mapDeviceTime(double deviceTimestampTicks) = 0;
};

}  // namespace libobsensor
