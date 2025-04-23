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

class G330Device : public DeviceBase {
public:
    G330Device(const std::shared_ptr<const IDeviceEnumInfo> &info);
    virtual ~G330Device() noexcept override;

    std::vector<std::shared_ptr<IFilter>> createRecommendedPostProcessingFilters(OBSensorType type) override;

private:
    void init() override;
    void initSensorList();
    void initSensorListGMSL();
    void initProperties();
    void initSensorStreamProfile(std::shared_ptr<ISensor> sensor);

    std::shared_ptr<const StreamProfile> loadDefaultStreamProfile(OBSensorType sensorType);
    void                                 loadDefaultDepthPostProcessingConfig();  // todo: declare in base class

private:
    const uint64_t                                              deviceTimeFreq_ = 1000;     // in ms
    const uint64_t                                              frameTimeFreq_  = 1000000;  // in us
    std::function<std::shared_ptr<IFrameTimestampCalculator>()> videoFrameTimestampCalculatorCreator_;
    bool                                                        isGmslDevice_;
};

//========================================================G330NetDevice==================================================

class G330NetDevice : public DeviceBase {
public:
    G330NetDevice(const std::shared_ptr<const IDeviceEnumInfo> &info);
    virtual ~G330NetDevice() noexcept;

    std::vector<std::shared_ptr<IFilter>> createRecommendedPostProcessingFilters(OBSensorType type) override;

private:
    void init() override;
    void initSensorList();
    void initProperties();
    void initSensorStreamProfileList(std::shared_ptr<ISensor> sensor);
    void initSensorStreamProfile(std::shared_ptr<ISensor> sensor);
    void initStreamProfileFilter(std::shared_ptr<ISensor> sensor);

    void fetchDeviceInfo() override;
    void fetchAllProfileList();

    std::shared_ptr<const StreamProfile> loadDefaultStreamProfile(OBSensorType sensorType);
    void                                 loadDefaultDepthPostProcessingConfig();  // todo: declare in base class

private:
    std::shared_ptr<const SourcePortInfo>                       vendorPortInfo_;
    const uint64_t                                              deviceTimeFreq_ = 1000;     // in ms
    const uint64_t                                              frameTimeFreq_  = 1000000;  // in us
    std::function<std::shared_ptr<IFrameTimestampCalculator>()> videoFrameTimestampCalculatorCreator_;

    StreamProfileList allNetProfileList_;

    int      netBandwidth_;
    uint32_t linkSpeed_;
};

}  // namespace libobsensor
