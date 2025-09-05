// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "IDevice.hpp"
#include "IPresetManager.hpp"
#include "InternalTypes.hpp"
#include "DeviceComponentBase.hpp"

#include <map>
#include <string>
#include <vector>

namespace Json {
class Value;  // forward declaration
}  // namespace Json

namespace libobsensor {

struct MegaPreset {
    // color sensor
    int colorAutoExposure;
    int colorExposureTime;
    int colorGain;
    int colorAutoWhiteBalance;
    int colorWhiteBalance;
    int colorSaturation;
    int colorContrast;
    int colorBrightness;
    int colorSharpness;
    int colorPowerLineFrequency;
};

class MegaPresetManager : public IPresetManager, public DeviceComponentBase {
public:
    MegaPresetManager(IDevice *owner);
    ~MegaPresetManager() override = default;

    void                            loadPreset(const std::string &presetName) override;
    const std::string              &getCurrentPresetName() const override;
    const std::vector<std::string> &getAvailablePresetList() const override;
    void                            loadPresetFromJsonData(const std::string &presetName, const std::vector<uint8_t> &jsonData) override;
    void                            loadPresetFromJsonFile(const std::string &filePath) override;
    const std::vector<uint8_t>     &exportSettingsAsPresetJsonData(const std::string &presetName) override;
    void                            exportSettingsAsPresetJsonFile(const std::string &filePath) override;
    void                            fetchPreset() override;

private:
    void        storeCurrentParamsAsCustomPreset(const std::string &presetName);
    void        loadCustomPreset(const std::string &presetName, const MegaPreset &preset);
    void        loadPresetFromJsonValue(const std::string &presetName, const Json::Value &root);
    Json::Value exportSettingsAsPresetJsonValue(const std::string &presetName);

private:
    std::vector<std::string> availablePresets_;
    std::string              currentPreset_;
    std::vector<uint8_t>     tmpJsonData_;

    std::map<std::string, MegaPreset> customPresets_;
};

}  // namespace libobsensor
