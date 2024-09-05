#include "DeviceManager.hpp"
#include "utils/Utils.hpp"

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
    LOG_DEBUG("DeviceManager createNetDevice...");
    std::string uid = address + ":" + std::to_string(port);
    {
        std::unique_lock<std::mutex> lock(createdDevicesMutex_);
        auto                         iter = createdDevices_.begin();
        for(; iter != createdDevices_.end(); ++iter) {
            if(iter->first == uid) {
                auto dev = iter->second.lock();
                if(dev) {
                    throw invalid_value_exception("Attempting to create a device that has already been created!! address=" + address
                                                  + ", port=" + std::to_string(port));
                }
                else {
                    createdDevices_.erase(iter);
                    break;
                }
            }
        }
    }

    auto device = NetDeviceEnumerator::createDevice(address, port);
    if(device == nullptr) {
        throw libobsensor::invalid_value_exception("Failed to create Net Device, address=" + address + ", port=" + std::to_string(port));
    }

    {
        std::unique_lock<std::mutex> lock(createdDevicesMutex_);
        createdDevices_.insert({ uid, device });
    }
    LOG_INFO("create Net Device success! address={0}, port={1}", address, port);
    return device;
#else
    utils::unusedVar(address);
    utils::unusedVar(port);
    throw libobsensor::unsupported_operation_exception("The OrbbecSDK library currently compiled does not support network functions."
                                                       "Please turn on the CMAKE \"BUILD_NET_PAL\" option and recompile.");
#endif
}

std::shared_ptr<IDevice> DeviceManager::createDevice(const std::shared_ptr<const IDeviceEnumInfo> &info) {
    LOG_DEBUG("DeviceManager  createDevice...");

    // check if the device has been created
    {
        std::unique_lock<std::mutex> lock(createdDevicesMutex_);
        auto                         iter = createdDevices_.begin();
        for(; iter != createdDevices_.end(); ++iter) {
            if(iter->first == info->getUid()) {
                if(iter->second.expired()) {
                    createdDevices_.erase(iter);
                    break;
                }
                return iter->second.lock();
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

    LOG_INFO("Device created successfully! Name: {0}, PID: 0x{1:04x}, SN/ID: {2}", info->getName(), info->getPid(), info->getDeviceSn());
    return device;
}

DeviceEnumInfoList DeviceManager::getDeviceInfoList() const {
    DeviceEnumInfoList deviceInfoList;
    for(auto &enumerator_: deviceEnumerators_) {
        auto infos = enumerator_->getDeviceInfoList();
        deviceInfoList.insert(deviceInfoList.end(), infos.begin(), infos.end());
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

#define MAX_RTT 50
void DeviceManager::multiDeviceSyncFunc(uint8_t retry, std::vector<std::string> uids) {
    std::map<std::string, int32_t> devicesMap;       // 用于记录rtt
    std::vector<std::string>       abnormalDevices;  // rtt 异常设备

    // todo: impl this
    utils::unusedVar(retry);
    utils::unusedVar(uids);

    // int32_t rtt;
    // for(auto &item: createdDevices_) {
    //     if(!uids.empty() && std::find(uids.begin(), uids.end(), item.first) == uids.end()) {
    //         continue;
    //     }
    //
    //     auto dev = item.second.lock();
    //     if(!dev) {
    //         continue;
    //     }
    // auto resLock         = dev->tryLockResource();
    // auto propertyManager = dev->getPropertyManager(resLock);
    // if(!propertyManager->isPropertySupported(OB_STRUCT_DEVICE_TIME, OB_PERMISSION_WRITE)) {
    //     continue;
    // }
    //
    // BEGIN_TRY_EXECUTE({ rtt = dev->syncDeviceTime(); })
    // CATCH_EXCEPTION_AND_EXECUTE(rtt = 0)

    // std::stringstream ss;
    // if(rtt > MAX_RTT) {  // rtt
    //     ss << "dev-uid@0x" << std::hex << item.first << ", update device time succeeded, but rtt is too large! round-trip-time=" << rtt << "ms\n";
    //     LOG_ERROR(ss.str());
    //     abnormalDevices.push_back(item.first);
    // }
    // else {
    //     ss << "dev-uid@0x" << std::hex << item.first << ", update device time succeeded! round-trip-time=" << std::dec << rtt << "ms\n";
    //     LOG_INFO(ss.str());
    //     devicesMap.insert({ item.first, rtt });
    // }
    // }
    //
    // // 找出rtt异常设备
    // if(devicesMap.size() >= 3) {
    //     // Standard Deviation
    //     uint64_t total = 0;
    //     for(auto item: devicesMap) {
    //         total += item.second;
    //     }
    //     double avg = (double)total / devicesMap.size();
    //
    //     double variance = 0;
    //     for(auto item: devicesMap) {
    //         variance += pow(item.second - avg, 2);
    //     }
    //
    //     variance /= devicesMap.size();  // 平方差
    //     double sd = sqrt(variance);     // 标准差
    //
    //     for(auto item: devicesMap) {
    //         if(abs((double)item.second - avg) > 2 * sd) {  // 与平均值距离大于2倍标准差（95.4%概率）
    //             abnormalDevices.push_back(item.first);
    //         }
    //     }
    // }
    //
    // // 重试一次
    // if(!abnormalDevices.empty() && retry) {
    //     retry--;
    //     LOG_DEBUG("Retry for abnormal devices...");
    //     multiDeviceSyncFunc(retry, abnormalDevices);
    // }
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
                multiDeviceSyncFunc(1);
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
