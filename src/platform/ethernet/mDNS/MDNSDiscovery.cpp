// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#if(defined(WIN32) || defined(_WIN32) || defined(WINCE))
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
#endif

#include <iostream>
#include <string.h>
#include <thread>
#include <algorithm>
#include <functional>

#include "MDNSDiscovery.hpp"
#include "MDNSUtil.hpp"
#include "exception/ObException.hpp"

#include "logger/LoggerInterval.hpp"
#include "logger/LoggerHelper.hpp"
#include "utils/StringUtils.hpp"
#include "mdns/mdns.h"

namespace libobsensor {

/**
 * @brief mDNS query replies
 */
typedef struct MDNSAckData {
    std::string              ip = "";
    uint16_t                 port = 0;
    std::string              pid = "";
    std::string              name = "";     // port srv name
    std::vector<std::string> txtList;  // data of record TXT
} MDNSAckData;

static std::string mDNSStrToStdString(mdns_string_t mdnsStr) {
    size_t len = strlen(mdnsStr.str);
    if(len > mdnsStr.length) {
        len = mdnsStr.length;
    }

    return std::string(mdnsStr.str, len);
}

// Callback handling parsing answers to queries sent
static int query_callback(int sock, const struct sockaddr *from, size_t addrlen, mdns_entry_type_t entry, uint16_t query_id, uint16_t rtype, uint16_t rclass,
                          uint32_t ttl, const void *data, size_t size, size_t name_offset, size_t name_length, size_t record_offset, size_t record_length,
                          void *user_data) {
    (void)from;
    (void)addrlen;
    (void)sizeof(sock);
    (void)sizeof(query_id);
    (void)rclass;
    (void)sizeof(name_length);
    (void)ttl;

    // char              addrbuffer[64];
    char              entrybuffer[256];
    char              namebuffer[256];
    mdns_record_txt_t txtbuffer[128];
    MDNSAckData *     ack = static_cast<MDNSAckData *>(user_data);

    if(entry != MDNS_ENTRYTYPE_ANSWER) {
        return 0;
    }

    // mdns_string_t fromaddrstr = MDNSUtil::ip_address_to_string(addrbuffer, sizeof(addrbuffer), from, addrlen);
    mdns_string_t entrystr = mdns_string_extract(data, size, &name_offset, entrybuffer, sizeof(entrybuffer));

    if(rtype == MDNS_RECORDTYPE_PTR) {
        // ignore
        // mdns_string_t namestr = mdns_record_parse_ptr(data, size, record_offset, record_length, namebuffer, sizeof(namebuffer));
    }
    else if(rtype == MDNS_RECORDTYPE_SRV) {
        // port
        mdns_record_srv_t srv = mdns_record_parse_srv(data, size, record_offset, record_length, namebuffer, sizeof(namebuffer));
        // save name and port
        ack->name = mDNSStrToStdString(entrystr);
        ack->port = srv.port;
    }
    else if(rtype == MDNS_RECORDTYPE_A) {
        struct sockaddr_in addr;
        mdns_record_parse_a(data, size, record_offset, record_length, &addr);
        mdns_string_t addrstr = MDNSUtil::ipv4_address_to_string(namebuffer, sizeof(namebuffer), &addr, sizeof(addr));
        ack->ip               = mDNSStrToStdString(addrstr);
    }
    else if(rtype == MDNS_RECORDTYPE_AAAA) {
        // ipv6 ignore
        // struct sockaddr_in6 addr;
        // mdns_record_parse_aaaa(data, size, record_offset, record_length, &addr);
        // mdns_string_t addrstr = MDNSUtil::ipv6_address_to_string(namebuffer, sizeof(namebuffer), &addr, sizeof(addr));
    }
    else if(rtype == MDNS_RECORDTYPE_TXT) {
        // txt: sn and model
        size_t parsed = mdns_record_parse_txt(data, size, record_offset, record_length, txtbuffer, sizeof(txtbuffer) / sizeof(mdns_record_txt_t));
        for(size_t itxt = 0; itxt < parsed; ++itxt) {
            std::string txt = mDNSStrToStdString(txtbuffer[itxt].key);
            if(txtbuffer[itxt].value.length) {
                txt += mDNSStrToStdString(txtbuffer[itxt].value);
            }
            ack->txtList.push_back(txt);
        }
    }

    return 0;
}

std::mutex                   MDNSDiscovery::instanceMutex_;
std::weak_ptr<MDNSDiscovery> MDNSDiscovery::instanceWeakPtr_;

std::shared_ptr<MDNSDiscovery> MDNSDiscovery::getInstance() {
    std::unique_lock<std::mutex> lock(instanceMutex_);
    auto                         ctxInstance = instanceWeakPtr_.lock();
    if(!ctxInstance) {
        ctxInstance      = std::shared_ptr<MDNSDiscovery>(new MDNSDiscovery());
        instanceWeakPtr_ = ctxInstance;
    }
    return ctxInstance;
}

MDNSDiscovery::MDNSDiscovery() {
#if(defined(WIN32) || defined(_WIN32) || defined(WINCE))
    WSADATA wsaData;
    if(WSAStartup(MAKEWORD(2, 2), &wsaData)) {
        throw libobsensor::invalid_value_exception(utils::string::to_string() << "Failed to initialize WinSock! err_code=" << GET_LAST_ERROR());
    }
#endif
}

MDNSDiscovery::~MDNSDiscovery() {
#if(defined(WIN32) || defined(_WIN32) || defined(WINCE))
    WSACleanup();
#endif
}

std::vector<SOCKET> MDNSDiscovery::openClientSockets() {
    std::vector<SOCKET> socks;

#if(defined(WIN32) || defined(_WIN32) || defined(WINCE))
    DWORD                                                                   ret, size;
    std::unique_ptr<IP_ADAPTER_ADDRESSES, void (*)(IP_ADAPTER_ADDRESSES *)> adapterAddresses(nullptr, [](IP_ADAPTER_ADDRESSES *p) { free(p); });

    ret = GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, NULL, NULL, &size);
    if(ret != ERROR_BUFFER_OVERFLOW) {
        LOG_INTVL(LOG_INTVL_OBJECT_TAG + "openClientSockets", MAX_LOG_INTERVAL, spdlog::level::debug, "GetAdaptersAddresses failed with error:{}",
                  GET_LAST_ERROR());
        return socks;
    }
    adapterAddresses.reset(reinterpret_cast<IP_ADAPTER_ADDRESSES *>(malloc(size)));

