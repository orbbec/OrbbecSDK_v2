#pragma once
#include "ISourcePort.hpp"
#include "ethernet/VendorNetDataPort.hpp"
#include "ethernet/rtp/ObRTPClient.hpp"

#include <string>
#include <memory>
#include <thread>
#include <iostream>

namespace libobsensor {

class RTPStreamPort : public IVideoStreamPort, public IDataStreamPort {
public:
    RTPStreamPort(std::shared_ptr<const RTPStreamPortInfo> portInfo);
    virtual ~RTPStreamPort() noexcept override;

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