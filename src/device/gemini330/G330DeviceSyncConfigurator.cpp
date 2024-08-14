#include "G330DeviceSyncConfigurator.hpp"
#include "exception/ObException.hpp"
#include "InternalTypes.hpp"

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

G330DeviceSyncConfigurator::G330DeviceSyncConfigurator(IDevice *owner, const std::vector<OBMultiDeviceSyncMode> &supportedSyncModes)
    : DeviceComponentBase(owner), supportedSyncModes_(supportedSyncModes), isSyncConfigInit_(false) {}

OBMultiDeviceSyncConfig G330DeviceSyncConfigurator::getSyncConfig() {
    if(isSyncConfigInit_) {
        return currentMultiDevSyncConfig_;
    }
    auto owner            = getOwner();
    auto propertyServer   = owner->getPropertyServer();
    auto configInternal   = propertyServer->getStructureDataProtoV1_1_T<OBMultiDeviceSyncConfigInternal, 1>(OB_STRUCT_MULTI_DEVICE_SYNC_CONFIG);

    currentMultiDevSyncConfig_.syncMode = (OBMultiDeviceSyncMode)configInternal.syncMode;
    if(currentMultiDevSyncConfig_.syncMode == OB_MULTI_DEVICE_SYNC_MODE_SECONDARY) {
        currentMultiDevSyncConfig_.syncMode = OB_MULTI_DEVICE_SYNC_MODE_SECONDARY_SYNCED;
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

void G330DeviceSyncConfigurator::setSyncConfig(const OBMultiDeviceSyncConfig &deviceSyncConfig) {
    if(isSyncConfigInit_ && 0 == memcmp(&currentMultiDevSyncConfig_, &deviceSyncConfig, sizeof(OBMultiDeviceSyncConfig))) {
        LOG_DEBUG("New sync config is same as current device sync config, the upgrade process would not execute!");
        return;
    }

    OBMultiDeviceSyncConfigInternal internalConfig;
    internalConfig.syncMode = deviceSyncConfig.syncMode;
    if(deviceSyncConfig.syncMode == 0) {
        internalConfig.syncMode = OB_MULTI_DEVICE_SYNC_MODE_FREE_RUN;
    }
    else if(deviceSyncConfig.syncMode == OB_MULTI_DEVICE_SYNC_MODE_SECONDARY_SYNCED) {
        internalConfig.syncMode = OB_MULTI_DEVICE_SYNC_MODE_SECONDARY;
    }
    internalConfig.depthDelayUs         = deviceSyncConfig.depthDelayUs;
    internalConfig.colorDelayUs         = deviceSyncConfig.colorDelayUs;
    internalConfig.trigger2ImageDelayUs = deviceSyncConfig.trigger2ImageDelayUs;
    internalConfig.triggerOutEnable     = deviceSyncConfig.triggerOutEnable;
    internalConfig.triggerOutDelayUs    = deviceSyncConfig.triggerOutDelayUs;
    internalConfig.framesPerTrigger     = deviceSyncConfig.framesPerTrigger;

    auto owner            = getOwner();
    auto propertyServer   = owner->getPropertyServer();
    propertyServer->setStructureDataProtoV1_1_T<OBMultiDeviceSyncConfigInternal, 1>(OB_STRUCT_MULTI_DEVICE_SYNC_CONFIG, internalConfig);

    currentMultiDevSyncConfig_ = deviceSyncConfig;
    isSyncConfigInit_          = true;
}

uint16_t G330DeviceSyncConfigurator::getSupportedSyncModeBitmap() {
    uint16_t supportedSyncModeBitmap = 0;
    for(auto &mode: supportedSyncModes_) {
        supportedSyncModeBitmap |= mode;
    }
    return supportedSyncModeBitmap;
}

void G330DeviceSyncConfigurator::triggerCapture() {
    auto owner            = getOwner();
    auto propertyServer   = owner->getPropertyServer();
    propertyServer->setPropertyValueT(OB_PROP_CAPTURE_IMAGE_SIGNAL_BOOL, true);
}

}  // namespace libobsensor