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
 * Formula: system_ts(us) = coefficientA * (device_ts - checkDataX) + refSysTime
 */
typedef struct {
    double   coefficientA; // Slope (system_us / device_clock_unit), typically ~1000.0
    double   refSysTime;   // Predicted system timestamp (us) at checkDataX
                           // EWLR-smoothed anchor to filter out single-sample RTT noise
    uint64_t checkDataX;   // Reference device timestamp (X anchor)
    uint64_t checkDataY;   // Latest raw measured system timestamp (us)
                           // For diagnostics only; not used in conversion
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

    virtual void enable(bool en)   = 0;
    virtual bool isEnabled() const = 0;
};

}  // namespace libobsensor
