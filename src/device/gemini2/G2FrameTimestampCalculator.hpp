// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "timestamp/FrameTimestampCalculator.hpp"

namespace libobsensor {
class G2VideoFrameTimestampCalculator : public FrameTimestampCalculatorBaseDeviceTime {
public:
    G2VideoFrameTimestampCalculator(IDevice *owner, uint64_t deviceTimeFreq, uint64_t frameTimeFreq);
    ~G2VideoFrameTimestampCalculator() = default;

    void calculate(std::shared_ptr<Frame> frame) override;
};

class G2LVideoFrameTimestampCalculator : public FrameTimestampCalculatorBaseDeviceTime {
public:
    G2LVideoFrameTimestampCalculator(IDevice *owner, uint64_t deviceTimeFreq, uint64_t frameTimeFreq);
    ~G2LVideoFrameTimestampCalculator() = default;

    void calculate(std::shared_ptr<Frame> frame) override;
};
}  // namespace libobsensor