    ret = GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, NULL, adapterAddresses.get(), &size);
    if(ret != ERROR_SUCCESS) {
        LOG_INTVL(LOG_INTVL_OBJECT_TAG + "openClientSockets", MAX_LOG_INTERVAL, spdlog::level::debug, "GetAdaptersAddresses failed with error:{}",
                  GET_LAST_ERROR());
        return socks;
    }

    for(PIP_ADAPTER_ADDRESSES aa = adapterAddresses.get(); aa != NULL; aa = aa->Next) {
        if(aa->TunnelType == TUNNEL_TYPE_TEREDO)
            continue;
        if(aa->OperStatus != IfOperStatusUp)
            continue;

        for(PIP_ADAPTER_UNICAST_ADDRESS ua = aa->FirstUnicastAddress; ua != NULL; ua = ua->Next) {
            // just support IPV4
            if(ua->Address.lpSockaddr->sa_family == AF_INET) {
                SOCKADDR_IN addrSrv;
                addrSrv            = *(SOCKADDR_IN *)ua->Address.lpSockaddr;
                addrSrv.sin_family = AF_INET;
                addrSrv.sin_port   = htons(MDNS_PORT);  // 5353
                auto ipStr         = inet_ntoa(addrSrv.sin_addr);
                // "169.254.xxx.xxx" is link-local address for windows, "127.0.0.1" is loopback address
                if(strncmp(ipStr, "169.254", 7) == 0 || strcmp(ipStr, "127.0.0.1") == 0 ) {
                    continue;
                }
                int sock = mdns_socket_open_ipv4(&addrSrv);
                if(sock >= 0) {
                    socks.push_back(sock);
                }
                else {
                    LOG_INTVL(LOG_INTVL_OBJECT_TAG + "openClientSockets", MAX_LOG_INTERVAL, spdlog::level::debug, "open ip({}) failed with error:{}",
                              ipStr, GET_LAST_ERROR());
                }
            }
        }
    }
