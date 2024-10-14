// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "UsbDeviceEnumerator.hpp"
#include "utils/Utils.hpp"

#include "gemini330/G330DeviceInfo.hpp"
#include "gemini2/G2DeviceInfo.hpp"
#include "astra2/Astra2DeviceInfo.hpp"
#include "femtobolt/FemtoBoltDeviceInfo.hpp"
#include "femtomega/FemtoMegaDeviceInfo.hpp"
#include "bootloader/BootDeviceInfo.hpp"

namespace libobsensor {
UsbDeviceEnumerator::UsbDeviceEnumerator(DeviceChangedCallback callback) : platform_(Platform::getInstance()) {
    devChangedCallback_ = [callback, this](const DeviceEnumInfoList &removedList, const DeviceEnumInfoList &addedList) {
        (void)this;
#ifdef __ANDROID__
        // On the Android platform, it is necessary to call back to Java in the same thread, and complete the release of relevant resources in the callback
        // function.
        callback(removedList, addedList);
#elif defined(__linux__)
        // Solve the problem of deadlock caused by multiple callbacks in a short period of time in Linux, and the related interfaces that access libusb are
        // called in the user callback function.
        auto cbThread = std::thread(callback, removedList, addedList);
        cbThread.detach();
#else
        // On the WIN platform, since the callback is called by MF-related threads, if the callback is directly made to the user program without switching
        // threads, the user program needs to update the MFC interface, which will cause the program to crash;
        if(devChangedCallbackThread_.joinable()) {
            devChangedCallbackThread_.join();
        }
        devChangedCallbackThread_ = std::thread(callback, removedList, addedList);
#endif
    };

    deviceInfoList_ = queryArrivalDevice();

    deviceArrivalHandleThread_ = std::thread(&UsbDeviceEnumerator::deviceArrivalHandleThreadFunc, this);

    deviceWatcher_ = platform_->createUsbDeviceWatcher();
    deviceWatcher_->start([this](OBDeviceChangedType changedType, std::string url) { onPlatformDeviceChanged(changedType, url); });

    std::unique_lock<std::recursive_mutex> lock(deviceInfoListMutex_);
    if(!deviceInfoList_.empty()) {
        LOG_DEBUG("Found {} device(s):", deviceInfoList_.size());
        for(auto &deviceInfo: deviceInfoList_) {
            LOG_DEBUG("  - Name: {}, PID: 0x{:04X}, SN/ID: {}, connection: {}", deviceInfo->getName(), deviceInfo->getPid(), deviceInfo->getDeviceSn(),
                      deviceInfo->getConnectionType());
        }
    }
    else {
        LOG_DEBUG("No matched usb device found!");
    }
}

UsbDeviceEnumerator::~UsbDeviceEnumerator() noexcept {
    destroy_ = true;

    newUsbPortArrivalCV_.notify_all();
    if(deviceArrivalHandleThread_.joinable()) {
        deviceArrivalHandleThread_.join();
    }

    if(devChangedCallbackThread_.joinable()) {
        devChangedCallbackThread_.join();
    }
    deviceWatcher_.reset();  // deviceWatcher should be released before platform_
    platform_.reset();
}

void UsbDeviceEnumerator::onPlatformDeviceChanged(OBDeviceChangedType changeType, std::string devUid) {
    if(changeType == OB_DEVICE_REMOVED) {
        std::vector<std::shared_ptr<const IDeviceEnumInfo>> removedDevList;
        {
            std::unique_lock<std::recursive_mutex> lock(deviceInfoListMutex_);
            removedDevList = queryRemovedDevice(devUid);
            for(auto iter = deviceInfoList_.begin(); iter != deviceInfoList_.end();) {
                if((*iter)->getUid() == devUid) {
                    iter = deviceInfoList_.erase(iter);
                }
                else {
                    iter++;
                }
            }
        }
        if(removedDevList.size()) {
            LOG_DEBUG("device list changed: removed={0}, current={1}", removedDevList.size(), deviceInfoList_.size());
            if(!removedDevList.empty()) {
                LOG_DEBUG("Removed device list:");
                for(auto &deviceInfo: removedDevList) {
                    LOG_DEBUG("  - Name: {}, PID: 0x{:04X}, SN/ID: {}", deviceInfo->getName(), deviceInfo->getPid(), deviceInfo->getDeviceSn());
                }
            }
            if(!deviceInfoList_.empty()) {
                LOG_DEBUG("Remained device list:");
                for(auto &deviceInfo: deviceInfoList_) {
                    LOG_DEBUG("  - Name: {}, PID: 0x{:04X}, SN/ID: {}", deviceInfo->getName(), deviceInfo->getPid(), deviceInfo->getDeviceSn());
                }
            }
            std::unique_lock<std::mutex> lock(callbackMutex_);
            if(!destroy_ && devChangedCallback_) {
                devChangedCallback_(removedDevList, {});
            }
        }
    }
    else {  // OB_DEVICE_ARRIVAL
        newUsbPortArrival_ = true;
        newUsbPortArrivalCV_.notify_all();
    }
}

DeviceEnumInfoList UsbDeviceEnumerator::queryRemovedDevice(std::string rmDevUid) {
    auto portInfoList = currentUsbPortInfoList_;
    auto iter         = portInfoList.begin();
    while(iter != portInfoList.end()) {
        auto portInfo = std::dynamic_pointer_cast<const USBSourcePortInfo>(*iter);
        if(portInfo->url == rmDevUid || portInfo->infUrl == rmDevUid) {
            iter = portInfoList.erase(iter);
            LOG_DEBUG("usb device removed: {}", rmDevUid);
            continue;
        }
        iter++;
    }
    LOG_DEBUG("Current usb device port list:");
    for(const auto &item: portInfoList) {
        auto portInfo = std::dynamic_pointer_cast<const USBSourcePortInfo>(item);
        LOG_DEBUG(" - {0} | {1}", portInfo->infUrl, portInfo->infName);
    }

    std::unique_lock<std::recursive_mutex> lock(deviceInfoListMutex_);
    if(portInfoList != currentUsbPortInfoList_) {
        currentUsbPortInfoList_           = portInfoList;
        DeviceEnumInfoList curList        = usbDeviceInfoMatch(portInfoList);
        auto               removedDevList = utils::subtract_sets(deviceInfoList_, curList);
        deviceInfoList_                   = curList;
        return removedDevList;
    }
    return {};
}

DeviceEnumInfoList UsbDeviceEnumerator::queryArrivalDevice() {
    std::unique_lock<std::recursive_mutex> lock(deviceInfoListMutex_);
    auto                                   portInfoList = platform_->queryUsbSourcePortInfos();
    if(portInfoList != currentUsbPortInfoList_) {
        currentUsbPortInfoList_ = portInfoList;
        LOG_DEBUG("Current usb device port list:");
        for(const auto &item: currentUsbPortInfoList_) {
            auto portInfo = std::dynamic_pointer_cast<const USBSourcePortInfo>(item);
            LOG_DEBUG(" - {0} | {1}", portInfo->infUrl, portInfo->infName);
        }
        DeviceEnumInfoList curList = usbDeviceInfoMatch(portInfoList);
        return utils::subtract_sets(curList, deviceInfoList_);
    }
    return {};
}

DeviceEnumInfoList UsbDeviceEnumerator::usbDeviceInfoMatch(const SourcePortInfoList portInfoList) {
    DeviceEnumInfoList deviceInfoList;
    auto               g330Devs = G330DeviceInfo::pickDevices(portInfoList);
    std::copy(g330Devs.begin(), g330Devs.end(), std::back_inserter(deviceInfoList));

    auto g2Devs = G2DeviceInfo::pickDevices(portInfoList);
    std::copy(g2Devs.begin(), g2Devs.end(), std::back_inserter(deviceInfoList));

    auto a2Devs = Astra2DeviceInfo::pickDevices(portInfoList);
    std::copy(a2Devs.begin(), a2Devs.end(), std::back_inserter(deviceInfoList));

    auto femtoBoltDevs = FemtoBoltDeviceInfo::pickDevices(portInfoList);
    std::copy(femtoBoltDevs.begin(), femtoBoltDevs.end(), std::back_inserter(deviceInfoList));

    auto femtoMegaDevs = FemtoMegaDeviceInfo::pickDevices(portInfoList);
    std::copy(femtoMegaDevs.begin(), femtoMegaDevs.end(), std::back_inserter(deviceInfoList));

    auto bootDevs = BootDeviceInfo::pickDevices(portInfoList);
    std::copy(bootDevs.begin(), bootDevs.end(), std::back_inserter(deviceInfoList));

    return deviceInfoList;
}

void UsbDeviceEnumerator::deviceArrivalHandleThreadFunc() {
    std::mutex                   mtx;
    std::unique_lock<std::mutex> lk(mtx);
    while(!destroy_) {
        newUsbPortArrivalCV_.wait(lk, [&]() { return newUsbPortArrival_ || destroy_; });
        if(destroy_) {
            break;
        }
        uint16_t delayTime = 1000;
#ifdef OS_MACOS
        delayTime = 3000;
#endif
        do {
            newUsbPortArrival_ = false;
            newUsbPortArrivalCV_.wait_for(lk, std::chrono::milliseconds(delayTime));
        } while(!destroy_ && newUsbPortArrival_);

        if(destroy_) {
            break;
        }
        DeviceEnumInfoList addedDevList;
        {
            std::unique_lock<std::recursive_mutex> lock(deviceInfoListMutex_);

            addedDevList = queryArrivalDevice();
            for(auto &item: addedDevList) {
                deviceInfoList_.emplace_back(item);
            }
        }

        if(!addedDevList.empty()) {
            LOG_DEBUG("device list changed: added={0}, current={1}", addedDevList.size(), deviceInfoList_.size());
            if(!deviceInfoList_.empty()) {
                LOG_DEBUG("Current device list: ");
                for(auto &deviceInfo: deviceInfoList_) {
                    LOG_DEBUG("  - Name: {}, PID: 0x{:04X}, SN/ID: {}", deviceInfo->getName(), deviceInfo->getPid(), deviceInfo->getDeviceSn());
                }
            }
            std::unique_lock<std::mutex> lock(callbackMutex_);
            if(!destroy_ && devChangedCallback_) {
                devChangedCallback_({}, addedDevList);
            }
        }
    }
}

DeviceEnumInfoList UsbDeviceEnumerator::getDeviceInfoList() {
    std::unique_lock<std::recursive_mutex> lock(deviceInfoListMutex_);
    return deviceInfoList_;
}

void UsbDeviceEnumerator::setDeviceChangedCallback(DeviceChangedCallback callback) {
    std::unique_lock<std::mutex> lock(callbackMutex_);
    devChangedCallback_ = [callback, this](const DeviceEnumInfoList &removedList, const DeviceEnumInfoList &addedList) {
        (void)this;
#ifdef __ANDROID__
        // On the Android platform, it is necessary to call back to Java in the same thread, and complete the release of relevant resources in the callback
        // function.
        callback(removedList, addedList);
#elif defined(__linux__)
        // Solve the problem of deadlock caused by multiple callbacks in a short period of time in Linux, and the related interfaces that access libusb are
        // called in the user callback function.
        auto cbThread = std::thread(callback, removedList, addedList);
        cbThread.detach();
#else
        // On the WIN platform, since the callback is called by MF-related threads, if the callback is directly made to the user program without switching
        // threads, the user program needs to update the MFC interface, which will cause the program to crash;

        if(devChangedCallbackThread_.joinable()) {
            devChangedCallbackThread_.join();
        }
        // auto cb = [this, removedList, addedList](){
        //     LOG_ERROR("device changed callback begin";
        //     callback(removedList, addedList);
        //     LOG_ERROR("device changed callback end";
        // };
        // devChangedCallbackThread_ = std::thread(cb);
        devChangedCallbackThread_ = std::thread(callback, removedList, addedList);
#endif
    };
}

}  // namespace libobsensor

