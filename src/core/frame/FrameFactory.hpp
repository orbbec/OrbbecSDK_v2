// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "frame/Frame.hpp"

namespace libobsensor {
class StreamProfile;

class FrameFactory {
public:
    static std::shared_ptr<Frame> createFrame(OBFrameType frameType, OBFormat frameFormat, size_t datasize);
    static std::shared_ptr<Frame> createVideoFrame(OBFrameType frameType, OBFormat frameFormat, uint32_t width, uint32_t height, uint32_t strideBytes);
    static std::shared_ptr<Frame> createFrameFromOtherFrame(std::shared_ptr<const Frame> frame, bool shouldCopyData = false);

    static std::shared_ptr<Frame> createFrameFromUserBuffer(OBFrameType frameType, OBFormat format, uint8_t *buffer, size_t bufferSize,
                                                            FrameBufferReclaimFunc bufferReclaimFunc);
    static std::shared_ptr<Frame> createVideoFrameFromUserBuffer(OBFrameType frameType, OBFormat format, uint32_t width, uint32_t height, uint32_t strideBytes,
                                                                 uint8_t *buffer, size_t bufferSize, FrameBufferReclaimFunc bufferReclaimFunc);

    static std::shared_ptr<Frame> createFrameFromStreamProfile(std::shared_ptr<const StreamProfile> sp);

    static std::shared_ptr<FrameSet> createFrameSet();
};
}  // namespace libobsensor

