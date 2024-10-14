// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "Astra2DeviceSyncConfigurator.hpp"

namespace libobsensor {

Astra2DeviceSyncConfigurator::Astra2DeviceSyncConfigurator(IDevice *owner, const std::vector<OBMultiDeviceSyncMode> &supportedSyncModes)
    : DeviceSyncConfiguratorOldProtocol(owner, supportedSyncModes) {}

OBMultiDeviceSyncConfig Astra2DeviceSyncConfigurator::getSyncConfig() {
    auto config = DeviceSyncConfiguratorOldProtocol::getSyncConfig();
    if(config.syncMode == OB_MULTI_DEVICE_SYNC_MODE_SECONDARY) {
        config.syncMode = OB_MULTI_DEVICE_SYNC_MODE_SECONDARY_SYNCED;
    }
    if(config.syncMode != OB_MULTI_DEVICE_SYNC_MODE_FREE_RUN && config.syncMode != OB_MULTI_DEVICE_SYNC_MODE_STANDALONE) {
        if(config.depthDelayUs == 0) {
            config.triggerOutDelayUs    = -1;
            config.trigger2ImageDelayUs = 0;
        }
        else {
            config.trigger2ImageDelayUs = config.depthDelayUs;
            config.triggerOutDelayUs    = 0;
        }
    }
    return config;
}

void Astra2DeviceSyncConfigurator::setSyncConfig(const OBMultiDeviceSyncConfig &deviceSyncConfig) {
    OBMultiDeviceSyncConfig syncConfig = deviceSyncConfig;
    if(syncConfig.syncMode != OB_MULTI_DEVICE_SYNC_MODE_FREE_RUN && syncConfig.syncMode != OB_MULTI_DEVICE_SYNC_MODE_STANDALONE) {
        if(syncConfig.triggerOutDelayUs == -1) {
            if(syncConfig.trigger2ImageDelayUs != 0) {
                LOG_WARN("For Astra2 device, when triggerOutDelayUs is -1, trigger2ImageDelayUs must be 0, current value is {}, will be set to 0",
                         syncConfig.trigger2ImageDelayUs);
            }
            syncConfig.depthDelayUs = 0;
        }
        else {
            if(syncConfig.triggerOutDelayUs != 0) {
                LOG_WARN("For Astra2 device, the triggerOutDelayUs only support -1 and 0, current value is {}, will be set to 0", syncConfig.triggerOutDelayUs);
            }
            syncConfig.depthDelayUs = syncConfig.trigger2ImageDelayUs;
            if(syncConfig.depthDelayUs <= 0) {
                LOG_WARN("For Astra2 device, when triggerOutDelayUs is 0, trigger2ImageDelayUs must be greater than 0, current value is {}, will be "
                         "set to 1",
                         syncConfig.depthDelayUs);
                syncConfig.depthDelayUs = 1;
            }
        }
    }
    DeviceSyncConfiguratorOldProtocol::setSyncConfig(syncConfig);
}

}  // namespace libobsensor
