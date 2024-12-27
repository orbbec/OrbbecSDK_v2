// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "DeviceBase.hpp"
#include "IDeviceManager.hpp"
#include "frameprocessor/FrameProcessor.hpp"

#include <map>
#include <memory>

namespace libobsensor {

class FemtoMegaINetDevice : public DeviceBase {
public:
    FemtoMegaINetDevice(const std::shared_ptr<const IDeviceEnumInfo> &info);
    virtual ~FemtoMegaINetDevice() noexcept;

private:
    void init() override;
    void initSensorList();
    void initProperties();
    void initSensorStreamProfile(std::shared_ptr<ISensor> sensor);
    void fetchAllVideoStreamProfileList();

    void fetchDeviceInfo() override;

private:
    uint64_t          deviceTimeFreq_     = 1000;
    uint64_t          depthFrameTimeFreq_ = 1000;
    uint64_t          colorFrameTimeFreq_ = 90000;

    StreamProfileList allVideoStreamProfileList_;  // fetch from device via vendor-specific protocol for all types of video stream
};

}  // namespace libobsensor
