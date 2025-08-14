// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "IDevice.hpp"
#include "IPresetManager.hpp"
#include "InternalTypes.hpp"
#include "DeviceComponentBase.hpp"
#include "gemini330/DaBaiAPresetManager.hpp"
#include "gemini330/G330PresetManager.hpp"

#include <map>
#include <string>
#include <vector>

namespace libobsensor {

class PlaybackPresetManager : public IPresetManager, public DeviceComponentBase {
public:
    PlaybackPresetManager(IDevice *owner);
    ~PlaybackPresetManager() override = default;

    void                            loadPreset(const std::string &presetName) override;
    const std::string              &getCurrentPresetName() const override;
    const std::vector<std::string> &getAvailablePresetList() const override;
    void                            loadPresetFromJsonData(const std::string &presetName, const std::vector<uint8_t> &jsonData) override;
    void                            loadPresetFromJsonFile(const std::string &filePath) override;
    const std::vector<uint8_t>     &exportSettingsAsPresetJsonData(const std::string &presetName) override;
    void                            exportSettingsAsPresetJsonFile(const std::string &filePath) override;
    void                            fetchPreset() override;

private:
    Json::Value exportSettingsAsPresetJsonValue();
    Json::Value exportSettingsAsPresetJsonValueDaBaiA();
    Json::Value exportSettingsAsPresetJsonValueG300();

private:
    bool                     isDaBaiADevice_ = false;
    std::vector<std::string> availablePresets_;    // Available preset names; playback devices support only one preset
    std::string              currentPreset_ = "";  // Current preset name
    std::vector<uint8_t>     tmpJsonData_;
};

}  // namespace libobsensor
