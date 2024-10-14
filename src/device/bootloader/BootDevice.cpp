// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "BootDevice.hpp"

#include "DevicePids.hpp"
#include "InternalTypes.hpp"

#include "Platform.hpp"
#include "utils/Utils.hpp"
#include "environment/EnvConfig.hpp"
#include "usb/uvc/UvcDevicePort.hpp"
#include "monitor/DeviceMonitor.hpp"
#include "firmwareupdater/FirmwareUpdater.hpp"
#include "property/PropertyServer.hpp"
#include "property/VendorPropertyAccessor.hpp"
#include "property/CommonPropertyAccessors.hpp"

#include <algorithm>

namespace libobsensor {

BootDevice::BootDevice(const std::shared_ptr<const IDeviceEnumInfo> &info) : DeviceBase(info) {
    init();
}

BootDevice::~BootDevice() noexcept {}

void BootDevice::init() {
    auto vendorPropertyAccessor = std::make_shared<LazySuperPropertyAccessor>([this]() {
        auto pal                    = Platform::getInstance();
        auto sourcePortInfoList     = enumInfo_->getSourcePortInfoList();
        auto sourcePortInfo         = sourcePortInfoList.front();  // assume only one source port
        auto port                   = pal->getSourcePort(sourcePortInfo);
        auto vendorPropertyAccessor = std::make_shared<VendorPropertyAccessor>(this, port);
        return vendorPropertyAccessor;
    });

    auto propertyServer = std::make_shared<PropertyServer>(this);
    propertyServer->registerProperty(OB_STRUCT_VERSION, "", "r", vendorPropertyAccessor);
    propertyServer->registerProperty(OB_STRUCT_ASIC_SERIAL_NUMBER, "r", "r", vendorPropertyAccessor);
    registerComponent(OB_DEV_COMPONENT_PROPERTY_SERVER, propertyServer, true);

    fetchDeviceInfo();

    registerComponent(OB_DEV_COMPONENT_DEVICE_MONITOR, [this]() {
        auto platform           = Platform::getInstance();
        auto sourcePortInfoList = enumInfo_->getSourcePortInfoList();
        auto sourcePortInfo     = sourcePortInfoList.front();  // assume only one source port
        auto port               = platform->getSourcePort(sourcePortInfo);
        auto devMonitor         = std::make_shared<DeviceMonitor>(this, port);
        return devMonitor;
    });

    registerComponent(OB_DEV_COMPONENT_FIRMWARE_UPDATER, [this]() {
        std::shared_ptr<FirmwareUpdater> firmwareUpdater;
        TRY_EXECUTE({ firmwareUpdater = std::make_shared<FirmwareUpdater>(this); })
        return firmwareUpdater;
    });
}

}  // namespace libobsensor
