#pragma once
#include "DeviceBase.hpp"
#include "IDeviceEnumerator.hpp"
#include "frameprocessor/FrameProcessor.hpp"

#include <map>
#include <memory>

namespace libobsensor {
class FemtoMegaDevice : public DeviceBase {
public:
    FemtoMegaDevice(const std::shared_ptr<const IDeviceEnumInfo> &info);
    virtual ~FemtoMegaDevice() noexcept;

    std::vector<std::shared_ptr<IFilter>> createRecommendedPostProcessingFilters(OBSensorType type) override;

private:
    void init() override;
    void initSensorList();
    void initProperties();
    void initSensorStreamProfile(std::shared_ptr<ISensor> sensor);
    void fetchDeviceInfo() override;

    // Net mode
    void initNetModeSensorList();
    void initNetModeProperties();
    void initNetModeSensorStreamProfileList(std::shared_ptr<ISensor> sensor);
    void fetchNetModeAllProfileList();

private:
    std::shared_ptr<IFrameMetadataParserContainer> colorMdParserContainer_;
    std::shared_ptr<IFrameMetadataParserContainer> depthMdParserContainer_;
    std::shared_ptr<IFrameTimestampCalculator>     videoFrameTimestampCalculator_;

    StreamProfileList allNetProfileList_;
};
}  // namespace libobsensor