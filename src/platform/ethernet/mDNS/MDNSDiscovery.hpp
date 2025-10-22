// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include <vector>
#include <string>
#include <mutex>
#include <set>
#include <memory>
#include <thread>
#include <condition_variable>
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
        return other.ip == ip && other.port == port && other.mac == mac;
    }
    virtual ~MDNSDeviceInfo() {}
} MDNSDeviceInfo;

/**
 * @brief mDNS socket info
 */
struct MDNSSocketInfo {
    SOCKET      sock = INVALID_SOCKET;
    std::string ip   = "unknown";
};

/**
 * @brief mDNS query replies
 */
typedef struct MDNSAckData {
    std::string              ip   = "";
    uint16_t                 port = 0;
    std::string              pid  = "";
    std::string              name = "";  // port srv name
    std::vector<std::string> txtList;    // data of record TXT
} MDNSAckData;

/**
 * @brief mDNS device discovery
 */
class MDNSDiscovery {
public:
    ~MDNSDiscovery();

    static std::shared_ptr<MDNSDiscovery> getInstance();

    /**
     * @brief query mDNS device list
     *
     * @return device list
     */
    std::vector<MDNSDeviceInfo> queryDeviceList();

    /**
     * @brief refresh mDNS query, clear socket cache
     *
     */
    void refreshQuery();

private:
    MDNSDiscovery();

    std::vector<MDNSSocketInfo> openClientSockets();
    void                        closeClientSockets(std::vector<MDNSSocketInfo> &socks);

    void MDNSQuery(std::vector<MDNSSocketInfo> &socks, int timeoutSec = 1, int timeoutUsec = 0);
    bool receiveMDNSResponses(const std::vector<MDNSSocketInfo> &sockInfos, uint8_t *buffer, size_t bufferSize, int timeoutSec, int timeoutUsec);

    std::string    findTxtRecord(const std::vector<std::string> &txtList, const std::string &key, const std::string &defValue);
    MDNSDeviceInfo parseDeviceInfo(const MDNSAckData &ack);

private:
    std::vector<MDNSDeviceInfo>         devInfoList_;
    std::mutex                          devInfoListMtx_;
    std::mutex                          queryMtx_;
    std::vector<MDNSSocketInfo>         cachedSockInfos_;
    std::mutex                          cacheMtx_;

    static std::mutex                   instanceMutex_;
    static std::weak_ptr<MDNSDiscovery> instanceWeakPtr_;
};

}  // namespace libobsensor
