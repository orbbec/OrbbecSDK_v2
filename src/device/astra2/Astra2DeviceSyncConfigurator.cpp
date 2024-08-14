#include "Astra2DeviceSyncConfigurator.hpp"
#include "exception/ObException.hpp"
#include "InternalTypes.hpp"
#include <map>

namespace libobsensor {
const std::map<OBMultiDeviceSyncMode, OBSyncMode> SyncModeMapV2ToV1 = {
    { OB_MULTI_DEVICE_SYNC_MODE_FREE_RUN, OB_SYNC_MODE_CLOSE },  //
    { OB_MULTI_DEVICE_SYNC_MODE_STANDALONE, OB_SYNC_MODE_STANDALONE },
    { OB_MULTI_DEVICE_SYNC_MODE_PRIMARY, OB_SYNC_MODE_PRIMARY },
    { OB_MULTI_DEVICE_SYNC_MODE_SECONDARY, OB_SYNC_MODE_SECONDARY },
    { OB_MULTI_DEVICE_SYNC_MODE_SECONDARY_SYNCED, OB_SYNC_MODE_SECONDARY },
    { OB_MULTI_DEVICE_SYNC_MODE_SOFTWARE_TRIGGERING, OB_SYNC_MODE_PRIMARY_SOFT_TRIGGER },
    { OB_MULTI_DEVICE_SYNC_MODE_HARDWARE_TRIGGERING, OB_SYNC_MODE_SECONDARY_SOFT_TRIGGER },
};

const std::map<OBSyncMode, OBMultiDeviceSyncMode> SyncModeMapV1ToV2 = {
    { OB_SYNC_MODE_CLOSE, OB_MULTI_DEVICE_SYNC_MODE_FREE_RUN },  //
    { OB_SYNC_MODE_STANDALONE, OB_MULTI_DEVICE_SYNC_MODE_STANDALONE },
    { OB_SYNC_MODE_PRIMARY, OB_MULTI_DEVICE_SYNC_MODE_PRIMARY },
    { OB_SYNC_MODE_PRIMARY_MCU_TRIGGER, OB_MULTI_DEVICE_SYNC_MODE_PRIMARY },
    { OB_SYNC_MODE_PRIMARY_IR_TRIGGER, OB_MULTI_DEVICE_SYNC_MODE_PRIMARY },
    { OB_SYNC_MODE_PRIMARY_SOFT_TRIGGER, OB_MULTI_DEVICE_SYNC_MODE_SOFTWARE_TRIGGERING },
    { OB_SYNC_MODE_SECONDARY, OB_MULTI_DEVICE_SYNC_MODE_SECONDARY },
    { OB_SYNC_MODE_SECONDARY_SOFT_TRIGGER, OB_MULTI_DEVICE_SYNC_MODE_HARDWARE_TRIGGERING },
};

Astra2DeviceSyncConfigurator::Astra2DeviceSyncConfigurator(IDevice *owner, const std::vector<OBMultiDeviceSyncMode> &supportedSyncModes)
    : DeviceComponentBase(owner), supportedSyncModes_(supportedSyncModes), isSyncConfigInit_(false) {}

OBMultiDeviceSyncConfig Astra2DeviceSyncConfigurator::getSyncConfig() {
    if(isSyncConfigInit_) {
        return currentMultiDevSyncConfig_;
    }
    auto owner                  = getOwner();
    auto propertyServer         = owner->getPropertyServer();
    auto configInternal         = propertyServer->getStructureDataT<OBDeviceSyncConfig>(OB_STRUCT_MULTI_DEVICE_SYNC_CONFIG);
    auto triggerSignalOutEnable = propertyServer->getPropertyValueT<bool>(OB_PROP_SYNC_SIGNAL_TRIGGER_OUT_BOOL);
    auto framePerTriggering     = propertyServer->getPropertyValueT<int>(OB_PROP_CAPTURE_IMAGE_FRAME_NUMBER_INT);

    currentMultiDevSyncConfig_.syncMode             = SyncModeMapV1ToV2.at(configInternal.syncMode);
    currentMultiDevSyncConfig_.colorDelayUs         = configInternal.rgbTriggerSignalInDelay;
    currentMultiDevSyncConfig_.depthDelayUs         = configInternal.irTriggerSignalInDelay;
    currentMultiDevSyncConfig_.trigger2ImageDelayUs = configInternal.irTriggerSignalInDelay;
    currentMultiDevSyncConfig_.triggerOutEnable     = triggerSignalOutEnable;
    currentMultiDevSyncConfig_.triggerOutDelayUs    = configInternal.deviceTriggerSignalOutDelay;
    currentMultiDevSyncConfig_.framesPerTrigger     = framePerTriggering;

    if(currentMultiDevSyncConfig_.syncMode == OB_MULTI_DEVICE_SYNC_MODE_SECONDARY) {
        currentMultiDevSyncConfig_.syncMode = OB_MULTI_DEVICE_SYNC_MODE_SECONDARY_SYNCED;
    }

    isSyncConfigInit_ = true;
    return currentMultiDevSyncConfig_;
}

void Astra2DeviceSyncConfigurator::setSyncConfig(const OBMultiDeviceSyncConfig &deviceSyncConfig) {
    if(isSyncConfigInit_ && memcmp(&deviceSyncConfig, &currentMultiDevSyncConfig_, sizeof(OBMultiDeviceSyncConfig)) == 0) {
        LOG_DEBUG("Sync config is the same as current config, no need to set!");
        return;
    }
    OBMultiDeviceSyncConfig v2SyncConfig = deviceSyncConfig;
    v2SyncConfig.depthDelayUs            = v2SyncConfig.trigger2ImageDelayUs;
    if(deviceSyncConfig.syncMode == OB_MULTI_DEVICE_SYNC_MODE_PRIMARY) {
        v2SyncConfig.triggerOutEnable  = true;
        v2SyncConfig.triggerOutDelayUs = 0;
    }
    OBDeviceSyncConfig v1SyncConfig;
    v1SyncConfig.syncMode                       = SyncModeMapV2ToV1.at(v2SyncConfig.syncMode);
    v1SyncConfig.rgbTriggerSignalInDelay        = static_cast<uint16_t>(v2SyncConfig.colorDelayUs);
    v1SyncConfig.irTriggerSignalInDelay         = static_cast<uint16_t>(v2SyncConfig.depthDelayUs);
    v1SyncConfig.deviceTriggerSignalOutDelay    = static_cast<uint16_t>(v2SyncConfig.triggerOutDelayUs);
    v1SyncConfig.deviceTriggerSignalOutPolarity = 0;
    v1SyncConfig.deviceId                       = 0;
    v1SyncConfig.mcuTriggerFrequency            = 0;

    auto owner          = getOwner();
    auto propertyServer = owner->getPropertyServer();
    propertyServer->setStructureDataT<OBDeviceSyncConfig>(OB_STRUCT_MULTI_DEVICE_SYNC_CONFIG, v1SyncConfig);
    propertyServer->setPropertyValueT<bool>(OB_PROP_SYNC_SIGNAL_TRIGGER_OUT_BOOL, v2SyncConfig.triggerOutEnable);
    propertyServer->setPropertyValueT<int>(OB_PROP_CAPTURE_IMAGE_FRAME_NUMBER_INT, v2SyncConfig.framesPerTrigger);

    currentMultiDevSyncConfig_ = v2SyncConfig;
    isSyncConfigInit_          = true;
}

uint16_t Astra2DeviceSyncConfigurator::getSupportedSyncModeBitmap() {
    uint16_t supportedSyncModeBitmap = 0;
    for(auto &mode: supportedSyncModes_) {
        supportedSyncModeBitmap |= mode;
    }
    return supportedSyncModeBitmap;
}

void Astra2DeviceSyncConfigurator::triggerCapture() {
    auto owner          = getOwner();
    auto propertyServer = owner->getPropertyServer();
    propertyServer->setPropertyValueT(OB_PROP_CAPTURE_IMAGE_SIGNAL_BOOL, true);
}
}  // namespace libobsensor