// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "OpenNIDeviceSyncConfigurator.hpp"
#include "exception/ObException.hpp"
#include "InternalTypes.hpp"
#include <map>

namespace libobsensor {

#pragma pack(1)
typedef struct {
    uint32_t syncMode;  // ob_multi_device_sync_mode
    int      depthDelayUs;
    int      colorDelayUs;
    int      trigger2ImageDelayUs;
    bool     triggerOutEnable;
    int      triggerOutDelayUs;
    int      framesPerTrigger;
} OBMultiDeviceSyncConfigInternal;
#pragma pack()

OpenNIDeviceSyncConfigurator::OpenNIDeviceSyncConfigurator(IDevice *owner, const std::vector<OBMultiDeviceSyncMode> &supportedSyncModes)
    : DeviceComponentBase(owner), supportedSyncModes_(supportedSyncModes) {}

OBMultiDeviceSyncConfig OpenNIDeviceSyncConfigurator::getSyncConfig() {
    auto owner          = getOwner();
    auto propertyServer = owner->getPropertyServer();
    auto configInternal = propertyServer->getStructureDataT<OBMultiDeviceSyncConfigInternal>(OB_STRUCT_MULTI_DEVICE_SYNC_CONFIG);

    OBMultiDeviceSyncConfig syncConfig{};
    syncConfig.syncMode = (OBMultiDeviceSyncMode)configInternal.syncMode;
    if(syncConfig.syncMode == 0) {
        syncConfig.syncMode = OB_MULTI_DEVICE_SYNC_MODE_SECONDARY_SYNCED;
    }
    else if(syncConfig.syncMode == 1) {
        syncConfig.syncMode = OB_MULTI_DEVICE_SYNC_MODE_PRIMARY;
    }
    else if(syncConfig.syncMode == 2) {
        syncConfig.syncMode = OB_MULTI_DEVICE_SYNC_MODE_FREE_RUN;
    }
    syncConfig.depthDelayUs         = 0;
    syncConfig.colorDelayUs         = 0;
    syncConfig.trigger2ImageDelayUs = 0;
    syncConfig.triggerOutEnable     = false;
    syncConfig.triggerOutDelayUs    = 0;
    syncConfig.framesPerTrigger     = 0;  // configInternal.framesPerTrigger; set to 1 at default

    std::lock_guard<std::mutex> lock(mutex_);
    currentMultiDevSyncConfig_ = syncConfig;
    return currentMultiDevSyncConfig_;
}

void OpenNIDeviceSyncConfigurator::setSyncConfig(const OBMultiDeviceSyncConfig &deviceSyncConfig) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if(currentMultiDevSyncConfig_.syncMode == deviceSyncConfig.syncMode) {
            LOG_DEBUG("New sync config is same as current device sync config, the upgrade process would not execute!");
            return;
        }
    }

    OBMultiDeviceSyncConfigInternal internalConfig;
    internalConfig.syncMode = deviceSyncConfig.syncMode;
    if(deviceSyncConfig.syncMode == OB_MULTI_DEVICE_SYNC_MODE_SECONDARY_SYNCED) {
        internalConfig.syncMode = 0;
    }
    else if(deviceSyncConfig.syncMode == OB_MULTI_DEVICE_SYNC_MODE_PRIMARY) {
        internalConfig.syncMode = 1;
    }
    else if(deviceSyncConfig.syncMode == OB_MULTI_DEVICE_SYNC_MODE_FREE_RUN) {
        internalConfig.syncMode = 2;
    }
    internalConfig.depthDelayUs         = 0;
    internalConfig.colorDelayUs         = 0;
    internalConfig.trigger2ImageDelayUs = 0;
    internalConfig.triggerOutEnable     = false;
    internalConfig.triggerOutDelayUs    = 0;
    internalConfig.framesPerTrigger     = 0;

    auto owner          = getOwner();
    auto propertyServer = owner->getPropertyServer();
    propertyServer->setStructureDataT<OBMultiDeviceSyncConfigInternal>(OB_STRUCT_MULTI_DEVICE_SYNC_CONFIG, internalConfig);

    std::lock_guard<std::mutex> lock(mutex_);
    currentMultiDevSyncConfig_ = deviceSyncConfig;
}

uint16_t OpenNIDeviceSyncConfigurator::getSupportedSyncModeBitmap() {
    uint16_t supportedSyncModeBitmap = 0;
    for(auto &mode: supportedSyncModes_) {
        supportedSyncModeBitmap |= mode;
    }
    return supportedSyncModeBitmap;
}

void OpenNIDeviceSyncConfigurator::triggerCapture() {
    // Not supported, do nothing.
}

}  // namespace libobsensor
