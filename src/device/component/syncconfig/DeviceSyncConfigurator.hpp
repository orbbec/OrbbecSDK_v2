#pragma once

#pragma once
#include "IDevice.hpp"
#include "IDeviceSyncConfigurator.hpp"
#include "DeviceComponentBase.hpp"

#include <vector>
#include <memory>
#include <map>

namespace libobsensor {

class DeviceSyncConfigurator : public IDeviceSyncConfigurator, public DeviceComponentBase {
public:
    DeviceSyncConfigurator(IDevice *owner, const std::vector<OBMultiDeviceSyncMode> &supportedSyncModes);
    virtual ~DeviceSyncConfigurator() = default;

    OBMultiDeviceSyncConfig getSyncConfig() override;
    void                    setSyncConfig(const OBMultiDeviceSyncConfig &deviceSyncConfig) override;
    uint16_t                getSupportedSyncModeBitmap() override;
    void                    triggerCapture() override;

private:
    const std::vector<OBMultiDeviceSyncMode> supportedSyncModes_;

    std::atomic<bool>       isSyncConfigInit_;
    OBMultiDeviceSyncConfig currentMultiDevSyncConfig_;
};

class DeviceSyncConfiguratorOldProtocol : public IDeviceSyncConfigurator, public DeviceComponentBase {
public:
    DeviceSyncConfiguratorOldProtocol(IDevice *owner, const std::vector<OBMultiDeviceSyncMode> &supportedSyncModes);
    virtual ~DeviceSyncConfiguratorOldProtocol() = default;

    OBMultiDeviceSyncConfig getSyncConfig() override;
    void                    setSyncConfig(const OBMultiDeviceSyncConfig &deviceSyncConfig) override;
    uint16_t                getSupportedSyncModeBitmap() override;
    void                    triggerCapture() override;

    void updateModeAliasMap(const std::map<OBSyncMode, OBMultiDeviceSyncMode> &oldToNewMap, const std::map<OBMultiDeviceSyncMode, OBSyncMode> &newToOldMap);

private:
    const std::vector<OBMultiDeviceSyncMode> supportedSyncModes_;

    std::atomic<bool>       isSyncConfigInit_;
    OBMultiDeviceSyncConfig currentMultiDevSyncConfig_;

    std::map<OBSyncMode, OBMultiDeviceSyncMode> syncModeOldToNewMap_;
    std::map<OBMultiDeviceSyncMode, OBSyncMode> syncModeNewToOldMap_;
};

}  // namespace libobsensor