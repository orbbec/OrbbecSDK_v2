// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "IDeviceMonitor.hpp"
#include "ISourcePort.hpp"
#include "DeviceComponentBase.hpp"

#include <map>

namespace libobsensor {
class DeviceMonitor : public IDeviceMonitor, public DeviceComponentBase {
public:
    DeviceMonitor(IDevice *owner, std::shared_ptr<ISourcePort> sourcePort);
    virtual ~DeviceMonitor() noexcept;

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
    void start();
    void stop();
    void heartbeatAndFetchState();

private:
    std::shared_ptr<IVendorDataPort> vendorDataPort_;

    uint32_t                                       cbIdCounter_;
    std::mutex                                     stateChangedCallbacksMutex_;
    std::map<uint32_t, DeviceStateChangedCallback> stateChangedCallbacks_;


    std::mutex                       commMutex_;
    std::condition_variable heartbeatAndFetchStateThreadCv_;
    std::thread             heartbeatAndFetchStateThread_;
    std::atomic<bool>       heartbeatAndFetchStateThreadStarted_;

    std::atomic<bool> heartbeatEnabled_;
    std::atomic<bool> heartbeatPaused_;

    std::vector<uint8_t> hbRecvData_;
    std::vector<uint8_t> hbSendData_;

    OBDeviceState devState_;
};
}  // namespace libobsensor
