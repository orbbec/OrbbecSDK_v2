// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "OpenNIFrameTimestampCalculator.hpp"
#include "logger/LoggerInterval.hpp"
#include "InternalTypes.hpp"
#include "frame/Frame.hpp"

namespace libobsensor {
OpenNIFrameTimestampCalculator::OpenNIFrameTimestampCalculator(IDevice *device, uint64_t deviceTimeFreq, uint64_t frameTimeFreq)
    : device_(device), deviceTimeFreq_(deviceTimeFreq), frameTimeFreq_(frameTimeFreq) {}

void OpenNIFrameTimestampCalculator::calculate(std::shared_ptr<Frame> frame) {
    const auto timestamp = utils::getHostTimestampUs();
    frame->setTimeStampUsec(timestamp.systemTimeUs);
    frame->setSystemTimeStampUsec(timestamp.systemTimeUs);
    frame->setSteadyTimeStampUsec(timestamp.steadyTimeUs);
    frame->setDeviceTimestampFromHost(true);
}

void OpenNIFrameTimestampCalculator::clear() {}

}  // namespace libobsensor
