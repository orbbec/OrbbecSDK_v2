// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#if defined(BUILD_NET_PAL)

#pragma once

#include "IDeviceAccessController.hpp"
#include "IDeviceManager.hpp"
#include "ethernet/gvcp/GVCPTransmit.hpp"
#include <atomic>
#include <mutex>
#include <thread>
#include "utils/SteadyCondVar.hpp"

namespace libobsensor {

class GvcpCcpController : public IDeviceAccessController {
public:
    /**
     * @brief Construction
     *
     * @param[in] info Enumeration info of the device.
     *
     * @throws access_denied_exception if the
     * acquisition fails.
     *         unsupported_operation_exception if the device doesn't support CCP
     */
    GvcpCcpController(const std::shared_ptr<const IDeviceEnumInfo> &info);
    virtual ~GvcpCcpController() override;

    static OBDeviceAccessState queryAccessState(const std::shared_ptr<const IDeviceEnumInfo> &info);

    bool isSupported() const override {
        return ccpSupported_;
    }

    OBDeviceAccessMode getState() const override {
        return accessMode_;
    }

    void acquireControl(OBDeviceAccessMode accessMode) override;
    void releaseControl() override;

private:
    static int32_t     getFirmwareVersionInt(const std::string &version);
    static std::string resolveCcpMinVersion(int vid, int pid);
    static bool        isTimeoutOrUnreachable(int32_t errorCode);
    bool               checkCcpCapability(const std::string &minVersion);

private:
    std::atomic<OBDeviceAccessMode>          accessMode_{ OB_DEVICE_ACCESS_DENIED };
    bool                                     ccpSupported_{ false };
    std::shared_ptr<const NetSourcePortInfo> portInfo_;
    std::shared_ptr<GVCPTransmit>            gvcpTransmit_;
    std::thread                              keepaliveThread_;
    utils::SteadyCondVar                     keepaliveCv_;
    std::atomic<bool>                        keepaliveStopped_{ false };
};

}  // namespace libobsensor

#endif  // BUILD_NET_PAL
