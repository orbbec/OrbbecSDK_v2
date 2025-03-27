// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "IAlgParamManager.hpp"
#include "PlaybackDevicePort.hpp"
#include "component/DeviceComponentBase.hpp"

namespace libobsensor {

class PlaybackDeviceParamManager : public IDisparityAlgParamManager, public DeviceComponentBase {
public:
    PlaybackDeviceParamManager(IDevice *owner, std::shared_ptr<PlaybackDevicePort> port);
    virtual ~PlaybackDeviceParamManager() noexcept override = default;

    virtual const OBDisparityParam &getDisparityParam() const override;

    void bindDisparityParam(std::vector<std::shared_ptr<const StreamProfile>> streamProfileList);

private:
    std::shared_ptr<PlaybackDevicePort> port_;
    OBDisparityParam                    disparityParam_;
};

}  // namespace libobsensor