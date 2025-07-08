// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "SocketTypes.hpp"
#include <string>

namespace libobsensor {

/**
 * @brief Communicate via UDP
 */
class VendorUDPClient {
public:
    VendorUDPClient(const std::string &address, uint16_t port, uint32_t commTimeout = 1000);
    virtual ~VendorUDPClient() noexcept;

    int  read(uint8_t *data, const uint32_t dataLen);
    void write(const uint8_t *data, const uint32_t dataLen);

    uint16_t getPort() const {
        return clientPort_;
    }

private:
    void socketConnect(uint32_t retryCount);
    void socketClose();
    void initOsSocket();
    void deinitOsSocket();

private:
    const std::string  address_;
    const uint16_t     port_;        // servert port
    uint16_t           clientPort_;  // client port
    SOCKET             socketFd_;
    struct sockaddr_in serverAddr_;
    const uint32_t     commTimeoutMs_  = 1000;
    const uint32_t     maxConnectRetry = 100;
};

}  // namespace libobsensor