#else
    struct ifaddrs *ifaddr, *ifa;
    int             family, n;

    if(getifaddrs(&ifaddr) == -1) {
        LOG_INTVL(LOG_INTVL_OBJECT_TAG + "openClientSockets", MAX_LOG_INTERVAL, spdlog::level::debug, "getifaddrs failed with error:{}", GET_LAST_ERROR());
        return socks;
    }

    for(ifa = ifaddr, n = 0; ifa != NULL; ifa = ifa->ifa_next, n++) {
        if(ifa->ifa_addr == NULL) {
            continue;
        }
        if(!(ifa->ifa_flags & IFF_UP) || !(ifa->ifa_flags & IFF_MULTICAST)) {
            continue;
        }
        if((ifa->ifa_flags & IFF_LOOPBACK) || (ifa->ifa_flags & IFF_POINTOPOINT)) {
            continue;
        }
        family = ifa->ifa_addr->sa_family;

        // just support IPV4
        if(family == AF_INET) {
            SOCKADDR_IN addrSrv;
            addrSrv            = *(SOCKADDR_IN *)ifa->ifa_addr;
            addrSrv.sin_family = AF_INET;
            addrSrv.sin_port   = htons(MDNS_PORT);  // 5353
            auto ipStr         = inet_ntoa(addrSrv.sin_addr);
            if(strcmp(ipStr, "127.0.0.1") == 0) {
                continue;
            }
            int sock = mdns_socket_open_ipv4(&addrSrv);
            if(sock >= 0) {
                socks.push_back(sock);
            }
            else {
                LOG_INTVL(LOG_INTVL_OBJECT_TAG + "openClientSockets", MAX_LOG_INTERVAL, spdlog::level::debug, "open ip({}) failed with error:{}", ipStr,
                          GET_LAST_ERROR());
            }
        }
    }
    freeifaddrs(ifaddr);
#endif

    return socks;
}

void MDNSDiscovery::closeClientSockets(std::vector<SOCKET> &socks) {
    for(auto sock: socks) {
        closesocket(sock);
    }
    socks.clear();
}

