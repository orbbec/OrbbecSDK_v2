// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

namespace libobsensor {
#include <string>
#include <vector>

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
};
}  // namespace libobsensor

#ifdef __cplusplus
extern "C" {
#endif

struct ob_device_preset_list_t {
    std::vector<std::string> presetList;
};

#ifdef __cplusplus
}
#endif
