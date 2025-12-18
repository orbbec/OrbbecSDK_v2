// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#if (defined(WIN32) || defined(_WIN32) || defined(WINCE))
#define _WINSOCK_DEPRECATED_NO_WARNINGS disable
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#pragma comment(lib, "Iphlpapi.lib")
#else
#include <netdb.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <sys/time.h>

#if defined(__APPLE__)
#include <net/if_dl.h>
#endif

#endif

#include <thread>
#include <algorithm>
#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>
#include <future>
#include <vector>

#include "GVCPTransmit.hpp"
#include "exception/ObException.hpp"

#include "logger/LoggerInterval.hpp"
#include "logger/LoggerHelper.hpp"
#include "utils/StringUtils.hpp"
#include "utils/Utils.hpp"

#if defined(__ANDROID__) && (__ANDROID_API__ < 24)
#ifdef __cplusplus
extern "C" {
#endif
#include "ifaddrs_add.h"
#ifdef __cplusplus
}
#endif
#endif  // __ANDROID__ and API < 24

namespace libobsensor {

#if (defined(WIN32) || defined(_WIN32) || defined(WINCE))
#define IS_EINTR(err) ((err) == EINTR || (err) == WSAEINTR)
#else
#define IS_EINTR(err) ((err) == EINTR)
#endif

GVCPTransmit::GVCPTransmit(std::shared_ptr<const NetSourcePortInfo> portInfo, uint32_t timeoutMs, uint8_t retryCount)
    : timeoutMs_(timeoutMs), retryCount_(retryCount) {
    init(portInfo, timeoutMs);
}

GVCPTransmit::~GVCPTransmit() {
    deInit();
}

void GVCPTransmit::init(std::shared_ptr<const NetSourcePortInfo> portInfo, uint32_t timeoutMs) {
    int    rst;
    SOCKET sockFd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);  // ipv4, udp(Streaming)
    if(sockFd == INVALID_SOCKET) {
        auto err = GET_LAST_ERROR();
        throw libobsensor::io_exception(utils::string::to_string() << "create socket failed! ip=" << portInfo->address << ", err_code=" << err);
    }

    // timeout
#if (defined(WIN32) || defined(_WIN32) || defined(WINCE))
    uint32_t commTimeout = timeoutMs;
#else
    TIMEVAL commTimeout;
    commTimeout.tv_sec  = timeoutMs / 1000;
    commTimeout.tv_usec = (timeoutMs % 1000) * 1000;
#endif
    setsockopt(sockFd, SOL_SOCKET, SO_RCVTIMEO, (char *)&commTimeout, sizeof(commTimeout));  // Receive timeout limit
    // buffer size
    int nRecvBuf = 1 * 1024 * 1024;
    setsockopt(sockFd, SOL_SOCKET, SO_RCVBUF, (const char *)&nRecvBuf, sizeof(int));

    // bind to ip:port
    struct sockaddr_in localAddr{};
    localAddr.sin_family      = AF_INET;
    localAddr.sin_addr.s_addr = inet_addr(portInfo->localAddress.c_str());
    localAddr.sin_port        = htons(0);
    if(bind(sockFd, (sockaddr *)&localAddr, sizeof(localAddr)) < 0) {
        auto err = GET_LAST_ERROR();
        closesocket(sockFd);
        throw libobsensor::invalid_value_exception(utils::string::to_string() << "bind to " << portInfo->localAddress << " failed! err_code=" << err);
    }

    // set to blocking mode
    unsigned long mode = 0;  // blocking mode
    rst                = ioctlsocket(sockFd, FIONBIO, &mode);
    if(rst < 0) {
        auto err = GET_LAST_ERROR();
        closesocket(sockFd);
        throw libobsensor::invalid_value_exception(utils::string::to_string()
                                                   << "ioctlsocket to blocking mode failed! addr=" << portInfo->address << ", err_code=" << err);
    }
    // ok
    sock_      = sockFd;
    ipAddress_ = portInfo->address;
}

void GVCPTransmit::deInit() {
    if(sock_ >= 0) {
        closesocket(sock_);
        sock_ = INVALID_SOCKET;
    }
    ipAddress_ = "";
}

