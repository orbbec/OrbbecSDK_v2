// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "DeviceBase.hpp"
#include "IDeviceManager.hpp"
#include "frameprocessor/FrameProcessor.hpp"

#include <map>
#include <memory>

namespace libobsensor {
class G435LeDeviceBase : public DeviceBase {
public:
    G435LeDeviceBase(const std::shared_ptr<const IDeviceEnumInfo> &info);
    virtual ~G435LeDeviceBase() noexcept;

    void init() override;

    std::vector<std::shared_ptr<IFilter>> createRecommendedPostProcessingFilters(OBSensorType type) override;

protected:
    void initSensorStreamProfile(std::shared_ptr<ISensor> sensor);

protected:
    uint64_t deviceTimeFreq_ = 1000;
};

class G435LeDevice : public G435LeDeviceBase {
public:
    G435LeDevice(const std::shared_ptr<const IDeviceEnumInfo> &info);
    virtual ~G435LeDevice() noexcept;

private:
    void init() override;
    void initSensorList();
    void initProperties();
    void initSensorStreamProfile(std::shared_ptr<ISensor> sensor);

    void fetchDeviceInfo() override;

private:
    const uint64_t frameTimeFreq_      = 1000;
    const uint64_t colorframeTimeFreq_ = 90000;
};

}  // namespace libobsensor

