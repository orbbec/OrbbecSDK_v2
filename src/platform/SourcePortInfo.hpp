// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

// TODO[port-info-refactor]:
// - Move USB-related SourcePortInfo to: usb/UsbSourcePortInfo.hpp
// - Move NET-related SourcePortInfo to: ethernet/NetSourcePortInfo.hpp
// - Route all subclass-specific access via SourcePortInfo (type-agnostic API), avoid concrete-type dependency.
#pragma once

#include "libobsensor/h/ObTypes.h"

#include <string>
#include <memory>
#include <vector>

enum UsbInterfaceFlag : uint64_t {
    USB_INF_FRAME_METADATA_PREPENDED_96B = 1ull << 0,  // 96-byte metadata embedded at the beginning of the frame data
};

namespace libobsensor {
enum SourcePortType {
    SOURCE_PORT_USB_VENDOR = 0x00,
    SOURCE_PORT_USB_UVC,
    SOURCE_PORT_USB_MULTI_UVC,
    SOURCE_PORT_USB_HID,
    SOURCE_PORT_NET_VENDOR = 0x10,
    SOURCE_PORT_NET_VENDOR_STREAM,
    SOURCE_PORT_NET_LIDAR_VENDOR_STREAM,  // LiDAR VENDOR stream
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
    NetSourcePortInfo(SourcePortType portType) : SourcePortInfo(portType) {}
    ~NetSourcePortInfo() noexcept override = default;

    bool equal(std::shared_ptr<const SourcePortInfo> cmpInfo) const override {
        if(cmpInfo->portType != portType) {
            return false;
        }
        auto netCmpInfo = std::dynamic_pointer_cast<const NetSourcePortInfo>(cmpInfo);
        return (netInterfaceName == netCmpInfo->netInterfaceName) && (localMac == netCmpInfo->localMac) && (localAddress == netCmpInfo->localAddress)
               && (address == netCmpInfo->address) && (port == netCmpInfo->port) && (mac == netCmpInfo->mac) && (serialNumber == netCmpInfo->serialNumber)
               && (pid == netCmpInfo->pid) && (vid == netCmpInfo->vid);
    }

    std::string netInterfaceName  = "unknown";
    std::string localMac          = "unknown";
    std::string localAddress      = "0.0.0.0";
    std::string address           = "0.0.0.0";
    uint16_t    port              = 0;
    std::string mac               = "unknown";
    std::string serialNumber      = "unknown";
    uint32_t    pid               = 0;
    uint32_t    vid               = 0;
    std::string mask              = "unknown";
    std::string gateway           = "unknown";
    uint8_t     localSubnetLength = 0;
    std::string localGateway      = "unknown";
    std::string devVersion        = "0.0.0.0";  // firmware or hardware version depending on device
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
    uint64_t    infFlag = 0;   // flag for usb interface. See UsbInterfaceFlag for detail
};

struct RTPStreamPortInfo : public NetSourcePortInfo {
    RTPStreamPortInfo(const NetSourcePortInfo &portInfo, uint16_t newPort, uint16_t vendorPort, OBStreamType streamType)
        : NetSourcePortInfo(portInfo), vendorPort(vendorPort), streamType(streamType) {
        port     = newPort;
        portType = SOURCE_PORT_NET_RTP;
    }

    virtual bool equal(std::shared_ptr<const SourcePortInfo> cmpInfo) const override {
        if(cmpInfo->portType != portType) {
            return false;
        }
        auto netCmpInfo = std::dynamic_pointer_cast<const RTPStreamPortInfo>(cmpInfo);
        return (address == netCmpInfo->address) && (port == netCmpInfo->port) && (vendorPort == netCmpInfo->vendorPort)
               && (streamType == netCmpInfo->streamType);
    };

    uint16_t     vendorPort;
    OBStreamType streamType;
};

struct RTSPStreamPortInfo : public NetSourcePortInfo {
    RTSPStreamPortInfo(const NetSourcePortInfo &portInfo, uint16_t newPort, uint16_t vendorPort, OBStreamType streamType)
        : NetSourcePortInfo(portInfo), vendorPort(vendorPort), streamType(streamType) {
        port     = newPort;
        portType = SOURCE_PORT_NET_RTSP;
    }

    virtual bool equal(std::shared_ptr<const SourcePortInfo> cmpInfo) const override {
        if(cmpInfo->portType != portType) {
            return false;
        }
        auto netCmpInfo = std::dynamic_pointer_cast<const RTSPStreamPortInfo>(cmpInfo);
        return (address == netCmpInfo->address) && (port == netCmpInfo->port) && (vendorPort == netCmpInfo->vendorPort)
               && (streamType == netCmpInfo->streamType);
    };

    uint16_t     vendorPort;
    OBStreamType streamType;
};

typedef std::vector<std::shared_ptr<const SourcePortInfo>> SourcePortInfoList;
}  // namespace libobsensor