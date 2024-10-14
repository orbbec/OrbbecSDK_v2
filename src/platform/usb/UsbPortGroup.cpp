// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "UsbPortGroup.hpp"

namespace libobsensor {
bool GroupUSBSourcePortBySN(const std::shared_ptr<const SourcePortInfo> &port0, const std::shared_ptr<const SourcePortInfo> &port1) {
    auto usbPort0 = std::dynamic_pointer_cast<const USBSourcePortInfo>(port0);
    auto usbPort1 = std::dynamic_pointer_cast<const USBSourcePortInfo>(port1);

    return usbPort0->serial == usbPort1->serial;
}

bool GroupUSBSourcePortByUID(const std::shared_ptr<const SourcePortInfo> &port0, const std::shared_ptr<const SourcePortInfo> &port1) {
    auto usbPort0 = std::dynamic_pointer_cast<const USBSourcePortInfo>(port0);
    auto usbPort1 = std::dynamic_pointer_cast<const USBSourcePortInfo>(port1);
    return usbPort0->uid == usbPort1->uid;
}

bool GroupUSBSourcePortByUrl(const std::shared_ptr<const SourcePortInfo> &port0, const std::shared_ptr<const SourcePortInfo> &port1) {
    auto usbPort0 = std::dynamic_pointer_cast<const USBSourcePortInfo>(port0);
    auto usbPort1 = std::dynamic_pointer_cast<const USBSourcePortInfo>(port1);
    return usbPort0->url == usbPort1->url;
}

}  // namespace libobsensor
