// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "DeviceBase.hpp"
#include "IDeviceManager.hpp"
#include "frameprocessor/FrameProcessor.hpp"
#include "property/PropertyServer.hpp"
#include "property/CommonPropertyAccessors.hpp"
#include "IFrameTimestamp.hpp"

#include <map>
#include <memory>

namespace libobsensor {

class OpenNIDeviceBase : public DeviceBase {
public:
    OpenNIDeviceBase(const std::shared_ptr<const IDeviceEnumInfo> &info);
    virtual ~OpenNIDeviceBase() noexcept;

    std::vector<std::shared_ptr<IFilter>> createRecommendedPostProcessingFilters(OBSensorType type) override;

    OpenNIFrameProcessParam getFrameProcessParam();

protected:
    virtual void init() override;
    virtual void initSensorList();
    virtual void initProperties();
    virtual void initSensorStreamProfile(std::shared_ptr<ISensor> sensor);

protected:
    const uint64_t deviceTimeFreq_ = 1000;     // in ms
    const uint64_t frameTimeFreq_  = 1000000;  // in us
    std::function<std::shared_ptr<IFrameTimestampCalculator>()> videoFrameTimestampCalculatorCreator_;
    std::shared_ptr<LazySuperPropertyAccessor>                  vendorPropertyAccessor_;
};

}  // namespace libobsensor

