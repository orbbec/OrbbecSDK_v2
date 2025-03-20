// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "SocketTypes.hpp"
#include <string>
#include <mutex>

namespace libobsensor {

class VendorTCPClient {
public:
    VendorTCPClient(std::string localAddress, std::string localMac, std::string address, uint16_t port, uint32_t connectTimeout = 2000,
                    uint32_t commTimeout = 5000);
    ~VendorTCPClient() noexcept;

    int  read(uint8_t *data, const uint32_t dataLen);
    void write(const uint8_t *data, const uint32_t dataLen);

    void flush();

    void socketReconnect();

private:
    void checkLocalIP();
    void socketConnect();
    void socketClose();

private:
    bool              isValidIP_;
    std::string       localAddress_;
    const std::string localMac_;
    const std::string address_;
    const uint16_t    port_;
    SOCKET            socketFd_;
    bool              flushed_;
    uint32_t          CONNECT_TIMEOUT_MS = 2000;
    uint32_t          COMM_TIMEOUT_MS    = 2000;

    std::mutex tcpMtx_;
};

}  // namespace libobsensor

