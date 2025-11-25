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

#include "GVCPClient.hpp"
#include "exception/ObException.hpp"

#include "logger/LoggerInterval.hpp"
#include "logger/LoggerHelper.hpp"
#include "utils/StringUtils.hpp"

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

GVCPClient::GVCPClient() {
#if (defined(WIN32) || defined(_WIN32) || defined(WINCE))
    WSADATA wsaData;
    if(WSAStartup(MAKEWORD(2, 2), &wsaData)) {
        throw libobsensor::invalid_value_exception(utils::string::to_string() << "Failed to initialize WinSock! err_code=" << GET_LAST_ERROR());
    }
#endif
    openClientSockets();
}

GVCPClient::~GVCPClient() {
    std::lock_guard<std::mutex> lck(queryMtx_);
#if (defined(WIN32) || defined(_WIN32) || defined(WINCE))
    WSACleanup();
#endif
    closeClientSockets();
}

std::vector<GVCPDeviceInfo> GVCPClient::queryNetDeviceList() {
    std::lock_guard<std::mutex> lck(queryMtx_);

    auto tmpList = devInfoList_;
    devInfoList_.clear();

    checkAndUpdateSockets();
    std::vector<std::thread> threads;
    for(int i = 0; i < sockCount_; ++i) {
        auto func = std::bind(&GVCPClient::sendGVCPDiscovery, this, socketInfos_[i]);
        threads.emplace_back(func, socketInfos_[i]);
    }

    for(auto &thread: threads) {
        thread.join();
    }

    // devInfoList_ remove duplication
    if(!devInfoList_.empty()) {
        auto iter = devInfoList_.begin();
        while(iter != devInfoList_.end()) {
            auto it = std::find_if(iter + 1, devInfoList_.end(), [&](const GVCPDeviceInfo &other) { return *iter == other; });
            if(it != devInfoList_.end()) {
                devInfoList_.erase(it);
                continue;
            }
            iter++;
        }
    }

    // Compare two std::vector:
    // 1. elements are unique in both vectors
    // 2. order of the item may be different
    auto compare = [](const std::vector<GVCPDeviceInfo> &vec1, std::vector<GVCPDeviceInfo> &vec2) -> bool {
        if(vec1.size() != vec2.size()) {
            return false;
        }
        // For small vectors, linear search might be faster
        for(const auto &item: vec2) {
            if(std::find(vec1.begin(), vec1.end(), item) == vec1.end()) {
                return false;
            }
        }

        return true;
    };

    if(!compare(tmpList, devInfoList_)) {
        LOG_DEBUG("queryNetDevice completed ({}):", devInfoList_.size());
        for(auto &&info: devInfoList_) {
            LOG_DEBUG("\t- mac:{}, ip:{}, sn:{}, pid:0x{:04x}", info.mac, info.ip, info.sn, info.pid);
        }
    }

    return devInfoList_;
}

bool GVCPClient::forceIpConfig(std::string macAddress, const OBNetIpConfig &config) {
    std::lock_guard<std::mutex> lck(queryMtx_);

    std::vector<std::future<bool>> futures;

    for(int i = 0; i < sockCount_; ++i) {
        futures.push_back(std::async(std::launch::async, &GVCPClient::sendGVCPForceIP, this, socketInfos_[i], macAddress, config));
    }
    bool success = false;
    for(auto &f: futures) {
        if(f.get()) {
            success = true;
        }
    }
    if(success) {
        LOG_INFO("Change net device ip config successfully. MAC: {}", macAddress);
        return true;
    }
    LOG_INFO("Failed to change net device ip config. MAC: {}", macAddress);
    return false;
}

#if defined(__APPLE__)
const uint8_t *GVCPClient::getMACAddress(struct ifaddrs *ifap, const char *interface_name) {
    struct ifaddrs *p = ifap;

    while(p != NULL) {
        if(p->ifa_addr && p->ifa_addr->sa_family == AF_LINK && strcmp(p->ifa_name, interface_name) == 0) {
            struct sockaddr_dl *sdl = (struct sockaddr_dl *)p->ifa_addr;
            if(sdl->sdl_alen == 6) {
                return (uint8_t *)LLADDR(sdl);
            }
            return nullptr;
        }
        p = p->ifa_next;
    }
    return nullptr;
}
#endif  // __APPLE__

#if (defined(__linux__) || defined(__ANDROID__))
std::string getGateway(const std::string &ifName) {
    std::ifstream routeFile("/proc/net/route");
    std::string   line;
    std::string   gateway = "0.0.0.0";

    while(std::getline(routeFile, line)) {
        std::istringstream iss(line);
        std::string        iface, destination, gatewayHex, flags;
        if(!(iss >> iface >> destination >> gatewayHex >> flags)) {
            continue;
        }

        if(iface == ifName && destination == "00000000") {
            unsigned long     gw;
            std::stringstream ss;
            ss << std::hex << gatewayHex;
            ss >> gw;

            struct in_addr addr;
            addr.s_addr = gw;
            gateway     = std::string(inet_ntoa(addr));
            break;
        }
    }
    return gateway;
}
#endif  // __linux__ || __ANDROID__

