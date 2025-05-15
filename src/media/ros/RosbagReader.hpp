// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "IReader.hpp"
#include "frame/Frame.hpp"
#include "frame/FrameFactory.hpp"
#include "IDevice.hpp"
#include "exception/ObException.hpp"
#include "stream/StreamProfile.hpp"
#include "libobsensor/h/ObTypes.h"

#include "RosFileFormat.hpp"
#include "rosbag/bag.h"
#include "rosbag/view.h"
#include "rosbag/structures.h"
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

class RosReader : public IReader {
public:
    RosReader(const std::string &file);
    virtual ~RosReader() noexcept override = default;

    virtual std::shared_ptr<DeviceInfo>    getDeviceInfo() override;
    virtual std::chrono::nanoseconds       getDuration() override;
    virtual std::shared_ptr<StreamProfile> getStreamProfile(OBStreamType streamType) override;
    virtual bool                           getIsEndOfFile() override;
    virtual std::vector<OBSensorType>      getSensorTypeList() const override;
    virtual std::chrono::nanoseconds       getCurTime() override;
    virtual std::vector<uint8_t>           getPropertyData(uint32_t propertyId) override;
    virtual bool                           isPropertySupported(uint32_t propertyId) const override;

    virtual std::shared_ptr<Frame> readNextData() override;
    virtual void                   seekToTime(const std::chrono::nanoseconds &seekTime) override;

    virtual void stop() override;

private:
    void                   initView();
    std::shared_ptr<Frame> createVideoFrame(const rosbag::MessageInstance &msg);
    std::shared_ptr<Frame> createImuFrame(const rosbag::MessageInstance &msg);
    void                   queryDeviceInfo();
    void                   querySreamProfileList();
    void                   queryProperty();
    void                   bindStreamProfileExtrinsic();

private:
    std::string                                            filePath_;
    rosbag::Bag                                            file_;
    std::chrono::nanoseconds                               totalDuration_;
    std::mutex                                             readMutex_;
    orbbecRosbag::Time                                     startTime_;
    std::unique_ptr<rosbag::View>                          sensorView_;
    rosbag::View::iterator                                 sensorIterator_;
    std::vector<std::string>                               enabledStreamsTopics_;
    std::shared_ptr<DeviceInfo>                            deviceInfo_;
    float                                                  unit_;
    float                                                  baseline_;
    std::map<OBStreamType, std::shared_ptr<StreamProfile>> streamProfileList_;
    std::map<uint32_t, std::vector<uint8_t>>               propertyList_;
};

}  // namespace libobsensor