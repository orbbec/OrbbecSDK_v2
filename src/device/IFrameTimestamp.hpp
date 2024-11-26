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

// First degree function coefficient y=ax+b
typedef struct {
    double   coefficientA;
    double   constantB;
    uint64_t checkDataX;
    uint64_t checkDataY;
} LinearFuncParam;

class IGlobalTimestampFitter {
public:
public:
    virtual ~IGlobalTimestampFitter() = default;

    virtual LinearFuncParam getLinearFuncParam() = 0;
    virtual void            reFitting()          = 0;
    virtual void            pause()              = 0;
    virtual void            resume()             = 0;

    virtual void enable(bool en)   = 0;
    virtual bool isEnabled() const = 0;
};

}  // namespace libobsensor
