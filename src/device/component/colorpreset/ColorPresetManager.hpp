// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include <map>
#include <string>
#include <vector>
#include <mutex>

#include "IPresetManager.hpp"
#include "IDevice.hpp"
#include "DeviceComponentBase.hpp"

namespace libobsensor {

class ColorPresetManager : public IColorPresetManager, public DeviceComponentBase {
public:
    ColorPresetManager(IDevice *owner, std::map<int32_t, std::string> valueToName);

    const std::string              &getCurrentName() const override;
    void                            switchPreset(const std::string &name) override;
    const std::vector<std::string> &getPresetList() const override;

private:
    std::map<int32_t, std::string> valueToPresetName_;
    std::map<std::string, int32_t> presetNameToValue_;
    std::vector<std::string>       presetList_;
    std::string                    currentPresetName_;
    mutable std::mutex             mutex_;
};

}  // namespace libobsensor
