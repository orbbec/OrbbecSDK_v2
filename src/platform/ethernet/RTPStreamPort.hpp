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
    RTPStreamPortInfo(std::string localAddress, std::string address, uint16_t port, uint16_t vendorPort, OBStreamType streamType, std::string mac = "unknown",
                      std::string serialNumber = "unknown", uint32_t pid = 0)
        : NetSourcePortInfo(SOURCE_PORT_NET_RTP, address, port, mac, serialNumber, pid),
          localAddress(localAddress),
          vendorPort(vendorPort),
          streamType(streamType) {}

    virtual bool equal(std::shared_ptr<const SourcePortInfo> cmpInfo) const override {
        if(cmpInfo->portType != portType) {
            return false;
        }
        auto netCmpInfo = std::dynamic_pointer_cast<const RTPStreamPortInfo>(cmpInfo);
        return (localAddress == netCmpInfo->localAddress) && (address == netCmpInfo->address) && (port == netCmpInfo->port) && (vendorPort == netCmpInfo->vendorPort)
               && (streamType == netCmpInfo->streamType);
    };

    std::string  localAddress;
    uint16_t     vendorPort;
    OBStreamType streamType;
};

class RTPStreamPort : public IVideoStreamPort, public IDataStreamPort {

public:
    RTPStreamPort(std::shared_ptr<const RTPStreamPortInfo> portInfo);
    virtual ~RTPStreamPort() noexcept;
    
    virtual StreamProfileList                     getStreamProfileList() override;
    virtual void                                  startStream(std::shared_ptr<const StreamProfile> profile, MutableFrameCallback callback) override;
    virtual void                                  stopStream(std::shared_ptr<const StreamProfile> profile) override;
    virtual void                                  stopAllStream() override;

    virtual void startStream(MutableFrameCallback callback) override;
    virtual void stopStream() override;

    virtual std::shared_ptr<const SourcePortInfo> getSourcePortInfo() const override;

private:
    void startClientTask(std::shared_ptr<const StreamProfile> profile, MutableFrameCallback callback);
    void closeClientTask();

private:
    std::shared_ptr<const RTPStreamPortInfo> portInfo_;
    bool                                     streamStarted_;
    std::shared_ptr<ObRTPClient>             rtpClient_;
    StreamProfileList                        streamProfileList_;
};
}