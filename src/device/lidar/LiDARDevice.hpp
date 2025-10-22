// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "LiDARDeviceBase.hpp"
#include "IDeviceManager.hpp"
#include "frameprocessor/FrameProcessor.hpp"

#include <map>
#include <memory>

namespace libobsensor {

/**
 * @brief LiDARDevice class for LiDAR ME450 device
 */
class LiDARDevice : public LiDARDeviceBase {
public:
    LiDARDevice(const std::shared_ptr<const IDeviceEnumInfo> &info);
    virtual ~LiDARDevice() noexcept;

private:
    void init() override;
    void initSensorList();
    void initProperties();
    void fetchDeviceInfo() override;
    void initSensorStreamProfile(std::shared_ptr<ISensor> sensor);
    void getImuFullScaleRange(OBAccelFullScaleRange &accel, OBGyroFullScaleRange &gyro);

private:
    std::map<std::string, std::shared_ptr<IFilter>> lidarFilterList_;
};

}  // namespace libobsensor
