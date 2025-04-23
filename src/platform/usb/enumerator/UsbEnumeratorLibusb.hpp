// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "UsbTypes.hpp"

#include <memory>
#include <vector>
#include <thread>
#include <mutex>

#include <libusb.h>
#include "IUsbEnumerator.hpp"

namespace libobsensor {

class UsbDeviceLibusb : public IUsbDevice {
public:
#ifndef __ANDROID__
    UsbDeviceLibusb(libusb_context *libusbCtx, std::shared_ptr<libusb_device_handle> handle);

    virtual ~UsbDeviceLibusb() noexcept override = default;
#else
    UsbDeviceLibusb(libusb_context *libusbCtx, std::shared_ptr<libusb_device_handle> handle, const std::string &devUrl);

    virtual ~UsbDeviceLibusb() noexcept override;
#endif

    libusb_device_handle      *getLibusbDeviceHandle() const;
    libusb_context            *getLibusbContext() const;
    libusb_endpoint_descriptor getEndpointDesc(int interfaceIndex, libusb_endpoint_transfer_type transferType, libusb_endpoint_direction direction) const;

private:
    libusb_context                       *libusbCtx_;
    std::shared_ptr<libusb_device_handle> handle_;

#ifdef __ANDROID__
    const std::string devUrl_;
#endif
};

#ifndef __ANDROID__
class UsbEnumeratorLibusb : public IUsbEnumerator {
public:
    UsbEnumeratorLibusb();
    virtual ~UsbEnumeratorLibusb() noexcept override;

    const std::vector<UsbInterfaceInfo> &queryUsbInterfaces() override;

    std::shared_ptr<IUsbDevice> openUsbDevice(const std::string &devUrl) override;

private:
    void                                  startEventHandleThread();
    void                                  stopEventHandleThread();
    std::shared_ptr<libusb_device_handle> openLibusbDevice(const std::string &devUrl);

private:
    std::vector<UsbInterfaceInfo> devInterfaceList_;

    libusb_context *libusbCtx_;
    std::thread     libusbEventHandlerThread_;
    int             libusbEventHandlerExit_ = 0;

    std::mutex                                                 libusbDeviceHandleMutex_;
    std::map<std::string, std::weak_ptr<libusb_device_handle>> libusbDeviceHandles_;
};
#endif
}  // namespace libobsensor
