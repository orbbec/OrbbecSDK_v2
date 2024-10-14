// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "FrameFactory.hpp"
#include "stream/StreamProfile.hpp"
#include "stream/StreamProfileFactory.hpp"
#include "FrameMemoryPool.hpp"
#include "exception/ObException.hpp"
#include "utils/PublicTypeHelper.hpp"

namespace libobsensor {

std::shared_ptr<Frame> FrameFactory::createFrame(OBFrameType frameType, OBFormat frameFormat, size_t datasize) {

    auto                                 memoryPool    = FrameMemoryPool::getInstance();
    std::shared_ptr<IFrameBufferManager> bufferManager = memoryPool->createFrameBufferManager(frameType, datasize);

    auto frame = bufferManager->acquireFrame();
    if(frame == nullptr) {
        throw libobsensor::memory_exception("Failed to create frame, out of memory or other memory allocation error.");
    }

    auto streamType = utils::mapFrameTypeToStreamType(frameType);
    auto sp         = StreamProfileFactory::createStreamProfile(streamType, frameFormat);
    frame->setStreamProfile(sp);
    return frame;
}

std::shared_ptr<Frame> FrameFactory::createFrameFromOtherFrame(std::shared_ptr<const Frame> frame, bool shouldCopyData) {
    if(frame->is<FrameSet>()) {
        auto newFrameSet = createFrameSet();
        auto frameSet    = frame->as<FrameSet>();
        auto frameCount  = frameSet->getCount();
        for(uint32_t i = 0; i < frameCount; i++) {
            std::shared_ptr<const Frame> oldFrame = frameSet->getFrame(i);
            if(shouldCopyData) {
                auto newFrame = createFrameFromStreamProfile(oldFrame->getStreamProfile());
                newFrame->updateData(oldFrame->getData(), oldFrame->getDataSize());
                newFrame->copyInfoFromOther(oldFrame);
                newFrameSet->pushFrame(std::move(newFrame));
            }
            else {
                newFrameSet->pushFrame(std::move(oldFrame));
            }
        }
        return newFrameSet;
    }
    else {
        auto newFrame = createFrameFromStreamProfile(frame->getStreamProfile());
        if(shouldCopyData) {
            newFrame->updateData(frame->getData(), frame->getDataSize());
        }
        newFrame->copyInfoFromOther(frame);
        return newFrame;
    }
}

std::shared_ptr<Frame> FrameFactory::createVideoFrame(OBFrameType frameType, OBFormat frameFormat, uint32_t width, uint32_t height, uint32_t strideBytes) {
    if(frameType == OB_FRAME_UNKNOWN || frameType == OB_FRAME_ACCEL || frameType == OB_FRAME_GYRO || frameType == OB_FRAME_SET) {
        throw libobsensor::invalid_value_exception("Invalid frame type for video frame.");
    }

    std::shared_ptr<IFrameBufferManager> bufferManager;
    auto                                 memoryPool = FrameMemoryPool::getInstance();
    size_t                               frameDataSize;
    if(strideBytes == 0) {
        strideBytes = utils::calcDefaultStrideBytes(frameFormat, width);
    }
    frameDataSize = height * strideBytes;
    bufferManager = memoryPool->createFrameBufferManager(frameType, frameDataSize);

    auto frame = bufferManager->acquireFrame();
    if(frame == nullptr) {
        throw libobsensor::memory_exception("Failed to create frame, out of memory or other memory allocation error.");
    }

    auto streamType = utils::mapFrameTypeToStreamType(frameType);
    auto sp         = StreamProfileFactory::createVideoStreamProfile(streamType, frameFormat, width, height, 0);
    frame->setStreamProfile(sp);
    frame->as<VideoFrame>()->setStride(strideBytes);
    return frame;
}

std::shared_ptr<Frame> FrameFactory::createFrameFromUserBuffer(OBFrameType frameType, OBFormat format, uint8_t *buffer, size_t bufferSize,
                                                               FrameBufferReclaimFunc bufferReclaimFunc) {
    std::shared_ptr<Frame>         frame;
    std::shared_ptr<StreamProfile> sp;
    switch(frameType) {
    case OB_FRAME_VIDEO:
    case OB_FRAME_DEPTH:
    case OB_FRAME_IR_LEFT:
    case OB_FRAME_IR_RIGHT:
    case OB_FRAME_IR:
    case OB_FRAME_COLOR:
        return createVideoFrameFromUserBuffer(frameType, format, 0, 0, 0, buffer, bufferSize, bufferReclaimFunc);
    case OB_FRAME_ACCEL:
        frame = std::make_shared<AccelFrame>(buffer, bufferSize, bufferReclaimFunc);
        sp    = StreamProfileFactory::createAccelStreamProfile(OB_ACCEL_FS_2g, OB_SAMPLE_RATE_1_5625_HZ);
        break;
    case OB_FRAME_GYRO:
        frame = std::make_shared<GyroFrame>(buffer, bufferSize, bufferReclaimFunc);
        sp    = StreamProfileFactory::createGyroStreamProfile(OB_GYRO_FS_16dps, OB_SAMPLE_RATE_1_5625_HZ);
        break;
    default:
        throw libobsensor::invalid_value_exception("Invalid frame type for user frame.");
        break;
    }

    frame->setStreamProfile(sp);
    return frame;
}

std::shared_ptr<Frame> FrameFactory::createVideoFrameFromUserBuffer(OBFrameType frameType, OBFormat format, uint32_t width, uint32_t height,
                                                                    uint32_t strideBytes, uint8_t *buffer, size_t bufferSize,
                                                                    FrameBufferReclaimFunc bufferReclaimFunc) {
    std::shared_ptr<Frame> frame;
    switch(frameType) {
    case OB_FRAME_VIDEO:
        frame = std::make_shared<VideoFrame>(buffer, bufferSize, frameType, bufferReclaimFunc);
        break;
    case OB_FRAME_DEPTH:
        frame = std::make_shared<DepthFrame>(buffer, bufferSize, bufferReclaimFunc);
        break;
    case OB_FRAME_IR_LEFT:
        frame = std::make_shared<IRLeftFrame>(buffer, bufferSize, bufferReclaimFunc);
        break;
    case OB_FRAME_IR_RIGHT:
        frame = std::make_shared<IRRightFrame>(buffer, bufferSize, bufferReclaimFunc);
        break;
    case OB_FRAME_IR:
        frame = std::make_shared<IRFrame>(buffer, bufferSize, bufferReclaimFunc);
        break;
    case OB_FRAME_COLOR:
        frame = std::make_shared<ColorFrame>(buffer, bufferSize, bufferReclaimFunc);
        break;
    default:
        throw libobsensor::invalid_value_exception("Invalid frame type for video frame.");
        break;
    }

    auto streamType = utils::mapFrameTypeToStreamType(frameType);
    auto sp         = StreamProfileFactory::createVideoStreamProfile(streamType, format, width, height, 0);

    frame->setStreamProfile(sp);

    if(strideBytes == 0) {
        strideBytes =  utils::calcDefaultStrideBytes(format, width);
    }
    frame->as<VideoFrame>()->setStride(strideBytes);
    if(strideBytes * height > bufferSize) {
        LOG_WARN("The strideBytes * height is greater than to the bufferSize, it is dangerous to access the buffer!");
    }
    return frame;
}

std::shared_ptr<Frame> FrameFactory::createFrameFromStreamProfile(std::shared_ptr<const StreamProfile> sp) {
    auto memoryPool    = libobsensor::FrameMemoryPool::getInstance();
    auto frameType     = utils::mapStreamTypeToFrameType(sp->getType());
    auto bufferManager = memoryPool->createFrameBufferManager(frameType, sp);

    auto frame = bufferManager->acquireFrame();
    if(frame == nullptr) {
        throw libobsensor::memory_exception("Failed to create frame, out of memory or other memory allocation error.");
    }

    frame->setStreamProfile(sp);
    return frame;
}

std::shared_ptr<FrameSet> FrameFactory::createFrameSet() {
    auto memoryPool            = libobsensor::FrameMemoryPool::getInstance();
    auto frameSetBufferManager = memoryPool->createFrameBufferManager(OB_FRAME_SET, OB_FRAME_TYPE_COUNT * sizeof(std::shared_ptr<Frame>));

    auto frame = frameSetBufferManager->acquireFrame();
    if(frame == nullptr) {
        throw libobsensor::memory_exception("Failed to create frame, out of memory or other memory allocation error.");
    }

    auto frameSet = std::dynamic_pointer_cast<FrameSet>(frame);
    if(frameSet == nullptr) {
        throw libobsensor::invalid_value_exception("Failed to create frame set.");
    }

    return frameSet;
}

}  // namespace libobsensor
