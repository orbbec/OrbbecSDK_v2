#include "NetDeviceEnumerator.hpp"
#include "femtomega/FemtoMegaDeviceInfo.hpp"
#include "gemini330/G330DeviceInfo.hpp"

#include "utils/Utils.hpp"

#include <map>
#include <string>

namespace libobsensor {

const std::map<uint16_t, std::string> pidToNameMap = {
    { static_cast<uint16_t>(0x0669), "Femto Mega" },
    { static_cast<uint16_t>(0x06c0), "Femto Mega i" },
    { static_cast<uint16_t>(0x0671), "Gemini2 XL" },
};

NetDeviceEnumerator::NetDeviceEnumerator(DeviceChangedCallback callback) : deviceChangedCallback_(callback), platform_(Platform::getInstance()) {
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

NetDeviceEnumerator::~NetDeviceEnumerator() {
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
    // auto vendorPort = platform_->queryNetVendorPort(address, port);
    // if(!vendorPort) {
    //     throw invalid_value_exception(utils ::string::to_string() << "Could not find device, address=" << address << ", port=" << port);
    // }
    // std::shared_ptr<IDevice> device;
    // SourcePortInfoList       portInfoList   = { vendorPort };
    // auto                     deviceInfoList = deviceInfoMatch(portInfoList);
    // auto                     deviceInfo     = deviceInfoList.front();

    // auto rstDeviceInfo = associatedSourcePortCompletion(obPal, deviceInfo);

    // if(isMatchDeviceByPid(rstDeviceInfo->pid_, FemtoMegaDevPids)) {
    //     device = std::make_shared<FemtoMegaNetDevice>(obPal, rstDeviceInfo);
    //     LOG_DEBUG("Create Net Device success! address={0}, port={1}, pid=0x{2:4x}", address, port, rstDeviceInfo->pid_);
    // }
    // else if(isMatchDeviceByPid(rstDeviceInfo->pid_, gGemini2XLPids)) {
    //     device = std::make_shared<Gemini2XLDevice>(obPal, rstDeviceInfo);
    //     LOG_DEBUG("Create Net Device success! address={0}, port={1}, pid=0x{2:4x}", address, port, rstDeviceInfo->pid_);
    // }
    // else {
    //     LOG_ERROR("Create Net Device failed, unsupported device! address={0}, port={1}, pid=0x{2:04x}", address, port, rstDeviceInfo->pid_);
    // }
    // return device;
    utils::unusedVar(address);
    utils::unusedVar(port);
    return nullptr;
}

std::shared_ptr<IDeviceEnumInfo> NetDeviceEnumerator::associatedSourcePortCompletion(std::shared_ptr<IDeviceEnumInfo> info) {
    // auto rstDeviceInfo = std::make_shared<IDeviceEnumInfo>(info);
    // if(rstDeviceInfo->sourcePortInfoList_.size() == 1) {
    //     auto associatedPortList = platform_->queryAssociatedNetSourcePort(rstDeviceInfo->sourcePortInfoList_.front());
    //     rstDeviceInfo->sourcePortInfoList_.insert(rstDeviceInfo->sourcePortInfoList_.end(), associatedPortList.begin(), associatedPortList.end());
    // }
    // return rstDeviceInfo;

    utils::unusedVar(info);
    return nullptr;
}

}  // namespace libobsensor