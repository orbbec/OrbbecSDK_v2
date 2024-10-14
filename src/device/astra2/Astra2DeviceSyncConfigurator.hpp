// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#pragma once
#include "IDevice.hpp"
#include "IDeviceSyncConfigurator.hpp"
#include "DeviceComponentBase.hpp"
#include "syncconfig/DeviceSyncConfigurator.hpp"

#include <vector>
#include <memory>
#include <map>

namespace libobsensor {

class Astra2DeviceSyncConfigurator : public DeviceSyncConfiguratorOldProtocol {
public:
    Astra2DeviceSyncConfigurator(IDevice *owner, const std::vector<OBMultiDeviceSyncMode> &supportedSyncModes);
    virtual ~Astra2DeviceSyncConfigurator() = default;

    OBMultiDeviceSyncConfig getSyncConfig() override;
    void                    setSyncConfig(const OBMultiDeviceSyncConfig &deviceSyncConfig) override;
};
}  // namespace libobsensor