void GVCPTransmit::clearSocketReceiveBuffer() {
    char         buf[2048];
    utils::Timer timer;
    uint64_t     elapsed = 0;

    do {
        struct timeval timeout;
        timeout.tv_sec  = 0;
        timeout.tv_usec = 0;  // return immediately

        auto   sock = sock_;
        auto   nfds = static_cast<int>(sock) + 1;
        fd_set readfs;

        FD_ZERO(&readfs);
        FD_SET(sock, &readfs);
        auto res = select(nfds, &readfs, 0, 0, &timeout);
        if(res <= 0) {
            // no any data
            break;
        }

        // read data and discard
        res = recvfrom(sock, buf, sizeof(buf), 0, NULL, NULL);
        if(res > 0) {
            LOG_DEBUG("Discarding {} bytes of leftover data before sending. IP: {}", res, ipAddress_);
        }
        elapsed = timer.touchMs(false);
        // Loop until timeout to avoid blocking indefinitely
    } while(elapsed < 1000);
}

std::vector<char> GVCPTransmit::transmit(const void *data, int dataLength) {
    if(data == nullptr || dataLength < static_cast<int>(sizeof(gvcp_cmd_header))) {
        lastError_ = EINVAL;
        return {};
    }

    SOCKADDR_IN destAddr;
    destAddr.sin_family      = AF_INET;
    destAddr.sin_addr.s_addr = inet_addr(ipAddress_.c_str());
    destAddr.sin_port        = htons(GVCP_PORT);

    // Clear socket buffer to prevent processing stale ACKs
    clearSocketReceiveBuffer();

    int     res     = 0;
    uint8_t attempt = 0;
    // send command
    do {
        // send data
        res = sendto(sock_, (const char *)data, dataLength, 0, reinterpret_cast<SOCKADDR *>(&destAddr), sizeof(destAddr));
        if(res == SOCKET_ERROR) {
            auto err = GET_LAST_ERROR();
            if(IS_EINTR(err)) {
                // interrupted, try again
                LOG_DEBUG("sendto interrupted, try again. ip: {}", ipAddress_.c_str());
                continue;
            }
            lastError_ = GET_LAST_ERROR();
            LOG_DEBUG("Failed to send GVCP commad! ip: {}, error: {}", ipAddress_.c_str(), lastError_);
            return {};
        }
        // receive ack
        struct timeval timeout;
        timeout.tv_sec  = timeoutMs_ / 1000;
        timeout.tv_usec = (timeoutMs_ % 1000) * 1000;

        fd_set readfs;
        FD_ZERO(&readfs);
        FD_SET(sock_, &readfs);
        auto nfds = static_cast<int>(sock_) + 1;
        res       = select(nfds, &readfs, 0, 0, &timeout);
        if(res == SOCKET_ERROR) {
            auto err = GET_LAST_ERROR();
            if(IS_EINTR(err)) {
                // interrupted, try again
                LOG_DEBUG("select interrupted, try again. ip: {}", ipAddress_.c_str());
                continue;
            }
            // other error
            lastError_ = err;
            LOG_DEBUG("Failed to receive ack! Current count: {}, ip: {}, error: {}", attempt, ipAddress_.c_str(), err);
            return {};
        }
        else if(res == 0) {
            // timeout
            lastError_ = ETIMEDOUT;
            ++attempt;
            continue;
        }

        if(FD_ISSET(sock_, &readfs)) {
            std::vector<char> buffer(GVCP_MAX_PACK_SIZE);
            SOCKADDR_IN       senderAddr{};
            socklen_t         senderLen = sizeof(senderAddr);
            int               err;

            do {
                res = recvfrom(sock_, buffer.data(), static_cast<int>(buffer.size()), 0, (struct sockaddr *)&senderAddr, &senderLen);
                err = GET_LAST_ERROR();
            } while(res == SOCKET_ERROR && IS_EINTR(err));

            if(res == SOCKET_ERROR) {
                lastError_ = err;
                LOG_DEBUG("recvfrom failed! Current count: {}, ip: {}, error: {}", attempt, ipAddress_.c_str(), err);
                return {};
            }
            // check response data
            if(res < static_cast<int>(sizeof(gvcp_ack_header))) {
                LOG_DEBUG("GVCP ack length error! Expected length >= {} but {}. Current count: {}, ip: {}", sizeof(gvcp_ack_header), res, attempt,
                          ipAddress_.c_str());
                lastError_ = EINVAL;
                return {};
            }
            // ok
            buffer.resize(res);
            lastError_ = 0;
            return buffer;
        }
        else {
            // error, socket is not ready, try again
            lastError_ = ETIMEDOUT;
            ++attempt;
            continue;
        }
    } while(attempt <= retryCount_);

    // timeout
    lastError_ = ETIMEDOUT;
    return {};
}

