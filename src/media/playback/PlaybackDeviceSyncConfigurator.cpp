// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "PlaybackDeviceSyncConfigurator.hpp"
#include "exception/ObException.hpp"
#include "InternalTypes.hpp"
#include <map>

namespace libobsensor {

PlaybackDeviceSyncConfigurator::PlaybackDeviceSyncConfigurator(IDevice *owner)
    : DeviceComponentBase(owner), isSyncConfigInit_(false), currentMultiDevSyncConfig_{} {}

OBMultiDeviceSyncConfig PlaybackDeviceSyncConfigurator::getSyncConfig() {
    if(isSyncConfigInit_) {
        std::lock_guard<std::mutex> lock(mutex_);
        return currentMultiDevSyncConfig_;
    }
    auto owner          = getOwner();
    auto propertyServer = owner->getPropertyServer();

    OBMultiDeviceSyncConfig syncConfig{};
    if(propertyServer->isPropertySupported(OB_STRUCT_MULTI_DEVICE_SYNC_CONFIG, PROP_OP_READ, PROP_ACCESS_INTERNAL)) {
        syncConfig = propertyServer->getStructureDataT<OBMultiDeviceSyncConfig>(OB_STRUCT_MULTI_DEVICE_SYNC_CONFIG);
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if(isSyncConfigInit_) {
        return currentMultiDevSyncConfig_;
    }
    currentMultiDevSyncConfig_ = syncConfig;
    isSyncConfigInit_          = true;
    return syncConfig;
}

void PlaybackDeviceSyncConfigurator::setSyncConfig(const OBMultiDeviceSyncConfig &deviceSyncConfig) {
    (void)deviceSyncConfig;
    THROW_UNSUPPORTED_OPERATION_EXCEPTION("Playback device doesn't support setSyncConfig");
}

uint16_t PlaybackDeviceSyncConfigurator::getSupportedSyncModeBitmap() {
    auto     config = getSyncConfig();
    uint16_t bitmap = 0;

    bitmap |= config.syncMode;
    return bitmap;
}

void PlaybackDeviceSyncConfigurator::triggerCapture() {
    THROW_UNSUPPORTED_OPERATION_EXCEPTION("Playback device doesn't support triggerCapture");
}

}  // namespace libobsensor
