#pragma once
#include "ISourcePort.hpp"
#include "ethernet/VendorNetDataPort.hpp"
#include "ethernet/rtp/ObRTPClient.hpp"

#include <string>
#include <memory>
#include <thread>
#include <iostream>

namespace libobsensor {

struct RTPStreamPortInfo : public NetSourcePortInfo {
    RTPStreamPortInfo(std::string netInterfaceName, std::string localMac, std::string localAddress, std::string address, uint16_t port, uint16_t vendorPort,
                      OBStreamType streamType, std::string mac = "unknown", std::string serialNumber = "unknown", uint32_t pid = 0)
        : NetSourcePortInfo(SOURCE_PORT_NET_RTP, netInterfaceName, localMac, localAddress, address, port, mac, serialNumber, pid),
          vendorPort(vendorPort),
          streamType(streamType) {}

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

class RTPStreamPort : public IVideoStreamPort, public IDataStreamPort {

public:
    RTPStreamPort(std::shared_ptr<const RTPStreamPortInfo> portInfo);
    virtual ~RTPStreamPort() noexcept;
    
    uint16_t getStreamPort();

    virtual StreamProfileList                     getStreamProfileList() override;
    virtual void                                  startStream(std::shared_ptr<const StreamProfile> profile, MutableFrameCallback callback) override;
    virtual void                                  stopStream(std::shared_ptr<const StreamProfile> profile) override;
    virtual void                                  stopAllStream() override;

    virtual void startStream(MutableFrameCallback callback) override;
    virtual void stopStream() override;

    virtual std::shared_ptr<const SourcePortInfo> getSourcePortInfo() const override;

private:
    void initRTPClient();

private:
    uint16_t                                 streamPort_;
    std::shared_ptr<const RTPStreamPortInfo> portInfo_;
    bool                                     streamStarted_;
    std::shared_ptr<ObRTPClient>             rtpClient_;
    StreamProfileList                        streamProfileList_;
};
}