std::pair<uint16_t, uint32_t> GVCPTransmit::readRegister(uint32_t registerAddress) {
    gvcp_readreg_cmd cmd{};
    auto             reqId = getReqId();
    cmd.header.cMsgKeyCode = GVCP_KEY_CODE;
    cmd.header.cFlag       = 0x01;  // need acknowledge
    cmd.header.wCmd        = htons(GVCP_READREG_CMD);
    cmd.header.wLen        = htons(0x04);  // len for reading a register
    cmd.header.wReqID      = htons(reqId);
    cmd.address            = htonl(registerAddress);

    // send data
    auto respData = transmit(reinterpret_cast<const void *>(&cmd), sizeof(gvcp_readreg_cmd));
    // check response data
    if(respData.empty()) {
        return { GEV_STATUS_ERROR, 0 };
    }
    if(respData.size() != sizeof(gvcp_readreg_ack)) {
        LOG_DEBUG("Read register error! Expected length is {} but {}. IP: {}, register: {:#04x}", sizeof(gvcp_readreg_ack), respData.size(), ipAddress_.c_str(),
                  registerAddress);
        return { GEV_STATUS_ERROR, 0 };
    }

    auto ack            = reinterpret_cast<gvcp_readreg_ack *>(respData.data());
    ack->header.wStatus = ntohs(ack->header.wStatus);
    ack->header.wAck    = ntohs(ack->header.wAck);
    ack->header.wLen    = ntohs(ack->header.wLen);
    ack->header.wReqID  = ntohs(ack->header.wReqID);
    if(ack->header.wAck != GVCP_READREG_ACK || ack->header.wReqID != reqId || ack->header.wLen != 0x04) {
        LOG_ERROR("Invalid ack header for readreg cmd! status: {:#04x}, ack: {:#04x}, len: {:#04x}, ack reqId: {:#04x}, send reqId: {:#04x}. IP: {}, "
                  "register: {}",
                  ack->header.wStatus, ack->header.wAck, ack->header.wLen, ack->header.wReqID, reqId, ipAddress_.c_str(), registerAddress);
        return { GEV_STATUS_ERROR, 0 };
    }

    // ok
    return { ack->header.wStatus, ntohl(ack->value) };
}

uint16_t GVCPTransmit::writeRegister(uint32_t registerAddress, uint32_t value) {
    gvcp_writereg_cmd cmd{};
    auto              reqId = getReqId();
    cmd.header.cMsgKeyCode  = GVCP_KEY_CODE;
    cmd.header.cFlag        = 0x01;  // need acknowledge
    cmd.header.wCmd         = htons(GVCP_WRITEREG_CMD);
    cmd.header.wLen         = htons(0x08);  // len for writing a register
    cmd.header.wReqID       = htons(reqId);
    cmd.address             = htonl(registerAddress);
    cmd.value               = htonl(value);

    // send data
    auto respData = transmit(reinterpret_cast<const void *>(&cmd), sizeof(gvcp_writereg_cmd));
    // check response data
    if(respData.empty()) {
        return GEV_STATUS_ERROR;
    }
    if(respData.size() != sizeof(gvcp_writereg_ack)) {
        LOG_DEBUG("Write register error! Expected length is {} but {}. IP: {}, register: {:#04x}", sizeof(gvcp_writereg_ack), respData.size(),
                  ipAddress_.c_str(), registerAddress);
        return GEV_STATUS_ERROR;
    }

    auto ack            = reinterpret_cast<gvcp_writereg_ack *>(respData.data());
    ack->header.wStatus = ntohs(ack->header.wStatus);
    ack->header.wAck    = ntohs(ack->header.wAck);
    ack->header.wLen    = ntohs(ack->header.wLen);
    ack->header.wReqID  = ntohs(ack->header.wReqID);
    if(ack->header.wAck != GVCP_WRITEREG_ACK || ack->header.wReqID != reqId || ack->header.wLen != 0x04) {
        LOG_ERROR("Invalid ack header for writereg cmd! status: {:#04x}, ack: {:#04x}, len: {:#04x}, ack reqId: {:#04x}, send reqId: {:#04x}. IP: {}, "
                  "register: {:#04x}",
                  ack->header.wStatus, ack->header.wAck, ack->header.wLen, ack->header.wReqID, reqId, ipAddress_.c_str(), registerAddress);
        return GEV_STATUS_ERROR;
    }

    // ok
    return ack->header.wStatus;
}

}  // namespace libobsensor
