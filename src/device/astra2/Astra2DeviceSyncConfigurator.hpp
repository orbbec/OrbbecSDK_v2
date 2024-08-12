#pragma once
#include "IDevice.hpp"
#include "IDeviceSyncConfigurator.hpp"
#include "DeviceComponentBase.hpp"

#include <vector>
#include <memory>

namespace libobsensor {

class Astra2DeviceSyncConfigurator : public IDeviceSyncConfigurator, public DeviceComponentBase {
public:
    Astra2DeviceSyncConfigurator(IDevice *owner, const std::vector<OBMultiDeviceSyncMode> &supportedSyncModes);
    virtual ~Astra2DeviceSyncConfigurator() = default;

    OBMultiDeviceSyncConfig      getSyncConfig() override;
    void                         setSyncConfig(const OBMultiDeviceSyncConfig &deviceSyncConfig) override;
    uint16_t                     getSupportedSyncModeBitmap() override;
    void                         triggerCapture() override;
    void                         setTimestampResetConfig(const OBDeviceTimestampResetConfig &timestampResetConfig) override;
    OBDeviceTimestampResetConfig getTimestampResetConfig() override;
    void                         timestampReset() override;
    void                         timerSyncWithHost() override;

private:
    const std::vector<OBMultiDeviceSyncMode> supportedSyncModes_;

    std::atomic<bool>       isSyncConfigInit_;
    OBMultiDeviceSyncConfig currentMultiDevSyncConfig_;

    std::atomic<bool>            isTimestampResetConfigInit_;
    OBDeviceTimestampResetConfig currentTimestampResetConfig_;
};

}  // namespace libobsensor