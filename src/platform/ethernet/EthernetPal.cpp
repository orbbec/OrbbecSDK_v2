#include "EthernetPal.hpp"
#include "exception/ObException.hpp"
#include "utils/Utils.hpp"
#include "RTPStreamPort.hpp"
#include "logger/Logger.hpp"

namespace libobsensor {

const uint16_t DEFAULT_CMD_PORT                     = 8090;
const uint16_t PID_FEMTO_MEGA                       = 0x0669;
const uint16_t PID_FEMTO_MEGA_I                     = 0x06C0;
const uint16_t PID_GEMINI2XL                        = 0x0671;
const uint16_t PID_GEMINI335LE                      = 0x080E;
const uint16_t DEVICE_WATCHER_POLLING_INTERVAL_MSEC = 5000;

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
                bool disconnected = true;
                // bool    exception    = false;
                // uint8_t retry        = 1;

                // todo: recover this part for Gemini2XL
                // if(info.pid == PID_GEMINI2XL) {
                //     do {
                //         BEGIN_TRY_EXECUTE({
                //             auto netVendorPortInfo = std::make_shared<NetSourcePortInfo>(SOURCE_PORT_NET_VENDOR, info.ip, DEFAULT_CMD_PORT);
                //             //auto            netVendorPort = std::make_shared<VendorNetDataPort>(netVendorPortInfo, 500, 500);
                //             auto            netVendorPort = std::make_shared<VendorNetDataPort>(netVendorPortInfo);
                //             auto            Protocol      = std::make_shared<Protocol>(netVendorPort);
                //             auto            command       = std::make_shared<VendorCommand>(Protocol);
                //             OBPropertyValue pidValue{};
                //             command->getPropertyValue(OB_PROP_PID_INT, &pidValue);
                //             if(pidValue.intValue != PID_GEMINI2XL) {
                //                 LOG_WARN("Create socket succeed ip:{} port:{},but pid is invalid {}", info.ip, DEFAULT_CMD_PORT, pidValue.intValue);
                //             }
                //             netDevInfoList_.push_back(info);
                //             disconnected = false;
                //         })
                //         CATCH_EXCEPTION_AND_EXECUTE({
                //             exception = true;
                //             LOG_WARN("Create socket failed ip:{} port:{},device is disconnect.", info.ip, DEFAULT_CMD_PORT);
                //         })
                //     } while(exception && retry--> 0);
                // }
                if(disconnected) {
                    callback_(OB_DEVICE_REMOVED, info.mac);
                }
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
        // todo: fixme
        // if(info.pid == 0) {
        //     TRY_EXECUTE({
        //         auto            netVendorPortInfo = std::make_shared<NetSourcePortInfo>(SOURCE_PORT_NET_VENDOR, info.ip, DEFAULT_CMD_PORT);
        //         auto            netVendorPort     = std::make_shared<VendorNetDataPort>(netVendorPortInfo);
        //         auto            Protocol          = std::make_shared<Protocol>(netVendorPort);
        //         auto            command           = std::make_shared<VendorCommand>(Protocol);
        //         OBPropertyValue pidValue{};
        //         BEGIN_TRY_EXECUTE({
        //             command->getPropertyValue(OB_PROP_PID_INT, &pidValue);
        //             info.pid = pidValue.intValue;
        //         })
        //         CATCH_EXCEPTION_AND_EXECUTE({ info.pid = PID_FEMTO_MEGA; })
        //     })
        // }

