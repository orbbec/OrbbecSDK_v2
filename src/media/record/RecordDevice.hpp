// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "IDevice.hpp"
#include "frame/FrameQueue.hpp"
#include "ros/RosbagWriter.hpp"

#include <queue>
#include <mutex>
#include <memory>
#include <string>
#include <map>
#include <atomic>
#include <condition_variable>

namespace libobsensor {

class RecordDevice {
public:
    RecordDevice(std::shared_ptr<IDevice> device, const std::string &filePath, bool compressionsEnabled = true);
    virtual ~RecordDevice() noexcept;

    void pause();
    void resume();

private:
    template<typename T>
    void writePropertyT(uint32_t id) {
        auto server = device_->getPropertyServer();
        try {
            T value = server->getPropertyValueT<T>(id);
            std::vector<uint8_t> data(sizeof(T));
            data.assign(reinterpret_cast<uint8_t *>(&value), reinterpret_cast<uint8_t *>(&value) + sizeof(T));
            writer_->writeProperty(id, data.data(), static_cast<uint32_t>(data.size()));
        }
        catch (const std::exception& e) {
            LOG_WARN("Failed to record property: {}, message: {}", id, e.what());
        }
    }

    void writeAllProperties();
    void onFrameRecordingCallback(std::shared_ptr<const Frame>);

    void initializeSensorOnce(OBSensorType sensorType);
    void initializeFrameQueueOnce(OBSensorType sensorType, std::shared_ptr<const Frame> frame);

    void stopRecord();
private:
    std::shared_ptr<IDevice>   device_;
    std::string                filePath_;
    bool                       isCompressionsEnabled_;
    size_t                     maxFrameQueueSize_;
    std::atomic<bool>          isPaused_;
    std::shared_ptr<IWriter>   writer_;
    std::mutex                 frameCallbackMutex_;

    std::unordered_map<OBSensorType, std::shared_ptr<FrameQueue<const Frame>>> frameQueueMap_;
    std::unordered_map<OBSensorType, std::unique_ptr<std::once_flag>>          sensorOnceFlags_;
};
}  // namespace libobsensor

#ifdef __cplusplus
extern "C" {
#endif

struct ob_record_device_t {
    std::shared_ptr<libobsensor::RecordDevice> recorder;
};

#ifdef __cplusplus
}  // extern "C"
#endif