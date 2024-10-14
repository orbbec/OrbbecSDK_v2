// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "VendorNetDataPort.hpp"

#include "utils/Utils.hpp"
#include "logger/Logger.hpp"

namespace libobsensor {
#define OB_VENDOR_CMD_RECV_LEN 1024
VendorNetDataPort::VendorNetDataPort(std::shared_ptr<const NetSourcePortInfo> portInfo) : portInfo_(portInfo) {
    auto noConstPortInfo = std::const_pointer_cast<const NetSourcePortInfo>(portInfo);
    tcpClient_           = std::make_shared<VendorTCPClient>(noConstPortInfo->address, noConstPortInfo->port);
}

VendorNetDataPort::~VendorNetDataPort() noexcept {}

uint32_t VendorNetDataPort::sendAndReceive(const uint8_t *sendData, uint32_t sendLen, uint8_t *recvData, uint32_t exceptedRecvLen) {
    utils::unusedVar(exceptedRecvLen);
    tcpClient_->write(sendData, sendLen);
    int recvd = tcpClient_->read(recvData, OB_VENDOR_CMD_RECV_LEN);
    if(recvd < 0) {
        LOG_ERROR("Failed to read data from tcp client, error_code: {}", recvd);
        return 0;
    }
    return static_cast<uint32_t>(recvd);
}

std::shared_ptr<const SourcePortInfo> VendorNetDataPort::getSourcePortInfo() const {
    return portInfo_;
}

}  // namespace libobsensor

