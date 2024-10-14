// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "IDevice.hpp"
#include "ISourcePort.hpp"

namespace libobsensor {
class IDeviceManager;
class IDeviceEnumInfo {
public:
    virtual int                             getPid() const                = 0;
    virtual int                             getVid() const                = 0;
    virtual const std::string              &getUid() const                = 0;
    virtual const std::string              &getConnectionType() const     = 0;
    virtual const std::string              &getName() const               = 0;
    virtual const std::string              &getFullName() const       = 0;
    virtual const std::string              &getDeviceSn() const           = 0;
    virtual const SourcePortInfoList       &getSourcePortInfoList() const = 0;
    virtual std::shared_ptr<IDevice>        createDevice() const          = 0;
    virtual std::shared_ptr<IDeviceManager> getDeviceManager() const      = 0;

    virtual bool operator==(const IDeviceEnumInfo &other) const = 0;
};

typedef std::vector<std::shared_ptr<const IDeviceEnumInfo>>                                     DeviceEnumInfoList;
typedef std::function<void(const DeviceEnumInfoList &removed, const DeviceEnumInfoList &added)> DeviceChangedCallback;

class IDeviceEnumerator {
public:
    virtual ~IDeviceEnumerator()                                                        = default;
    virtual DeviceEnumInfoList getDeviceInfoList()                                      = 0;
    virtual void               setDeviceChangedCallback(DeviceChangedCallback callback) = 0;
};

class IDeviceManager {
public:
    virtual std::shared_ptr<IDevice> createDevice(const std::shared_ptr<const IDeviceEnumInfo> &info) = 0;
    virtual std::shared_ptr<IDevice> createNetDevice(std::string address, uint16_t port)              = 0;

    virtual DeviceEnumInfoList getDeviceInfoList() const                                = 0;
    virtual void               setDeviceChangedCallback(DeviceChangedCallback callback) = 0;

    virtual void enableNetDeviceEnumeration(bool enable) = 0;
    virtual bool isNetDeviceEnumerationEnable() const    = 0;

    virtual void enableDeviceClockSync(uint64_t repeatInterval) = 0;
};

}  // namespace libobsensor

#ifdef __cplusplus
extern "C" {
#endif
struct ob_device_list_t {
    libobsensor::DeviceEnumInfoList              list;
    std::shared_ptr<libobsensor::IDeviceManager> manager;
};
#ifdef __cplusplus
}
#endif