int GVCPClient::openClientSockets() {
#if (defined(WIN32) || defined(_WIN32) || defined(WINCE))
    DWORD                                                                   ret;
    DWORD                                                                   size = 0;
    std::unique_ptr<IP_ADAPTER_ADDRESSES, void (*)(IP_ADAPTER_ADDRESSES *)> adapterAddresses(nullptr, [](IP_ADAPTER_ADDRESSES *p) { free(p); });

    ret = GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, NULL, NULL, &size);
    if(ret != ERROR_BUFFER_OVERFLOW) {
        fprintf(stderr, "GetAdaptersAddresses() failed...");
        return -1;
    }
    adapterAddresses.reset(reinterpret_cast<IP_ADAPTER_ADDRESSES *>(malloc(size)));

    ret = GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, NULL, adapterAddresses.get(), &size);
    if(ret != ERROR_SUCCESS) {
        fprintf(stderr, "GetAdaptersAddresses() failed...");
        return -1;
    }

    int index = 0;

    for(PIP_ADAPTER_ADDRESSES aa = adapterAddresses.get(); aa != NULL; aa = aa->Next) {
        std::ostringstream macAddressStream;
        for(int i = 0; i < 6; i++) {
            macAddressStream << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(aa->PhysicalAddress[i]);
            if(i < 5)
                macAddressStream << ":";
        }
        std::string macAddress = macAddressStream.str();

        for(PIP_ADAPTER_UNICAST_ADDRESS ua = aa->FirstUnicastAddress; ua != NULL; ua = ua->Next) {
            SOCKADDR_IN addrSrv;
            addrSrv                  = *(SOCKADDR_IN *)ua->Address.lpSockaddr;
            addrSrv.sin_family       = AF_INET;
            addrSrv.sin_port         = htons(0);
            std::string ipStr        = inet_ntoa(addrSrv.sin_addr);
            uint8_t     subnetLength = ua->OnLinkPrefixLength;
            std::string gateway      = "";
            if(aa->FirstGatewayAddress != nullptr) {
                SOCKADDR_IN *gatewayAddr = reinterpret_cast<SOCKADDR_IN *>(aa->FirstGatewayAddress->Address.lpSockaddr);
                gateway                  = inet_ntoa(gatewayAddr->sin_addr);
            }
            // "127.0.0.1" is loopback address
            if(strcmp(ipStr.c_str(), "127.0.0.1") == 0) {
                continue;
            }

            SOCKET socket                       = openClientSocket(addrSrv);
            int    curIndex                     = index++;
            socketInfos_[curIndex].sock         = socket;
            socketInfos_[curIndex].mac          = macAddress;
            socketInfos_[curIndex].address      = ipStr;
            socketInfos_[curIndex].subnetLength = subnetLength;
            socketInfos_[curIndex].gateway      = gateway;
            LOG_DEBUG("local mac address: {},local ip address:{}", macAddress, ipStr);
        }
    }
    sockCount_ = index;
