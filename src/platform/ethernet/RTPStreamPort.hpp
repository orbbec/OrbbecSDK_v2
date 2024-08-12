#pragma once
#include "ISourcePort.hpp"
#include "ethernet/VendorNetDataPort.hpp"

#include <string>
#include <memory>
#include <thread>

namespace libobsensor {

struct RTPStreamPortInfo : public NetSourcePortInfo {
    RTPStreamPortInfo(std::string address, uint16_t port, uint16_t vendorPort, OBStreamType streamType, std::string mac = "unknown",
                      std::string serialNumber = "unknown", uint32_t pid = 0)
        : NetSourcePortInfo(SOURCE_PORT_NET_RTP, address, port, mac, serialNumber, pid), vendorPort(vendorPort), streamType(streamType) {}

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

class RTPStreamPort : public IVideoStreamPort {
public:
    RTPStreamPort(std::shared_ptr<const RTPStreamPortInfo> portInfo);
    virtual ~RTPStreamPort() noexcept;

    virtual StreamProfileList                     getStreamProfileList() override;
    virtual void                                  startStream(std::shared_ptr<const StreamProfile> profile, MutableFrameCallback callback) override;
    virtual void                                  stopStream(std::shared_ptr<const StreamProfile> profile) override;
    virtual void                                  stopAllStream() override;
    virtual std::shared_ptr<const SourcePortInfo> getSourcePortInfo() const override;

private:
    void stopStream();
    void createClient(std::shared_ptr<const StreamProfile> profile, MutableFrameCallback callback);
    void closeClient();

private:
    std::shared_ptr<const RTPStreamPortInfo> portInfo_;
    std::thread                              eventLoopThread_;
    char                                     destroy_;
    bool                                     streamStarted_;

    // ObRTPClient                        *currentRtspClient_;

    std::shared_ptr<const StreamProfile> currentStreamProfile_;
    StreamProfileList                    streamProfileList_;
};
}