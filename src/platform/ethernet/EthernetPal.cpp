// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "EthernetPal.hpp"
#include "exception/ObException.hpp"
#include "utils/Utils.hpp"
#include "RTPStreamPort.hpp"
#include "logger/Logger.hpp"

namespace libobsensor {

const uint16_t DEFAULT_CMD_PORT                     = 8090;
const uint16_t DEVICE_WATCHER_POLLING_INTERVAL_MSEC = 2000;

NetDeviceWatcher::~NetDeviceWatcher() noexcept {
    if(!stopWatch_) {
        stop();
    }
}

void NetDeviceWatcher::start(deviceChangedCallback callback) {
    callback_          = callback;
    stopWatch_         = false;
    deviceWatchThread_ = std::thread([&]() {
        std::mutex                   mutex;
        std::unique_lock<std::mutex> lock(mutex);
        while(!stopWatch_) {
            auto list    = GVCPClient::instance().queryNetDeviceList();
            auto added   = utils::subtract_sets(list, netDevInfoList_);
            auto removed = utils::subtract_sets(netDevInfoList_, list);
            for(auto &&info: removed) {
                callback_(OB_DEVICE_REMOVED, info.mac);
            }
            for(auto &&info: added) {
                callback_(OB_DEVICE_ARRIVAL, info.mac);
            }

            netDevInfoList_ = list;
            condVar_.wait_for(lock, std::chrono::milliseconds(DEVICE_WATCHER_POLLING_INTERVAL_MSEC), [&]() { return stopWatch_; });
        }
    });
}

void NetDeviceWatcher::stop() {
    stopWatch_ = true;
    condVar_.notify_all();
    if(deviceWatchThread_.joinable()) {
        deviceWatchThread_.join();
    }
}

std::shared_ptr<ISourcePort> EthernetPal::getSourcePort(std::shared_ptr<const SourcePortInfo> portInfo) {
    std::unique_lock<std::mutex> lock(sourcePortMapMutex_);
    std::shared_ptr<ISourcePort> port;
    // clear expired weak_ptr
    for(auto it = sourcePortMap_.begin(); it != sourcePortMap_.end();) {
        if(it->second.expired()) {
            it = sourcePortMap_.erase(it);
        }
        else {
            ++it;
        }
    }

    // check if the port already exists in the map
    for(const auto &pair: sourcePortMap_) {
        if(pair.first == portInfo) {
            port = pair.second.lock();
            if(port != nullptr) {
                return port;
            }
        }
    }

    const auto &portType = portInfo->portType;
    switch(portType) {
    case SOURCE_PORT_NET_VENDOR:
        port = std::make_shared<VendorNetDataPort>(std::dynamic_pointer_cast<const NetSourcePortInfo>(portInfo));
        break;
    case SOURCE_PORT_NET_VENDOR_STREAM:
        port = std::make_shared<NetDataStreamPort>(std::dynamic_pointer_cast<const NetDataStreamPortInfo>(portInfo));
        break;
    case SOURCE_PORT_NET_RTSP:
        port = std::make_shared<RTSPStreamPort>(std::dynamic_pointer_cast<const RTSPStreamPortInfo>(portInfo));
        break;
    case SOURCE_PORT_NET_RTP:
        port = std::make_shared<RTPStreamPort>(std::dynamic_pointer_cast<const RTPStreamPortInfo>(portInfo));
        break;
    default:
        throw invalid_value_exception("Invalid port type!");
    }
    sourcePortMap_.insert(std::make_pair(portInfo, port));
    return port;
}

SourcePortInfoList EthernetPal::querySourcePortInfos() {
    auto infos = GVCPClient::instance().queryNetDeviceList();

    auto added         = utils::subtract_sets(infos, netDeviceInfoList_);
    auto removed       = utils::subtract_sets(netDeviceInfoList_, infos);
    netDeviceInfoList_ = infos;

    // Only re-query port information for newly online devices
    for(auto &&info: added) {
        sourcePortInfoList_.push_back(std::make_shared<NetSourcePortInfo>(SOURCE_PORT_NET_VENDOR, info.netInterfaceName, info.localMac, info.localIp, info.ip,
                                                                          DEFAULT_CMD_PORT, info.mac, info.sn, info.pid));
    }

    // Delete devices that have been offline from the list
    for(auto &&info: removed) {
        auto iter = sourcePortInfoList_.begin();
        while(iter != sourcePortInfoList_.end()) {
            auto item = std::dynamic_pointer_cast<const NetSourcePortInfo>(*iter);
            if(item->address == info.ip && item->mac == info.mac && item->serialNumber == info.sn) {
                iter = sourcePortInfoList_.erase(iter);
            }
            else {
                ++iter;
            }
        }
    }
    return sourcePortInfoList_;
}

std::shared_ptr<IDeviceWatcher> EthernetPal::createDeviceWatcher() const {
    return std::make_shared<NetDeviceWatcher>();
}

std::shared_ptr<IPal> createNetPal() {
    return std::make_shared<EthernetPal>();
}

bool EthernetPal::changeNetDeviceIpConfig(std::string ipAddress, const OBNetIpConfig &config) {
    utils::unusedVar(ipAddress);
    utils::unusedVar(config);
#ifdef _WIN32
    return GVCPClient::instance().changeNetDeviceIpConfig(ipAddress, config);
#else
    return false;
#endif
}
}  // namespace libobsensor

