// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#pragma once
#include "IDevice.hpp"
#include "IDeviceSyncConfigurator.hpp"
#include "DeviceComponentBase.hpp"

#include <vector>
#include <memory>
#include <map>

namespace libobsensor {

class OpenNIDeviceSyncConfigurator : public IDeviceSyncConfigurator, public DeviceComponentBase {
public:
    OpenNIDeviceSyncConfigurator(IDevice *owner, const std::vector<OBMultiDeviceSyncMode> &supportedSyncModes);
    virtual ~OpenNIDeviceSyncConfigurator() override = default;

    OBMultiDeviceSyncConfig getSyncConfig() override;
    void                    setSyncConfig(const OBMultiDeviceSyncConfig &deviceSyncConfig) override;
    uint16_t                getSupportedSyncModeBitmap() override;
    void                    triggerCapture() override;

private:
    const std::vector<OBMultiDeviceSyncMode> supportedSyncModes_;

    OBMultiDeviceSyncConfig currentMultiDevSyncConfig_;
};

}  // namespace libobsensor
