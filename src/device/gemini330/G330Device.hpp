#pragma once
#include "DeviceBase.hpp"
#include "IDeviceEnumerator.hpp"
#include "frameprocessor/FrameProcessor.hpp"

#if defined(BUILD_NET_PAL)
#include "ethernet/RTPStreamPort.hpp"
#endif

#include <map>
#include <memory>

namespace libobsensor {

class G330Device : public DeviceBase {
public:
    G330Device(const std::shared_ptr<const IDeviceEnumInfo> &info);
    virtual ~G330Device() noexcept;

    std::vector<std::shared_ptr<IFilter>> createRecommendedPostProcessingFilters(OBSensorType type) override;

private:
    void init() override;
    void initSensorList();
    void initProperties();
    void initFrameMetadataParserContainer();
    void initSensorStreamProfile(std::shared_ptr<ISensor> sensor);

private:
    std::shared_ptr<IFrameMetadataParserContainer> colorMdParserContainer_;
    std::shared_ptr<IFrameMetadataParserContainer> depthMdParserContainer_;

    const uint64_t                                              deviceTimeFreq_ = 1000;     // in ms
    const uint64_t                                              frameTimeFreq_  = 1000000;  // in us
    std::function<std::shared_ptr<IFrameTimestampCalculator>()> videoFrameTimestampCalculatorCreator_;
};

//========================================================G330NetDevice==================================================

class G330NetDevice : public DeviceBase {
public:
    G330NetDevice(const std::shared_ptr<const IDeviceEnumInfo> &info);
    virtual ~G330NetDevice() noexcept;

    std::vector<std::shared_ptr<IFilter>> createRecommendedPostProcessingFilters(OBSensorType type) override;

private:
    void init() override;
    void initModeSensorList();
    void initModeProperties();
    void initFrameMetadataParserContainer();
    void initSensorStreamProfile(std::shared_ptr<ISensor> sensor);

    void fetchDeviceInfo() override;
private:
    std::shared_ptr<IFrameMetadataParserContainer> colorMdParserContainer_;
    std::shared_ptr<IFrameMetadataParserContainer> depthMdParserContainer_;

    const uint64_t                                              deviceTimeFreq_ = 1000;     // in ms
    const uint64_t                                              frameTimeFreq_  = 1000000;  // in us
    std::function<std::shared_ptr<IFrameTimestampCalculator>()> videoFrameTimestampCalculatorCreator_;
};

}  // namespace libobsensor
