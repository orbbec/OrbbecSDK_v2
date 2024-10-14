// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "FrameMemoryPool.hpp"
#include "utils/PublicTypeHelper.hpp"
#include "stream/StreamProfile.hpp"
#include <sstream>

namespace libobsensor {

std::mutex                     FrameMemoryPool::instanceMutex_;
std::weak_ptr<FrameMemoryPool> FrameMemoryPool::instanceWeakPtr_;

std::shared_ptr<FrameMemoryPool> FrameMemoryPool::getInstance() {
    std::unique_lock<std::mutex> lk(instanceMutex_);
    auto                         instance = instanceWeakPtr_.lock();
    if(!instance) {
        instance         = std::shared_ptr<FrameMemoryPool>(new FrameMemoryPool());
        instanceWeakPtr_ = instance;
    }
    return instance;
}

void FrameMemoryPool::setMaxFrameMemorySize(uint64_t sizeInMB) {
    FrameMemoryAllocator::getInstance()->setMaxFrameMemorySize(sizeInMB);
}

FrameMemoryPool::FrameMemoryPool() : logger_(Logger::getInstance()) {
    LOG_DEBUG("FrameMemoryPool created!");
}

FrameMemoryPool::~FrameMemoryPool() noexcept {
    bufMgrMap_.clear();
}

std::shared_ptr<IFrameBufferManager> FrameMemoryPool::createFrameBufferManager(OBFrameType type, size_t frameBufferSize) {
    std::unique_lock<std::mutex> lock(bufMgrMapMutex_);
    FrameBufferManagerInfo       info = { type, frameBufferSize };

    auto iter = bufMgrMap_.find(info);
    if(iter != bufMgrMap_.end()) {
        return iter->second;
    }

    std::shared_ptr<IFrameBufferManager> frameBufMgr;
    switch(type) {
    case OB_FRAME_VIDEO:

        frameBufMgr = std::shared_ptr<FrameBufferManager<VideoFrame>>(new FrameBufferManager<VideoFrame>(frameBufferSize));
        LOG_DEBUG("VideoFrame bufferManager created!");
        break;
    case OB_FRAME_DEPTH:
        frameBufMgr = std::shared_ptr<FrameBufferManager<DepthFrame>>(new FrameBufferManager<DepthFrame>(frameBufferSize));
        LOG_DEBUG("DepthFrame bufferManager created!");
        break;
    case OB_FRAME_IR_LEFT:
        frameBufMgr = std::shared_ptr<FrameBufferManager<IRLeftFrame>>(new FrameBufferManager<IRLeftFrame>(frameBufferSize));
        LOG_DEBUG("IRFrame bufferManager created!");
        break;
    case OB_FRAME_IR_RIGHT:
        frameBufMgr = std::shared_ptr<FrameBufferManager<IRRightFrame>>(new FrameBufferManager<IRRightFrame>(frameBufferSize));
        LOG_DEBUG("IRFrame bufferManager created!");
        break;
    case OB_FRAME_IR:
        frameBufMgr = std::shared_ptr<FrameBufferManager<IRFrame>>(new FrameBufferManager<IRFrame>(frameBufferSize));
        LOG_DEBUG("IRFrame bufferManager created!");
        break;
    case OB_FRAME_COLOR:
        frameBufMgr = std::shared_ptr<FrameBufferManager<ColorFrame>>(new FrameBufferManager<ColorFrame>(frameBufferSize));
        LOG_DEBUG("ColorFrame bufferManager created!");
        break;
    case OB_FRAME_GYRO:
        frameBufMgr = std::shared_ptr<FrameBufferManager<GyroFrame>>(new FrameBufferManager<GyroFrame>(frameBufferSize));
        LOG_DEBUG("GyroFrame bufferManager created!");
        break;
    case OB_FRAME_ACCEL:
        frameBufMgr = std::shared_ptr<FrameBufferManager<AccelFrame>>(new FrameBufferManager<AccelFrame>(frameBufferSize));
        LOG_DEBUG("AccelFrame bufferManager created!");
        break;
    case OB_FRAME_POINTS:
        frameBufMgr = std::shared_ptr<FrameBufferManager<PointsFrame>>(new FrameBufferManager<PointsFrame>(frameBufferSize));
        LOG_DEBUG("PointsFrame bufferManager created!");
        break;
    case OB_FRAME_SET:
        frameBufMgr = std::shared_ptr<FrameBufferManager<FrameSet>>(new FrameBufferManager<FrameSet>(frameBufferSize));
        LOG_DEBUG("Frameset bufferManager created!");
        break;
    default:
        if(frameBufferSize != 0) {
            frameBufMgr = std::shared_ptr<FrameBufferManager<Frame>>(new FrameBufferManager<Frame>(frameBufferSize));
            LOG_DEBUG("Frame bufferManager created!");
        }
        else {
            std::ostringstream oss_msg;
            oss_msg << "Unsupported Frame Type to create buffer manager! frameType: " << type;
            throw unsupported_operation_exception(oss_msg.str());
        }
        break;
    }

    bufMgrMap_.insert({ info, frameBufMgr });

    return frameBufMgr;
}

std::shared_ptr<IFrameBufferManager> FrameMemoryPool::createFrameBufferManager(OBFrameType type, std::shared_ptr<const StreamProfile> streamProfile) {
    if(streamProfile->is<VideoStreamProfile>()) {
        auto sp = streamProfile->as<VideoStreamProfile>();
        return createFrameBufferManager(type, sp->getFormat(), sp->getWidth(), sp->getHeight());
    }
    else if(streamProfile->is<AccelStreamProfile>()) {
        return createFrameBufferManager(type, sizeof(AccelFrame::Data));
    }
    else if(streamProfile->is<GyroStreamProfile>()) {
        return createFrameBufferManager(type, sizeof(GyroFrame::Data));
    }
    LOG_WARN("unsupported streamProfile type");
    return nullptr;
}

std::shared_ptr<IFrameBufferManager> FrameMemoryPool::createFrameBufferManager(OBFrameType type, OBFormat format, uint32_t width, uint32_t height) {
    auto frameBufferSize = utils::calcVideoFrameMaxDataSize(format, width, height);
    return createFrameBufferManager(type, frameBufferSize);
}

void FrameMemoryPool::freeIdleMemory() {
    std::unique_lock<std::mutex> lock(bufMgrMapMutex_);
    auto                         iter = bufMgrMap_.begin();
    while(iter != bufMgrMap_.end()) {
        if(iter->second.use_count() == 1) {
            iter = bufMgrMap_.erase(iter);
            continue;
        }
        iter->second->releaseIdleBuffer();
        iter++;
    }

    auto vecIter = bufMgrWeakList_.begin();
    while(vecIter != bufMgrWeakList_.end()) {
        if(vecIter->expired()) {
            vecIter = bufMgrWeakList_.erase(vecIter);
            continue;
        }
        auto bufMgr = vecIter->lock();
        if(bufMgr) {
            bufMgr->releaseIdleBuffer();
        }
        vecIter++;
    }
}

}  // namespace libobsensor
