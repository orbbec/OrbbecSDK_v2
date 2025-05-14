// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "IDevice.hpp"
#include "frame/FrameQueue.hpp"
#include "ros/RosbagWriter.hpp"
#include "component/property/PropertyHelper.hpp"

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
    template <typename T> void writePropertyT(uint32_t id) {
        auto server = device_->getPropertyServer();
        if(!server->isPropertySupported(id, PROP_OP_READ, PROP_ACCESS_INTERNAL)) {
            LOG_DEBUG("Property {} is not supported by device, skipping recording value", id);
            return;
        }

        try {
            T                    value = server->getPropertyValueT<T>(id);
            std::vector<uint8_t> data(sizeof(T));
            data.assign(reinterpret_cast<uint8_t *>(&value), reinterpret_cast<uint8_t *>(&value) + sizeof(T));
            writer_->writeProperty(id, data.data(), static_cast<uint32_t>(data.size()));
            writeProperyRangeT<T>(id);
        }
        catch(const std::exception &e) {
            LOG_WARN("Failed to record property: {}, message: {}", id, e.what());
        }
    }

    template <typename T> void writeProperyRangeT(uint32_t id) {
        auto server = device_->getPropertyServer();
        if(!server->isPropertySupported(id, PROP_OP_READ, PROP_ACCESS_INTERNAL)) {
            LOG_DEBUG("Property {} is not supported by device, skipping recording range", id);
            return;
        }

        try {
            auto                 range = server->getPropertyRangeT<T>(id);
            std::vector<uint8_t> data(sizeof(range));
            data.assign(reinterpret_cast<uint8_t *>(&range), reinterpret_cast<uint8_t *>(&range) + sizeof(range));
            writer_->writeProperty(id + rangeOffset_, data.data(), static_cast<uint32_t>(data.size()));
        }
        catch(const std::exception &e) {
            LOG_WARN("Failed to record property range: {}, message: {}", id, e.what());
        }
    }

    void writeVersionProperty();
    void writeFilterProperty();
    void writeFrameGeometryProperty();
    void writeMetadataProperty();
    void writeExposureAndGainProperty();
    void writeCalibrationParamProperty();
    void writeDepthWorkModeProperty();
    void writeAllProperties();

    void onFrameRecordingCallback(std::shared_ptr<const Frame>);

    void initializeSensorOnce(OBSensorType sensorType);
    void initializeFrameQueueOnce(OBSensorType sensorType, std::shared_ptr<const Frame> frame);

    void stopRecord();
private:
    std::shared_ptr<IDevice> device_;
    std::string              filePath_;
    bool                     isCompressionsEnabled_;
    size_t                   maxFrameQueueSize_;
    std::atomic<bool>        isPaused_;
    std::shared_ptr<IWriter> writer_;
    std::mutex               frameCallbackMutex_;
    std::mutex               frameQueueInitMutex_;

    std::map<OBSensorType, std::shared_ptr<FrameQueue<const Frame>>> frameQueueMap_;
    std::map<OBSensorType, std::unique_ptr<std::once_flag>>          sensorOnceFlags_;

    const uint32_t rangeOffset_       = UINT16_MAX;  // used to record property range
    const uint32_t versionPropertyId_ = 0;           // used to record version of recording file
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