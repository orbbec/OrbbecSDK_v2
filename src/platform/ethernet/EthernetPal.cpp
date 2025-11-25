// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "EthernetPal.hpp"
#include "exception/ObException.hpp"
#include "utils/Utils.hpp"
#include "mDNS/MDNSDiscovery.hpp"
#include "LiDARDataStreamPort.hpp"
#include "RTPStreamPort.hpp"
#include "logger/Logger.hpp"

namespace libobsensor {

const uint16_t DEFAULT_CMD_PORT                           = 8090;
const uint16_t DEVICE_WATCHER_POLLING_INTERVAL_MSEC       = 3000;
const uint16_t DEVICE_WATCHER_POLLING_SHORT_INTERVAL_MSEC = 1000;

EthernetPal::EthernetPal() {
    mdnsDiscovery_ = MDNSDiscovery::getInstance();
}

EthernetPal::~EthernetPal() noexcept {
    mdnsDiscovery_.reset();
    if(!stopWatch_) {
        stop();
    }
}

void EthernetPal::start(deviceChangedCallback callback) {
    callback_  = callback;
    stopWatch_ = false;

    auto getNow = []() {
        // get now
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    };

    // GVCP device
    deviceWatchThread_ = std::thread([&]() {
        std::mutex                   mutex;
        std::unique_lock<std::mutex> lock(mutex);
        while(!stopWatch_) {
            auto list    = GVCPClient::instance().queryNetDeviceList();
            auto start   = getNow();
            auto added   = utils::subtract_sets(list, netDevInfoList_);
            auto removed = utils::subtract_sets(netDevInfoList_, list);
            updateSourcePortInfoList(added, removed);

            for(auto &&info: removed) {
                if(!callback_(OB_DEVICE_REMOVED, info.mac)) {
                    // if device is still online, restore it to the list
                    list.push_back(info);
                    updateSourcePortInfoList({ info }, {});
                }
            }
            for(auto &&info: added) {
                (void)callback_(OB_DEVICE_ARRIVAL, info.mac);
            }
            // update info list
            netDevInfoList_ = list;
            // calc the interval
            int64_t interval = DEVICE_WATCHER_POLLING_INTERVAL_MSEC;
            if(netDevInfoList_.empty()) {
                // Speed up discovery when no devices are found
                interval = DEVICE_WATCHER_POLLING_SHORT_INTERVAL_MSEC;
            }
            auto now = getNow();
            if(now >= start + interval) {
                // Callback takes too long, query the device list immediately for optimization
                continue;
            }
            interval = start + interval - now;
            condVar_.wait_for(lock, std::chrono::milliseconds(interval), [&]() { return stopWatch_.load(); });
        }
    });

    // mDNS device
    mdnsWatchThread_ = std::thread([&]() {
        std::mutex                   mutex;
        std::unique_lock<std::mutex> lock(mutex);
        int                          socketRefreshCounter    = 0;
        const int                    SOCKET_REFRESH_INTERVAL = 3;

        while(!stopWatch_) {
            auto list    = mdnsDiscovery_->queryDeviceList();
            auto start   = getNow();
            auto added   = utils::subtract_sets(list, mdnsDevInfoList_);
            auto removed = utils::subtract_sets(mdnsDevInfoList_, list);
            updateMDNSDeviceSourceInfo(added, removed);

            for(auto &&info: removed) {
                if(!callback_(OB_DEVICE_REMOVED, info.mac)) {
                    // if device is still online, restore it to the list
                    list.push_back(info);
                    updateMDNSDeviceSourceInfo({ info }, {});
                }
            }
            for(auto &&info: added) {
                callback_(OB_DEVICE_ARRIVAL, info.mac);
            }

            // refresh sockets periodically
            if(++socketRefreshCounter >= SOCKET_REFRESH_INTERVAL) {
                mdnsDiscovery_->refreshQuery();
                socketRefreshCounter = 0;
            }

            mdnsDevInfoList_ = list;

            // calc the interval
            int64_t interval = DEVICE_WATCHER_POLLING_INTERVAL_MSEC;
            if(netDevInfoList_.empty()) {
                // Speed up discovery when no devices are found
                interval = DEVICE_WATCHER_POLLING_SHORT_INTERVAL_MSEC;
            }
            auto now = getNow();
            if(now >= start + interval) {
                // Callback takes too long, query the device list immediately for optimization
                continue;
            }
            interval = start + interval - now;
            mdnsCondVar_.wait_for(lock, std::chrono::milliseconds(interval), [&]() { return stopWatch_.load(); });
        }

        mdnsDiscovery_->refreshQuery();
    });
}

void EthernetPal::stop() {
    stopWatch_ = true;
    condVar_.notify_all();
    mdnsCondVar_.notify_all();
    if(deviceWatchThread_.joinable()) {
        deviceWatchThread_.join();
    }
    if(mdnsWatchThread_.joinable()) {
        mdnsWatchThread_.join();
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
    case SOURCE_PORT_NET_LIDAR_VENDOR_STREAM:
        port = std::make_shared<LiDARDataStreamPort>(std::dynamic_pointer_cast<const LiDARDataStreamPortInfo>(portInfo));
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

void EthernetPal::updateMDNSDeviceSourceInfo(const std::vector<MDNSDeviceInfo> &added, const std::vector<MDNSDeviceInfo> &removed) {
    std::lock_guard<std::mutex> lock(sourcePortInfoMetux_);
    // Only re-query port information for newly online devices
    for(auto &&info: added) {
        auto portInfo              = std::make_shared<NetSourcePortInfo>(SOURCE_PORT_NET_VENDOR);
        portInfo->netInterfaceName = "unknown";
        portInfo->localMac         = "unknown";
        portInfo->localAddress     = "unknown";
        portInfo->address          = info.ip;
        portInfo->port             = info.port;
        portInfo->mac              = info.mac;
        portInfo->serialNumber     = info.sn;
        portInfo->pid              = info.pid;
        portInfo->vid              = ORBBEC_DEVICE_VID;
        sourcePortInfoList_.push_back(portInfo);
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
}

void EthernetPal::updateSourcePortInfoList(const std::vector<GVCPDeviceInfo> &added, const std::vector<GVCPDeviceInfo> &removed) {
    std::lock_guard<std::mutex> lock(sourcePortInfoMetux_);
    // Only re-query port information for newly online devices
    for(auto &&info: added) {
        auto portInfo               = std::make_shared<NetSourcePortInfo>(SOURCE_PORT_NET_VENDOR);
        portInfo->netInterfaceName  = info.netInterfaceName;
        portInfo->localMac          = info.localMac;
        portInfo->localAddress      = info.localIp;
        portInfo->address           = info.ip;
        portInfo->port              = DEFAULT_CMD_PORT;
        portInfo->mac               = info.mac;
        portInfo->serialNumber      = info.sn;
        portInfo->pid               = info.pid;
        portInfo->vid               = info.vid;
        portInfo->mask              = info.mask;
        portInfo->gateway           = info.gateway;
        portInfo->localSubnetLength = info.localSubnetLength;
        portInfo->localGateway      = info.localGateway;
        portInfo->devVersion        = info.devVersion;
        sourcePortInfoList_.push_back(portInfo);
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
}

SourcePortInfoList EthernetPal::querySourcePortInfos() {
    if(!deviceWatchThread_.joinable()) {
        // watcher thread is not runnig
        auto list       = GVCPClient::instance().queryNetDeviceList();
        auto added      = utils::subtract_sets(list, netDevInfoList_);
        auto removed    = utils::subtract_sets(netDevInfoList_, list);
        netDevInfoList_ = list;
        updateSourcePortInfoList(added, removed);
    }

    if(!mdnsWatchThread_.joinable()) {
        // mDNS watcher thread is not runnig
        auto list        = mdnsDiscovery_->queryDeviceList();
        auto added       = utils::subtract_sets(list, mdnsDevInfoList_);
        auto removed     = utils::subtract_sets(mdnsDevInfoList_, list);
        mdnsDevInfoList_ = list;

        updateMDNSDeviceSourceInfo(added, removed);
    }

    std::lock_guard<std::mutex> lock(sourcePortInfoMetux_);
    return sourcePortInfoList_;
}

std::shared_ptr<IDeviceWatcher> EthernetPal::createDeviceWatcher() const {
    auto self_nonconst = std::const_pointer_cast<EthernetPal>(shared_from_this());
    return std::static_pointer_cast<IDeviceWatcher>(self_nonconst);
    // return std::make_shared<NetDeviceWatcher>();
}

std::shared_ptr<IPal> createNetPal() {
    return std::make_shared<EthernetPal>();
}

bool EthernetPal::forceIpConfig(std::string macAddress, const OBNetIpConfig &config) {
    return GVCPClient::instance().forceIpConfig(macAddress, config);
}
}  // namespace libobsensor
