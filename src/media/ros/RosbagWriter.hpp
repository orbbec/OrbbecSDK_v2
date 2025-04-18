// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "IWriter.hpp"
#include "RosFileFormat.hpp"
#include "frame/Frame.hpp"
#include "IDevice.hpp"
#include "libobsensor/h/ObTypes.h"
#include "logger/Logger.hpp"
#include "stream/StreamProfile.hpp"

#include "rosbag/bag.h"
#include "rosbag/view.h"
#include "ros/time.h"
#include "sensor_msgs/Image.h"
#include "sensor_msgs/CompressedImage.h"
#include "sensor_msgs/image_encodings.h"
#include "sensor_msgs/Imu.h"
#include "custom_msg/OBDeviceInfo.h"
#include "custom_msg/OBStreamProfile.h"
#include "custom_msg/OBImuStreamProfile.h"
#include "custom_msg/OBDisparityParam.h"
#include "custom_msg/OBProperty.h"

#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace libobsensor {

class RosWriter : public IWriter {
public:
    explicit RosWriter(const std::string &file, bool compressWhileRecord);
    virtual ~RosWriter() noexcept override;

    virtual void writeFrame(const OBSensorType &sensorType, std::shared_ptr<const Frame> curFrame) override;
    virtual void writeDeviceInfo(const std::shared_ptr<const DeviceInfo> &deviceInfo) override;
    virtual void writeProperty(uint32_t propertyID, const uint8_t *data, const uint32_t datasize) override;

    virtual void writeStreamProfiles() override;

private:
    void writeVideoFrame(const OBSensorType &sensorType, std::shared_ptr<const Frame> curFrame);
    void writeImuFrame(const OBSensorType &sensorType, std::shared_ptr<const Frame> curFrame);
    void writeVideoStreamProfile(const OBSensorType sensorType, const std::shared_ptr<const StreamProfile> &streamProfile);
    void writeAccelStreamProfile(const std::shared_ptr<const StreamProfile> &streamProfile);
    void writeGyroStreamProfile(const std::shared_ptr<const StreamProfile> &streamProfile);
    void writeDisparityParam(std::shared_ptr<const DisparityBasedStreamProfile> disparityParam);

private:
    std::string                                                  filePath_;
    std::shared_ptr<rosbag::Bag>                                 file_;
    std::mutex                                                   writeMutex_;
    uint64_t                                                     startTime_;
    std::shared_ptr<const StreamProfile>                         colorStreamProfile_;
    std::shared_ptr<const StreamProfile>                         depthStreamProfile_;
    std::map<OBSensorType, std::shared_ptr<const StreamProfile>> streamProfileMap_;

    uint64_t minFrameTime_;
    uint64_t maxFrameTime_;
};

}  // namespace libobsensor