#else
    struct ifaddrs *ifaddr, *ifa;
    int             family, s, n;
    char            host[NI_MAXHOST];

    if(getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        exit(EXIT_FAILURE);
    }

    int index = 0;

    for(ifa = ifaddr, n = 0; ifa != NULL; ifa = ifa->ifa_next, n++) {
        if(ifa->ifa_addr == NULL)
            continue;

        family = ifa->ifa_addr->sa_family;

        if(family == AF_INET) {
            s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
            if(s != 0) {
                LOG_TRACE("getnameinfo() failed: {}", gai_strerror(s));
                exit(EXIT_FAILURE);
            }

            SOCKADDR_IN addrSrv;
            addrSrv            = *(SOCKADDR_IN *)ifa->ifa_addr;
            addrSrv.sin_family = AF_INET;
            addrSrv.sin_port   = htons(0);
            std::string ipStr  = inet_ntoa(addrSrv.sin_addr);
            // "127.0.0.1" is loopback address
            if(strcmp(ipStr.c_str(), "127.0.0.1") == 0) {
                continue;
            }
            ipAddressStrSet_.insert(ipStr);

            SOCKET socket                   = openClientSocket(addrSrv);
            int    curIndex                 = index++;
            socketInfos_[curIndex].sock     = socket;
            socketInfos_[curIndex].sockRecv = openClientRecvSocket(socket);
            int prefixLength                = 0;
            if(ifa->ifa_netmask != NULL) {
                struct sockaddr_in *netmask = (struct sockaddr_in *)ifa->ifa_netmask;
                uint32_t            mask    = ntohl(netmask->sin_addr.s_addr);
                while(mask) {
                    prefixLength += (mask & 1);
                    mask >>= 1;
                }
            }

            LOG_DEBUG("getnameinfo-name: {}", ifa->ifa_name);
#if defined(__APPLE__)
            auto mac = getMACAddress(ifaddr, ifa->ifa_name);
            if(mac != nullptr) {
#else
            struct ifreq ifr;
            std::memset(&ifr, 0, sizeof(ifr));
            std::strncpy(ifr.ifr_name, ifa->ifa_name, IFNAMSIZ - 1);

            if(ioctl(socket, SIOCGIFHWADDR, &ifr) >= 0) {
                unsigned char *mac = reinterpret_cast<unsigned char *>(ifr.ifr_hwaddr.sa_data);
#endif
                std::ostringstream macAddressStream;
                for(int i = 0; i < 6; ++i) {
                    macAddressStream << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(mac[i]);
                    if(i < 5) {
                        macAddressStream << ":";
                    }
                }

                std::string interfaceNameStr(ifa->ifa_name);
#if defined(__APPLE__)
                // TODO get gateway for macOS
                std::string gateway = "0.0.0.0";
#else
                std::string gateway = getGateway(interfaceNameStr);
#endif
                std::string macAddress                  = macAddressStream.str();
                socketInfos_[curIndex].mac              = macAddress;
                socketInfos_[curIndex].address          = ipStr;
                socketInfos_[curIndex].netInterfaceName = interfaceNameStr;
                socketInfos_[curIndex].gateway          = gateway;
                socketInfos_[curIndex].subnetLength     = prefixLength;
                LOG_DEBUG("local mac address: {},local ip address:{}, interfaceName:{}", macAddress, ipStr, interfaceNameStr);
            }
        }
    }

    sockCount_ = index;
    freeifaddrs(ifaddr);
#endif

    return sockCount_;
}

void GVCPClient::closeClientSockets() {
    for(int i = 0; i < sockCount_; i++) {
        closesocket(socketInfos_[i].sock);
        if(socketInfos_[i].sockRecv > 0) {
            closesocket(socketInfos_[i].sockRecv);
        }
    }
}

SOCKET GVCPClient::openClientRecvSocket(SOCKET srcSock) {
#if (defined(WIN32) || defined(_WIN32) || defined(WINCE))
    // for windows, do nothing
    (void)(srcSock);
    return INVALID_SOCKET;
#else
    // get source socket port
    SOCKADDR_IN srcAddr;
    socklen_t   addrLen = sizeof(srcAddr);
    if(getsockname(srcSock, (struct sockaddr *)&srcAddr, &addrLen) == -1) {
        LOG_TRACE("getsockname() failed: {}", strerror(errno));
        return 0;
    }

    // create new socket
    SOCKADDR_IN addrSrv;
    memset(&addrSrv, 0, sizeof(addrSrv));
    addrSrv.sin_family      = AF_INET;
    addrSrv.sin_addr.s_addr = INADDR_ANY;
    addrSrv.sin_port        = srcAddr.sin_port;

    return openClientSocket(addrSrv);
#endif
}

SOCKET GVCPClient::openClientSocket(SOCKADDR_IN addr) {
    // Create socket
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(sock == INVALID_SOCKET) {
        throw libobsensor::invalid_value_exception(utils::string::to_string() << "Failed to create socket! err_code=" << GET_LAST_ERROR());
    }

    // Set broadcast options
    int bBroadcast = 1;
#if (defined(WIN32) || defined(_WIN32) || defined(WINCE))
    int err = setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (char *)&bBroadcast, sizeof(bBroadcast));
#else
    // int     err = setsockopt(sock, SOL_SOCKET, SO_BROADCAST | SO_REUSEADDR, (char *)&bBroadcast, sizeof(bBroadcast));
    int err = setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (char *)&bBroadcast, sizeof(bBroadcast));
    if(err == SOCKET_ERROR) {
        throw libobsensor::invalid_value_exception(utils::string::to_string() << "Failed to set socket boardcast option! err_code=" << GET_LAST_ERROR());
    }
    err = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&bBroadcast, sizeof(bBroadcast));
    if(err == SOCKET_ERROR) {
        throw libobsensor::invalid_value_exception(utils::string::to_string() << "Failed to set socket reuseaddr option! err_code=" << GET_LAST_ERROR());
    }
#endif

// Set receive timeout options
#if (defined(WIN32) || defined(_WIN32) || defined(WINCE))
    uint32_t dwTimeout = 5000;
#else
    TIMEVAL dwTimeout;
    dwTimeout.tv_sec  = 5;
    dwTimeout.tv_usec = 0;
