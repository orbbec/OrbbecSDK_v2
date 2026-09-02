// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "IDevice.hpp"
#include "InternalTypes.hpp"
#include "IDepthWorkModeManager.hpp"
#include "DeviceComponentBase.hpp"
#include "PlaybackDevicePort.hpp"

namespace libobsensor {

class PlaybackDepthWorkModeManager : public IDepthWorkModeManager, public DeviceComponentBase {
public:
    PlaybackDepthWorkModeManager(IDevice *owner, std::shared_ptr<PlaybackDevicePort> port);
    virtual ~PlaybackDepthWorkModeManager() noexcept override = default;

    virtual std::vector<DepthWorkModeItem> getDepthWorkModeList() const override;
    virtual const DepthWorkModeItem       &getCurrentDepthWorkMode() const override;
    virtual void                           switchDepthWorkMode(const std::string &name, const std::string &version = "") override;
    void                                   fetchDepthWorkModeList() override;

private:
    std::shared_ptr<PlaybackDevicePort> port_;
    DepthWorkModeItem                   currentItem_;
};
}  // namespace libobsensor
