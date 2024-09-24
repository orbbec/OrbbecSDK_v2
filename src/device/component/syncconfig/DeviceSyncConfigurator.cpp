#include "DeviceSyncConfigurator.hpp"
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

DeviceSyncConfigurator::DeviceSyncConfigurator(IDevice *owner, const std::vector<OBMultiDeviceSyncMode> &supportedSyncModes)
    : DeviceComponentBase(owner), supportedSyncModes_(supportedSyncModes), isSyncConfigInit_(false) {}

OBMultiDeviceSyncConfig DeviceSyncConfigurator::getSyncConfig() {
    if(isSyncConfigInit_) {
        return currentMultiDevSyncConfig_;
    }
    auto owner          = getOwner();
    auto propertyServer = owner->getPropertyServer();
    auto configInternal = propertyServer->getStructureDataProtoV1_1_T<OBMultiDeviceSyncConfigInternal, 1>(OB_STRUCT_MULTI_DEVICE_SYNC_CONFIG);

    currentMultiDevSyncConfig_.syncMode = (OBMultiDeviceSyncMode)configInternal.syncMode;
    if(currentMultiDevSyncConfig_.syncMode == 0) {
        currentMultiDevSyncConfig_.syncMode = OB_MULTI_DEVICE_SYNC_MODE_FREE_RUN;
    }
    else if(currentMultiDevSyncConfig_.syncMode == OB_MULTI_DEVICE_SYNC_MODE_SECONDARY_SYNCED
            && std::find(supportedSyncModes_.begin(), supportedSyncModes_.end(), OB_MULTI_DEVICE_SYNC_MODE_SECONDARY_SYNCED) == supportedSyncModes_.end()
            && std::find(supportedSyncModes_.begin(), supportedSyncModes_.end(), OB_MULTI_DEVICE_SYNC_MODE_SECONDARY) == supportedSyncModes_.end()) {
        currentMultiDevSyncConfig_.syncMode = OB_MULTI_DEVICE_SYNC_MODE_SECONDARY;
    }
    currentMultiDevSyncConfig_.depthDelayUs         = configInternal.depthDelayUs;
    currentMultiDevSyncConfig_.colorDelayUs         = configInternal.colorDelayUs;
    currentMultiDevSyncConfig_.trigger2ImageDelayUs = configInternal.trigger2ImageDelayUs;
    currentMultiDevSyncConfig_.triggerOutEnable     = configInternal.triggerOutEnable;
    currentMultiDevSyncConfig_.triggerOutDelayUs    = configInternal.triggerOutDelayUs;
    currentMultiDevSyncConfig_.framesPerTrigger     = 1;  // configInternal.framesPerTrigger; set to 1 at default

    isSyncConfigInit_ = true;
    return currentMultiDevSyncConfig_;
}

void DeviceSyncConfigurator::setSyncConfig(const OBMultiDeviceSyncConfig &deviceSyncConfig) {
    if(isSyncConfigInit_ && 0 == memcmp(&currentMultiDevSyncConfig_, &deviceSyncConfig, sizeof(OBMultiDeviceSyncConfig))) {
        LOG_DEBUG("New sync config is same as current device sync config, the upgrade process would not execute!");
        return;
    }

    OBMultiDeviceSyncConfigInternal internalConfig;
    internalConfig.syncMode = deviceSyncConfig.syncMode;
    if(deviceSyncConfig.syncMode == 0) {
        internalConfig.syncMode = OB_MULTI_DEVICE_SYNC_MODE_FREE_RUN;
    }
    else if(deviceSyncConfig.syncMode == OB_MULTI_DEVICE_SYNC_MODE_SECONDARY_SYNCED
            && std::find(supportedSyncModes_.begin(), supportedSyncModes_.end(), OB_MULTI_DEVICE_SYNC_MODE_SECONDARY_SYNCED) == supportedSyncModes_.end()
            && std::find(supportedSyncModes_.begin(), supportedSyncModes_.end(), OB_MULTI_DEVICE_SYNC_MODE_SECONDARY) == supportedSyncModes_.end()) {
        internalConfig.syncMode = OB_MULTI_DEVICE_SYNC_MODE_SECONDARY;
    }
    internalConfig.depthDelayUs         = deviceSyncConfig.depthDelayUs;
    internalConfig.colorDelayUs         = deviceSyncConfig.colorDelayUs;
    internalConfig.trigger2ImageDelayUs = deviceSyncConfig.trigger2ImageDelayUs;
    internalConfig.triggerOutEnable     = deviceSyncConfig.triggerOutEnable;
    internalConfig.triggerOutDelayUs    = deviceSyncConfig.triggerOutDelayUs;
    internalConfig.framesPerTrigger     = deviceSyncConfig.framesPerTrigger;

    auto owner          = getOwner();
    auto propertyServer = owner->getPropertyServer();
    propertyServer->setStructureDataProtoV1_1_T<OBMultiDeviceSyncConfigInternal, 1>(OB_STRUCT_MULTI_DEVICE_SYNC_CONFIG, internalConfig);

    currentMultiDevSyncConfig_ = deviceSyncConfig;
    isSyncConfigInit_          = true;
}

