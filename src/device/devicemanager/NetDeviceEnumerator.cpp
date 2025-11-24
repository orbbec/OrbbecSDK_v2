// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "NetDeviceEnumerator.hpp"
#include "femtomega/FemtoMegaDeviceInfo.hpp"
#include "gemini330/G330DeviceInfo.hpp"
#include "lidar/LiDARDeviceInfo.hpp"
#include "gemini2/G2DeviceInfo.hpp"
#include "bootloader/BootDeviceInfo.hpp"
#include "SourcePortInfo.hpp"
#include "property/VendorPropertyAccessor.hpp"
#include "property/LiDARPropertyAccessor.hpp"
#include "property/InternalProperty.hpp"
#include "DevicePids.hpp"
#include "utils/Utils.hpp"
#if defined(BUILD_NET_PAL)
#include "ethernet/NetDataStreamPort.hpp"
#endif

#include <map>
#include <string>

namespace libobsensor {

NetDeviceEnumerator::NetDeviceEnumerator(DeviceChangedCallback callback, std::shared_ptr<DeviceActivityManager> deviceActivityManager)
    : platform_(Platform::getInstance()), deviceChangedCallback_(callback), deviceActivityManager_(deviceActivityManager) {
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
    deviceWatcher_->start([this](OBDeviceChangedType changedType, std::string url) { return onPlatformDeviceChanged(changedType, url); });
}

NetDeviceEnumerator::~NetDeviceEnumerator() noexcept {
    deviceWatcher_->stop();
}

DeviceEnumInfoList NetDeviceEnumerator::queryDeviceList() {
    SourcePortInfoList sourcePortInfoList;
    auto               portInfoList = platform_->queryNetSourcePort();

    if(portInfoList.empty()) {
        LOG_DEBUG("No net source port found!");
        return {};
    }

    for(const auto &portInfo: portInfoList) {
        auto info = std::dynamic_pointer_cast<const NetSourcePortInfo>(portInfo);
        if(info->pid != 0) {
            sourcePortInfoList.push_back(info);
            continue;
        }

        // try fetch pid from device via vendor property
        auto pid = NetDeviceEnumerator::getDevicePid(info, OB_FEMTO_MEGA_PID);
        if(pid != 0) {
            auto newInfo = std::make_shared<NetSourcePortInfo>(*info);
            newInfo->pid = pid;
            sourcePortInfoList.push_back(newInfo);
        }
    }

    LOG_DEBUG("Current net source port list:");
    for(const auto &item: sourcePortInfoList) {
        auto info = std::dynamic_pointer_cast<const NetSourcePortInfo>(item);
        LOG_DEBUG(" - mac:{}, ip:{}, port:{}", info->mac, info->address, info->port);
    }
    return deviceInfoMatch(sourcePortInfoList);
}

uint16_t NetDeviceEnumerator::getDevicePid(std::shared_ptr<const NetSourcePortInfo> info, uint16_t defaultPid, bool tryCamera, bool tryLiDAR) {
    if(info == nullptr) {
        return 0;
    }

    // check if camera device
    if(tryCamera) {
        BEGIN_TRY_EXECUTE({
            OBPropertyValue value;
            value.intValue = 0;

            auto port         = Platform::getInstance()->getSourcePort(info);
            auto propAccessor = std::make_shared<VendorPropertyAccessor>(nullptr, port);
            propAccessor->getPropertyValue(OB_PROP_DEVICE_PID_INT, &value);
            return static_cast<uint16_t>(value.intValue);
        })
        CATCH_EXCEPTION_AND_EXECUTE({ LOG_WARN("Get device pid failed with VendorPropertyAccessor! address:{}, port:{}", info->address, info->port); });
    }

    // check if is LiDAR device
    if (tryLiDAR) {
        BEGIN_TRY_EXECUTE({
            OBPropertyValue value;
            value.intValue = 0;

            auto port         = Platform::getInstance()->getSourcePort(info);
            auto propAccessor = std::make_shared<LiDARPropertyAccessor>(nullptr, port);
            propAccessor->getPropertyValue(OB_PROP_DEVICE_PID_INT, &value);
            return static_cast<uint16_t>(value.intValue);
        })
        CATCH_EXCEPTION_AND_EXECUTE({ LOG_WARN("Get device pid failed with LiDARPropertyAccessor! address:{}, port:{}", info->address, info->port); });
    }

    if(defaultPid != 0) {
        LOG_WARN("Use default pid({}) for Net Device! address:{}, port:{}", defaultPid, info->address, info->port);
    }
    return defaultPid;
}

DeviceEnumInfoList NetDeviceEnumerator::getDeviceInfoList() {
    std::unique_lock<std::recursive_mutex> lock(deviceInfoListMutex_);
    return deviceInfoList_;
}

DeviceEnumInfoList NetDeviceEnumerator::deviceInfoMatch(const SourcePortInfoList infoList) {
    DeviceEnumInfoList deviceInfoList;

    auto megaDevices = FemtoMegaDeviceInfo::pickNetDevices(infoList);
    deviceInfoList.insert(deviceInfoList.end(), megaDevices.begin(), megaDevices.end());

    auto dev330Devices = G330DeviceInfo::pickNetDevices(infoList);
    deviceInfoList.insert(deviceInfoList.end(), dev330Devices.begin(), dev330Devices.end());

    auto g2Devices = G2DeviceInfo::pickNetDevices(infoList);
    deviceInfoList.insert(deviceInfoList.end(), g2Devices.begin(), g2Devices.end());

    auto lidarDevices = LiDARDeviceInfo::pickNetDevices(infoList);
    deviceInfoList.insert(deviceInfoList.end(), lidarDevices.begin(), lidarDevices.end());

    auto bootDevices = BootDeviceInfo::pickNetDevices(infoList);
    deviceInfoList.insert(deviceInfoList.end(), bootDevices.begin(), bootDevices.end());

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

bool NetDeviceEnumerator::checkDeviceActivity(std::shared_ptr<const IDeviceEnumInfo> dev, std::shared_ptr<const NetSourcePortInfo> info) {
    if(deviceActivityManager_ == nullptr) {
        LOG_DEBUG("The device activity manager is nullptr");
        return false;
    }

    // check the last active of the device
    std::string   mac           = info ? info->mac : "unknown";
    std::string   ip            = info ? info->address : "unknown";
    auto          elapsed       = deviceActivityManager_->getElapsedSinceLastActive(dev->getUid());
    const int64_t timeThreshold = 2000;  // 2s
    if(elapsed < 0) {
        LOG_DEBUG("Can't find activity of the device, it might be offline. Name: {}, PID: 0x{:04X}, SN/ID: {}, MAC: {}, IP: {}", dev->getName(), dev->getPid(),
                  dev->getDeviceSn(), mac, ip);
        return false;
    }
    else if(elapsed <= timeThreshold) {
        // If the device is active within the allowed elapsed time threshold, consider it online
        LOG_DEBUG("The device responded {} ms ago and is considered online. Name: {}, PID: 0x{:04X}, SN/ID: {}, MAC: {}, IP: {}", elapsed, dev->getName(),
                  dev->getPid(), dev->getDeviceSn(), mac, ip);
        return true;
    }
    else {
        LOG_DEBUG("The device responded {} ms ago, it might be offline. Name: {}, PID: 0x{:04X}, SN/ID: {}, MAC: {}, IP: {}", elapsed, dev->getName(),
                  dev->getPid(), dev->getDeviceSn(), mac, ip);
        return false;
    }
}

bool NetDeviceEnumerator::handleDeviceRemoved(std::string devUid) {
    // remove: handle only the current device
    DeviceEnumInfoList                     removedDevs;
    std::shared_ptr<const IDeviceEnumInfo> removedDevice;
    // get removed device info
    {
        std::unique_lock<std::recursive_mutex> lock(deviceInfoListMutex_);
        for(auto &&item: deviceInfoList_) {
            if(item->getUid() == devUid) {
                removedDevice = item;
            }
        }
    }
    if(removedDevice == nullptr) {
        LOG_DEBUG("NetDeviceEnumerator::onPlatformDeviceChanged: can't find device from current device info list. devUid: {}", devUid);
        return true;
    }

    LOG_DEBUG("Net device list changed!");
    auto firstPortInfo = removedDevice->getSourcePortInfoList().front();
    auto info          = std::dynamic_pointer_cast<const NetSourcePortInfo>(firstPortInfo);

    if(deviceActivityManager_ && deviceActivityManager_->hasDeviceRebooted(removedDevice->getUid())) {
        LOG_DEBUG("The device has reboot, consider it as offline. Name: {}, PID: 0x{:04X}, SN/ID: {}, MAC:{}, IP:{}", removedDevice->getName(),
                  removedDevice->getPid(), removedDevice->getDeviceSn(), info->mac, info->address);
    }
    else {
        // We assume the device has gone offline and perform extra verification
        // check the last active of the device
        if(checkDeviceActivity(removedDevice, info)) {
            // If the device is active within the allowed elapsed time threshold, consider it online
            return false;
        }

        // Continue device check via PID acquisition to confirm online status
        // TODO: Busy devices may be misjudged as offline due to temporary unresponsiveness
        auto vid = info->vid;
        auto pid = info->pid;

        bool isLiDAR = isDeviceInOrbbecSeries(LiDARDevPids, vid, pid);
        if(isDeviceInContainer(G335LeDevPids, vid, pid) || (isDeviceInOrbbecSeries(FemtoMegaDevPids, vid, pid)) || isDeviceInContainer(G435LeDevPids, vid, pid)
           || isLiDAR) {
            auto devPid = getDevicePid(info, 0, !isLiDAR, isLiDAR);
            if(devPid == 0) {
                // check device activity again
                if(checkDeviceActivity(removedDevice, info)) {
                    // If the device is active within the allowed elapsed time threshold, consider it online
                    return false;
                }
            }
            else {
                // device is still online
                LOG_DEBUG("Got device PID successfully, consider it online: name: {}, PID: 0x{:04X}, SN/ID: {}, MAC:{}, IP:{}", removedDevice->getName(),
                          removedDevice->getPid(), removedDevice->getDeviceSn(), info->mac, info->address);
                return false;
            }
        }
    }
    // device has gone offline
    removedDevs.push_back(removedDevice);
    LOG_WARN("1 net device removed:");
    LOG_WARN("  - Name: {}, PID: 0x{:04X}, SN/ID: {}, MAC:{}, IP:{}", removedDevice->getName(), removedDevice->getPid(), removedDevice->getDeviceSn(),
             info->mac, info->address);
    // remove activity record
    if(deviceActivityManager_) {
        deviceActivityManager_->removeActivity(removedDevice->getUid());
    }
    // printf current device list
    {
        std::unique_lock<std::recursive_mutex> lock(deviceInfoListMutex_);
        LOG_DEBUG("Current net device list: ({})", deviceInfoList_.size());
        for(auto it = deviceInfoList_.begin(); it != deviceInfoList_.end();) {
            if((*it)->getUid() == devUid) {
                it = deviceInfoList_.erase(it);
            }
            else {
                auto &item    = *it;
                firstPortInfo = item->getSourcePortInfoList().front();
                info          = std::dynamic_pointer_cast<const NetSourcePortInfo>(firstPortInfo);
                LOG_DEBUG("  - Name: {}, PID: 0x{:04X}, SN/ID: {}, MAC:{}, IP:{}", item->getName(), item->getPid(), item->getDeviceSn(), info->mac,
                          info->address);
                ++it;
            }
        }
    }
    // callback
    std::unique_lock<std::mutex> lock(deviceChangedCallbackMutex_);
    if(deviceChangedCallback_) {
        deviceChangedCallback_(removedDevs, {});
    }
    return true;
}

bool NetDeviceEnumerator::handleDeviceArrival(std::string devUid) {
    utils::unusedVar(devUid);

    // arrival: handle all arrival devices
    DeviceEnumInfoList addedDevs;
    {
        auto                                   devices = queryDeviceList();
        std::unique_lock<std::recursive_mutex> lock(deviceInfoListMutex_);
        addedDevs = utils::subtract_sets(devices, deviceInfoList_);
        if(!addedDevs.empty()) {
            // update device list
            deviceInfoList_.insert(deviceInfoList_.end(), addedDevs.begin(), addedDevs.end());
        }
    }
    if(!addedDevs.empty()) {
        LOG_DEBUG("Net device list changed!");
        for(auto &&item: addedDevs) {
            auto firstPortInfo = item->getSourcePortInfoList().front();
            auto info          = std::dynamic_pointer_cast<const NetSourcePortInfo>(firstPortInfo);
            LOG_DEBUG("  - Name: {}, PID: 0x{:04X}, SN/ID: {}, MAC:{}, IP:{}", item->getName(), item->getPid(), item->getDeviceSn(), info->mac, info->address);
        }
    }

    // printf current device list
    {
        std::unique_lock<std::recursive_mutex> lock(deviceInfoListMutex_);
        LOG_DEBUG("Current net device list: ({})", deviceInfoList_.size());
        for(auto &&item: deviceInfoList_) { 
            auto firstPortInfo = item->getSourcePortInfoList().front();
            auto info          = std::dynamic_pointer_cast<const NetSourcePortInfo>(firstPortInfo);
            LOG_DEBUG("  - Name: {}, PID: 0x{:04X}, SN/ID: {}, MAC:{}, IP:{}", item->getName(), item->getPid(), item->getDeviceSn(), info->mac, info->address);
        }
    }
    // callback
    std::unique_lock<std::mutex> lock(deviceChangedCallbackMutex_);
    if(deviceChangedCallback_ && !addedDevs.empty()) {
        deviceChangedCallback_({}, addedDevs);
    }
    return true;
}

bool NetDeviceEnumerator::onPlatformDeviceChanged(OBDeviceChangedType changeType, std::string devUid) {
    LOG_DEBUG("NetDeviceEnumerator::onPlatformDeviceChanged: changeType: {}, devUid: {}", static_cast<uint32_t>(changeType), devUid);

    utils::unusedVar(changeType);
    utils::unusedVar(devUid);

    if(changeType == OB_DEVICE_REMOVED) {
        return handleDeviceRemoved(devUid);
    }
    else if(changeType == OB_DEVICE_ARRIVAL) {
        return handleDeviceArrival(devUid);
    }
    else {
        return false;
    }
}

std::shared_ptr<const IDeviceEnumInfo> NetDeviceEnumerator::queryNetDevice(std::string address, uint16_t port) {
    auto info              = std::make_shared<NetSourcePortInfo>(SOURCE_PORT_NET_VENDOR);
    info->netInterfaceName = "Unknown";
    info->localMac         = "Unknown";
    info->localAddress     = "Unknown";
    info->address          = address;
    info->port             = port;
    info->mac              = address + ":" + std::to_string(port);
    info->serialNumber     = "Unknown";
    info->pid              = NetDeviceEnumerator::getDevicePid(info, OB_FEMTO_MEGA_PID);
    info->vid              = ORBBEC_DEVICE_VID;

    if(isDeviceInContainer(G335LeDevPids, info->vid, info->pid)) {
        throw invalid_value_exception("No supported G335Le found for address: " + address + ":" + std::to_string(port));
    }

    auto deviceEnumInfoList = deviceInfoMatch({ info });
    if(deviceEnumInfoList.empty()) {
        throw invalid_value_exception("No supported device found for address: " + address + ":" + std::to_string(port));
    }

    return deviceEnumInfoList.front();
}

}  // namespace libobsensor
