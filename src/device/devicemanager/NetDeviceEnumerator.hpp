// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "IDeviceManager.hpp"
#include "IDeviceWatcher.hpp"
#include "Platform.hpp"
#include "DeviceActivityManager.hpp"

namespace libobsensor {
class NetDeviceEnumerator : public IDeviceEnumerator {
public:
    NetDeviceEnumerator(DeviceChangedCallback callback, std::shared_ptr<DeviceActivityManager> deviceActivityManager);
    virtual ~NetDeviceEnumerator() noexcept override;
    virtual DeviceEnumInfoList getDeviceInfoList() override;
    virtual void               setDeviceChangedCallback(DeviceChangedCallback callback) override;

    static std::shared_ptr<const IDeviceEnumInfo> queryNetDevice(std::string address, uint16_t port);

private:
    static DeviceEnumInfoList deviceInfoMatch(const SourcePortInfoList infoList);

    bool               checkDeviceActivity(std::shared_ptr<const IDeviceEnumInfo> dev, std::shared_ptr<const NetSourcePortInfo> info);
    bool               onPlatformDeviceChanged(OBDeviceChangedType changeType, std::string devUid);
    bool               handleDeviceRemoved(std::string devUid);
    bool               handleDeviceArrival(std::string devUid);
    DeviceEnumInfoList queryDeviceList();

private:
    std::shared_ptr<Platform> platform_;

    std::mutex            deviceChangedCallbackMutex_;
    DeviceChangedCallback deviceChangedCallback_;
    std::thread           devEnumChangedCallbackThread_;

    std::recursive_mutex deviceInfoListMutex_;
    DeviceEnumInfoList   deviceInfoList_;

    std::shared_ptr<IDeviceWatcher>        deviceWatcher_;
    std::shared_ptr<DeviceActivityManager> deviceActivityManager_;
};
}  // namespace libobsensor