#endif
    err = setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&dwTimeout, sizeof(dwTimeout));
    if(err == SOCKET_ERROR) {
        throw libobsensor::invalid_value_exception(utils::string::to_string() << "Failed to set socket timeout option! err_code=" << GET_LAST_ERROR());
    }

#if (defined(__linux__) || defined(OS_IOS) || defined(OS_MACOS) || defined(__ANDROID__))
    // addr.sin_addr.s_addr = inet_addr("0.0.0.0");
#endif
    std::string ip = inet_ntoa(addr.sin_addr);
    LOG_INTVL(LOG_INTVL_OBJECT_TAG + "GVCP bind", MAX_LOG_INTERVAL, spdlog::level::debug, "bind {}:{}", ip, ntohs(addr.sin_port));
    err = bind(sock, (SOCKADDR *)&addr, sizeof(SOCKADDR));
    if(err == SOCKET_ERROR) {
        return 0;
    }

    return sock;
}

int GVCPClient::recvAndParseGVCPResponse(SOCKET sock, const GVCPSocketInfo &socketInfo) {
    char recvBuf[1024] = { 0 };

    // Receive data
    SOCKADDR_IN srcAddr;
    socklen_t   srcAddrLen = sizeof(srcAddr);

    auto err = recvfrom(sock, recvBuf, sizeof(recvBuf), 0, (SOCKADDR *)&srcAddr, &srcAddrLen);
    if(err == SOCKET_ERROR || err < 0) {
        LOG_INTVL(LOG_INTVL_OBJECT_TAG + "GVCP recvfrom", DEF_MIN_LOG_INTVL, spdlog::level::err, "recvfrom failed with error: {}", GET_LAST_ERROR());
        return err;
    }

    uint32_t respLen = static_cast<uint32_t>(err);
    // Parse response data
    if(respLen < sizeof(gvcp_ack_header)) {
        LOG_INTVL(LOG_INTVL_OBJECT_TAG + "GVCP parse response", MAX_LOG_INTERVAL, spdlog::level::debug, "invalid response data, len={}", respLen);
        return 0;
    }

    gvcp_ack_header *ackHeader = reinterpret_cast<gvcp_ack_header *>(recvBuf);
    respLen -= sizeof(gvcp_ack_header);

    uint16_t status = ntohs(ackHeader->wStatus);
    uint16_t ack    = ntohs(ackHeader->wAck);
    uint16_t len    = ntohs(ackHeader->wLen);
    uint16_t reqID  = ntohs(ackHeader->wReqID);

    if(status == GEV_STATUS_SUCCESS && ack == GVCP_DISCOVERY_ACK && reqID == GVCP_REQUEST_ID) {
        gvcp_discover_ack_payload *ackPayload = reinterpret_cast<gvcp_discover_ack_payload *>(recvBuf + sizeof(gvcp_ack_header));
        if(respLen < sizeof(gvcp_discover_ack_payload)) {
            LOG_INTVL(LOG_INTVL_OBJECT_TAG + "GVCP parse gvcp payload", MAX_LOG_INTERVAL, spdlog::level::debug, "invalid payload len: {}", respLen);
            return 0;
        }

        // auto specVer  = ntohl(ackPayload->dwSpecVer);
        // auto devMode  = ntohl(ackPayload->dwDevMode);
        // auto supIpSet = ntohl(ackPayload->dwSupIpSet);
        // auto curIpSet = ntohl(ackPayload->dwCurIpSet);
        auto curPID = ntohl(ackPayload->dwPID);

        // Get Mac address
        char macStr[18];
        sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X", ackPayload->Mac[2], ackPayload->Mac[3], ackPayload->Mac[4], ackPayload->Mac[5], ackPayload->Mac[6],
                ackPayload->Mac[7]);

        // Read CurIP field
        uint32_t curIP = *((uint32_t *)&ackPayload->CurIP[12]);
        char     curIPStr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &curIP, curIPStr, INET_ADDRSTRLEN);

        // Read the SubMask field
        uint32_t subMask = *((uint32_t *)&ackPayload->SubMask[12]);
        char     subMaskStr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &subMask, subMaskStr, INET_ADDRSTRLEN);

        // Read the Gateway field
        uint32_t gateway = *((uint32_t *)&ackPayload->Gateway[12]);
        char     gatewayStr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &gateway, gatewayStr, INET_ADDRSTRLEN);

        // LOG_INTVL(LOG_INTVL_OBJECT_TAG + "GVCP get ack info", DEF_MIN_LOG_INTVL, spdlog::level::info, "{},{}, {},{}, {},{}, {}, {}, {}, {}, {}, {}, {}, {},
        // {}",
        //           specVer, devMode, std::string(macStr), supIpSet, curIpSet, curPID, std::string(curIPStr), std::string(subMaskStr), std::string(gatewayStr),
        //           std::string(ackPayload->szFacName), std::string(ackPayload->szModelName), std::string(ackPayload->szDevVer),
        //           std::string(ackPayload->szFacInfo), std::string(ackPayload->szSerial), std::string(ackPayload->szUserName));

        // Filter non-Orbbec devices
        std::string deviceManufacturer = ackPayload->szFacName;
        auto        it                 = libobsensor::manufacturerVidMap.find(deviceManufacturer);
        if(it == manufacturerVidMap.end()) {
            return 0;
        }

        GVCPDeviceInfo info;
        info.netInterfaceName  = socketInfo.netInterfaceName;
        info.localIp           = socketInfo.address;
        info.localMac          = socketInfo.mac;
        info.localGateway      = socketInfo.gateway;
        info.localSubnetLength = socketInfo.subnetLength;
        info.mac               = macStr;
        info.ip                = curIPStr;
        info.mask              = subMaskStr;
        info.gateway           = gatewayStr;
        info.sn                = ackPayload->szSerial;
        info.name              = ackPayload->szModelName;
        info.pid               = curPID;
        info.vid               = it->second;
        info.devVersion        = ackPayload->szDevVer;
        // info.manufacturer      = ackPayload->szFacName;

        if(info.pid == 0x0000 && info.name == "OI-BC300I") {
            // TODO: Treat OI-BC300I (pid=0x0000) as Femto Mega I (pid=0x06c0) for compatibility
            info.pid = 0x06c0;
        }

        std::lock_guard<std::mutex> lock(devInfoListMtx_);
        devInfoList_.push_back(info);
        return 1;
    }
    else {
        LOG_INTVL(LOG_INTVL_OBJECT_TAG + "GVCP get info", DEF_MIN_LOG_INTVL, spdlog::level::debug,
                  "GVCP response error: status:{}, ack:{}, device response reqID:{}, response len:{}", status, ack, reqID, len);
    }
    return 0;
}

