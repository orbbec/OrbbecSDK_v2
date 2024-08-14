#pragma once
#include "IDevice.hpp"
#include "IDeviceSyncConfigurator.hpp"
#include "DeviceComponentBase.hpp"

#include <vector>
#include <memory>

namespace libobsensor {

class G330DeviceSyncConfigurator : public IDeviceSyncConfigurator, public DeviceComponentBase {
public:
    G330DeviceSyncConfigurator(IDevice *owner, const std::vector<OBMultiDeviceSyncMode> &supportedSyncModes);
    virtual ~G330DeviceSyncConfigurator() = default;

    OBMultiDeviceSyncConfig      getSyncConfig() override;
    void                         setSyncConfig(const OBMultiDeviceSyncConfig &deviceSyncConfig) override;
    uint16_t                     getSupportedSyncModeBitmap() override;
    void                         triggerCapture() override;

private:
    const std::vector<OBMultiDeviceSyncMode> supportedSyncModes_;

    std::atomic<bool>       isSyncConfigInit_;
    OBMultiDeviceSyncConfig currentMultiDevSyncConfig_;
};

}  // namespace libobsensor