void MDNSDiscovery::sendAndRecvMDNSQuery(SOCKET sock) {
    uint8_t buffer[4096] = { 0 };
    size_t  capacity     = sizeof(buffer) / sizeof(buffer[0]);

    // Sending mDNS query
    int id = mdns_multiquery_send(static_cast<int>(sock), NULL, 0, buffer, capacity, 0);
    if(id < 0) {
        LOG_INTVL(LOG_INTVL_OBJECT_TAG + "sendAndRecvMDNSQuery", MAX_LOG_INTERVAL, spdlog::level::debug, "mdns_multiquery_send failed with error:{}",
                  GET_LAST_ERROR());
    }

    // Reading mDNS query replies
    int    res;
    int    nfds = 0;
    fd_set readfs;

    FD_ZERO(&readfs);
    nfds = static_cast<int>(sock) + 1;
    FD_SET(sock, &readfs);

    struct timeval timeout;
    timeout.tv_sec  = 1;
    timeout.tv_usec = 0;

    res = select(nfds, &readfs, 0, 0, &timeout);
    if(res > 0) {
        if(FD_ISSET(sock, &readfs)) {
            do {
                // for udp, read one packet at a time
                MDNSAckData ack;
                memset(buffer, 0, capacity);
                size_t      rec = mdns_query_recv(static_cast<int>(sock), buffer, capacity, query_callback, &ack, id);
                if(rec <= 0) {
                    LOG_INTVL(LOG_INTVL_OBJECT_TAG + "sendAndRecvMDNSQuery", MAX_LOG_INTERVAL, spdlog::level::debug,
                              "mdns_query_recv failed with error:{}", GET_LAST_ERROR());
                    break;
                }

                if(ack.name.find("_oradar_udp") == std::string::npos || ack.ip.empty()) {
                    LOG_INTVL(LOG_INTVL_OBJECT_TAG + "sendAndRecvMDNSQuery", MAX_LOG_INTERVAL, spdlog::level::debug,
                              "Invalid device, srv name: {}, ip: {}, port, {}", ack.name, ack.ip, ack.port);
                    continue;
                }

                auto formatMacAddress = [](const std::string &mac) -> std::string {
                    return mac.substr(0, 2) + ":" + mac.substr(2, 2) + ":" + mac.substr(4, 2) + ":" + mac.substr(6, 2) + ":" + mac.substr(8, 2) + ":"
                           + mac.substr(10, 2);
                };

                MDNSDeviceInfo info;

                info.ip   = ack.ip;
                info.port = ack.port;
                auto mac  = findTxtRecord(ack.txtList, "MAC", "");
                if(mac.length() == 12) {
                    info.mac = formatMacAddress(mac);
                }
                else {
                    info.mac = info.ip + ":" + std::to_string(info.port);
                }

                info.sn    = findTxtRecord(ack.txtList, "SN", "unknown");
                info.model = findTxtRecord(ack.txtList, "MODEL", "");
                auto pid   = findTxtRecord(ack.txtList, "PID", "");
                if(!pid.empty()) {
                    try {
                        uint32_t uintPid = std::stoul(pid, nullptr, 16);
                        info.pid         = static_cast<uint16_t>(uintPid);
                    }
                    catch(const std::exception &e) {
                        (void)e;
                        info.pid = 0;
                        LOG_INTVL(LOG_INTVL_OBJECT_TAG + "sendAndRecvMDNSQuery", MAX_LOG_INTERVAL, spdlog::level::debug,
                                  "Parse PID failed. pid: {}, ip: {}, port, {}", pid, ack.ip, ack.port);
                    }
                }

                std::lock_guard<std::mutex> lock(devInfoListMtx_);
                // check for duplication
                auto it = std::find_if(devInfoList_.begin(), devInfoList_.end(), [&info](const MDNSDeviceInfo &item) { return item == info; });
                if(it == devInfoList_.end()) {
                    // TODO: It is recommended to send a command to get the informations
                    // pid
                    if(info.pid == 0) {
                        if(info.model == "ME450") {
                            info.pid = 0x1302;  // multi-lines
                        }
                        else if(info.model == "MS600") {
                            info.pid = 0x1300;  // single-line
                        }
                        else if(info.model == "SL450") {
                            info.pid = 0x1301;  // single-line
                        }
                    }
                    devInfoList_.emplace_back(info);
                }
            } while(0);
        }
        FD_SET(sock, &readfs);
    }
}

std::string MDNSDiscovery::findTxtRecord(const std::vector<std::string> &txtList, const std::string &key, const std::string &defValue) {
    // data format: "key:value,key:value,key:value"
    for(const auto &str: txtList) {
        std::istringstream iss(str);
        std::string        token;
        while(std::getline(iss, token, ',')) {
            size_t pos = token.find(':');
            if(pos != std::string::npos && token.substr(0, pos) == key) {
                return token.substr(pos + 1);
            }
        }
    }
    return defValue;  // not found
}

std::vector<MDNSDeviceInfo> MDNSDiscovery::queryDeviceList() {
    std::lock_guard<std::mutex> lck(queryMtx_);
    auto lastDevList = devInfoList_;
    devInfoList_.clear();

    std::vector<SOCKET> socks = openClientSockets();
    if(socks.empty()) {
        return devInfoList_;
    }

    if(socks.size() > 1) {
        std::vector<std::thread> threads;
        for(auto sock: socks) {
            auto func = std::bind(&MDNSDiscovery::sendAndRecvMDNSQuery, this, sock);
            threads.emplace_back(func, sock);
        }

        for(auto &thread: threads) {
            thread.join();
        }
    }
    else {
        sendAndRecvMDNSQuery(socks[0]);
    }

    closeClientSockets(socks);

    if (lastDevList != devInfoList_) {
        LOG_DEBUG("queryMDNSDevice completed ({}):", devInfoList_.size());
        for(auto &&info: devInfoList_) {
            LOG_DEBUG("\t- ip:{}, port:{}, model:{}, sn:{}, pid:0x{:04x}", info.ip, info.port, info.model, info.sn, info.pid);
        }
    }
    return devInfoList_;
}

}  // namespace libobsensor
