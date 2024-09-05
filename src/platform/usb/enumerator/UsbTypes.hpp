// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2020 Orbbec Corporation. All Rights Reserved.

#pragma once

#include "ISourcePort.hpp"

// #include <libusb.h>

#include <map>
#include <sstream>

#define ORBBEC_USB_VID 0x2BC5

namespace libobsensor {
typedef enum {
    OB_USB_CLASS_UNSPECIFIED          = 0x00,
    OB_USB_CLASS_AUDIO                = 0x01,
    OB_USB_CLASS_COM                  = 0x02,
    OB_USB_CLASS_HID                  = 0x03,
    OB_USB_CLASS_PID                  = 0x05,
    OB_USB_CLASS_IMAGE                = 0x06,
    OB_USB_CLASS_PRINTER              = 0x07,
    OB_USB_CLASS_MASS_STORAGE         = 0x08,
    OB_USB_CLASS_HUB                  = 0x09,
    OB_USB_CLASS_CDC_DATA             = 0x0A,
    OB_USB_CLASS_SMART_CARD           = 0x0B,
    OB_USB_CLASS_CONTENT_SECURITY     = 0x0D,
    OB_USB_CLASS_VIDEO                = 0x0E,
    OB_USB_CLASS_PHDC                 = 0x0F,
    OB_USB_CLASS_AV                   = 0x10,
    OB_USB_CLASS_BILLBOARD            = 0x11,
    OB_USB_CLASS_DIAGNOSTIC_DEVICE    = 0xDC,
    OB_USB_CLASS_WIRELESS_CONTROLLER  = 0xE0,
    OB_USB_CLASS_MISCELLANEOUS        = 0xEF,
    OB_USB_CLASS_APPLICATION_SPECIFIC = 0xFE,
    OB_USB_CLASS_VENDOR_SPECIFIC      = 0xFF
} UsbClass;

typedef enum { OB_USB_ENDPOINT_DIRECTION_WRITE = 0, OB_USB_ENDPOINT_DIRECTION_READ = 0x80 } EndpointDirection;

// Binary-coded decimal represent the USB specification to which the UVC device
// complies
typedef enum {
    usb_undefined = 0,
    usb1_type     = 0x0100,
    usb1_1_type   = 0x0110,
    usb2_type     = 0x0200,
    usb2_1_type   = 0x0210,
    usb3_type     = 0x0300,
    usb3_1_type   = 0x0310,
    usb3_2_type   = 0x0320,
    gmsl2_type    = 0x2000,
} UsbSpec;

std::string usbSpecToString(UsbSpec spec);

SourcePortType cvtUsbClassToPortType(uint8_t cls);

}  // namespace libobsensor