void GVCPClient::sendGVCPDiscovery(GVCPSocketInfo socketInfo) {
    // LOG_TRACE("send gvcp discovery {}", socketInfo.sock);
    gvcp_discover_cmd discoverCmd;

    SOCKADDR_IN destAddr;
    destAddr.sin_family      = AF_INET;
    destAddr.sin_addr.s_addr = INADDR_BROADCAST;  // Broadcast address
    destAddr.sin_port        = htons(GVCP_PORT);

    // device discovery
    gvcp_cmd_header cmdHeader;
    cmdHeader.cMsgKeyCode = GVCP_KEY_CODE;
    cmdHeader.cFlag       = GVCP_DISCOVERY_FLAGS;
    cmdHeader.wCmd        = htons(GVCP_DISCOVERY_CMD);
    cmdHeader.wLen        = htons(0);
    cmdHeader.wReqID      = htons(1);

    discoverCmd.header = cmdHeader;

    // Get local ip
    // struct sockaddr_in addr;
    // socklen_t addrLen = sizeof(addr);

    // Get the address information of the socket
    // if(getsockname(socketInfo.sock, (struct sockaddr *)&addr, &addrLen) == 0) {
    //     LOG_INFO("cur addr {}:{}", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
    // }

    // send data
    int err = sendto(socketInfo.sock, (const char *)&discoverCmd, sizeof(discoverCmd), 0, (SOCKADDR *)&destAddr, sizeof(destAddr));
    if(err == SOCKET_ERROR) {
        LOG_INTVL(LOG_INTVL_OBJECT_TAG + "GVCP sendto", MAX_LOG_INTERVAL, spdlog::level::debug, "sendto failed with error:{}", GET_LAST_ERROR());
    }
    // LOG_INFO("sendto get info with error:{}", GET_LAST_ERROR());

    int res;
    int failedCount = 100;
    do {
        struct timeval timeout;
        timeout.tv_sec  = 1;
        timeout.tv_usec = 0;

        int    nfds = 0;
        fd_set readfs;
        FD_ZERO(&readfs);
        nfds = static_cast<int>(socketInfo.sock) + 1;
        FD_SET(socketInfo.sock, &readfs);
        if(socketInfo.sockRecv > 0) {
            FD_SET(socketInfo.sockRecv, &readfs);
            if(socketInfo.sockRecv > socketInfo.sock) {
                nfds = static_cast<int>(socketInfo.sockRecv) + 1;
            }
        }

        res = select(nfds, &readfs, 0, 0, &timeout);
        if(res > 0) {
            if(FD_ISSET(socketInfo.sock, &readfs)) {
                err = recvAndParseGVCPResponse(socketInfo.sock, socketInfo);
                if(err == SOCKET_ERROR) {
                    if(failedCount-- < 0) {
                        LOG_WARN("GVCP recvfrom failed!!!");
                        break;
                    }
                }
            }
            if(socketInfo.sockRecv > 0 && FD_ISSET(socketInfo.sockRecv, &readfs)) {
                err = recvAndParseGVCPResponse(socketInfo.sockRecv, socketInfo);
                if(err == SOCKET_ERROR) {
                    if(failedCount-- < 0) {
                        LOG_WARN("GVCP recvfrom failed!!!");
                        break;
                    }
                }
            }
            FD_SET(socketInfo.sock, &readfs);
        }
    } while(res > 0);
    // LOG_INFO("finish gvcp discovery {}", sock);
}