uint16_t DeviceSyncConfigurator::getSupportedSyncModeBitmap() {
    uint16_t supportedSyncModeBitmap = 0;
    for(auto &mode: supportedSyncModes_) {
        supportedSyncModeBitmap |= mode;
    }
    return supportedSyncModeBitmap;
}

void DeviceSyncConfigurator::triggerCapture() {
    auto owner          = getOwner();
    auto propertyServer = owner->getPropertyServer();
    propertyServer->setPropertyValueT(OB_PROP_CAPTURE_IMAGE_SIGNAL_BOOL, true);
}

const std::map<OBMultiDeviceSyncMode, OBSyncMode> DefaultSyncModeMapV2ToV1 = {
    { OB_MULTI_DEVICE_SYNC_MODE_FREE_RUN, OB_SYNC_MODE_CLOSE },  //
    { OB_MULTI_DEVICE_SYNC_MODE_STANDALONE, OB_SYNC_MODE_STANDALONE },
    { OB_MULTI_DEVICE_SYNC_MODE_PRIMARY, OB_SYNC_MODE_PRIMARY },
    { OB_MULTI_DEVICE_SYNC_MODE_SECONDARY, OB_SYNC_MODE_SECONDARY },
    { OB_MULTI_DEVICE_SYNC_MODE_SECONDARY_SYNCED, OB_SYNC_MODE_SECONDARY },
    { OB_MULTI_DEVICE_SYNC_MODE_SOFTWARE_TRIGGERING, OB_SYNC_MODE_PRIMARY_SOFT_TRIGGER },
    { OB_MULTI_DEVICE_SYNC_MODE_HARDWARE_TRIGGERING, OB_SYNC_MODE_SECONDARY_SOFT_TRIGGER },
};

const std::map<OBSyncMode, OBMultiDeviceSyncMode> DefaultSyncModeMapV1ToV2 = {
    { OB_SYNC_MODE_CLOSE, OB_MULTI_DEVICE_SYNC_MODE_FREE_RUN },  //
    { OB_SYNC_MODE_STANDALONE, OB_MULTI_DEVICE_SYNC_MODE_STANDALONE },
    { OB_SYNC_MODE_PRIMARY, OB_MULTI_DEVICE_SYNC_MODE_PRIMARY },
    { OB_SYNC_MODE_PRIMARY_MCU_TRIGGER, OB_MULTI_DEVICE_SYNC_MODE_PRIMARY },
    { OB_SYNC_MODE_PRIMARY_IR_TRIGGER, OB_MULTI_DEVICE_SYNC_MODE_PRIMARY },
    { OB_SYNC_MODE_PRIMARY_SOFT_TRIGGER, OB_MULTI_DEVICE_SYNC_MODE_SOFTWARE_TRIGGERING },
    { OB_SYNC_MODE_SECONDARY, OB_MULTI_DEVICE_SYNC_MODE_SECONDARY },
    { OB_SYNC_MODE_SECONDARY_SOFT_TRIGGER, OB_MULTI_DEVICE_SYNC_MODE_HARDWARE_TRIGGERING },
};

DeviceSyncConfiguratorOldProtocol::DeviceSyncConfiguratorOldProtocol(IDevice *owner, const std::vector<OBMultiDeviceSyncMode> &supportedSyncModes)
    : DeviceComponentBase(owner),
      supportedSyncModes_(supportedSyncModes),
      isSyncConfigInit_(false),
      syncModeOldToNewMap_(DefaultSyncModeMapV1ToV2),
      syncModeNewToOldMap_(DefaultSyncModeMapV2ToV1),
      isDepthDelaySupported_(false) {}

