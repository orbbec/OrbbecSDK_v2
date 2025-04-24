// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "NetDeviceEnumerator.hpp"
#include "femtomega/FemtoMegaDeviceInfo.hpp"
#include "gemini330/G330DeviceInfo.hpp"
#include "gemini2/G2DeviceInfo.hpp"
#include "bootloader/BootDeviceInfo.hpp"
#include "ethernet/RTSPStreamPort.hpp"
#include "ethernet/NetDataStreamPort.hpp"
#include "property/VendorPropertyAccessor.hpp"
#include "property/InternalProperty.hpp"
#include "DevicePids.hpp"

#include "utils/Utils.hpp"

#include <map>
#include <string>

namespace libobsensor {

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
    sourcePortInfoList_.clear();
    auto portInfoList = platform_->queryNetSourcePort();

    if(portInfoList.empty()) {
        LOG_DEBUG("No net source port found!");
        return {};
    }

    for(const auto &portInfo: portInfoList) {
        auto info = std::dynamic_pointer_cast<const NetSourcePortInfo>(portInfo);
        if(info->pid != 0) {
            sourcePortInfoList_.push_back(info);
            continue;
        }

        // try fetch pid from device via vendor property
        BEGIN_TRY_EXECUTE({
            auto            port               = platform_->getSourcePort(info);
            auto            vendorPropAccessor = std::make_shared<VendorPropertyAccessor>(nullptr, port);
            OBPropertyValue value;
            value.intValue = 0;
            vendorPropAccessor->getPropertyValue(OB_PROP_DEVICE_PID_INT, &value);
            auto newInfo = std::make_shared<NetSourcePortInfo>(info->portType, info->netInterfaceName, info->localMac, info->localAddress, info->address,
                                                               info->port, info->mac, info->serialNumber, value.intValue);
            sourcePortInfoList_.push_back(newInfo);
        })
        CATCH_EXCEPTION_AND_LOG(DEBUG, "Get device pid failed! address:{}, port:{}", info->address, info->port);
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
    auto               bootDevices = BootDeviceInfo::pickNetDevices(infoList);
    deviceInfoList.insert(deviceInfoList.end(), bootDevices.begin(), bootDevices.end());

    auto               megaDevices = FemtoMegaDeviceInfo::pickNetDevices(infoList);
    deviceInfoList.insert(deviceInfoList.end(), megaDevices.begin(), megaDevices.end());

    auto dev330Devices = G330DeviceInfo::pickNetDevices(infoList);
    deviceInfoList.insert(deviceInfoList.end(), dev330Devices.begin(), dev330Devices.end());

    auto g2Devices = G2DeviceInfo::pickNetDevices(infoList);
    deviceInfoList.insert(deviceInfoList.end(), g2Devices.begin(), g2Devices.end());

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
    DeviceEnumInfoList newRmDevs;

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
            LOG_DEBUG("{} net device(s) in removedeviceList:", rmDevs.size());
            for(auto &&item: rmDevs) {
                auto firstPortInfo = item->getSourcePortInfoList().front();
                auto info          = std::dynamic_pointer_cast<const NetSourcePortInfo>(firstPortInfo);
                if(info->pid == OB_DEVICE_G335LE_PID || info->pid == OB_FEMTO_MEGA_PID || IS_OB_FEMTO_MEGA_I_PID(info->pid) || info->pid == OB_DEVICE_G435LE_PID) {
                    bool    disconnected = true;
                    BEGIN_TRY_EXECUTE({
                        auto            sourcePort         = Platform::getInstance()->getNetSourcePort(info);
                        auto            vendorPropAccessor = std::make_shared<VendorPropertyAccessor>(nullptr, sourcePort);
                        OBPropertyValue value;
                        value.intValue = 0;
                        vendorPropAccessor->getPropertyValue(OB_PROP_DEVICE_PID_INT, &value);
                        disconnected = false;
                        LOG_DEBUG("Get device pid success, pid:{}", value.intValue);
                    })
                    CATCH_EXCEPTION_AND_EXECUTE({
                        LOG_WARN("Get pid failed, ip:{},mac:{},device is disconnect.", info->address, info->mac);
                    });

                    if(!disconnected) {
                        deviceInfoList_.push_back(item);
                    }
                    else {
                        newRmDevs.push_back(item);
                    }
                }
                else {
                    newRmDevs.push_back(item);
                }
            }
        }

        if(!newRmDevs.empty()) {
            LOG_WARN("{} net device(s) removed:", newRmDevs.size());
            for(auto &&item: newRmDevs) {
                auto firstPortInfo = item->getSourcePortInfoList().front();
                auto info          = std::dynamic_pointer_cast<const NetSourcePortInfo>(firstPortInfo);
                LOG_WARN("  - Name: {}, PID: 0x{:04X}, SN/ID: {}, MAC:{}, IP:{}", item->getName(), item->getPid(), item->getDeviceSn(), info->mac,
                          info->address);
            }
        }

        LOG_DEBUG("Current net device list: ({})", deviceInfoList_.size());
        for(auto &&item: deviceInfoList_) {
            auto firstPortInfo = item->getSourcePortInfoList().front();
            auto info          = std::dynamic_pointer_cast<const NetSourcePortInfo>(firstPortInfo);
            LOG_DEBUG("  - Name: {}, PID: 0x{:04X}, SN/ID: {}, MAC:{}, IP:{}", item->getName(), item->getPid(), item->getDeviceSn(), info->mac, info->address);
        }

        deviceChangedCallback_(newRmDevs, addDevs);
    }
}

std::shared_ptr<const IDeviceEnumInfo> NetDeviceEnumerator::queryNetDevice(std::string address, uint16_t port) {
    auto info = std::make_shared<NetSourcePortInfo>(SOURCE_PORT_NET_VENDOR,  //
                                            "Unknown", "Unknown", "Unknown", address, static_cast<uint16_t>(8090), address + ":" + std::to_string(port), "Unknown", 0);

    auto sourcePort         = Platform::getInstance()->getNetSourcePort(info);
    auto vendorPropAccessor = std::make_shared<VendorPropertyAccessor>(nullptr, sourcePort);

    BEGIN_TRY_EXECUTE({
        OBPropertyValue value;
        value.intValue = 0;
        vendorPropAccessor->getPropertyValue(OB_PROP_DEVICE_PID_INT, &value);
        info->pid = static_cast<uint16_t>(value.intValue);
    })
    CATCH_EXCEPTION_AND_EXECUTE({
        LOG_WARN("Get device pid failed, use default pid as Femto Mega Device! address:{}, port:{}", address, port);
        info->pid = OB_FEMTO_MEGA_PID;
    });

    if(info->pid == OB_DEVICE_G335LE_PID) {
        throw invalid_value_exception("No supported G335Le found for address: " + address + ":" + std::to_string(port));
    }

    auto deviceEnumInfoList = deviceInfoMatch({ info });
    if(deviceEnumInfoList.empty()) {
        throw invalid_value_exception("No supported device found for address: " + address + ":" + std::to_string(port));
    }

    return deviceEnumInfoList.front();
}

}  // namespace libobsensor