bool GVCPClient::sendGVCPForceIP(GVCPSocketInfo socketInfo, std::string mac, const OBNetIpConfig &config) {
    gvcp_forceip_cmd forceIPCmd;
    // gvcp_forceip_ack forceIPAck;

    SOCKADDR_IN destAddr;
    destAddr.sin_family      = AF_INET;
    destAddr.sin_addr.s_addr = INADDR_BROADCAST;  // Broadcast address
    destAddr.sin_port        = htons(GVCP_PORT);

    gvcp_cmd_header cmdHeader = {};
    cmdHeader.cMsgKeyCode     = GVCP_KEY_CODE;
    cmdHeader.cFlag           = GVCP_FORCEIP_FLAGS;
    cmdHeader.wCmd            = htons(GVCP_FORCEIP_CMD);
    cmdHeader.wLen            = htons(sizeof(gvcp_forceip_payload));
    cmdHeader.wReqID          = htons(1);

    forceIPCmd.header = cmdHeader;

    gvcp_forceip_payload payload = {};
    sscanf(mac.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &payload.Mac[2], &payload.Mac[3], &payload.Mac[4], &payload.Mac[5], &payload.Mac[6], &payload.Mac[7]);
    // memcpy(&payload.Mac[2], &discoverAck.payload.Mac[2], 6);

    // IP address that needs to be modified
    // char m_szLocalIp[32];
    // strcpy(m_szLocalIp, config.address);
    // char m_szLocalMask[32];
    // strcpy(m_szLocalMask, config.mask);
    // char m_szLocalGateway[32];
    // strcpy(m_szLocalGateway, config.gateway);

    //*((uint32_t *)&payload.CurIP[12])   = inet_addr(m_szLocalIp);       //last 4 byte
    //*((uint32_t *)&payload.SubMask[12]) = inet_addr(m_szLocalMask);     //last 4 byte
    //*((uint32_t *)&payload.Gateway[12]) = inet_addr(m_szLocalGateway);  //last 4 byte

    memcpy(payload.CurIP + sizeof(payload.CurIP) - 4, config.address, 4);
    memcpy(payload.SubMask + sizeof(payload.SubMask) - 4, config.mask, 4);
    memcpy(payload.Gateway + sizeof(payload.Gateway) - 4, config.gateway, 4);

    forceIPCmd.payload = payload;

    // send data
    int err = sendto(socketInfo.sock, (const char *)&forceIPCmd, sizeof(forceIPCmd), 0, (SOCKADDR *)&destAddr, sizeof(destAddr));
    if(err == SOCKET_ERROR) {
        LOG_TRACE("sendto failed with error: {}", GET_LAST_ERROR());
    }
    const uint8_t FORCEIP_ACK = 0x05;
    int           res;
    int           failedCount = 100;

    do {
        struct timeval timeout{};
        timeout.tv_sec  = 1;
        timeout.tv_usec = 0;

        int    nfds = static_cast<int>(socketInfo.sock) + 1;
        fd_set readfs;
        FD_ZERO(&readfs);
        FD_SET(socketInfo.sock, &readfs);

        if(socketInfo.sockRecv > 0) {
            FD_SET(socketInfo.sockRecv, &readfs);
            if(socketInfo.sockRecv > socketInfo.sock) {
                nfds = static_cast<int>(socketInfo.sockRecv) + 1;
            }
        }

        res = select(nfds, &readfs, nullptr, nullptr, &timeout);
        if(res > 0) {
            if(FD_ISSET(socketInfo.sock, &readfs)) {
                uint8_t     buffer[1024]{};
                SOCKADDR_IN senderAddr{};
                socklen_t   senderLen = sizeof(senderAddr);

                int recvLen = recvfrom(socketInfo.sock, (char *)buffer, sizeof(buffer), 0, (struct sockaddr *)&senderAddr, &senderLen);
                if(recvLen >= (int)sizeof(gvcp_ack_header)) {
                    gvcp_ack_header *ack = (gvcp_ack_header *)buffer;
                    if(ntohs(ack->wAck) == FORCEIP_ACK && ack->wReqID == forceIPCmd.header.wReqID && ntohs(ack->wStatus) == 0) {
                        std::string ip = inet_ntoa(senderAddr.sin_addr);
                        LOG_TRACE("Received GVCP ForceIP ACK from {}", ip.c_str());
                        return true;
                    }
                }
                else if(failedCount-- < 0) {
                    LOG_WARN("GVCP recvfrom failed on sock");
                    break;
                }
            }
            if(socketInfo.sockRecv > 0 && FD_ISSET(socketInfo.sockRecv, &readfs)) {
                uint8_t     buffer[1024]{};
                SOCKADDR_IN senderAddr{};
                socklen_t   senderLen = sizeof(senderAddr);

                int recvLen = recvfrom(socketInfo.sockRecv, (char *)buffer, sizeof(buffer), 0, (struct sockaddr *)&senderAddr, &senderLen);
                if(recvLen >= (int)sizeof(gvcp_ack_header)) {
                    gvcp_ack_header *ack = (gvcp_ack_header *)buffer;

                    if(ntohs(ack->wAck) == FORCEIP_ACK && ack->wReqID == forceIPCmd.header.wReqID) {
                        uint16_t    status = ntohs(ack->wStatus);
                        std::string ipStr  = inet_ntoa(senderAddr.sin_addr);

                        if(status == 0x0000) {
                            // GEV_STATUS_SUCCESS
                            LOG_TRACE("Received GVCP ForceIP ACK from {}, status=0x0000 (SUCCESS)", ipStr.c_str());
                            return true;
                        }
                        else if(status == 0x8003) {
                            // GEV_STATUS_INVALID_ADDRESS
                            LOG_WARN("Received GVCP ForceIP ACK from {}, status=0x8003 (INVALID ADDRESS: IP not acceptable)", ipStr.c_str());
                        }
                        else {
                            LOG_WARN("Received GVCP ForceIP ACK from {}, unknown status=0x{:04x}", ipStr.c_str(), status);
                        }
                    }
                }
                else if(failedCount-- < 0) {
                    LOG_WARN("GVCP recvfrom failed on sockRecv");
                    break;
                }
            }
        }
    } while(res > 0);

    return false;
}

