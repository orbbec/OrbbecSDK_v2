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
    NetDataStreamPortInfo(const NetSourcePortInfo &portInfo, uint16_t newPort, uint16_t vendorPort) : NetSourcePortInfo(portInfo), vendorPort(vendorPort) {
        port     = newPort;
        portType = SOURCE_PORT_NET_VENDOR_STREAM;
    }

    virtual bool equal(std::shared_ptr<const SourcePortInfo> cmpInfo) const override {
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
