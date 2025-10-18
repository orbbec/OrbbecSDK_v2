// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "DeviceBase.hpp"
#include "IDeviceManager.hpp"
#include "IFrameTimestamp.hpp"

#if defined(BUILD_NET_PAL)
#include "ethernet/RTPStreamPort.hpp"
#endif

#include <map>
#include <memory>

namespace libobsensor {

class G305Device : public DeviceBase {
public:
    G305Device(const std::shared_ptr<const IDeviceEnumInfo> &info);
    virtual ~G305Device() noexcept override;

    std::vector<std::shared_ptr<IFilter>> createRecommendedPostProcessingFilters(OBSensorType type) override;

private:
    void                                 init() override;
    void                                 initProperties();
    void                                 initSensorList();
    void                                 initSensorStreamProfile(std::shared_ptr<ISensor> sensor);
    //void                                 initSensorListGMSL();
    void                                 loadDefaultDepthPostProcessingConfig();
    std::shared_ptr<const StreamProfile> loadDefaultStreamProfile(OBSensorType sensorType);
    void updateSensorStreamProfile();

private:
    const uint64_t                                              deviceTimeFreq_ = 1000;     // in ms
    const uint64_t                                              frameTimeFreq_  = 1000000;  // in us
    std::function<std::shared_ptr<IFrameTimestampCalculator>()> videoFrameTimestampCalculatorCreator_;
    bool                                                        isGmslDevice_;
};

}  // namespace libobsensor
