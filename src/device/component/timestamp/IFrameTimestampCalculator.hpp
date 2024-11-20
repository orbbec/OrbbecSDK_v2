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

}  // namespace libobsensor
