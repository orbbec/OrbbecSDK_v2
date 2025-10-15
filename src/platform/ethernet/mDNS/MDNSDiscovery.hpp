// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include <vector>
#include <string>
#include <mutex>
#include <set>
#include "ethernet/socket/SocketTypes.hpp"

namespace libobsensor {

/**
 * @brief mDNS device info
 */
typedef struct MDNSDeviceInfo {
    std::string ip    = "unknown";
    std::string sn    = "unknown";
    std::string model = "unknown";
    std::string mac   = "unknown"; // unique
    uint16_t    port  = 0;
    uint32_t    pid   = 0;

    virtual bool operator==(const MDNSDeviceInfo &other) const {
        return other.mac == mac;
    }
    virtual ~MDNSDeviceInfo() {}
} MDNSDeviceInfo;

/**
 * @brief mDNS device discovery
 */
class MDNSDiscovery {
public:
    ~MDNSDiscovery();

    static std::shared_ptr<MDNSDiscovery> getInstance();

    std::vector<MDNSDeviceInfo> queryDeviceList();

private:
    MDNSDiscovery();

    std::vector<SOCKET> openClientSockets();
    void                closeClientSockets(std::vector<SOCKET> &socks);
    void                sendAndRecvMDNSQuery(SOCKET sock);
    std::string         findTxtRecord(const std::vector<std::string> &txtList, const std::string &key);

private:
    std::vector<MDNSDeviceInfo>         devInfoList_;
    std::mutex                          queryMtx_;
    std::mutex                          devInfoListMtx_;
    static std::mutex                   instanceMutex_;
    static std::weak_ptr<MDNSDiscovery> instanceWeakPtr_;
};

}  // namespace libobsensor
