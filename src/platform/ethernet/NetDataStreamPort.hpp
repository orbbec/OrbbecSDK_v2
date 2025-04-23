// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "ISourcePort.hpp"
#include "ethernet/socket/VendorTCPClient.hpp"
#include <set>
#include <thread>
#include <mutex>

namespace libobsensor {

struct NetDataStreamPortInfo : public NetSourcePortInfo {
    NetDataStreamPortInfo(std::string netInterfaceName, std::string localMac, std::string localAddress, std::string address, uint16_t port, uint16_t vendorPort,
                          std::string mac = "unknown", std::string serialNumber = "unknown", uint32_t pid = 0)
        : NetSourcePortInfo(SOURCE_PORT_NET_VENDOR_STREAM, netInterfaceName, localMac, localAddress, address, port, mac, serialNumber, pid),
          vendorPort(vendorPort) {}

    virtual bool equal(std::shared_ptr<const SourcePortInfo> cmpInfo) const {
        if(cmpInfo->portType != portType) {
            return false;
        }
        auto netCmpInfo = std::dynamic_pointer_cast<const NetDataStreamPortInfo>(cmpInfo);
        return (address == netCmpInfo->address) && (port == netCmpInfo->port) && (vendorPort == netCmpInfo->vendorPort);
    };

    uint16_t vendorPort;
};

class NetDataStreamPort : public IDataStreamPort {
public:
    NetDataStreamPort(std::shared_ptr<const NetDataStreamPortInfo> portInfo);
    virtual ~NetDataStreamPort() noexcept override;
    std::shared_ptr<const SourcePortInfo> getSourcePortInfo() const override {
        return portInfo_;
    }

    void startStream(MutableFrameCallback callback) override;
    void stopStream() override;

public:
    void readData();

private:
    std::shared_ptr<const NetDataStreamPortInfo> portInfo_;
    bool                                         isStreaming_;

    std::shared_ptr<VendorTCPClient> tcpClient_;
    std::thread                      readDataThread_;

    MutableFrameCallback callback_;
};

}  // namespace libobsensor
