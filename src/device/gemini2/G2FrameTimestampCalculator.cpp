// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "G2FrameTimestampCalculator.hpp"
#include "usb/uvc/UvcTypes.hpp"
#include "frame/Frame.hpp"

namespace libobsensor {

G2VideoFrameTimestampCalculator::G2VideoFrameTimestampCalculator(IDevice *owner, uint64_t deviceTimeFreq, uint64_t frameTimeFreq)
    : FrameTimestampCalculatorBaseDeviceTime(owner, deviceTimeFreq, frameTimeFreq) {}

void G2VideoFrameTimestampCalculator::calculate(std::shared_ptr<Frame> frame) {
    auto srcTimestamp = frame->getTimeStampUsec();

    auto metadata   = frame->getMetadata();
    auto uvcPayload = reinterpret_cast<const StandardUvcFramePayloadHeader *>(metadata);
    // merge byte1 and byte2 as int16_t as the timestamp offset
    auto timestampOffsetUs = (static_cast<int16_t>(uvcPayload->scrSourceClock[2] << 8) | uvcPayload->scrSourceClock[1]) * 100;
    srcTimestamp += timestampOffsetUs;

    auto rstTimestamp = FrameTimestampCalculatorBaseDeviceTime::calculate(srcTimestamp);
    frame->setTimeStampUsec(rstTimestamp);
}

G2LVideoFrameTimestampCalculator::G2LVideoFrameTimestampCalculator(IDevice *owner, uint64_t deviceTimeFreq, uint64_t frameTimeFreq)
    : FrameTimestampCalculatorBaseDeviceTime(owner, deviceTimeFreq, frameTimeFreq) {}

void G2LVideoFrameTimestampCalculator::calculate(std::shared_ptr<Frame> frame) {
    auto metadata     = frame->getMetadata();
    auto uvcPayload   = reinterpret_cast<const StandardUvcFramePayloadHeader *>(metadata);
    auto srcTimestamp = *reinterpret_cast<const uint32_t *>(uvcPayload->scrSourceClock);
    auto rstTimestamp = FrameTimestampCalculatorBaseDeviceTime::calculate(srcTimestamp);
    frame->setTimeStampUsec(rstTimestamp);
}

}  // namespace libobsensor
