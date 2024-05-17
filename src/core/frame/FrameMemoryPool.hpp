#pragma once

#include <memory>
#include <functional>
#include <string>
#include "FrameBufferManager.hpp"

namespace ob {
namespace core {

struct FrameBufferManagerInfo {
    OBFrameType frameType;
    uint32_t    maxFrameDataSize;

    friend bool operator<(const FrameBufferManagerInfo &l, const FrameBufferManagerInfo &r) {
        if(l.frameType == r.frameType) {
            return l.maxFrameDataSize < r.maxFrameDataSize;
        }
        return l.frameType < r.frameType;
    }
};


class FrameMemoryPool {
private:
    FrameMemoryPool();

public:
    ~FrameMemoryPool();
    static std::shared_ptr<FrameMemoryPool> getInstance();
    static void                             releaseInstance();
    static void                             setMaxFrameMemorySize(uint64_t sizeInMB);
    static void                             enableReuseFrameBufferManager(bool enable);

    std::shared_ptr<IFrameBufferManager> createFrameBufferManager(OBFrameType type, uint32_t maxDataSize);
    std::shared_ptr<IFrameBufferManager> createFrameBufferManager(OBFrameType type, std::shared_ptr<const StreamProfile> streamProfile);
    std::shared_ptr<IFrameBufferManager> createFrameBufferManager(OBFrameType type, OBFormat format, uint32_t width, uint32_t height);

    void freeIdleMemory();

private:
    std::map<FrameBufferManagerInfo, std::shared_ptr<IFrameBufferManager>> bufMgrMap_;
    std::mutex                                                             bufMgrMapMutex_;
    std::vector<std::weak_ptr<IFrameBufferManager>>                        bufMgrWeakList_;
};

}  // namespace core
}  // namespace ob

template <> struct std::hash<ob::core::FrameBufferManagerInfo> {
    size_t operator()(ob::core::FrameBufferManagerInfo x) const {
        return std::hash<std::string>()(std::to_string((int)x.frameType) + std::to_string(x.maxFrameDataSize));
    }
};