OBMultiDeviceSyncConfig DeviceSyncConfiguratorOldProtocol::getSyncConfig() {
    if(isSyncConfigInit_) {
        return currentMultiDevSyncConfig_;
    }
    auto owner          = getOwner();
    auto propertyServer = owner->getPropertyServer();
    auto configInternal = propertyServer->getStructureDataT<OBDeviceSyncConfig>(OB_STRUCT_MULTI_DEVICE_SYNC_CONFIG);

    bool triggerSignalOutEnable = true;
    if(propertyServer->isPropertySupported(OB_PROP_SYNC_SIGNAL_TRIGGER_OUT_BOOL, PROP_OP_READ, PROP_ACCESS_INTERNAL)) {
        triggerSignalOutEnable = propertyServer->getPropertyValueT<bool>(OB_PROP_SYNC_SIGNAL_TRIGGER_OUT_BOOL);
    }
    int framePerTriggering = 0;
    if(propertyServer->isPropertySupported(OB_PROP_CAPTURE_IMAGE_FRAME_NUMBER_INT, PROP_OP_READ, PROP_ACCESS_INTERNAL)) {
        framePerTriggering = propertyServer->getPropertyValueT<int>(OB_PROP_CAPTURE_IMAGE_FRAME_NUMBER_INT);
    }

    currentMultiDevSyncConfig_.syncMode             = syncModeOldToNewMap_.at(configInternal.syncMode);
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

void DeviceSyncConfiguratorOldProtocol::setSyncConfig(const OBMultiDeviceSyncConfig &deviceSyncConfig) {
    if(isSyncConfigInit_ && memcmp(&deviceSyncConfig, &currentMultiDevSyncConfig_, sizeof(OBMultiDeviceSyncConfig)) == 0) {
        LOG_DEBUG("Sync config is the same as current config, no need to set!");
        return;
    }
    OBMultiDeviceSyncConfig v2SyncConfig = deviceSyncConfig;
    if(!isDepthDelaySupported_) {
        v2SyncConfig.depthDelayUs = v2SyncConfig.trigger2ImageDelayUs;
    }
    if(deviceSyncConfig.syncMode == OB_MULTI_DEVICE_SYNC_MODE_PRIMARY) {
        v2SyncConfig.triggerOutEnable  = true;
        v2SyncConfig.triggerOutDelayUs = 0;
    }
    OBDeviceSyncConfig v1SyncConfig;
    v1SyncConfig.syncMode                       = syncModeNewToOldMap_.at(v2SyncConfig.syncMode);
    v1SyncConfig.rgbTriggerSignalInDelay        = static_cast<uint16_t>(v2SyncConfig.colorDelayUs);
    v1SyncConfig.irTriggerSignalInDelay         = static_cast<uint16_t>(v2SyncConfig.depthDelayUs);
    v1SyncConfig.deviceTriggerSignalOutDelay    = static_cast<uint16_t>(v2SyncConfig.triggerOutDelayUs);
    v1SyncConfig.deviceTriggerSignalOutPolarity = 0;
    v1SyncConfig.deviceId                       = 0;
    v1SyncConfig.mcuTriggerFrequency            = 0;

    auto owner          = getOwner();
    auto propertyServer = owner->getPropertyServer();
    propertyServer->setStructureDataT<OBDeviceSyncConfig>(OB_STRUCT_MULTI_DEVICE_SYNC_CONFIG, v1SyncConfig);
    if(propertyServer->isPropertySupported(OB_PROP_SYNC_SIGNAL_TRIGGER_OUT_BOOL, PROP_OP_WRITE, PROP_ACCESS_INTERNAL)) {
        propertyServer->setPropertyValueT<bool>(OB_PROP_SYNC_SIGNAL_TRIGGER_OUT_BOOL, v2SyncConfig.triggerOutEnable);
    }
    if(propertyServer->isPropertySupported(OB_PROP_CAPTURE_IMAGE_FRAME_NUMBER_INT, PROP_OP_WRITE, PROP_ACCESS_INTERNAL)) {
        propertyServer->setPropertyValueT<int>(OB_PROP_CAPTURE_IMAGE_FRAME_NUMBER_INT, v2SyncConfig.framesPerTrigger);
    }

    currentMultiDevSyncConfig_ = v2SyncConfig;
    isSyncConfigInit_          = true;
}

uint16_t DeviceSyncConfiguratorOldProtocol::getSupportedSyncModeBitmap() {
    uint16_t supportedSyncModeBitmap = 0;
    for(auto &mode: supportedSyncModes_) {
        supportedSyncModeBitmap |= mode;
    }
    return supportedSyncModeBitmap;
}

void DeviceSyncConfiguratorOldProtocol::triggerCapture() {
    auto owner          = getOwner();
    auto propertyServer = owner->getPropertyServer();
    propertyServer->setPropertyValueT(OB_PROP_CAPTURE_IMAGE_SIGNAL_BOOL, true);
}

void DeviceSyncConfiguratorOldProtocol::updateModeAliasMap(const std::map<OBSyncMode, OBMultiDeviceSyncMode> &oldToNewMap,
                                                           const std::map<OBMultiDeviceSyncMode, OBSyncMode> &newToOldMap) {
    syncModeOldToNewMap_ = oldToNewMap;
    syncModeNewToOldMap_ = newToOldMap;
}

void DeviceSyncConfiguratorOldProtocol::enableDepthDelaySupport(bool enable) {
    isDepthDelaySupported_ = enable;
}

}  // namespace libobsensor