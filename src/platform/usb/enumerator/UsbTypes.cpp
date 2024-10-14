// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "UsbTypes.hpp"
#include <libusb.h>
namespace libobsensor {

const std::map<UsbSpec, std::string> usb_spec_names = {
    { usb_undefined, "USB" },  { usb1_type, "USB1.0" }, { usb1_1_type, "USB1.1" }, { usb2_type, "USB2.0" },
    { usb2_1_type, "USB2.1" }, { usb3_type, "USB3.0" }, { usb3_1_type, "USB3.1" }, { usb3_2_type, "USB3.2" }
};
std::string usbSpecToString(UsbSpec spec) {
    if(spec == gmsl2_type) {
        return "GMSL2";
    }
    std::string name = "USB";
    auto        it   = usb_spec_names.find(spec);
    if(it != usb_spec_names.end()) {
        name = it->second;
    }
    return name;
}

SourcePortType cvtUsbClassToPortType(uint8_t cls) {
    switch(static_cast<libusb_class_code>(cls)) {
    case LIBUSB_CLASS_HID:
        return SOURCE_PORT_USB_HID;
    case LIBUSB_CLASS_VIDEO:
        return SOURCE_PORT_USB_UVC;
    case LIBUSB_CLASS_VENDOR_SPEC:
        return SOURCE_PORT_USB_VENDOR;
    default:
        break;
    }
    return SOURCE_PORT_UNKNOWN;
}

}  // namespace libobsensor

