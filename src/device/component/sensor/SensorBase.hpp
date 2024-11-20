// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "ISensor.hpp"
#include "IDevice.hpp"
#include "ISourcePort.hpp"
#include "timestamp/IFrameTimestampCalculator.hpp"

#include <map>
#include <mutex>
#include <thread>
#include <condition_variable>

namespace libobsensor {

class SensorBase : public ISensor, public std::enable_shared_from_this<SensorBase> {
    static constexpr int DefaultNoStreamTimeoutMs        = 3000;
    static constexpr int DefaultStreamInterruptTimeoutMs = 3000;
    static constexpr int DefaultMaxRecoveryCount         = 3;

public:
    SensorBase(IDevice *owner, OBSensorType sensorType, const std::shared_ptr<ISourcePort> &backend);
    ~SensorBase() noexcept override;

    OBSensorType                 getSensorType() const override;
    IDevice                     *getOwner() const override;
    std::shared_ptr<ISourcePort> getBackend() const;

    OBStreamState getStreamState() const override;
    bool          isStreamActivated() const override;
    uint32_t      registerStreamStateChangedCallback(StreamStateChangedCallback callback) override;
    void          unregisterStreamStateChangedCallback(uint32_t token) override;

    StreamProfileList                    getStreamProfileList() const override;
    void                                 setStreamProfileFilter(std::shared_ptr<IStreamProfileFilter> filter) override;
    void                                 setStreamProfileList(const StreamProfileList &profileList) override;
    void                                 updateDefaultStreamProfile(const std::shared_ptr<const StreamProfile> &profile) override;
    std::shared_ptr<const StreamProfile> getActivatedStreamProfile() const override;

    FrameCallback getFrameCallback() const override;

    // when start Stream fails or interrupts, try to recover by restarting the stream; If Timeout<0, never timeout. if Timeout=0, use default timeout.
    void enableStreamRecovery(uint32_t maxRecoveryCount = DefaultMaxRecoveryCount, int noStreamTimeoutMs = DefaultNoStreamTimeoutMs,
                              int streamInterruptTimeoutMs = DefaultStreamInterruptTimeoutMs);
    // stop trying to recover the stream
    void disableStreamRecovery();

    void setFrameMetadataParserContainer(std::shared_ptr<IFrameMetadataParserContainer> container);
    void setFrameTimestampCalculator(std::shared_ptr<IFrameTimestampCalculator> calculator);
    void setGlobalTimestampCalculator(std::shared_ptr<IFrameTimestampCalculator> calculator);

protected:
    virtual void restartStream();
    virtual void updateStreamState(OBStreamState state);
    virtual void watchStreamState();

    virtual void outputFrame(std::shared_ptr<Frame> frame);

protected:
    IDevice                     *owner_;
    const OBSensorType           sensorType_;
    std::shared_ptr<ISourcePort> backend_;

    StreamProfileList                     streamProfileList_;
    std::shared_ptr<IStreamProfileFilter> streamProfileFilter_;

    std::shared_ptr<const StreamProfile> activatedStreamProfile_;
    FrameCallback                        frameCallback_;

    std::mutex                                     streamStateCallbackMutex_;
    std::map<uint32_t, StreamStateChangedCallback> streamStateChangedCallbacks_;
    uint32_t                                       StreamStateChangedCallbackTokenCounter_ = 0;

    std::mutex                 streamStateMutex_;
    std::condition_variable    streamStateCv_;
    std::atomic<OBStreamState> streamState_;
    std::thread                streamStateWatcherThread_;

    bool     onRecovering_;
    bool     recoveryEnabled_;
    uint32_t maxRecoveryCount_;
    uint32_t recoveryCount_;
    uint32_t noStreamTimeoutMs_;
    uint32_t streamInterruptTimeoutMs_;

    std::shared_ptr<IFrameMetadataParserContainer> frameMetadataParserContainer_;
    std::shared_ptr<IFrameTimestampCalculator>     frameTimestampCalculator_;
    std::shared_ptr<IFrameTimestampCalculator>     globalTimestampCalculator_;
};

}  // namespace libobsensor