void GVCPClient::checkAndUpdateSockets() {
#if (defined(WIN32) || defined(_WIN32) || defined(WINCE))
    DWORD                                                                   ret;
    DWORD                                                                   size = 0;
    std::unique_ptr<IP_ADAPTER_ADDRESSES, void (*)(IP_ADAPTER_ADDRESSES *)> adapterAddresses(nullptr, [](IP_ADAPTER_ADDRESSES *p) { free(p); });

    ret = GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, NULL, NULL, &size);
    if(ret != ERROR_BUFFER_OVERFLOW) {
        LOG_ERROR("GetAdaptersAddresses() failed...");
        return;
    }
    adapterAddresses.reset(reinterpret_cast<IP_ADAPTER_ADDRESSES *>(malloc(size)));

    ret = GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, NULL, adapterAddresses.get(), &size);
    if(ret != ERROR_SUCCESS) {
        LOG_ERROR("GetAdaptersAddresses() failed...");
        return;
    }

    int index = sockCount_;

    for(PIP_ADAPTER_ADDRESSES aa = adapterAddresses.get(); aa != NULL; aa = aa->Next) {
        for(PIP_ADAPTER_UNICAST_ADDRESS ua = aa->FirstUnicastAddress; ua != NULL; ua = ua->Next) {
            SOCKADDR_IN addrSrv;
            addrSrv                  = *(SOCKADDR_IN *)ua->Address.lpSockaddr;
            addrSrv.sin_family       = AF_INET;
            addrSrv.sin_port         = htons(0);
            std::string ipStr        = inet_ntoa(addrSrv.sin_addr);
            uint8_t     subnetLength = ua->OnLinkPrefixLength;
            std::string gateway      = "";
            if(aa->FirstGatewayAddress != nullptr) {
                SOCKADDR_IN *gatewayAddr = reinterpret_cast<SOCKADDR_IN *>(aa->FirstGatewayAddress->Address.lpSockaddr);
                gateway                  = inet_ntoa(gatewayAddr->sin_addr);
            }
            if(strcmp(ipStr.c_str(), "127.0.0.1") == 0) {
                continue;
            }

            bool found = false;
            for(auto socketInfo: socketInfos_) {
                if(socketInfo.sock == 0) {
                    continue;
                }

                SOCKADDR_IN addr;
                socklen_t   addrLen = sizeof(addr);
                if(getsockname(socketInfo.sock, (struct sockaddr *)&addr, &addrLen) == 0) {
                    std::string sockIp = inet_ntoa(addr.sin_addr);
                    if(ipStr == sockIp) {
                        found = true;
                        break;
                    }
                }
                else {
                    LOG_INFO("get socket ip addr failed,{}", ipStr);
                }
            }

            if(!found) {
                try {
                    auto socketFd = openClientSocket(addrSrv);
                    if(socketFd != 0) {
                        // socks_[index++] = socketFd;
                        // LOG_INFO("new ip segment found,new ip addr:{}", ipStr);

                        std::ostringstream macAddressStream;
                        for(int i = 0; i < 6; i++) {
                            macAddressStream << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(aa->PhysicalAddress[i]);
                            if(i < 5)
                                macAddressStream << ":";
                        }
                        std::string macAddress = macAddressStream.str();

                        int curIndex                        = index++;
                        socketInfos_[curIndex].sock         = socketFd;
                        socketInfos_[curIndex].mac          = macAddress;
                        socketInfos_[curIndex].address      = ipStr;
                        socketInfos_[curIndex].subnetLength = subnetLength;
                        socketInfos_[curIndex].gateway      = gateway;
                        LOG_INFO("New ip segment found,new ip addr:{}, mac:{}", ipStr, macAddress);
                    }
                }
                catch(const std::exception &) {
                    LOG_WARN("Open client socket failed");
                }
            }
        }
    }
    sockCount_ = index;
