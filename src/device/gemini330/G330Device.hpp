#pragma once
#include "Device.hpp"
#include "IDeviceEnumerator.hpp"

#include "timestamp/GlobalTimestampFitter.hpp"
#include "frameprocessor/FrameProcessor.hpp"

#include <map>
#include <memory>

namespace libobsensor {

class G330Device : public Device, public std::enable_shared_from_this<IDevice> {
public:
    G330Device(const std::shared_ptr<const IDeviceEnumInfo> &info);
    virtual ~G330Device() noexcept;

    std::shared_ptr<const DeviceInfo> getInfo() const override;
    const std::string                &getExtensionInfo(const std::string &infoKey) override;

    ResourcePtr<IPropertyAccessor> getPropertyAccessor() override;

    std::vector<OBSensorType>             getSensorTypeList() const override;
    std::vector<std::shared_ptr<IFilter>> createRecommendedPostProcessingFilters(OBSensorType type) override;
    ResourcePtr<ISensor>                  getSensor(OBSensorType type) override;

    void enableHeadBeat(bool enable) override;

    OBDeviceState getDeviceState() override;
    int           registerDeviceStateChangeCallback(DeviceStateChangedCallback callback) override;
    void          unregisterDeviceStateChangeCallback(int index) override;

    void reboot() override;
    void deactivate() override;

    void updateFirmware(const char *data, uint32_t dataSize, DeviceFwUpdateCallback updateCallback, bool async) override;

    const std::vector<uint8_t> &sendAndReceiveData(const std::vector<uint8_t> &data) override;

private:
    void initSensors();
    void initProperties();
    void initFrameMetadataParserContainer();

    ResourceLock tryLockResource();

    std::shared_ptr<IFilter> getSpecifyFilter(const std::string &name,bool createIfNotExist = true);

private:
    const std::shared_ptr<const IDeviceEnumInfo> enumInfo_;
    std::shared_ptr<DeviceInfo>                 deviceInfo_;
    std::map<std::string, std::string>          extensionInfo_;
    std::shared_ptr<IPropertyAccessor>          propertyAccessor_;
    std::map<OBSensorType, SensorEntry>         sensors_;

    std::shared_ptr<GlobalTimestampFitter>         globalTimestampFitter_;
    std::shared_ptr<IFrameMetadataParserContainer> colorMdParserContainer_;
    std::shared_ptr<IFrameMetadataParserContainer> depthMdParserContainer_;
    std::shared_ptr<IFrameTimestampCalculator>     videoFrameTimestampCalculator_;
    // std::shared_ptr<IFrameTimestampCalculator>     imuFrameTimestampCalculator_;

    std::recursive_timed_mutex componentLock_;

    std::vector<std::shared_ptr<IFilter>> filters_;

    std::shared_ptr<FrameProcessorFactory> frameProcessorFactory_ = nullptr;
};
/* #endregion ------------G330Device declare end---------------- */

}  // namespace libobsensor