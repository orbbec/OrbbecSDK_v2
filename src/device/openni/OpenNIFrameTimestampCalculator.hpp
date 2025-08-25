// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "IFrame.hpp"
#include "IDevice.hpp"
#include "IFrameTimestamp.hpp"

namespace libobsensor {

class OpenNIFrameTimestampCalculator : public IFrameTimestampCalculator {
public:
    OpenNIFrameTimestampCalculator(IDevice *device, uint64_t deviceTimeFreq, uint64_t frameTimeFreq);

    virtual ~OpenNIFrameTimestampCalculator() override = default;

    void calculate(std::shared_ptr<Frame> frame) override;
    void clear() override;

private:
    IDevice *device_;
    
    uint64_t deviceTimeFreq_;
    uint64_t frameTimeFreq_;
};

}  // namespace libobsensor
