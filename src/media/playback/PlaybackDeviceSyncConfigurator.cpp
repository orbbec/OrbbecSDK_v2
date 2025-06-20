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
        return currentMultiDevSyncConfig_;
    }
    auto owner          = getOwner();
    auto propertyServer = owner->getPropertyServer();  // Auto-lock when getting propertyServer
    // double check after get propertyServer
    if(isSyncConfigInit_) {
        return currentMultiDevSyncConfig_;
    }
    // read from proper server
    if(propertyServer->isPropertySupported(OB_STRUCT_MULTI_DEVICE_SYNC_CONFIG, PROP_OP_READ, PROP_ACCESS_INTERNAL)) {
        currentMultiDevSyncConfig_ = propertyServer->getStructureDataT<OBMultiDeviceSyncConfig>(OB_STRUCT_MULTI_DEVICE_SYNC_CONFIG);
    }
    else {
        currentMultiDevSyncConfig_ = {};
    }
    isSyncConfigInit_          = true;
    return currentMultiDevSyncConfig_;
}

void PlaybackDeviceSyncConfigurator::setSyncConfig(const OBMultiDeviceSyncConfig &deviceSyncConfig) {
    (void)deviceSyncConfig;
    throw unsupported_operation_exception("Playback device doesn't support setSyncConfig");
}

uint16_t PlaybackDeviceSyncConfigurator::getSupportedSyncModeBitmap() {
    auto     config = getSyncConfig();
    uint16_t bitmap = 0;

    bitmap |= config.syncMode;
    return bitmap;
}

void PlaybackDeviceSyncConfigurator::triggerCapture() {
    throw unsupported_operation_exception("Playback device doesn't support triggerCapture");
}

}  // namespace libobsensor
