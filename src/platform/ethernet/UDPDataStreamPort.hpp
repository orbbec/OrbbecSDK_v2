// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include <set>
#include <thread>
#include <mutex>
#include "ISourcePort.hpp"
#include "ethernet/socket/VendorUDPClient.hpp"
#include "NetDataStreamPort.hpp"

namespace libobsensor {

class UDPDataStreamPort : public IDataStreamPort {
public:
    UDPDataStreamPort(std::shared_ptr<const NetDataStreamPortInfo> portInfo);
    virtual ~UDPDataStreamPort() noexcept;
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

    std::shared_ptr<VendorUDPClient> udpClient_;
    std::thread                      readDataThread_;

    MutableFrameCallback callback_;
};

}  // namespace libobsensor