#else
    struct ifaddrs *ifaddr, *ifa;
    int             family, s, n;
    char            host[NI_MAXHOST];

    if(getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        exit(EXIT_FAILURE);
    }

    int index = sockCount_;

    for(ifa = ifaddr, n = 0; ifa != NULL; ifa = ifa->ifa_next, n++) {
        if(ifa->ifa_addr == NULL)
            continue;

        family = ifa->ifa_addr->sa_family;

        if(family == AF_INET) {
            s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
            if(s != 0) {
                LOG_TRACE("getnameinfo() failed: {}", gai_strerror(s));
                exit(EXIT_FAILURE);
            }

            SOCKADDR_IN addrSrv;
            addrSrv            = *(SOCKADDR_IN *)ifa->ifa_addr;
            addrSrv.sin_family = AF_INET;
            addrSrv.sin_port   = htons(0);
            std::string ipStr  = inet_ntoa(addrSrv.sin_addr);
            // "127.0.0.1" is loopback address
            if(strcmp(ipStr.c_str(), "127.0.0.1") == 0) {
                continue;
            }

            bool found = false;
            for(auto sockIp: ipAddressStrSet_) {
                if(ipStr == sockIp) {
                    found = true;
                    break;
                }
            }

            if(!found) {
                auto sock                       = openClientSocket(addrSrv);
                int  curIndex                   = index++;
                socketInfos_[curIndex].sock     = sock;
                socketInfos_[curIndex].sockRecv = openClientRecvSocket(sock);

                LOG_DEBUG("getnameinfo-name: {}", ifa->ifa_name);

                int prefixLength = 0;
                if(ifa->ifa_netmask != NULL) {
                    struct sockaddr_in *netmask = (struct sockaddr_in *)ifa->ifa_netmask;
                    uint32_t            mask    = ntohl(netmask->sin_addr.s_addr);
                    while(mask) {
                        prefixLength += (mask & 1);
                        mask >>= 1;
                    }
                }

#if defined(__APPLE__)
                auto mac = getMACAddress(ifaddr, ifa->ifa_name);
                if(mac != nullptr) {
#else
                struct ifreq ifr;
                std::memset(&ifr, 0, sizeof(ifr));
                std::strncpy(ifr.ifr_name, ifa->ifa_name, IFNAMSIZ - 1);

                if(ioctl(sock, SIOCGIFHWADDR, &ifr) >= 0) {
                    unsigned char *mac = reinterpret_cast<unsigned char *>(ifr.ifr_hwaddr.sa_data);
#endif
                    std::ostringstream macAddressStream;
                    for(int i = 0; i < 6; ++i) {
                        macAddressStream << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(mac[i]);
                        if(i < 5) {
                            macAddressStream << ":";
                        }
                    }

                    std::string interfaceNameStr(ifa->ifa_name);
#if defined(__APPLE__)
                    // TODO get gateway for macOS
                    std::string gateway = "0.0.0.0";
#else
                    std::string gateway = getGateway(interfaceNameStr);
#endif
                    std::string macAddress                  = macAddressStream.str();
                    socketInfos_[curIndex].mac              = macAddress;
                    socketInfos_[curIndex].address          = ipStr;
                    socketInfos_[curIndex].netInterfaceName = interfaceNameStr;
                    socketInfos_[curIndex].gateway          = gateway;
                    socketInfos_[curIndex].subnetLength     = prefixLength;
                    LOG_INFO("New ip segment found,new ip addr:{}, mac:{}, interfaceName:{}", ipStr, macAddress, interfaceNameStr);
                }

                ipAddressStrSet_.insert(ipStr);
            }
        }
    }
    sockCount_ = index;
    freeifaddrs(ifaddr);
#endif
}

}  // namespace libobsensor
