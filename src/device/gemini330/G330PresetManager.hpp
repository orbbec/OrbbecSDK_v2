// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "IDevice.hpp"
#include "IPresetManager.hpp"
#include "InternalTypes.hpp"
#include "DeviceComponentBase.hpp"
#include "G330PresetEngine.hpp"

#include <map>
#include <set>
#include <string>
#include <vector>
#include <atomic>
#include <mutex>

namespace Json {
class Value;  // forward declaration
}  // namespace Json

namespace libobsensor {
class ApplicationConfig;

class G330PresetManager : public IPresetManager, public DeviceComponentBase {
public:
    G330PresetManager(IDevice *owner);
    ~G330PresetManager() override = default;

    void                               loadPreset(const std::string &presetName) override;
    const std::string                 &getCurrentPresetName() const override;
    const std::vector<std::string>    &getAvailablePresetList() const override;
    void                               loadPresetFromJsonData(const std::string &presetName, const std::vector<uint8_t> &jsonData) override;
    void                               loadPresetFromJsonFile(const std::string &filePath) override;
    const std::vector<uint8_t>        &exportSettingsAsPresetJsonData(const std::string &presetName) override;
    void                               exportSettingsAsPresetJsonFile(const std::string &filePath) override;
    void                               fetchPreset() override;
    bool                               isApplicationConfigSupported() const override;
    std::shared_ptr<ApplicationConfig> getApplicationConfig() override;
    std::shared_ptr<ApplicationConfig> getApplicationConfig(const std::string &presetName) override;

private:
    std::shared_ptr<IPresetEngine> getPresetEngine(const Json::Value &root);
    std::shared_ptr<IPresetEngine> getCurrentPresetEngine();
    void                           storeCurrentParamsAsCustomPreset(const std::string &presetName);
    void                           loadCustomPreset(const std::string &presetName, const Json::Value &preset);
    void                           loadPresetFromJsonValue(const std::string &presetName, const Json::Value &root);
    Json::Value                    exportSettingsAsPresetJsonValue(const std::string &presetName);
    void                           storeImportedApplicationConfig(const std::string &presetName, const Json::Value &root);

private:
    std::vector<std::string> availablePresets_;
    std::string              currentPresetName_;
    std::vector<uint8_t>     tmpJsonData_;
    std::atomic<bool>        isExternalDataLoading_{ false };

    std::map<std::string, Json::Value>  customPresets_;
    std::shared_ptr<G330PresetEngine>   presetEngine_;
    std::shared_ptr<G330PresetEngineV1> presetEngineV1_;
    std::mutex                          applicationConfigMutex_;
    std::shared_ptr<ApplicationConfig>  applicationConfig_;

    // Application config carried by externally imported presets, keyed by preset name. Only populated on
    // import (loadPresetFromJsonFile/Data); built-in presets and "Custom" are never stored here.
    std::map<std::string, std::shared_ptr<ApplicationConfig>> importedAppConfigs_;
};

}  // namespace libobsensor
