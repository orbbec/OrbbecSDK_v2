// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "DeviceBase.hpp"
#include "IDeviceManager.hpp"
#include "frameprocessor/FrameProcessor.hpp"

#include <map>
#include <memory>

namespace libobsensor {

class OpenNIDeviceBase : public DeviceBase {
public:
    OpenNIDeviceBase(const std::shared_ptr<const IDeviceEnumInfo> &info);
    virtual ~OpenNIDeviceBase() noexcept;

    std::vector<std::shared_ptr<IFilter>> createRecommendedPostProcessingFilters(OBSensorType type) override;

private:
    void init() override;
    void initSensorList();
    void initProperties();

protected:
    const uint64_t deviceTimeFreq_ = 1000;     // in ms
    const uint64_t frameTimeFreq_  = 1000000;  // in us
};

}  // namespace libobsensor

