// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "IPal.hpp"
#include "logger/Logger.hpp"
#include "exception/ObException.hpp"
#include <iostream>
#include <vector>
#include "logger/Logger.hpp"
#include "exception/ObException.hpp"

#if defined(BUILD_USB_PAL)
#include "usb/enumerator/Enumerator.hpp"
#include <libusb.h>
#endif
namespace libobsensor {

class MacUsbPal : public IPal {
public:
    MacUsbPal();
    ~MacUsbPal() noexcept;

    virtual std::shared_ptr<ISourcePort> getSourcePort(std::shared_ptr<const SourcePortInfo> portInfo) override;

#if defined(BUILD_USB_PAL)
public:
    virtual std::shared_ptr<IDeviceWatcher> createDeviceWatcher() const override;
    virtual SourcePortInfoList              querySourcePortInfos() override;
    virtual std::shared_ptr<ISourcePort>    createOpenNIDevicePort(std::shared_ptr<const SourcePortInfo>) override;
    virtual std::shared_ptr<ISourcePort>    createMultiUvcDevicePort(std::shared_ptr<const SourcePortInfo> portInfo) override;
    virtual std::shared_ptr<ISourcePort>    createRawPhaseConverterDevicePort(RawPhaseConverterPortType type, std::shared_ptr<const SourcePortInfo>) override;

private:
    std::shared_ptr<UsbEnumerator> usbEnumerator_;
#endif

private:
    std::mutex                                                                  sourcePortMapMutex_;
    std::map<std::shared_ptr<const SourcePortInfo>, std::weak_ptr<ISourcePort>> sourcePortMap_;
};

#if defined(BUILD_USB_PAL)
int deviceArrivalCallback(libusb_context *ctx, libusb_device *device, libusb_hotplug_event event, void *user_data);
int deviceRemovedCallback(libusb_context *ctx, libusb_device *device, libusb_hotplug_event event, void *user_data);
class LibusbDeviceWatcher : public IDeviceWatcher {
public:
    LibusbDeviceWatcher() {
        // libusb_init(); // Creating MacUsbPal will initialize
    }
    ~LibusbDeviceWatcher() noexcept {
        TRY_EXECUTE(stop());
        // libusb_exit();
    }
    virtual void start(deviceChangedCallback callback) override {
        callback_ = callback;
        auto rc   = libusb_hotplug_register_callback(NULL, LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED, 0, 0x2BC5, LIBUSB_HOTPLUG_MATCH_ANY, LIBUSB_HOTPLUG_MATCH_ANY,
                                                     deviceArrivalCallback, this, &hp[0]);
        if(LIBUSB_SUCCESS != rc) {
            LOG_WARN("register libusb hotplug failed!");
        }
        rc = libusb_hotplug_register_callback(NULL, LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT, 0, 0x2BC5, LIBUSB_HOTPLUG_MATCH_ANY, LIBUSB_HOTPLUG_MATCH_ANY,
                                              deviceRemovedCallback, this, &hp[1]);
        if(LIBUSB_SUCCESS != rc) {
            LOG_WARN("register libusb hotplug failed!");
        }
    }
    virtual void stop() override {
        libusb_hotplug_deregister_callback(NULL, hp[0]);
        libusb_hotplug_deregister_callback(NULL, hp[1]);
    }

    static bool hasCapability() {
        return libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG);
    }

private:
    libusb_hotplug_callback_handle hp[2];
    deviceChangedCallback          callback_;

    friend int deviceArrivalCallback(libusb_context *ctx, libusb_device *device, libusb_hotplug_event event, void *user_data);
    friend int deviceRemovedCallback(libusb_context *ctx, libusb_device *device, libusb_hotplug_event event, void *user_data);
};
#endif

}  // namespace libobsensor

