// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "IDevice.hpp"
#include "InternalTypes.hpp"
#include "IDepthWorkModeManager.hpp"
#include "DeviceComponentBase.hpp"

namespace libobsensor {

class G330DepthWorkModeManager : public IDepthWorkModeManager, public DeviceComponentBase {
public:
    G330DepthWorkModeManager(IDevice *owner);
    virtual ~G330DepthWorkModeManager() noexcept override = default;

    std::vector<DepthWorkModeItem> getDepthWorkModeList() const override;
    const DepthWorkModeItem       &getCurrentDepthWorkMode() const override;
    void                           switchDepthWorkMode(const std::string &modeName, const std::string &version = "") override;
    void                           fetchDepthWorkModeList() override;
    bool                           isVersionSupported() const override;

private:
    void switchDepthWorkMode(const OBDepthWorkModeV2_Internal &targetDepthMode);

private:
    std::vector<DepthWorkModeItem> depthWorkModeItemList_;
    DepthWorkModeItem              currentWorkMode_;
    bool                           versionSupported_;
};

}  // namespace libobsensor
