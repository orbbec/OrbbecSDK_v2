// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "libobsensor/h/ObTypes.h"
#include "ethernet/socket/SocketTypes.hpp"
#include "common/DeviceSeriesInfo.hpp"
#include "GVCPTransmit.hpp"
#include <vector>
#include <string>
#include <mutex>
#include <set>

namespace libobsensor {

struct GVCPDeviceInfo {
    std::string netInterfaceName  = "unknown";
    std::string localIp           = "unknown";
    std::string localMac          = "unknown";
    std::string localGateway      = "unknown";
    uint8_t     localSubnetLength = 0;
    std::string mac               = "unknown";
    std::string ip                = "unknown";
    std::string mask              = "unknown";
    std::string gateway           = "unknown";
    std::string sn                = "unknown";
    std::string name              = "unknown";
    uint32_t    pid               = 0;
    uint32_t    vid               = 0;
    // std::string version      = "";
    // std::string manufacturer = "";

    virtual bool operator==(const GVCPDeviceInfo &other) const {
        return other.mac == mac && other.sn == sn && other.ip == ip && other.localIp == localIp;
    }
    virtual ~GVCPDeviceInfo() {}
};

struct GVCPSocketInfo {
    std::string mac              = "unknown";
    std::string address          = "unknown";
    std::string netInterfaceName = "unknown";
    std::string gateway          = "unknown";
    uint8_t     subnetLength     = 0;
    SOCKET      sock             = 0;
    SOCKET      sockRecv         = 0;
};

#define MAX_SOCKETS 32

class GVCPClient {
public:
    ~GVCPClient();

    std::vector<GVCPDeviceInfo> queryNetDeviceList();
    bool                        forceIpConfig(std::string macAddress, const OBNetIpConfig &config);

    static GVCPClient &instance() {
        static GVCPClient instance;
        return instance;
    }

private:
    GVCPClient();
    int    openClientSockets();
    void   closeClientSockets();
    SOCKET openClientSocket(SOCKADDR_IN addr);
    /**
     * @brief receive parse gvcp response
     *
     * @param[in] socketInfo socket info
     *
     * @return < 0: error
     *         other: device count
     */
    int  recvAndParseGVCPResponse(SOCKET sock, const GVCPSocketInfo &socketInfo);
    void sendGVCPDiscovery(GVCPSocketInfo socketInfo);
    bool sendGVCPForceIP(GVCPSocketInfo socketInfo, std::string mac, const OBNetIpConfig &config);

#if defined(__APPLE__)
    const uint8_t *getMACAddress(struct ifaddrs *ifap, const char *interface_name);
#endif

    //
    void checkAndUpdateSockets();

    SOCKET openClientRecvSocket(SOCKET srcSock);

private:
    SOCKET                      socks_[MAX_SOCKETS];
    GVCPSocketInfo              socketInfos_[MAX_SOCKETS];
    int                         sockCount_ = 0;
    std::vector<GVCPDeviceInfo> devInfoList_;
    std::mutex                  queryMtx_;
    std::mutex                  devInfoListMtx_;

#ifndef WIN32
    std::set<std::string> ipAddressStrSet_;
#endif
};

}  // namespace libobsensor
