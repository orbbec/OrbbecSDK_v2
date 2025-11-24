// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include <set>
#include <thread>
#include <mutex>
#include "ISourcePort.hpp"
#include "ethernet/socket/VendorUDPClient.hpp"
#include "NetDataStreamPort.hpp"
#include "common/CommonFields.hpp"

namespace libobsensor {

struct LiDARDataStreamPortInfo : public NetSourcePortInfo {
    LiDARDataStreamPortInfo(const NetSourcePortInfo &portInfo, uint16_t newPort, uint16_t vendorPort, OBStreamType streamType)
        : NetSourcePortInfo(portInfo), vendorPort(vendorPort), streamType(streamType) {
        port     = newPort;
        portType = SOURCE_PORT_NET_LIDAR_VENDOR_STREAM;
    }

    virtual bool equal(std::shared_ptr<const SourcePortInfo> cmpInfo) const override {
        if(!NetSourcePortInfo::equal(cmpInfo)) {
            return false;
        }
        auto netCmpInfo = std::dynamic_pointer_cast<const LiDARDataStreamPortInfo>(cmpInfo);
        return (vendorPort == netCmpInfo->vendorPort) && (streamType == netCmpInfo->streamType);
    };

    uint16_t     vendorPort;
    OBStreamType streamType;
};

class LiDARDataStreamPort : public IDataStreamPort {
public:
    LiDARDataStreamPort(std::shared_ptr<const LiDARDataStreamPortInfo> portInfo);
    virtual ~LiDARDataStreamPort() noexcept override;

    std::shared_ptr<const SourcePortInfo> getSourcePortInfo() const override {
        return portInfo_;
    }

    uint16_t getSocketPort() const {
        if(udpClient_) {
            return udpClient_->getPort();
        }
        return 0;
    }

    void startStream(MutableFrameCallback callback) override;
    void stopStream() override;

public:
    void stop();
    void readData();

private:
    std::shared_ptr<const LiDARDataStreamPortInfo> portInfo_;
    std::atomic<bool>                              isStreaming_;
    std::shared_ptr<VendorUDPClient>               udpClient_;
    std::thread                                    readDataThread_;
    MutableFrameCallback                           callback_;
};

}  // namespace libobsensor
