// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "DeviceBase.hpp"
#include "IDeviceManager.hpp"
#include "frameprocessor/FrameProcessor.hpp"
#if defined(BUILD_NET_PAL)
#include "accesscontroller/GvcpCcpController.hpp"
#endif

#include <map>
#include <memory>

namespace libobsensor {
class G435LeDeviceBase : public DeviceBase {
public:
    G435LeDeviceBase(const std::shared_ptr<const IDeviceEnumInfo> &info, OBDeviceAccessMode accessMode);
    virtual ~G435LeDeviceBase() noexcept override;

    void init() override;

protected:
    void initSensorStreamProfile(std::shared_ptr<ISensor> sensor);

    void updateDefaultStreamProfile(std::shared_ptr<libobsensor::ISensor> sensor, std::shared_ptr<const libobsensor::StreamProfile> streamProfile);

protected:
    uint64_t deviceTimeFreq_ = 1000;
};

class G435LeDevice : public G435LeDeviceBase {
public:
    G435LeDevice(const std::shared_ptr<const IDeviceEnumInfo> &info, OBDeviceAccessMode accessMode);
    virtual ~G435LeDevice() noexcept override;

    void deactivate() override;

private:
    void init() override;
    void initSensorList();
    void initProperties();
    void initSensorStreamProfile(std::shared_ptr<ISensor> sensor);
    void updateSensorStreamProfile();

    void fetchDeviceInfo() override;
#if defined(BUILD_NET_PAL)
    void checkAndAcquireCCP();
#endif

private:
    const uint64_t frameTimeFreq_      = 1000;
    const uint64_t colorframeTimeFreq_ = 90000;
#if defined(BUILD_NET_PAL)
    std::shared_ptr<GvcpCcpController> ccpController_;
#endif
};

}  // namespace libobsensor
