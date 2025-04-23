// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "ISourcePort.hpp"
#include "ethernet/socket/VendorTCPClient.hpp"
#include <mutex>

namespace libobsensor {

class VendorNetDataPort : public IVendorDataPort {
public:
    VendorNetDataPort(std::shared_ptr<const NetSourcePortInfo> portInfo);
    virtual ~VendorNetDataPort() noexcept override;

    std::shared_ptr<const SourcePortInfo> getSourcePortInfo() const override;
    uint32_t sendAndReceive(const uint8_t *sendData, uint32_t sendLen, uint8_t *recvData, uint32_t exceptedRecvLen) override;

private:
    std::mutex tcpMtx_;

    std::shared_ptr<VendorTCPClient>         tcpClient_;
    std::shared_ptr<const NetSourcePortInfo> portInfo_;
};

}  // namespace libobsensor
