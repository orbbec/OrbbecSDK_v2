// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include <IFrame.hpp>

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
    double   coefficientA;  // Slope (system_us / device_clock_unit), typically ~1000.0
    uint64_t refDevTime;    // Predicted device timestamp (ms)
    uint64_t refSysTime;    // Predicted system timestamp (us)
    uint64_t devTime;       // Latest raw measured device timestamp (ms)
    uint64_t sysTime;       // Latest raw measured system timestamp (us)
} LinearFuncParam;

class IGlobalTimestampFitter {
public:
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

    virtual void enable(bool en)     = 0;
    virtual bool isEnabled() const   = 0;
    virtual bool isPtpActive() const = 0;
};

}  // namespace libobsensor
