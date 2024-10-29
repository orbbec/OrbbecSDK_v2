// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "IDeviceManager.hpp"

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <thread>
#include <condition_variable>

namespace libobsensor {

class DeviceManager : public IDeviceManager {
private:
    DeviceManager();

    static std::weak_ptr<DeviceManager> instanceWeakPtr_;
    static std::mutex                   instanceMutex_;

public:
    static std::shared_ptr<DeviceManager> getInstance();
    ~DeviceManager() noexcept;

    std::shared_ptr<IDevice> createDevice(const std::shared_ptr<const IDeviceEnumInfo> &info) override;
    std::shared_ptr<IDevice> createNetDevice(std::string address, uint16_t port) override;

    DeviceEnumInfoList getDeviceInfoList() override;
    void               setDeviceChangedCallback(DeviceChangedCallback callback) override;

    void enableNetDeviceEnumeration(bool enable) override;
    bool isNetDeviceEnumerationEnable() const override;

    void enableDeviceClockSync(uint64_t repeatInterval) override;

private:
    void onDeviceChanged(const DeviceEnumInfoList &removed, const DeviceEnumInfoList &added);

private:
    bool destroy_;

    std::mutex            callbackMutex_;
    DeviceChangedCallback devChangedCallback_ = nullptr;

    std::map<std::string, std::weak_ptr<IDevice>> createdDevices_;
    std::mutex                                    createdDevicesMutex_;

    std::thread             multiDeviceSyncThread_;
    std::condition_variable multiDeviceSyncCv_;
    uint64_t                multiDeviceSyncIntervalMs_;  // unit: ms

    std::vector<std::shared_ptr<IDeviceEnumerator>> deviceEnumerators_;

    std::map<std::string, std::shared_ptr<const IDeviceEnumInfo>> customConnectedDevices_;
    std::mutex                                                    customConnectedDevicesMutex_;
};
}  // namespace libobsensor

