// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include <memory>
#include <vector>
#include <string>

#include "libobsensor/h/ObTypes.h"
#include "IFrame.hpp"
#include "IStreamProfile.hpp"
#include "IStreamer.hpp"

#include <functional>

namespace libobsensor {
enum SourcePortType {
    SOURCE_PORT_USB_VENDOR = 0x00,
    SOURCE_PORT_USB_UVC,
    SOURCE_PORT_USB_MULTI_UVC,
    SOURCE_PORT_USB_HID,
    SOURCE_PORT_NET_VENDOR = 0x10,
    SOURCE_PORT_NET_VENDOR_STREAM,
    SOURCE_PORT_NET_RTSP,
    SOURCE_PORT_NET_RTP,
    SOURCE_PORT_IPC_VENDOR,  // Inter-process communication port
    SOURCE_PORT_UNKNOWN = 0xff,
};

#define IS_USB_PORT(type) ((type) >= SOURCE_PORT_USB_VENDOR && (type) <= SOURCE_PORT_USB_HID)
#define IS_NET_PORT(type) ((type) >= SOURCE_PORT_NET_VENDOR && (type) <= SOURCE_PORT_NET_RTP)

struct SourcePortInfo {
    SourcePortInfo(SourcePortType portType) : portType(portType) {}
    virtual ~SourcePortInfo() noexcept                                      = default;
    virtual bool equal(std::shared_ptr<const SourcePortInfo> cmpInfo) const = 0;

    SourcePortType portType;
};

struct NetSourcePortInfo : public SourcePortInfo {
    NetSourcePortInfo(SourcePortType portType, std::string netInterfaceName, std::string localMac, std::string localAddress, std::string address, uint16_t port,
                      std::string mac, std::string serialNumber, uint32_t pid)
        : SourcePortInfo(portType),
          netInterfaceName(netInterfaceName),
          localMac(localMac),
          localAddress(localAddress),
          address(address),
          port(port),
          mac(mac),
          serialNumber(serialNumber),
          pid(pid) {}

    ~NetSourcePortInfo() noexcept override = default;

    bool equal(std::shared_ptr<const SourcePortInfo> cmpInfo) const override {
        if(cmpInfo->portType != portType) {
            return false;
        }
        auto netCmpInfo = std::dynamic_pointer_cast<const NetSourcePortInfo>(cmpInfo);
        return (netInterfaceName == netCmpInfo->netInterfaceName) && (localMac == netCmpInfo->localMac) && (localAddress == netCmpInfo->localAddress)
               && (address == netCmpInfo->address) && (port == netCmpInfo->port) && (mac == netCmpInfo->mac) && (serialNumber == netCmpInfo->serialNumber)
               && (pid == netCmpInfo->pid);
    }

    std::string netInterfaceName;
    std::string localMac;
    std::string localAddress;
    std::string address;
    uint16_t    port;
    std::string mac;
    std::string serialNumber;
    uint32_t    pid;
};

struct ShmStreamPortInfo : public SourcePortInfo {  // shared memory stream port
    ShmStreamPortInfo(SourcePortType portType, std::string shmName, int32_t blockSize, int32_t blockCount)
        : SourcePortInfo(portType), shmName(shmName), blockSize(blockSize), blockCount(blockCount) {}
    ~ShmStreamPortInfo() noexcept override = default;
    virtual bool equal(std::shared_ptr<const SourcePortInfo> cmpInfo) const override {
        if(cmpInfo->portType != portType) {
            return false;
        }
        auto netCmpInfo = std::dynamic_pointer_cast<const ShmStreamPortInfo>(cmpInfo);
        return (shmName == netCmpInfo->shmName) && (blockSize == netCmpInfo->blockSize) && (blockCount == netCmpInfo->blockCount);
    };

    std::string shmName;
    int32_t     blockSize;
    int32_t     blockCount;
};

struct USBSourcePortInfo : public SourcePortInfo {
    USBSourcePortInfo() : SourcePortInfo(SOURCE_PORT_USB_VENDOR) {};
    explicit USBSourcePortInfo(SourcePortType type) : SourcePortInfo(type) {}
    ~USBSourcePortInfo() noexcept override = default;

    bool equal(std::shared_ptr<const SourcePortInfo> cmpInfo) const override {
        if(cmpInfo->portType != portType) {
            return false;
        }
        auto netCmpInfo = std::dynamic_pointer_cast<const USBSourcePortInfo>(cmpInfo);
        return (url == netCmpInfo->url) && (vid == netCmpInfo->vid) && (pid == netCmpInfo->pid) && (infUrl == netCmpInfo->infUrl)
               && (infIndex == netCmpInfo->infIndex) && (infName == netCmpInfo->infName) && (hubId == netCmpInfo->hubId);
    };

    std::string url;  // usb device url
    std::string uid;
    uint16_t    vid = 0;  // usb device vid
    uint16_t    pid = 0;  // usb device pid
    std::string serial;   // usb device serial number
    std::string connSpec;

    std::string infUrl;        // interface url(interface uid)
    uint8_t     infIndex = 0;  // interface index
    std::string infName;       // interface name
    std::string hubId;         // hub id
};

typedef std::vector<std::shared_ptr<const SourcePortInfo>> SourcePortInfoList;

class ISourcePort {
public:
    virtual ~ISourcePort() noexcept = default;

    virtual std::shared_ptr<const SourcePortInfo> getSourcePortInfo() const = 0;
};

// for vendor command
class IVendorDataPort : virtual public ISourcePort {  // Virtual inheritance solves diamond inheritance problem
public:
    ~IVendorDataPort() noexcept override = default;

    virtual uint32_t sendAndReceive(const uint8_t *sendData, uint32_t sendLen, uint8_t *recvData, uint32_t exceptedRecvLen) = 0;
};

// for imu data stream
class IDataStreamPort : virtual public ISourcePort {  // Virtual inheritance solves diamond inheritance problem
public:
    ~IDataStreamPort() noexcept override = default;

    virtual void startStream(MutableFrameCallback callback) = 0;
    virtual void stopStream()                               = 0;
};

// for video data stream: depth, color, ir, etc.
class IVideoStreamPort : virtual public ISourcePort, public IStreamer {  // Virtual inheritance solves diamond inheritance problem
public:
    ~IVideoStreamPort() noexcept override = default;

    virtual StreamProfileList getStreamProfileList() = 0;
    virtual void              stopAllStream()        = 0;
};

}  // namespace libobsensor
