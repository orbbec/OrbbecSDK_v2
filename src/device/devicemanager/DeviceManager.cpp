// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "DeviceManager.hpp"
#include "utils/Utils.hpp"
#include "IDeviceClockSynchronizer.hpp"

#if defined(BUILD_USB_PAL)
#include "UsbDeviceEnumerator.hpp"
#endif

#if defined(BUILD_NET_PAL)
#include "NetDeviceEnumerator.hpp"
#endif

namespace libobsensor {

void printDeviceList(std::string title, const DeviceEnumInfoList &deviceList) {
    LOG_INFO(title + ": ({})", deviceList.size());
    for(auto &deviceInfo: deviceList) {
        if(deviceInfo->getConnectionType() == "Ethernet") {
            auto netPortInfo = std::dynamic_pointer_cast<const NetSourcePortInfo>(deviceInfo->getSourcePortInfoList().front());
            LOG_INFO("\t- Name: {0}, PID: 0x{1:04x}, SN/ID: {2}, Connection: {3}, MAC:{4}, ip:{5}", deviceInfo->getName(), deviceInfo->getPid(),
                     deviceInfo->getDeviceSn(), deviceInfo->getConnectionType(), netPortInfo->mac, netPortInfo->address);
        }
        else {
            LOG_INFO("\t- Name: {0}, PID: 0x{1:04x}, SN/ID: {2}, Connection: {3}", deviceInfo->getName(), deviceInfo->getPid(), deviceInfo->getDeviceSn(),
                     deviceInfo->getConnectionType());
        }
    }
}

std::weak_ptr<DeviceManager>   DeviceManager::instanceWeakPtr_;
std::mutex                     DeviceManager::instanceMutex_;
std::shared_ptr<DeviceManager> DeviceManager::getInstance() {
    std::unique_lock<std::mutex> lock(instanceMutex_);
    auto                         instance = instanceWeakPtr_.lock();
    if(!instance) {
        instance         = std::shared_ptr<DeviceManager>(new DeviceManager());
        instanceWeakPtr_ = instance;
    }
    return instance;
}

DeviceManager::DeviceManager() : destroy_(false), multiDeviceSyncIntervalMs_(0) {
    LOG_DEBUG("DeviceManager init ...");

#if defined(BUILD_USB_PAL)
    LOG_DEBUG("Enable USB Device Enumerator ...");
    auto usbDeviceEnumerator =
        std::make_shared<UsbDeviceEnumerator>([&](const DeviceEnumInfoList &removed, const DeviceEnumInfoList &added) { onDeviceChanged(removed, added); });

    deviceEnumerators_.emplace_back(usbDeviceEnumerator);
#endif

#if defined(BUILD_NET_PAL)
    LOG_DEBUG("Enable Net Device Enumerator ...");
    auto netDeviceEnumerator =
        std::make_shared<NetDeviceEnumerator>([&](const DeviceEnumInfoList &removed, const DeviceEnumInfoList &added) { onDeviceChanged(removed, added); });
    deviceEnumerators_.emplace_back(netDeviceEnumerator);
#endif

    auto deviceInfoList = getDeviceInfoList();
    printDeviceList("Current found device(s)", deviceInfoList);
    LOG_DEBUG("DeviceManager construct done!");
}

DeviceManager::~DeviceManager() noexcept {
    LOG_DEBUG("DeviceManager destroy ...");
    destroy_ = true;

    multiDeviceSyncIntervalMs_ = 0;
    multiDeviceSyncCv_.notify_all();
    if(multiDeviceSyncThread_.joinable()) {
        multiDeviceSyncThread_.join();
    }

    LOG_DEBUG("DeviceManager Destructors done");
}

std::shared_ptr<IDevice> DeviceManager::createNetDevice(std::string address, uint16_t port) {
#if defined(BUILD_NET_PAL)
    LOG_DEBUG("DeviceManager createNetDevice.... address={0}, port={1}", address, port);
    auto deviceInfo = NetDeviceEnumerator::queryNetDevice(address, port);
    if(!deviceInfo) {
        throw libobsensor::invalid_value_exception("Failed to query Net Device, address=" + address + ", port=" + std::to_string(port));
    }
    auto device     = createDevice(deviceInfo);

    {
        std::unique_lock<std::mutex> lock(customConnectedDevicesMutex_);
        auto                         iter = customConnectedDevices_.find(deviceInfo->getUid());
        if(iter == customConnectedDevices_.end()) {
            customConnectedDevices_.insert({ deviceInfo->getUid(), deviceInfo });
        }
    }

    return device;
#else
    utils::unusedVar(address);
    utils::unusedVar(port);
    throw libobsensor::unsupported_operation_exception("The OrbbecSDK library currently compiled does not support network functions."
                                                       "Please turn on the CMAKE \"BUILD_NET_PAL\" option and recompile.");
#endif
}

std::shared_ptr<IDevice> DeviceManager::createDevice(const std::shared_ptr<const IDeviceEnumInfo> &info) {
    LOG_DEBUG("DeviceManager createDevice...");

    // check if the device has been created
    {
        std::unique_lock<std::mutex> lock(createdDevicesMutex_);
        auto                         iter = createdDevices_.begin();
        for(; iter != createdDevices_.end(); ++iter) {
            if(iter->first == info->getUid()) {
                auto dev = iter->second.lock();
                if(!dev) {
                    createdDevices_.erase(iter);
                    break;
                }
                auto devInfo = dev->getInfo();
                LOG_DEBUG("Device has already been created, return existing device! Name: {0}, PID: 0x{1:04x}, SN/ID: {2}, FW: {3}", devInfo->name_,
                          devInfo->pid_, devInfo->deviceSn_, devInfo->fwVersion_);
                return dev;
            }
        }
    }

    // create device
    auto device = info->createDevice();

    // add to createdDevices_
    {
        std::unique_lock<std::mutex> lock(createdDevicesMutex_);
        createdDevices_.insert({ info->getUid(), device });
    }
    auto devInfo = device->getInfo();
    LOG_INFO("Device created successfully! Name: {0}, PID: 0x{1:04x}, SN/ID: {2} FW: {3}", devInfo->name_, devInfo->pid_, devInfo->deviceSn_,
             devInfo->fwVersion_);
    return device;
}

DeviceEnumInfoList DeviceManager::getDeviceInfoList() {
    DeviceEnumInfoList deviceInfoList;
    for(auto &enumerator_: deviceEnumerators_) {
        auto infos = enumerator_->getDeviceInfoList();
        deviceInfoList.insert(deviceInfoList.end(), infos.begin(), infos.end());
    }

    {
        std::unique_lock<std::mutex> lock(customConnectedDevicesMutex_);
        std::unique_lock<std::mutex> lock2(createdDevicesMutex_);
        auto                         customIter = customConnectedDevices_.begin();
        while(customIter != customConnectedDevices_.end()) {
            auto createdIter = createdDevices_.find(customIter->first);
            if(createdIter == createdDevices_.end()) {
                customIter = customConnectedDevices_.erase(customIter);
                continue;
            }

            if(createdIter->second.expired()) {
                createdDevices_.erase(createdIter);
                customIter = customConnectedDevices_.erase(customIter);
                continue;
            }

            deviceInfoList.push_back(customIter->second);
            ++customIter;
        }
    }

    return deviceInfoList;
}

void DeviceManager::setDeviceChangedCallback(DeviceChangedCallback callback) {
    std::unique_lock<std::mutex> lock(callbackMutex_);
    devChangedCallback_ = callback;
}

void DeviceManager::onDeviceChanged(const DeviceEnumInfoList &removed, const DeviceEnumInfoList &added) {
    std::unique_lock<std::mutex> lock(callbackMutex_);

    LOG_INFO("Device changed! removed: {0}, added: {1}", removed.size(), added.size());
    if(!removed.empty()) {
        for(const auto &info: removed) {
            auto iter = createdDevices_.find(info->getUid());
            if(iter != createdDevices_.end()) {
                auto dev = iter->second.lock();
                if(dev) {
                    dev->deactivate();
                }
                createdDevices_.erase(iter);
            }
        }
        printDeviceList("Removed device(s) list", removed);
    }
    auto deviceInfoList = getDeviceInfoList();
    printDeviceList("Current device(s) list", deviceInfoList);

    if(devChangedCallback_) {
        devChangedCallback_(removed, added);
    }
}

void DeviceManager::enableDeviceClockSync(uint64_t repeatInterval) {
    LOG_DEBUG("Enable multi-device clock sync, repeatInterval={0}ms", repeatInterval);

    // stop previous thread
    multiDeviceSyncIntervalMs_ = 0;
    multiDeviceSyncCv_.notify_all();
    if(multiDeviceSyncThread_.joinable()) {
        multiDeviceSyncThread_.join();
    }

    // create new thread
    multiDeviceSyncIntervalMs_ = repeatInterval;
    multiDeviceSyncThread_     = std::thread([this]() {
        do {
            std::unique_lock<std::mutex> lock(createdDevicesMutex_);
            if(!destroy_) {
                for(auto &item: createdDevices_) {
                    auto dev = item.second.lock();
                    if(!dev || !dev->isComponentExists(OB_DEV_COMPONENT_DEVICE_CLOCK_SYNCHRONIZER)) {
                        continue;
                    }
                    auto synchronizer = dev->getComponentT<IDeviceClockSynchronizer>(OB_DEV_COMPONENT_DEVICE_CLOCK_SYNCHRONIZER);
                    TRY_EXECUTE(synchronizer->timerSyncWithHost());
                }
            }
            multiDeviceSyncCv_.wait_for(lock, std::chrono::milliseconds(multiDeviceSyncIntervalMs_));
        } while(multiDeviceSyncIntervalMs_ > 0 && !destroy_);
    });
}

void DeviceManager::enableNetDeviceEnumeration(bool enable) {
#if defined(BUILD_NET_PAL)
    LOG_INFO("Enable net device enumeration: {0}", enable);
    auto iter = std::find_if(deviceEnumerators_.begin(), deviceEnumerators_.end(), [](const std::shared_ptr<IDeviceEnumerator> &enumerator) {  //
        return std::dynamic_pointer_cast<NetDeviceEnumerator>(enumerator) != nullptr;
    });
    if(enable && iter == deviceEnumerators_.end()) {
        auto netDeviceEnumerator =
            std::make_shared<NetDeviceEnumerator>([&](DeviceEnumInfoList removed, DeviceEnumInfoList added) { onDeviceChanged(removed, added); });
        deviceEnumerators_.emplace_back(netDeviceEnumerator);
        auto deviceInfoList = getDeviceInfoList();
        printDeviceList("Current device(s) list", deviceInfoList);
    }
    else if(!enable && iter != deviceEnumerators_.end()) {
        deviceEnumerators_.erase(iter);
    }
#else
    utils::unusedVar(enable);
#endif
}

bool DeviceManager::isNetDeviceEnumerationEnable() const {
#if defined(BUILD_NET_PAL)

    auto iter = std::find_if(deviceEnumerators_.begin(), deviceEnumerators_.end(), [](const std::shared_ptr<IDeviceEnumerator> &enumerator) {  //
        return std::dynamic_pointer_cast<NetDeviceEnumerator>(enumerator) != nullptr;
    });
    return iter != deviceEnumerators_.end();
#endif
    return false;
}

}  // namespace libobsensor
