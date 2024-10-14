// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include <memory>
#include <functional>
#include <string>
#include <map>

#include "FrameBufferManager.hpp"
#include "logger/Logger.hpp"

namespace libobsensor {

struct FrameBufferManagerInfo {
    OBFrameType frameType;
    size_t      maxFrameDataSize;
};

struct FrameBufferManagerInfoCompare {
    bool operator()(const FrameBufferManagerInfo &l, const FrameBufferManagerInfo &r) const {
        if(l.frameType == r.frameType) {
            return l.maxFrameDataSize < r.maxFrameDataSize;
        }
        return l.frameType < r.frameType;
    }
};

class FrameMemoryPool : public std::enable_shared_from_this<FrameMemoryPool> {
private:
    FrameMemoryPool();

    static std::mutex                     instanceMutex_;
    static std::weak_ptr<FrameMemoryPool> instanceWeakPtr_;

public:
    ~FrameMemoryPool() noexcept;
    static std::shared_ptr<FrameMemoryPool> getInstance();
    static void                             setMaxFrameMemorySize(uint64_t sizeInMB);

    std::shared_ptr<IFrameBufferManager> createFrameBufferManager(OBFrameType type, size_t frameBufferSize);
    std::shared_ptr<IFrameBufferManager> createFrameBufferManager(OBFrameType type, std::shared_ptr<const StreamProfile> streamProfile);
    std::shared_ptr<IFrameBufferManager> createFrameBufferManager(OBFrameType type, OBFormat format, uint32_t width, uint32_t height);

    void freeIdleMemory();

private:
    std::map<FrameBufferManagerInfo, std::shared_ptr<IFrameBufferManager>, FrameBufferManagerInfoCompare> bufMgrMap_;
    std::mutex                                                                                            bufMgrMapMutex_;
    std::vector<std::weak_ptr<IFrameBufferManager>>                                                       bufMgrWeakList_;

    std::shared_ptr<Logger> logger_;  // Manages the lifecycle of the logger object.
};

}  // namespace libobsensor

