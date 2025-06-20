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

/**
 * @brief Playback device sync configurator, read only
 */
class PlaybackDeviceSyncConfigurator : public IDeviceSyncConfigurator, public DeviceComponentBase {
public:
    PlaybackDeviceSyncConfigurator(IDevice *owner);
    virtual ~PlaybackDeviceSyncConfigurator() override = default;

    OBMultiDeviceSyncConfig getSyncConfig() override;
    void                    setSyncConfig(const OBMultiDeviceSyncConfig &deviceSyncConfig) override;
    uint16_t                getSupportedSyncModeBitmap() override;
    void                    triggerCapture() override;

private:
    std::atomic<bool>       isSyncConfigInit_;
    OBMultiDeviceSyncConfig currentMultiDevSyncConfig_;
};

}  // namespace libobsensor
