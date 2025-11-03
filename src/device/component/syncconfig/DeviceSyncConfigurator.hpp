// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#pragma once
#include "IDevice.hpp"
#include "IDeviceSyncConfigurator.hpp"
#include "DeviceComponentBase.hpp"
#include "IFrameTimestamp.hpp"

#include <vector>
#include <memory>
#include <map>

namespace libobsensor {

struct OBDeviceSoftSyncTime {
    uint64_t deviceTriggerTime;
};

class DeviceSyncConfigurator : public IDeviceSyncConfigurator, public DeviceComponentBase {
public:
    DeviceSyncConfigurator(IDevice *owner, const std::vector<OBMultiDeviceSyncMode> &supportedSyncModes);
    virtual ~DeviceSyncConfigurator() override = default;

    OBMultiDeviceSyncConfig getSyncConfig() override;
    void                    setSyncConfig(const OBMultiDeviceSyncConfig &deviceSyncConfig) override;
    uint16_t                getSupportedSyncModeBitmap() override;
    void                    triggerCapture() override;
    void                    triggerTimeCapture(uint64_t systemTime, uint64_t triggerTime, LinearFuncParam param);

private:
    const std::vector<OBMultiDeviceSyncMode> supportedSyncModes_;

    std::atomic<bool>       isSyncConfigInit_;
    OBMultiDeviceSyncConfig currentMultiDevSyncConfig_;
};

class DeviceSyncConfiguratorOldProtocol : public IDeviceSyncConfigurator, public DeviceComponentBase {
public:
    DeviceSyncConfiguratorOldProtocol(IDevice *owner, const std::vector<OBMultiDeviceSyncMode> &supportedSyncModes);
    virtual ~DeviceSyncConfiguratorOldProtocol() override = default;

    OBMultiDeviceSyncConfig getSyncConfig() override;
    void                    setSyncConfig(const OBMultiDeviceSyncConfig &deviceSyncConfig) override;
    uint16_t                getSupportedSyncModeBitmap() override;
    void                    triggerCapture() override;

    void updateModeAliasMap(const std::map<OBSyncMode, OBMultiDeviceSyncMode> &oldToNewMap, const std::map<OBMultiDeviceSyncMode, OBSyncMode> &newToOldMap);

    void enableDepthDelaySupport(bool enable);

private:
    const std::vector<OBMultiDeviceSyncMode> supportedSyncModes_;

    std::atomic<bool>       isSyncConfigInit_;
    OBMultiDeviceSyncConfig currentMultiDevSyncConfig_;

    std::map<OBSyncMode, OBMultiDeviceSyncMode> syncModeOldToNewMap_;
    std::map<OBMultiDeviceSyncMode, OBSyncMode> syncModeNewToOldMap_;

    bool isDepthDelaySupported_;
};

}  // namespace libobsensor
