// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "ISourcePort.hpp"
#include "StateMachineBase.hpp"
#include "frame/FrameQueue.hpp"
#include "ros/RosbagReader.hpp"
#include "component/DeviceComponentBase.hpp"

#include <unordered_map>
#include <memory>
#include <vector>
#include <string>
#include <mutex>
#include <bitset>

namespace libobsensor {

typedef std::function<void(OBPlaybackStatus status)> PlaybackStatusCallback;
typedef StateMachineBase<OBPlaybackStatus>           PlaybackStateMachine;

class PlaybackDevicePort : public IVideoStreamPort, public IDataStreamPort {
public:
    PlaybackDevicePort(const std::string &filePath);
    virtual ~PlaybackDevicePort() noexcept override;

    // IStreamer
    virtual void startStream(std::shared_ptr<const StreamProfile> profile, MutableFrameCallback callback) override;
    virtual void stopStream(std::shared_ptr<const StreamProfile> profile) override;

    // IVideoStreamPort
    virtual StreamProfileList getStreamProfileList() override;
    virtual void              stopAllStream() override;

    // IDataStreamPort
    virtual void startStream(MutableFrameCallback callback) override;
    virtual void stopStream() override;

    // ISourcePort
    virtual std::shared_ptr<const SourcePortInfo> getSourcePortInfo() const;

public:
    StreamProfileList           getStreamProfileList(OBSensorType sensorType);  // add for compatibility using one port for multisensor
    std::shared_ptr<DeviceInfo> getDeviceInfo() const;
    std::vector<OBSensorType>   getSensorList() const;
    OBPlaybackStatus            getCurrentPlaybackStatus() const;

    uint64_t getDuration() const;
    uint64_t getPosition() const;
    void     seek(uint64_t position);

    void pause();
    void resume();
    void restoreDefaults();
    void resetBaseTimestamp();

    void setPlaybackRate(const float &rate);
    void setPlaybackStatusCallback(const PlaybackStatusCallback callback);
    void updateFrameBaseTimestamp(uint64_t frameTimestamp, uint64_t sysTimestamp);

    static uint64_t getCurrentTimenUs() {
        auto now = std::chrono::high_resolution_clock::now();
        auto us  = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
        return static_cast<uint64_t>(us);
    }

    void getRecordedPropertyValue(uint32_t propertyId, OBPropertyValue *value);
    void getRecordedPropertyRange(uint32_t propertyId, OBPropertyRange *range);
    std::vector<uint8_t> getRecordedStructData(uint32_t propertyId);

private:

    void     playbackLoop();
    uint64_t calculateSleepTime(uint64_t timestamp);

    void     startAsyncThread();
    void     stopAsyncThread();

    std::shared_ptr<FrameQueue<Frame>> &getFrameQueue(OBSensorType sensorType);

private:
    std::unordered_map<OBSensorType, std::shared_ptr<FrameQueue<Frame>>> frameQueues_;
    std::bitset<OB_SENSOR_TYPE_COUNT - 1>                                activeSensors_;  // exclude OB_SENSOR_UNKNOWN

    StreamProfileList          streamProfileList_;
    std::shared_ptr<IReader>   reader_;

    bool                    needUpdateBaseTime_;
    bool                    isLooping_;
    std::mutex              playbackMutex_;
    std::mutex              baseTimestampMutex_;
    std::condition_variable playbackCv_;
    std::thread             playbackThread_;

    PlaybackStatusCallback playbackStatusCallback_;
    PlaybackStateMachine   playbackStatus_;

    uint64_t duration_;

    uint64_t baseFrameTimestamp_;
    uint64_t baseSystemTimestamp_;
    float    rate_;

    const uint32_t maxFrameQueueSize_ = 10;
    const uint32_t playbackTimeFreq_  = 1000000;  // for converting ns to ms
    const uint32_t rangeOffset_       = UINT16_MAX;
};
}  // namespace libobsensor