// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "ISourcePort.hpp"
#include "usb/enumerator/IUsbEnumerator.hpp"
#include <mutex>
namespace libobsensor {

class VendorUsbDevicePort : public IVendorDataPort {
public:
    VendorUsbDevicePort(const std::shared_ptr<IUsbDevice> &usbDevice, std::shared_ptr<const USBSourcePortInfo> portInfo);
    ~VendorUsbDevicePort() noexcept override;

    uint32_t sendAndReceive(const uint8_t *sendData, uint32_t sendLen, uint8_t *recvData, uint32_t exceptedRecvLen) override;

    virtual bool readFromBulkEndPoint(std::vector<uint8_t> &data);  // bulk transfer
    virtual bool writeToBulkEndPoint(std::vector<uint8_t> &data);   // bulk transfer

    std::shared_ptr<const SourcePortInfo> getSourcePortInfo() const override;

#ifdef __ANDROID__
    virtual std::string getUsbConnectType();
#endif

protected:
    std::mutex                               mutex_;
    std::shared_ptr<const USBSourcePortInfo> portInfo_;
    std::shared_ptr<IUsbDevice>              usbDev_;
};

}  // namespace libobsensor

