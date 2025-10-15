// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "IDeviceMonitor.hpp"
#include "ISourcePort.hpp"
#include "DeviceComponentBase.hpp"

#include <map>

namespace libobsensor {

/**
 * @brief LiDARDeviceMonitor class
 * @details TODO: LiDAR device doesn't support heartbeat and get state now
 */
class LiDARDeviceMonitor : public IDeviceMonitor, public DeviceComponentBase {
public:
    LiDARDeviceMonitor(IDevice *owner, std::shared_ptr<ISourcePort> sourcePort);
    virtual ~LiDARDeviceMonitor() noexcept;

    OBDeviceState getCurrentDeviceState() const override;
    int           registerStateChangedCallback(DeviceStateChangedCallback callback) override;
    void          unregisterStateChangedCallback(int callbackId) override;
    void          enableHeartbeat() override;
    void          disableHeartbeat() override;
    bool          isHeartbeatEnabled() const override;
    void          pauseHeartbeat() override;
    void          resumeHeartbeat() override;

    void sendAndReceiveData(const uint8_t *sendData, uint32_t sendDataSize, uint8_t *receiveData, uint32_t *receiveDataSize) override;

private:
    std::shared_ptr<IVendorDataPort> vendorDataPort_;
};

}  // namespace libobsensor
