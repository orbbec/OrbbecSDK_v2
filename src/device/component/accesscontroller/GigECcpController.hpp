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
    /**
     * @brief Construction
     *
     * @param[in] info Enumeration info of the device.
     * @param[in] minVersion The The minimum firmware version for access control, in "a.b.c" format.
     *                       Empty or invalid strings are treated as 0.0.0 (all versions supported).
     *
     * @throws access_denied_exception if the acquisition fails.
     *         unsupported_operation_exception if the device doesn't support CCP
     */
    GigECcpController(const std::shared_ptr<const IDeviceEnumInfo> &info, const std::string &minVersion);
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
    int32_t getFirmwareVersionInt(const std::string &version);
    bool checkCcpCapability(const std::string &minVersion);

private:
    std::atomic<OBDeviceAccessMode>          accessMode_{ OB_DEVICE_ACCESS_DENIED };
    bool                                     ccpSupported_{ false };
    std::shared_ptr<const NetSourcePortInfo> portInfo_;
    std::shared_ptr<GVCPTransmit>            gvcpTransmit_;
    std::thread                              keepaliveThread_;
    std::condition_variable                  keepaliveCv_;
    std::atomic<bool>                        keepaliveStopped_{ false };
};

}  // namespace libobsensor

#endif  // BUILD_NET_PAL
