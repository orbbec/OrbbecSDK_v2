// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "BootDevice.hpp"

#include "DevicePids.hpp"
#include "InternalTypes.hpp"

#include "utils/Utils.hpp"
#include "environment/EnvConfig.hpp"
#include "usb/uvc/UvcDevicePort.hpp"
#include "monitor/DeviceMonitor.hpp"
#include "firmwareupdater/FirmwareUpdater.hpp"
#include "property/PropertyServer.hpp"
#include "property/VendorPropertyAccessor.hpp"
#include "property/CommonPropertyAccessors.hpp"
#include "property/InternalProperty.hpp"

#include <algorithm>

namespace libobsensor {

BootDevice::BootDevice(const std::shared_ptr<const IDeviceEnumInfo> &info) : DeviceBase(info) {
    init();
}

BootDevice::~BootDevice() noexcept {}

void BootDevice::init() {
    auto vendorPropertyAccessor = std::make_shared<LazySuperPropertyAccessor>([this]() {
        auto sourcePortInfoList     = enumInfo_->getSourcePortInfoList();
        auto sourcePortInfo         = sourcePortInfoList.front();  // assume only one source port
        auto port                   = getSourcePort(sourcePortInfo);
        auto vendorPropertyAccessor = std::make_shared<VendorPropertyAccessor>(this, port);
        return vendorPropertyAccessor;
    });

    auto propertyServer = std::make_shared<PropertyServer>(this);
    propertyServer->registerProperty(OB_STRUCT_VERSION, "", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_STRUCT_ASIC_SERIAL_NUMBER, "r", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_PROP_REBOOT_DEVICE_BOOL, "", "w", vendorPropertyAccessor);
    registerComponent(OB_DEV_COMPONENT_PROPERTY_SERVER, propertyServer, true);

    fetchDeviceInfo();

    registerComponent(OB_DEV_COMPONENT_DEVICE_MONITOR, [this]() {
        auto sourcePortInfoList = enumInfo_->getSourcePortInfoList();
        auto sourcePortInfo     = sourcePortInfoList.front();  // assume only one source port
        auto port               = getSourcePort(sourcePortInfo);
        auto devMonitor         = std::make_shared<DeviceMonitor>(this, port);
        return devMonitor;
    });

    registerComponent(OB_DEV_COMPONENT_FIRMWARE_UPDATER, [this]() {
        std::shared_ptr<FirmwareUpdater> firmwareUpdater;
        TRY_EXECUTE({ firmwareUpdater = std::make_shared<FirmwareUpdater>(this); })
        return firmwareUpdater;
    });
}

void BootDevice::fetchDeviceInfo() {

    auto propServer                   = getPropertyServer();
    auto version                      = propServer->getStructureDataT<OBVersionInfo>(OB_STRUCT_VERSION);
    if(enumInfo_->getConnectionType() == "Ethernet") {
        auto deviceInfo                   = std::make_shared<NetDeviceInfo>();
        auto portInfo                     = enumInfo_->getSourcePortInfoList().front();
        auto netPortInfo                  = std::dynamic_pointer_cast<const NetSourcePortInfo>(portInfo);
        deviceInfo->ipAddress_            = netPortInfo->address;
        deviceInfo_                       = deviceInfo;
        deviceInfo_->name_                = version.deviceName;
        deviceInfo_->fwVersion_           = version.firmwareVersion;
        deviceInfo_->deviceSn_            = version.serialNumber;
        deviceInfo_->asicName_            = version.depthChip;
        deviceInfo_->hwVersion_           = version.hardwareVersion;
        deviceInfo_->type_                = static_cast<uint16_t>(version.deviceType);
        deviceInfo_->supportedSdkVersion_ = version.sdkVersion;
        deviceInfo_->pid_                 = enumInfo_->getPid();
        deviceInfo_->vid_                 = enumInfo_->getVid();
        deviceInfo_->uid_                 = enumInfo_->getUid();
        deviceInfo_->connectionType_      = enumInfo_->getConnectionType();
    }
    else {
        deviceInfo_                       = std::make_shared<DeviceInfo>();
        deviceInfo_->name_                = version.deviceName;
        deviceInfo_->fwVersion_           = version.firmwareVersion;
        deviceInfo_->deviceSn_            = version.serialNumber;
        deviceInfo_->asicName_            = version.depthChip;
        deviceInfo_->hwVersion_           = version.hardwareVersion;
        deviceInfo_->type_                = static_cast<uint16_t>(version.deviceType);
        deviceInfo_->supportedSdkVersion_ = version.sdkVersion;
        deviceInfo_->pid_                 = enumInfo_->getPid();
        deviceInfo_->vid_                 = enumInfo_->getVid();
        deviceInfo_->uid_                 = enumInfo_->getUid();
        deviceInfo_->connectionType_      = enumInfo_->getConnectionType();
    }


    // remove the prefix "Orbbec " from the device name if contained
    if(deviceInfo_->name_.find("Orbbec ") == 0) {
        deviceInfo_->name_ = deviceInfo_->name_.substr(7);
    }
    deviceInfo_->fullName_ = "Orbbec " + deviceInfo_->name_;

    // mark the device as a multi-sensor device with same clock at default
    extensionInfo_["AllSensorsUsingSameClock"] = "true";

}


}  // namespace libobsensor
