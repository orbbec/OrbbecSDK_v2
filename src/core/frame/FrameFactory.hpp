#pragma once

#include "frame/Frame.hpp"

namespace libobsensor {
namespace FrameFactory {

std::shared_ptr<Frame> createFrame(OBFrameType frameType, OBFormat frameFormat, size_t datasize);
std::shared_ptr<Frame> createVideoFrame(OBFrameType frameType, OBFormat frameFormat, uint32_t width, uint32_t height, uint32_t strideBytes);

std::shared_ptr<Frame> createFrameFromUserBuffer(OBFrameType frameType, OBFormat format, uint8_t *buffer, size_t bufferSize,
                                                 FrameBufferReclaimFunc bufferReclaimFunc);
std::shared_ptr<Frame> createVideoFrameFromUserBuffer(OBFrameType frameType, OBFormat format, uint32_t width, uint32_t height, uint32_t strideBytes,
                                                      uint8_t *buffer, size_t bufferSize, FrameBufferReclaimFunc bufferReclaimFunc);

std::shared_ptr<Frame> createFrameSet();

}  // namespace FrameFactory
}  // namespace libobsensor