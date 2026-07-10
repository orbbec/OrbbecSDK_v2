// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include <memory>
#include <string>
#include <vector>

namespace libobsensor {
class ApplicationConfig;

constexpr const char kCustomPresetName[] = "Custom";

class IPresetManager {
public:
    virtual ~IPresetManager() = default;

    virtual void                            loadPreset(const std::string &presetName)                                                   = 0;
    virtual const std::string              &getCurrentPresetName() const                                                                = 0;
    virtual const std::vector<std::string> &getAvailablePresetList() const                                                              = 0;
    virtual void                            loadPresetFromJsonData(const std::string &presetName, const std::vector<uint8_t> &jsonData) = 0;
    virtual void                            loadPresetFromJsonFile(const std::string &filePath)                                         = 0;
    virtual const std::vector<uint8_t>     &exportSettingsAsPresetJsonData(const std::string &presetName)                               = 0;
    virtual void                            exportSettingsAsPresetJsonFile(const std::string &filePath)                                 = 0;
    virtual void                            fetchPreset()                                                                               = 0;

    virtual bool isApplicationConfigSupported() const {
        return false;
    }
    virtual std::shared_ptr<ApplicationConfig> getApplicationConfig() {
        return nullptr;
    }
    // Return the application config carried by an externally imported preset, keyed by preset name.
    // Returns nullptr for built-in presets or presets that carry no application config.
    virtual std::shared_ptr<ApplicationConfig> getApplicationConfig(const std::string &presetName) {
        (void)presetName;
        return nullptr;
    }
};

class IColorPresetManager {
public:
    virtual ~IColorPresetManager() = default;

    virtual const std::string              &getCurrentName() const                = 0;
    virtual void                            switchPreset(const std::string &name) = 0;
    virtual const std::vector<std::string> &getPresetList() const                 = 0;
};
}  // namespace libobsensor

#ifdef __cplusplus
extern "C" {
#endif

struct ob_device_preset_list_t {
    std::vector<std::string> presetList;
};

struct ob_color_preset_list_t {
    std::vector<std::string> list;
};

#ifdef __cplusplus
}
#endif
