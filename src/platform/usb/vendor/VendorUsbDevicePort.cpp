// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "VendorUsbDevicePort.hpp"

#include "logger/Logger.hpp"
#include <iomanip>
#include <sstream>
#include <algorithm>

#include "utils/Utils.hpp"
#include "exception/ObException.hpp"
#include "usb/enumerator/UsbEnumeratorLibusb.hpp"

namespace libobsensor {

VendorUsbDevicePort::VendorUsbDevicePort(const std::shared_ptr<IUsbDevice> &usbDevice, std::shared_ptr<const USBSourcePortInfo> portInfo)
    : portInfo_(portInfo), usbDev_(usbDevice) {}

VendorUsbDevicePort::~VendorUsbDevicePort() noexcept = default;

uint32_t VendorUsbDevicePort::sendAndReceive(const uint8_t *sendData, uint32_t sendLen, uint8_t *recvData, uint32_t exceptedRecvLen) {
    std::unique_lock<std::mutex> lock(mutex_);
    auto                         libusbDevice = std::dynamic_pointer_cast<UsbDeviceLibusb>(usbDev_);
    auto                         transferred  = libusb_control_transfer(libusbDevice->getLibusbDeviceHandle(),  //
                                                                        0x40, 0, 0, 0,                          //
                                                                        const_cast<uint8_t *>(sendData),        //
                                                                        static_cast<uint16_t>(sendLen),         //
                                                                        5000);
    if(transferred != static_cast<int>(sendLen)) {
        if(transferred >= 0) {
            LOG_ERROR("Failed to send data to device. Sent: {}, Transferred: {}", sendLen, transferred);
        }
        else {
            LOG_ERROR("Failed to send data to device. Error: {}", libusb_strerror(transferred));
        }
        return 0;
    }
    transferred = libusb_control_transfer(libusbDevice->getLibusbDeviceHandle(),   //
                                          0xc0, 0, 0, 0,                           //
                                          recvData,                                //
                                          static_cast<uint16_t>(exceptedRecvLen),  //
                                          5000);
    if(transferred < 0) {
        LOG_ERROR("Failed to receive data from device. Error: {}", libusb_strerror(transferred));
        return 0;
    }
    return transferred;
}

bool VendorUsbDevicePort::readFromBulkEndPoint(std::vector<uint8_t> &data) {
    utils::unusedVar(data);
    std::unique_lock<std::mutex> lock(mutex_);
    return false;
}

bool VendorUsbDevicePort::writeToBulkEndPoint(std::vector<uint8_t> &data) {
    utils::unusedVar(data);
    std::unique_lock<std::mutex> lock(mutex_);
    return false;
}

std::shared_ptr<const SourcePortInfo> VendorUsbDevicePort::getSourcePortInfo() const {
    return portInfo_;
}

#ifdef __ANDROID__
std::string VendorUsbDevicePort::getUsbConnectType() {
    auto                     libusbDev = std::static_pointer_cast<UsbDeviceLibusb>(usbDev_);
    libusb_device_descriptor desc;
    auto                     ret = libusb_get_device_descriptor(libusbDev->getDevice(), &desc);
    return usb_spec_names.find(UsbSpec(desc.bcdUSB))->second;
}
#endif

}  // namespace libobsensor