        sourcePortInfoList_.push_back(std::make_shared<NetSourcePortInfo>(SOURCE_PORT_NET_VENDOR, info.ip, DEFAULT_CMD_PORT, info.mac, info.sn, info.pid));
        if(info.pid == PID_FEMTO_MEGA) {
            sourcePortInfoList_.push_back(
                std::make_shared<RTSPStreamPortInfo>(info.ip, static_cast<uint16_t>(8888), DEFAULT_CMD_PORT, OB_STREAM_COLOR, info.mac, info.sn, info.pid));
            sourcePortInfoList_.push_back(
                std::make_shared<RTSPStreamPortInfo>(info.ip, static_cast<uint16_t>(8554), DEFAULT_CMD_PORT, OB_STREAM_DEPTH, info.mac, info.sn, info.pid));
            sourcePortInfoList_.push_back(
                std::make_shared<RTSPStreamPortInfo>(info.ip, static_cast<uint16_t>(8554), DEFAULT_CMD_PORT, OB_STREAM_IR, info.mac, info.sn, info.pid));
            sourcePortInfoList_.push_back(
                std::make_shared<NetDataStreamPortInfo>(info.ip, static_cast<uint16_t>(8900), DEFAULT_CMD_PORT, info.mac, info.sn, info.pid));
<<<<<<< HEAD
        }
        else if(info.pid == PID_GEMINI335LE) {
            LOG_INFO("Create 335L rtp stream portInfo.");
            sourcePortInfoList_.push_back(
                std::make_shared<RTPStreamPortInfo>(info.ip, static_cast<uint16_t>(20000), DEFAULT_CMD_PORT, OB_STREAM_COLOR, info.mac, info.sn, info.pid));
            sourcePortInfoList_.push_back(
                std::make_shared<RTPStreamPortInfo>(info.ip, static_cast<uint16_t>(20002), DEFAULT_CMD_PORT, OB_STREAM_DEPTH, info.mac, info.sn, info.pid));
            sourcePortInfoList_.push_back(
                std::make_shared<RTPStreamPortInfo>(info.ip, static_cast<uint16_t>(20004), DEFAULT_CMD_PORT, OB_STREAM_IR_LEFT, info.mac, info.sn, info.pid));
            sourcePortInfoList_.push_back(
                std::make_shared<RTPStreamPortInfo>(info.ip, static_cast<uint16_t>(20006), DEFAULT_CMD_PORT, OB_STREAM_IR_RIGHT, info.mac, info.sn, info.pid));
            sourcePortInfoList_.push_back(
                std::make_shared<RTPStreamPortInfo>(info.ip, static_cast<uint16_t>(20010), DEFAULT_CMD_PORT, OB_STREAM_ACCEL, info.mac, info.sn, info.pid));
=======
>>>>>>> develop
        }
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

SourcePortInfoList EthernetPal::queryAssociatedNetSourcePort(const std::shared_ptr<NetSourcePortInfo> info) {
    // todo: refactor this code, move it to device business layer
    SourcePortInfoList list;

    // todo: move this call to device business layer
    if(info->pid == PID_FEMTO_MEGA || info->pid == PID_FEMTO_MEGA_I) {
        list.emplace_back(std::make_shared<RTSPStreamPortInfo>(info->address, static_cast<uint16_t>(8888), info->port, OB_STREAM_COLOR, info->mac,
                                                               info->serialNumber, info->pid));
        list.emplace_back(std::make_shared<RTSPStreamPortInfo>(info->address, static_cast<uint16_t>(8554), info->port, OB_STREAM_DEPTH, info->mac,
                                                               info->serialNumber, info->pid));
        list.emplace_back(std::make_shared<RTSPStreamPortInfo>(info->address, static_cast<uint16_t>(8554), info->port, OB_STREAM_IR, info->mac,
                                                               info->serialNumber, info->pid));
        list.emplace_back(
            std::make_shared<NetDataStreamPortInfo>(info->address, static_cast<uint16_t>(8900), info->port, info->mac, info->serialNumber, info->pid));
    }
    else if(info->pid == PID_GEMINI2XL) {
        list.emplace_back(std::make_shared<RTSPStreamPortInfo>(info->address, static_cast<uint16_t>(8888), info->port, OB_STREAM_COLOR, info->mac,
                                                               info->serialNumber, info->pid));
        list.emplace_back(std::make_shared<RTSPStreamPortInfo>(info->address, static_cast<uint16_t>(8554), info->port, OB_STREAM_DEPTH, info->mac,
                                                               info->serialNumber, info->pid));
        list.emplace_back(std::make_shared<RTSPStreamPortInfo>(info->address, static_cast<uint16_t>(8555), info->port, OB_STREAM_IR_LEFT, info->mac,
                                                               info->serialNumber, info->pid));
        list.emplace_back(std::make_shared<RTSPStreamPortInfo>(info->address, static_cast<uint16_t>(8556), info->port, OB_STREAM_IR_RIGHT, info->mac,
                                                               info->serialNumber, info->pid));
        list.emplace_back(
            std::make_shared<NetDataStreamPortInfo>(info->address, static_cast<uint16_t>(8900), info->port, info->mac, info->serialNumber, info->pid));
    }
    // todo: fixme
    // else {
    //     auto                                 netVendorPortInfo = std::make_shared<NetSourcePortInfo>(SOURCE_PORT_NET_VENDOR, info->address, info->port);
    //     auto                                 netVendorPort     = std::make_shared<VendorNetDataPort>(netVendorPortInfo);
    //     auto                                 Protocol          = std::make_shared<Protocol>(netVendorPort);
    //     auto                                 command           = std::make_shared<VendorCommand>(Protocol);
    //     std::vector<OBNetworkStreamPortInfo> networkStreamPortInfos;
    //     std::vector<uint8_t>                 dataVec;
    //     TRY_EXECUTE({
    //         command->getRawData(
    //             OB_RAW_DATA_NET_PORT_INFO_LIST,
    //             [&](OBDataTranState state, OBDataChunk *dataChunk) {
    //                 if(state == DATA_TRAN_STAT_TRANSFERRING) {
    //                     dataVec.insert(dataVec.end(), dataChunk->data, dataChunk->data + dataChunk->size);
    //                 }
    //             },
    //             false);
    //         if(!dataVec.empty()) {
    //             networkStreamPortInfos = processNetworkStreamPortInfoList(dataVec.data(), dataVec.size());
    //         }
    //     });
    //     for(auto streamPortInfo: networkStreamPortInfos) {
    //         if(streamPortInfo.sourcePortType == SOURCE_PORT_NET_VENDOR_STREAM) {
    //             list.emplace_back(
    //                 std::make_shared<NetDataStreamPortInfo>(info->address, streamPortInfo.port, info->port, info->mac, info->serialNumber, info->pid));
    //         }
    //         else if(streamPortInfo.sourcePortType == SOURCE_PORT_NET_RTSP) {
    //             list.emplace_back(std::make_shared<RTSPStreamPortInfo>(info->address, streamPortInfo.port, info->port, streamPortInfo.streamType, info->mac,
    //                                                                    info->serialNumber, info->pid));
    //         }
    //     }
    // }
    return list;
}

}  // namespace libobsensor
