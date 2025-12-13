// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "IDeviceManager.hpp"
#include "DeviceActivityManager.hpp"

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <atomic>
#include <unordered_map>

namespace libobsensor {

class DeviceManager : public IDeviceManager {
private:
    DeviceManager();

    static std::weak_ptr<DeviceManager> instanceWeakPtr_;
    static std::mutex                   instanceMutex_;

public:
    static std::shared_ptr<DeviceManager> getInstance();
    ~DeviceManager() noexcept override;

    std::shared_ptr<IDevice> createDevice(const std::shared_ptr<const IDeviceEnumInfo> &info, OBDeviceAccessMode accessMode) override;
    std::shared_ptr<IDevice> createNetDevice(std::string address, uint16_t port, OBDeviceAccessMode accessMode) override;
    bool                     forceIpConfig(std::string deviceUid, const OBNetIpConfig &config) override;

    DeviceEnumInfoList getDeviceInfoList() override;
    OBCallbackId       registerDeviceChangedCallback(DeviceChangedCallback callback) override;
    bool               unregisterDeviceChangedCallback(OBCallbackId id) override;

    void enableNetDeviceEnumeration(bool enable) override;
    bool isNetDeviceEnumerationEnable() const override;

    void enableDeviceClockSync(uint64_t repeatInterval) override;

private:
    void onDeviceChanged(const DeviceEnumInfoList &removed, const DeviceEnumInfoList &added);

    void startDeviceActivitySync();
    void stopDeviceActivitySync();

private:
    std::atomic<bool> destroy_;

    std::mutex callbackMutex_;
    // trying to resolve the multi-node callback issues in ROS1
    // DeviceChangedCallback devChangedCallback_ = nullptr;
    // std::vector<DeviceChangedCallback> devChangedCallbacks_;
    std::atomic<OBCallbackId>                               callbackId_;
    std::unordered_map<OBCallbackId, DeviceChangedCallback> devChangedCallbacks_;

    std::map<std::string, std::weak_ptr<IDevice>> createdDevices_;
    std::mutex                                    createdDevicesMutex_;

    std::thread             multiDeviceSyncThread_;
    std::condition_variable multiDeviceSyncCv_;
    uint64_t                multiDeviceSyncIntervalMs_;  // unit: ms

    std::vector<std::shared_ptr<IDeviceEnumerator>> deviceEnumerators_;

    std::map<std::string, std::shared_ptr<const IDeviceEnumInfo>> customConnectedDevices_;
    std::mutex                                                    customConnectedDevicesMutex_;
    bool                                                          isCustomConnectedDevice_ = false;

    std::thread             deviceActivitySyncThread_;
    std::condition_variable deviceActivityCv_;
    std::atomic<bool>       deviceActivitySyncStopped_{ false };

    std::shared_ptr<DeviceActivityManager> deviceActivityManager_;
};

}  // namespace libobsensor
