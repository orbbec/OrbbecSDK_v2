// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#if defined(BUILD_NET_PAL)

#pragma once

#include "IDeviceAccessController.hpp"
#include "IDeviceManager.hpp"
#include "ethernet/gige/GVCPTransmit.hpp"
#include <atomic>
#include <mutex>
#include <thread>
#include <condition_variable>

namespace libobsensor {

class GigECcpController : public IDeviceAccessController {
public:
    GigECcpController(const std::shared_ptr<const IDeviceEnumInfo> &info);
    virtual ~GigECcpController() override;

    bool isSupported() const override {
        return ccpSupported_;
    }

    OBDeviceAccessMode getState() const override {
        return accessMode_;
    }

    void acquireControl(OBDeviceAccessMode accessMode) override;
    void releaseControl() override;

private:
    void checkCcpCapability();

private:
    std::atomic<OBDeviceAccessMode>              accessMode_{ OB_DEVICE_ACCESS_DENIED };
    bool                                         ccpSupported_{ false };
    const std::shared_ptr<const IDeviceEnumInfo> enumInfo_;
    std::shared_ptr<GVCPTransmit>                gvcpTransmit_;
    std::thread                                  keepaliveThread_;
    std::condition_variable                      keepaliveCv_;
    std::atomic<bool>                            keepaliveStopped_{ false };
};

}  // namespace libobsensor

#endif  // BUILD_NET_PAL
