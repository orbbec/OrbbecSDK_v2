// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include <string>
#include <sstream>
#include <vector>
#include <mutex>

namespace libobsensor {

struct UsbInterfaceInfo {
    std::string url;
    std::string uid;
    std::string hubId;
    uint16_t    vid = 0;
    uint16_t    pid = 0;
    std::string serial;
    uint16_t    conn_spec = 0;

    uint8_t     infIndex         = 0;
    uint8_t     infNameDescIndex = 0;  // iInterface (index of string description of interface)
    std::string infName;
    std::string infUrl;  // to distinguish between different pins of the same device
    uint8_t     cls = 0;

    operator std::string() {
        std::stringstream s;

        s << "vid- " << std::hex << vid << "\npid- " << std::hex << pid << "\ninfIndex- " << (uint32_t)infIndex << "\nusb specification- " << std::hex
          << (uint16_t)conn_spec << std::dec << "\nuid- " << uid;

        return s.str();
    }
};

inline bool operator==(const UsbInterfaceInfo &a, const UsbInterfaceInfo &b) {
    return (a.vid == b.vid) && (a.pid == b.pid) && (a.infIndex == b.infIndex) && (a.uid == b.uid) && (a.infUrl == b.infUrl);
}

class IUsbDevice {
public:
    virtual ~IUsbDevice() = default;
};

class IUsbEnumerator {
private:
    static std::weak_ptr<IUsbEnumerator> instanceWeakPtr_;
    static std::mutex                    instanceMutex_;

public:
    static std::shared_ptr<IUsbEnumerator> getInstance();

    virtual ~IUsbEnumerator() = default;

    virtual const std::vector<UsbInterfaceInfo> &queryUsbInterfaces()                     = 0;
    virtual std::shared_ptr<IUsbDevice>          openUsbDevice(const std::string &devUrl) = 0;
};

}  // namespace libobsensor
