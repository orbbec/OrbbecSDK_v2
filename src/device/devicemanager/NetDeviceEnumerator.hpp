// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "IDeviceManager.hpp"
#include "IDeviceWatcher.hpp"
#include "Platform.hpp"

namespace libobsensor {
class NetDeviceEnumerator : public IDeviceEnumerator {
public:
    NetDeviceEnumerator(DeviceChangedCallback callback);
    virtual ~NetDeviceEnumerator() noexcept;
    virtual DeviceEnumInfoList getDeviceInfoList() override;
    virtual void               setDeviceChangedCallback(DeviceChangedCallback callback) override;

    static std::shared_ptr<IDevice> createDevice(std::string address, uint16_t port);

private:
    static DeviceEnumInfoList deviceInfoMatch(const SourcePortInfoList infoList);

    void               onPlatformDeviceChanged(OBDeviceChangedType changeType, std::string devUid);
    DeviceEnumInfoList queryDeviceList();

private:
    std::shared_ptr<Platform> platform_;

    std::mutex            deviceChangedCallbackMutex_;
    DeviceChangedCallback deviceChangedCallback_;
    std::thread           devEnumChangedCallbackThread_;

    std::recursive_mutex deviceInfoListMutex_;
    DeviceEnumInfoList   deviceInfoList_;
    SourcePortInfoList   sourcePortInfoList_;

    std::shared_ptr<IDeviceWatcher> deviceWatcher_;
};
}  // namespace libobsensor
