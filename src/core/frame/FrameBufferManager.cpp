// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "FrameBufferManager.hpp"
#include "FrameMemoryPool.hpp"

#include "environment/EnvConfig.hpp"
#include "exception/ObException.hpp"
#include "logger/Logger.hpp"

namespace libobsensor {

#define DEFAULT_MAX_FRAME_MEMORY_SIZE ((uint64_t)2 * 1024 * 1024 * 1024)  // 2GB

FrameMemoryAllocator::FrameMemoryAllocator() : maxSizeInByte_(DEFAULT_MAX_FRAME_MEMORY_SIZE), usedSize_(0), logger_(Logger::getInstance()) {
    auto envConfig = EnvConfig::getInstance();

    if(envConfig->isNodeContained("Memory.MaxFrameBufferSize")) {
        int frameBufferSize;
        envConfig->getIntValue("Memory.MaxFrameBufferSize", frameBufferSize);
        if(frameBufferSize < 100) {
            LOG_WARN("The frame buffer size you set is too small, will set to 100MB instead");
            frameBufferSize = 100;
        }
        maxSizeInByte_ = static_cast<uint64_t>(frameBufferSize) * 1024 * 1024;  // MB to Byte
    }
    LOG_DEBUG("FrameMemoryAllocator created! The max frame memory size has been set to {:.3f}MB", byteToMB(maxSizeInByte_));
}

FrameMemoryAllocator::~FrameMemoryAllocator() noexcept {
    if(usedSize_ > 0) {
        LOG_WARN("FrameMemoryAllocator destroyed while still has memory used! usedSize={0:.3f}MB", byteToMB(usedSize_));
    }
}

std::mutex                            FrameMemoryAllocator::instanceMutex_;
std::weak_ptr<FrameMemoryAllocator>   FrameMemoryAllocator::instanceWeakPtr_;
std::shared_ptr<FrameMemoryAllocator> FrameMemoryAllocator::getInstance() {
    std::lock_guard<std::mutex> lock(instanceMutex_);
    auto                        instance = instanceWeakPtr_.lock();
    if(!instance) {
        instance         = std::shared_ptr<FrameMemoryAllocator>(new FrameMemoryAllocator());
        instanceWeakPtr_ = instance;
    }
    return instance;
}

void FrameMemoryAllocator::setMaxFrameMemorySize(uint64_t sizeInMb) {
    std::unique_lock<std::mutex> lock(mutex_);
    maxSizeInByte_ = sizeInMb * 1024 * 1024;
    if(maxSizeInByte_ < usedSize_) {
        LOG_WARN("The max frame memory size you set is {:.3f}MB,  less than the current used size, will set to {:.3f}MB instead", byteToMB(maxSizeInByte_),
                 byteToMB(usedSize_));
    }
    if(sizeInMb < 100) {  // 100 MB
        LOG_WARN("The size you is less than 100MB, size={:.3f}MB, will set to 100MB instead", (double)sizeInMb);
        maxSizeInByte_ = 100 * 1024 * 1024;
    }
    LOG_DEBUG("FrameMemoryAllocator max frame memory size has been set to {:.3f}MB", byteToMB(maxSizeInByte_));
}

uint8_t *FrameMemoryAllocator::allocate(size_t size) {
    std::unique_lock<std::mutex> lock(mutex_);
    if(usedSize_ + size > maxSizeInByte_) {
        LOG_WARN("FrameMemoryAllocator out of memory! require={0:.3f}MB, total usage: allocated={1:.3f}MB, max limit={2:.3f}MB", byteToMB(size),
                 byteToMB(usedSize_), byteToMB(maxSizeInByte_));
        return nullptr;
    }

    void *ptr = malloc(size);
    if(ptr == nullptr && reinterpret_cast<uintptr_t>(ptr) == 0xdddddddd) {
        LOG_ERROR("FrameMemoryAllocator malloc failed! ptr={0:x}", (uintptr_t)ptr);
    }

    memset(ptr, 0, size);
    usedSize_ += size;
    LOG_DEBUG("New frame buffer allocated={0:.3f}MB, total usage: allocated={1:.3f}MB, max limit={2:.3f}MB", byteToMB(size), byteToMB(usedSize_),
              byteToMB(maxSizeInByte_));
    return (uint8_t *)ptr;
}

void FrameMemoryAllocator::deallocate(uint8_t *ptr, size_t size) {
    std::unique_lock<std::mutex> lock(mutex_);
    usedSize_ -= size;
    free(ptr);
    LOG_DEBUG("Frame buffer released={0:.3f}MB, total usage: allocated={1:.3f}MB, max limit={2:.3f}MB", byteToMB(size), byteToMB(usedSize_),
              byteToMB(maxSizeInByte_));
}

FrameBufferManagerBase::FrameBufferManagerBase(size_t frameDataBufferSize, size_t frameObjSize)
    : frameDataBufferSize_(frameDataBufferSize), frameObjSize_(frameObjSize), frameMemoryAllocator_(FrameMemoryAllocator::getInstance()) {
    frameTotalSize_ = frameDataBufferSize_ + frameObjSize_ + FRAME_DATA_ALIGN_IN_BYTE
                      - 1;  // Apply for more FRAME_DATA_ALIGN_IN_BYTE-1 to facilitate offset part of the data address and achieve alignment
}

FrameBufferManagerBase::~FrameBufferManagerBase() noexcept {
    std::unique_lock<std::recursive_mutex> lock_(mutex_);
    while(!availableFrameBuffers_.empty()) {
        frameMemoryAllocator_->deallocate(availableFrameBuffers_.front(), frameTotalSize_);
        availableFrameBuffers_.erase(availableFrameBuffers_.begin());
    }
    LOG_DEBUG("FrameBufferManagerBase destroyed! manager type:{0},  obj addr:0x{1:x}", typeid(*this).name(), uint64_t(this));
}

uint8_t *FrameBufferManagerBase::acquireBuffer() {
    std::unique_lock<std::recursive_mutex> lock_(mutex_);
    uint8_t                               *bufferPtr = nullptr;
    if(!availableFrameBuffers_.empty()) {
        bufferPtr = *availableFrameBuffers_.begin();
        availableFrameBuffers_.erase(availableFrameBuffers_.begin());
    }
    else {
        bufferPtr = frameMemoryAllocator_->allocate(frameTotalSize_);
        if(bufferPtr == nullptr) {
            LOG_WARN("allocBuffer failed! Will retry after release idle memory on FrameMemoryPool");
            auto memoryPool = FrameMemoryPool::getInstance();
            memoryPool->freeIdleMemory();
            bufferPtr = frameMemoryAllocator_->allocate(frameTotalSize_);
            if(bufferPtr == nullptr) {
                auto msg = std::string("Alloc frame buffer failed! size=") + std::to_string(frameTotalSize_);
                LOG_FATAL(msg);
                throw memory_exception(msg);
            }
        }
    }
    return bufferPtr;
}

std::unique_lock<std::recursive_mutex> FrameBufferManagerBase::lockBuffers() {
    std::unique_lock<std::recursive_mutex> lock_(mutex_);
    return lock_;
}

void FrameBufferManagerBase::reclaimBuffer(void *buffer) {
    std::unique_lock<std::recursive_mutex> lock_(mutex_);
    availableFrameBuffers_.push_back((uint8_t *)buffer);

    if(availableFrameBuffers_.size() > 100) {
        // Release the memory in time when there are enough availableFrameBuffers_
        // The current buffer cannot be deleted directly, which will cause frame destruction and crash.
        frameMemoryAllocator_->deallocate(availableFrameBuffers_.front(), frameTotalSize_);
        availableFrameBuffers_.erase(availableFrameBuffers_.begin());
    }
}

void FrameBufferManagerBase::releaseIdleBuffer() {
    std::unique_lock<std::recursive_mutex> lock_(mutex_);
    while(!availableFrameBuffers_.empty()) {
        // Release the memory in time when there are enough availableFrameBuffers_
        // The current buffer cannot be deleted directly, which will cause frame destruction and crash.
        frameMemoryAllocator_->deallocate(availableFrameBuffers_.front(), frameTotalSize_);
        availableFrameBuffers_.erase(availableFrameBuffers_.begin());
    }
}

}  // namespace libobsensor
