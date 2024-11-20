// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "IDevice.hpp"
#include "timestamp/IFrameTimestampCalculator.hpp"

namespace libobsensor {

class G330FrameTimestampCalculatorBaseDeviceTime : public IFrameTimestampCalculator {
public:
    G330FrameTimestampCalculatorBaseDeviceTime(IDevice *device, uint64_t deviceTimeFreq, uint64_t frameTimeFreq);

    virtual ~G330FrameTimestampCalculatorBaseDeviceTime() = default;

    void calculate(std::shared_ptr<Frame> frame) override;
    void clear() override;

    uint64_t calculate(uint64_t srcTimestamp);

private:
    IDevice *device_;

    uint64_t deviceTimeFreq_;
    uint64_t frameTimeFreq_;

    uint64_t prevSrcTsp_;
    uint64_t prevHostTsp_;
    uint64_t baseDevTime_;
};

}  // namespace libobsensor
