// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "DeviceBase.hpp"
#include "IDeviceManager.hpp"
#include "frameprocessor/FrameProcessor.hpp"

#include <map>
#include <memory>

namespace libobsensor {
class FemtoBoltDevice : public DeviceBase {
public:
    FemtoBoltDevice(const std::shared_ptr<const IDeviceEnumInfo> &info);
    virtual ~FemtoBoltDevice() noexcept override;

    std::vector<std::shared_ptr<IFilter>> createRecommendedPostProcessingFilters(OBSensorType type) override;

private:
    void init() override;
    void initSensorList();
    void initProperties();
    void initSensorStreamProfile(std::shared_ptr<ISensor> sensor);

private:
    uint64_t deviceTimeFreq_ = 1000;
    uint64_t frameTimeFreq_  = 1000;
};
}  // namespace libobsensor
