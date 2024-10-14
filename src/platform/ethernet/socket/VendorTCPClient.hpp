// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "SocketTypes.hpp"
#include <string>

namespace libobsensor {

class VendorTCPClient {
public:
    VendorTCPClient(std::string address, uint16_t port, uint32_t connectTimeout = 2000, uint32_t commTimeout = 5000);
    ~VendorTCPClient() noexcept;

    int  read(uint8_t *data, const uint32_t dataLen);
    void write(const uint8_t *data, const uint32_t dataLen);

    void flush();

private:
    void socketConnect();
    void socketClose();
    void socketReconnect();

private:
    const std::string address_;
    const uint16_t    port_;
    SOCKET            socketFd_;
    bool              flushed_;
    uint32_t          CONNECT_TIMEOUT_MS   = 2000;
    uint32_t          COMM_TIMEOUT_MS      = 5000;
};

}  // namespace libobsensor

