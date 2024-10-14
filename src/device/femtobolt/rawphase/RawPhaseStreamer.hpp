// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include <condition_variable>
#include <atomic>
#include <map>
#include <mutex>
#include <queue>

#include "ISourcePort.hpp"
#include "IDeviceComponent.hpp"
#include "depthengine/DepthEngineLoader.hpp"
#include "depthengine/YeatsFrameHdr.h"

namespace libobsensor {
class RawPhaseStreamer : public IDeviceComponent, public IVideoStreamPort {
public:
    RawPhaseStreamer(IDevice *owner, const std::shared_ptr<IVideoStreamPort> &backend);
    virtual ~RawPhaseStreamer() noexcept;

    IDevice *getOwner() const override;

    std::shared_ptr<const SourcePortInfo> getSourcePortInfo() const override;
    StreamProfileList                     getStreamProfileList() override;
    void                                  startStream(std::shared_ptr<const StreamProfile> profile, MutableFrameCallback callback) override;
    void                                  stopStream(std::shared_ptr<const StreamProfile> profile) override;
    void                                  stopAllStream() override;

    void enablePassiveIRMode(bool enable);
    bool isRunning() const;
    void waitNvramDataReady();

private:
    void parseAndOutputFrame(std::shared_ptr<Frame> frame);

    // depth engine
    void                    initNvramData();
    void                    startDepthEngineThread(std::shared_ptr<const StreamProfile> profile);
    void                    stopDepthEngineThread();
    void                    initDepthEngine(std::shared_ptr<const StreamProfile> profile);
    void                    deinitDepthEngine();
    k4a_depth_engine_mode_t getDepthEngineMode(std::shared_ptr<const StreamProfile> profile);

    void setStreamProfileList();

private:
    IDevice                          *owner_;
    std::shared_ptr<IVideoStreamPort> backend_;

    std::mutex                                                           cbMtx_;
    std::map<std::shared_ptr<const StreamProfile>, MutableFrameCallback> callbacks_;

    std::mutex                                                             streamMutex_;
    std::atomic_bool                                                       running_;
    std::map<std::pair<uint32_t, uint32_t>, std::pair<uint32_t, uint32_t>> backendProfileMap_;
    StreamProfileList                                                      backendStreamProfileList_;
    StreamProfileList                                                      streamProfileList_;
    bool                                                                   passiveIRModeEnabled_ = false;

    // nvram
    std::mutex              nvramMutex_;
    std::condition_variable nvramCV_;
    std::vector<uint8_t>    nvramData_;

    //  Depth Engine
    k4a_depth_engine_context_t *depthEngineContext_ = nullptr;
    k4a_depth_engine_mode_t     curDepthEngineMode_ = K4A_DEPTH_ENGINE_MODE_UNKNOWN;

    std::shared_ptr<DepthEngineLoadFactory> depthEngineLoader_;
    bool                                    depthEngineThreadExit_ = false;
    std::thread                             depthEngineThread_;
    std::shared_ptr<const StreamProfile>    lastStreamProfile_;

    std::mutex                         frameQueueMutex_;
    std::condition_variable            frameQueueCV_;
    std::queue<std::shared_ptr<Frame>> frameQueue_;
};

}  // namespace libobsensor
