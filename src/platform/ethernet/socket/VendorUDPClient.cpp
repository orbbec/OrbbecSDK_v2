// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "VendorUDPClient.hpp"
#include "logger/Logger.hpp"
#include "logger/LoggerInterval.hpp"
#include "utils/Utils.hpp"
#include "exception/ObException.hpp"

#include <thread>

namespace libobsensor {

VendorUDPClient::VendorUDPClient(const std::string &address, uint16_t port, uint32_t commTimeout)
    : address_(address), port_(port), clientPort_(port), socketFd_(INVALID_SOCKET), commTimeoutMs_(commTimeout) {
    // os socket
    initOsSocket();
    // try to connect
    BEGIN_TRY_EXECUTE({ socketConnect(0); })
    CATCH_EXCEPTION_AND_EXECUTE({
        deinitOsSocket();
        throw;
    })
}

VendorUDPClient::~VendorUDPClient() noexcept {
    socketClose();
    deinitOsSocket();
}

void VendorUDPClient::initOsSocket() {
#if(defined(WIN32) || defined(_WIN32) || defined(WINCE))
    WSADATA wsaData;
    int     rst = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if(rst != 0) {
        throw libobsensor::invalid_value_exception(utils::string::to_string() << "Failed to load Winsock! err_code=" << GET_LAST_ERROR());
    }
#endif

    // Due to network configuration changes causing socket connection pipeline errors, macOS throws a SIGPIPE exception to the application, leading to a crash
    // (this does not occur on Linux). This exception needs to be filtered out.
#if(defined(OS_IOS) || defined(OS_MACOS))
    signal(SIGPIPE, SIG_IGN);
#endif
}

void VendorUDPClient::deinitOsSocket() {
#if(defined(WIN32) || defined(_WIN32) || defined(WINCE))
    WSACleanup();
#endif
}

void VendorUDPClient::socketConnect(uint32_t retryCount) {
    int    rst;
    int    errCode = 0;
    SOCKET sockFd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);  // ipv4, udp(Streaming)
    if(sockFd == INVALID_SOCKET) {
        throw libobsensor::io_exception(utils::string::to_string() << "create socket failed! err_code=" << GET_LAST_ERROR());
    }

    // timeout
#if(defined(WIN32) || defined(_WIN32) || defined(WINCE))
    uint32_t commTimeout = commTimeoutMs_;
#else
    TIMEVAL commTimeout;
    commTimeout.tv_sec  = commTimeoutMs_ / 1000;
    commTimeout.tv_usec = commTimeoutMs_ % 1000 * 1000;
#endif
    setsockopt(sockFd, SOL_SOCKET, SO_RCVTIMEO, (char *)&commTimeout, sizeof(commTimeout));  // Receive timeout limit
    // buffer size
    int nRecvBuf = 128 * 1024 * 1024;
    setsockopt(sockFd, SOL_SOCKET, SO_RCVBUF, (const char *)&nRecvBuf, sizeof(int));

    // get server addr
    serverAddr_.sin_family = AF_INET;                                       // ipv4
    serverAddr_.sin_port   = htons(port_);                                  // convert uint16_t from host to network byte sequence
    if(inet_pton(AF_INET, address_.c_str(), &serverAddr_.sin_addr) <= 0) {  // address string to sin_addr
        closesocket(sockFd);
        throw libobsensor::invalid_value_exception("Invalid address!");
    }

    struct sockaddr_in localAddr{};
    localAddr.sin_family      = AF_INET;
    localAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    localAddr.sin_port        = htons(clientPort_);
    if(bind(sockFd, (sockaddr *)&localAddr, sizeof(localAddr)) < 0) {
        errCode = GET_LAST_ERROR();
        closesocket(sockFd);
        if(errCode == ERR_ADDR_IN_USE && retryCount < maxConnectRetry) {
            // port is in use, try next port
            ++clientPort_;
            socketConnect(retryCount+1);
            return;
        }
        else {
            throw libobsensor::invalid_value_exception(utils::string::to_string() << "VendorUDPClient: bind to 0.0.0.0 failed! addr=" << address_
                                                                                  << ", port=" << clientPort_ << ", err_code=" << errCode);
        }
    }

    // set to blocking mode
    unsigned long mode = 0;  // blocking mode
    rst                = ioctlsocket(sockFd, FIONBIO, &mode);
    if(rst < 0) {
        errCode = GET_LAST_ERROR();
        closesocket(sockFd);
        throw libobsensor::invalid_value_exception(utils::string::to_string() << "VendorUDPClient: ioctlsocket to blocking mode failed! addr=" << address_
                                                                              << ", port=" << clientPort_ << ", err_code=" << errCode);
    }
    // ok
    socketFd_ = sockFd;
    LOG_DEBUG("UDP client socket created!, addr={0}, server port={1}, client port={2}, socket={3}", address_, port_, clientPort_, sockFd);
}

void VendorUDPClient::socketClose() {
    if(socketFd_ > 0) {
        auto rst = closesocket(socketFd_);
        if(rst < 0) {
            LOG_ERROR("close socket failed! socket={0}, err_code={1}", socketFd_, GET_LAST_ERROR());
        }
    }
    LOG_DEBUG("UDP client socket closed! socket={}", socketFd_);
    socketFd_ = INVALID_SOCKET;
}

int VendorUDPClient::read(uint8_t *data, const uint32_t dataLen) {
    uint8_t retry = 2;

    while(retry--) {
        struct sockaddr_in addr;
        socklen_t          addrSize = sizeof(addr);
        int                rst      = recvfrom(socketFd_, (char *)data, dataLen, 0, (struct sockaddr *)&addr, &addrSize);

        if(rst < 0) {
            rst = GET_LAST_ERROR();
#if(defined(WIN32) || defined(_WIN32) || defined(WINCE))
            if(rst == WSAETIMEDOUT || rst == WSAEWOULDBLOCK) {
#else
            if(rst == EAGAIN || rst == EWOULDBLOCK) {
#endif
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
            else {
                LOG_ERROR_INTVL(utils::string::to_string() << "VendorUDPClient read data failed! socket=" << socketFd_ << ", err_code=" << rst);
                continue;
            }
        }
        else if(rst == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        else {
            // check sender ip address
            char sender_ip[INET_ADDRSTRLEN] = { 0 };
            inet_ntop(AF_INET, &addr.sin_addr, sender_ip, sizeof(sender_ip));
            if(address_ != std::string(sender_ip)) {
                // sender's ip address is error, ignore
                ++retry;
                continue;
            }
            return rst;
        }
    }

    return -1;
}

void VendorUDPClient::write(const uint8_t *data, const uint32_t dataLen) {
    uint8_t retry = 2;
    int     rst   = -1;

    while(retry--) {
        rst = sendto(socketFd_, (const char *)data, dataLen, 0, (struct sockaddr *)&serverAddr_, sizeof(serverAddr_));
        if(rst < 0) {
            rst = GET_LAST_ERROR();
#if(defined(WIN32) || defined(_WIN32) || defined(WINCE))
            if(rst == WSAETIMEDOUT || rst == WSAEWOULDBLOCK) {
#else
            if(rst == EAGAIN || rst == EWOULDBLOCK) {
#endif
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
            else {
                break;
            }
        }
        // ok
        return;
    }
    // error
    throw libobsensor::io_exception(utils::string::to_string()
                                     << "VendorUDPClient write data failed! socket=" << socketFd_ << ", err_code=" << rst);
    return;
}

}  // namespace libobsensor
