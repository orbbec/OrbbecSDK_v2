#include "NetDeviceEnumerator.hpp"
#include "femtomega/FemtoMegaDeviceInfo.hpp"
#include "gemini330/G330DeviceInfo.hpp"
#include "ethernet/RTSPStreamPort.hpp"
#include "ethernet/NetDataStreamPort.hpp"

#include "utils/Utils.hpp"

#include <map>
#include <string>

namespace libobsensor {

const std::map<uint16_t, std::string> pidToNameMap = {
    { static_cast<uint16_t>(0x0669), "Femto Mega" },
    { static_cast<uint16_t>(0x06c0), "Femto Mega i" },
    { static_cast<uint16_t>(0x0671), "Gemini2 XL" },
};

NetDeviceEnumerator::NetDeviceEnumerator(DeviceChangedCallback callback) : platform_(Platform::getInstance()), deviceChangedCallback_(callback) {
    deviceInfoList_ = queryDeviceList();
    if(!deviceInfoList_.empty()) {
        LOG_DEBUG("Current net device list: ({})", deviceInfoList_.size());
        for(auto &&item: deviceInfoList_) {
            auto firstPortInfo = item->getSourcePortInfoList().front();
            auto info          = std::dynamic_pointer_cast<const NetSourcePortInfo>(firstPortInfo);
            LOG_DEBUG("  - Name: {}, PID: 0x{:04X}, SN/ID: {}, MAC:{}, IP:{}", item->getName(), item->getPid(), item->getDeviceSn(), info->mac, info->address);
        }
    }
    else {
        LOG_DEBUG("No net device found!");
    }

    deviceWatcher_ = platform_->createNetDeviceWatcher();
    deviceWatcher_->start([this](OBDeviceChangedType changedType, std::string url) { onPlatformDeviceChanged(changedType, url); });
}

NetDeviceEnumerator::~NetDeviceEnumerator() noexcept {
    deviceWatcher_->stop();
}

DeviceEnumInfoList NetDeviceEnumerator::queryDeviceList() {
    std::unique_lock<std::recursive_mutex> lock(deviceInfoListMutex_);
    sourcePortInfoList_ = platform_->queryNetSourcePort();

    if(sourcePortInfoList_.empty()) {
        LOG_DEBUG("No net source port found!");
        return {};
    }

    LOG_DEBUG("Current net source port list:");
    for(const auto &item: sourcePortInfoList_) {
        auto info = std::dynamic_pointer_cast<const NetSourcePortInfo>(item);
        LOG_DEBUG(" - mac:{}, ip:{}, port:{}", info->mac, info->address, info->port);
    }
    return deviceInfoMatch(sourcePortInfoList_);
}

DeviceEnumInfoList NetDeviceEnumerator::getDeviceInfoList() {
    std::unique_lock<std::recursive_mutex> lock(deviceInfoListMutex_);
    return deviceInfoList_;
}

DeviceEnumInfoList NetDeviceEnumerator::deviceInfoMatch(const SourcePortInfoList infoList) {
    DeviceEnumInfoList deviceInfoList;
    auto               megaDevices = FemtoMegaDeviceInfo::pickDevices(infoList);
    deviceInfoList.insert(deviceInfoList.end(), megaDevices.begin(), megaDevices.end());

    auto               dev330Devices = G330DeviceInfo::pickDevices(infoList);
    deviceInfoList.insert(deviceInfoList.end(), dev330Devices.begin(), dev330Devices.end());

    return deviceInfoList;
}

void NetDeviceEnumerator::setDeviceChangedCallback(DeviceChangedCallback callback) {
    std::unique_lock<std::mutex> lock(deviceChangedCallbackMutex_);
    deviceChangedCallback_ = [callback, this](DeviceEnumInfoList removed, DeviceEnumInfoList added) {
        if(devEnumChangedCallbackThread_.joinable()) {
            devEnumChangedCallbackThread_.join();
        }
        devEnumChangedCallbackThread_ = std::thread(callback, removed, added);
    };
}

void NetDeviceEnumerator::onPlatformDeviceChanged(OBDeviceChangedType changeType, std::string devUid) {
    utils::unusedVar(changeType);
    utils::unusedVar(devUid);

    DeviceEnumInfoList addDevs;
    DeviceEnumInfoList rmDevs;

    {
        auto                                   devices = queryDeviceList();
        std::unique_lock<std::recursive_mutex> lock(deviceInfoListMutex_);
        addDevs         = utils ::subtract_sets(devices, deviceInfoList_);
        rmDevs          = utils ::subtract_sets(deviceInfoList_, devices);
        deviceInfoList_ = devices;
    }

    // callback
    std::unique_lock<std::mutex> lock(deviceChangedCallbackMutex_);
    if(deviceChangedCallback_ && (!addDevs.empty() || !rmDevs.empty())) {
        LOG_DEBUG("Net device list changed!");
        if(!addDevs.empty()) {
            LOG_DEBUG("{} net device(s) found:", addDevs.size());
            for(auto &&item: addDevs) {
                auto firstPortInfo = item->getSourcePortInfoList().front();
                auto info          = std::dynamic_pointer_cast<const NetSourcePortInfo>(firstPortInfo);
                LOG_DEBUG("  - Name: {}, PID: 0x{:04X}, SN/ID: {}, MAC:{}, IP:{}", item->getName(), item->getPid(), item->getDeviceSn(), info->mac,
                          info->address);
            }
        }

        if(!rmDevs.empty()) {
            LOG_DEBUG("{} net device(s) removed:", rmDevs.size());
            for(auto &&item: rmDevs) {
                auto firstPortInfo = item->getSourcePortInfoList().front();
                auto info          = std::dynamic_pointer_cast<const NetSourcePortInfo>(firstPortInfo);
                LOG_DEBUG("  - Name: {}, PID: 0x{:04X}, SN/ID: {}, MAC:{}, IP:{}", item->getName(), item->getPid(), item->getDeviceSn(), info->mac,
                          info->address);
            }
        }

        LOG_DEBUG("Current net device list: ({})", deviceInfoList_.size());
        for(auto &&item: deviceInfoList_) {
            auto firstPortInfo = item->getSourcePortInfoList().front();
            auto info          = std::dynamic_pointer_cast<const NetSourcePortInfo>(firstPortInfo);
            LOG_DEBUG("  - Name: {}, PID: 0x{:04X}, SN/ID: {}, MAC:{}, IP:{}", item->getName(), item->getPid(), item->getDeviceSn(), info->mac, info->address);
        }

        deviceChangedCallback_(rmDevs, addDevs);
    }
}

std::shared_ptr<IDevice> NetDeviceEnumerator::createDevice(std::string address, uint16_t port) {
    // todo: refactor this code, try get pid from device and create source port info at XxxDeviceInfo class
    SourcePortInfoList list;
    auto               info =
        std::make_shared<NetSourcePortInfo>(SOURCE_PORT_NET_VENDOR, address, static_cast<uint16_t>(8090), address + ":" + std::to_string(port), "Unknown",
                                            static_cast<uint16_t>(0x0669));  // 0x0669 for mega pid
    list.push_back(info);
    list.emplace_back(std::make_shared<RTSPStreamPortInfo>(info->address, static_cast<uint16_t>(8888), info->port, OB_STREAM_COLOR, info->mac,
                                                           info->serialNumber, info->pid));
    list.emplace_back(std::make_shared<RTSPStreamPortInfo>(info->address, static_cast<uint16_t>(8554), info->port, OB_STREAM_DEPTH, info->mac,
                                                           info->serialNumber, info->pid));
    list.emplace_back(
        std::make_shared<RTSPStreamPortInfo>(info->address, static_cast<uint16_t>(8554), info->port, OB_STREAM_IR, info->mac, info->serialNumber, info->pid));
    list.emplace_back(
        std::make_shared<NetDataStreamPortInfo>(info->address, static_cast<uint16_t>(8900), info->port, info->mac, info->serialNumber, info->pid));

    auto deviceEnumInfoList = deviceInfoMatch(list);
    if(deviceEnumInfoList.empty()) {
        throw std::runtime_error("No device found");
    }

    return deviceEnumInfoList.front()->createDevice();
}

}  // namespace libobsensor
