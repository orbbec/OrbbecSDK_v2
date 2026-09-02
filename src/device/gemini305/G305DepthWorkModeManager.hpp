// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "IDevice.hpp"
#include "InternalTypes.hpp"
#include "IDepthWorkModeManager.hpp"
#include "DeviceComponentBase.hpp"

namespace libobsensor {

constexpr const char *kDoubleRgbMode = "Dual Color Streams";

class G305DepthWorkModeManager : public IDepthWorkModeManager, public DeviceComponentBase {
public:
    G305DepthWorkModeManager(IDevice *owner);
    virtual ~G305DepthWorkModeManager() noexcept override = default;

    std::vector<DepthWorkModeItem> getDepthWorkModeList() const override;
    const DepthWorkModeItem       &getCurrentDepthWorkMode() const override;
    void                           switchDepthWorkMode(const std::string &modeName, const std::string &version = "") override;
    void                           fetchDepthWorkModeList() override;

private:
    void switchDepthWorkMode(const OBDepthWorkModeV2_Internal &targetDepthMode);

private:
    std::vector<DepthWorkModeItem> depthWorkModeList_;
    DepthWorkModeItem              currentWorkMode_;
};

}  // namespace libobsensor
