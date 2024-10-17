#pragma once

#include "timestamp/FrameTimestampCalculator.hpp"

namespace libobsensor {
class FemtoMegaColorFrameTimestampCalculatorV10300 : public FrameTimestampCalculatorBaseDeviceTime {
public:
    FemtoMegaColorFrameTimestampCalculatorV10300(IDevice *device, uint64_t deviceTimeFreq, uint64_t frameTimeFreq);

    virtual ~FemtoMegaColorFrameTimestampCalculatorV10300() = default;

    void calculate(std::shared_ptr<Frame> frame) override;
};
}  // namespace libobsensor