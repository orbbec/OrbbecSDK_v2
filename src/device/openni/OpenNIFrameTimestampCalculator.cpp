// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "OpenNIFrameTimestampCalculator.hpp"
#include "logger/LoggerInterval.hpp"
#include "InternalTypes.hpp"
#include "frame/Frame.hpp"

namespace libobsensor {
OpenNIFrameTimestampCalculator::OpenNIFrameTimestampCalculator(IDevice *device, uint64_t deviceTimeFreq, uint64_t frameTimeFreq)
    : device_(device), deviceTimeFreq_(deviceTimeFreq), frameTimeFreq_(frameTimeFreq) {
}

void OpenNIFrameTimestampCalculator::calculate(std::shared_ptr<Frame> frame) {
    auto realtime = utils::getNowTimesUs();
    frame->setTimeStampUsec(realtime);
    frame->setSystemTimeStampUsec(realtime);
}

void OpenNIFrameTimestampCalculator::clear() {
}

}  // namespace libobsensor

