// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "timestamp/FrameTimestampCalculator.hpp"

namespace libobsensor {
class Astra2VideoFrameTimestampCalculator : public FrameTimestampCalculatorBaseDeviceTime {
public:
    Astra2VideoFrameTimestampCalculator(IDevice *owner, uint64_t deviceTimeFreq, uint64_t frameTimeFreq);
    ~Astra2VideoFrameTimestampCalculator() = default;

    void calculate(std::shared_ptr<Frame> frame) override;
};

class Astra2LVideoFrameTimestampCalculator : public FrameTimestampCalculatorBaseDeviceTime {
public:
    Astra2LVideoFrameTimestampCalculator(IDevice *owner, uint64_t deviceTimeFreq, uint64_t frameTimeFreq);
    ~Astra2LVideoFrameTimestampCalculator() = default;

    void calculate(std::shared_ptr<Frame> frame) override;
};
}  // namespace libobsensor
