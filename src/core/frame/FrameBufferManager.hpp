// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2020 Orbbec Corporation. All Rights Reserved.
#pragma once

#include <memory>
#include <mutex>
#include <vector>
#include "Frame.hpp"
#include "logger/Logger.hpp"

#define FRAME_DATA_ALIGN_IN_BYTE 16  // 16-byte alignment

namespace ob {
namespace core {

class FrameMemoryAllocator {
public:
    static std::shared_ptr<FrameMemoryAllocator> getInstance();
    ~FrameMemoryAllocator() noexcept;
    void     setMaxFrameMemorySize(uint64_t sizeInMb);
    uint8_t *allocate(size_t size);
    void     deallocate(uint8_t *ptr, size_t size);

private:
    FrameMemoryAllocator();

private:
    uint64_t   maxSize_;
    uint64_t   usedSize_;
    std::mutex mutex_;
};

class IFrameBufferManager {
public:
    virtual ~IFrameBufferManager() noexcept {};
    virtual void                   reclaimBuffer(void *buffer) = 0;
    virtual std::shared_ptr<Frame> acquireFrame()              = 0;
    virtual void                   releaseIdleBuffer()         = 0;
    virtual uint32_t               getFrameDataBufferSize()    = 0;
};

inline double byteToMB(uint64_t sizeInByte) {
    return (double)sizeInByte / 1024.0 / 1024.0;
}

class AbstractFrameBufferManager : public IFrameBufferManager {
public:
    AbstractFrameBufferManager(uint32_t frameDataBufferSize, uint32_t frameObjSize);

    virtual ~AbstractFrameBufferManager() noexcept;
    virtual void     reclaimBuffer(void *buffer) override;
    virtual void     releaseIdleBuffer() override;
    virtual uint32_t getFrameDataBufferSize() override {
        return frameDataBufferSize_;
    }

    std::unique_lock<std::recursive_mutex> lockBuffers();

    virtual std::shared_ptr<Frame> acquireFrame() override = 0;

protected:
    uint8_t *acquireBuffer();

protected:
    std::recursive_mutex mutex_;
    uint32_t             frameDataBufferSize_;
    uint32_t             frameObjSize_;
    uint32_t             frameTotalSize_;

private:
    std::vector<uint8_t *> availableFrameBuffers_;
};

class FrameMemoryPool;
template <typename T> class FrameBufferManager : public AbstractFrameBufferManager, public std::enable_shared_from_this<FrameBufferManager<T>> {
private:
    // Must be created through FrameMemoryPool to ensure that all FrameBufferManager objects are managed by FrameMemoryPool
    FrameBufferManager(uint32_t frameDataBufferSize) : AbstractFrameBufferManager(frameDataBufferSize, sizeof(T)) {
        LOG_DEBUG("FrameBufferManager created! frame type:{0}, obj addr:0x{1:x}, frame obj total size:{2:.3f}MB", typeid(T).name(), uint64_t(this),
                  byteToMB(frameTotalSize_));
    }

    friend class FrameMemoryPool;

public:
    virtual ~FrameBufferManager() {
        LOG_DEBUG("FrameBufferManager destroying...! frame type: {0},  obj addr:0x{1:x}", typeid(T).name(), uint64_t(this));
    }

    virtual std::shared_ptr<Frame> acquireFrame() override {
        uint8_t *bufferPtr = acquireBuffer();
        if(bufferPtr != nullptr) {
            static uint16_t align       = FRAME_DATA_ALIGN_IN_BYTE;
            uint16_t        alignOffset = (align - (uint64_t)(bufferPtr + frameObjSize_) % align) % align;

            // 1. placement new -> new(ptr)Target()
            // 2. Custom deletion function construction of shared_ptr
            // 3. You need to pass bufMgr into the smart pointer custom deletion function lambda to add a reference, otherwise bufMgr may be destructed first
            // when frame->~T(), the memory will be recycled in advance, and the frame destructor will crash.
            auto bufMgr = this->shared_from_this();
            return std::shared_ptr<T>(new(bufferPtr) T(bufferPtr + frameObjSize_ + alignOffset, frameDataBufferSize_,
                                                       [bufMgr, bufferPtr]() {  // Customized frame buffer release function
                                                           bufMgr->reclaimBuffer(bufferPtr);
                                                       }),
                                      [bufMgr](T *frame) mutable {  // Custom shared_pt delete function
                                          // Purpose of locking: To solve the problem of the buffer being used to create a new frame just after it is returned,
                                          // causing the subsequent destruction operation of the original frame to modify the new frame and causing the program
                                          // to crash.
                                          auto lk = bufMgr->lockBuffers();
                                          frame->~T();
                                          lk.unlock();
                                          bufMgr.reset();
                                      });
        }
        return nullptr;
    }
};
}  // namespace core
}  // namespace ob