// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "FemtoMegaPresetManager.hpp"
#include "property/InternalProperty.hpp"
#include "InternalTypes.hpp"
#include "exception/ObException.hpp"
#include "utils/Utils.hpp"

#include <json/json.h>

namespace libobsensor {

MegaPresetManager::MegaPresetManager(IDevice *owner) : DeviceComponentBase(owner) {
    currentPreset_         = "Custom";
    auto propServer = owner->getPropertyServer();

    propServer->registerAccessCallback(
        {
            OB_PROP_COLOR_AUTO_EXPOSURE_BOOL,
            OB_PROP_COLOR_EXPOSURE_INT,
            OB_PROP_COLOR_AUTO_WHITE_BALANCE_BOOL,
            OB_PROP_COLOR_WHITE_BALANCE_INT,
            OB_PROP_COLOR_GAIN_INT,
            OB_PROP_COLOR_CONTRAST_INT,
            OB_PROP_COLOR_SATURATION_INT,
            OB_PROP_COLOR_SHARPNESS_INT,
            OB_PROP_COLOR_BRIGHTNESS_INT,
            OB_PROP_COLOR_POWER_LINE_FREQUENCY_INT,
        },
        [&](uint32_t, const uint8_t *, size_t, PropertyOperationType operationType) {
            if(operationType == PROP_OP_WRITE) {
                currentPreset_ = "Custom";
            }
        });
    storeCurrentParamsAsCustomPreset("Custom");
}

void MegaPresetManager::loadPreset(const std::string &presetName) {
    if(std::find(availablePresets_.begin(), availablePresets_.end(), presetName) == availablePresets_.end()) {
        throw std::invalid_argument("Invalid preset name: " + presetName);
    }

    // store current parameters to  "Custom"
    if(currentPreset_ == "Custom") {
        storeCurrentParamsAsCustomPreset("Custom");
    }

    auto iter = customPresets_.find(presetName);
    if(iter != customPresets_.end()) {
        // Load custom preset
        loadCustomPreset(iter->first, iter->second);
    }
    else {
        currentPreset_ = presetName;
    }
}

const std::string &MegaPresetManager::getCurrentPresetName() const {
    return currentPreset_;
}

const std::vector<std::string> &MegaPresetManager::getAvailablePresetList() const {
    return availablePresets_;
}

void MegaPresetManager::loadPresetFromJsonData(const std::string &presetName, const std::vector<uint8_t> &jsonData) {
    Json::Value  root;
    Json::Reader reader;
    if(!reader.parse(std::string((const char *)jsonData.data(), jsonData.size()), root)) {
        throw std::invalid_argument("Invalid JSON data");
    }
    // store current parameters to  "Custom"
    if(currentPreset_ == "Custom") {
        storeCurrentParamsAsCustomPreset("Custom");
    }
    loadPresetFromJsonValue(presetName, root);
}

void MegaPresetManager::loadPresetFromJsonFile(const std::string &filePath) {
    Json::Value   root;
    std::ifstream ifs(filePath);
    ifs >> root;
    // store current parameters to  "Custom"
    if(currentPreset_ == "Custom") {
        storeCurrentParamsAsCustomPreset("Custom");
    }
    loadPresetFromJsonValue(filePath, root);
}

void MegaPresetManager::loadPresetFromJsonValue(const std::string &presetName, const Json::Value &root) {
    MegaPreset preset{};
    preset.colorAutoExposure          = root["color_auto_exposure"].asBool();
    preset.colorExposureTime          = root["color_exposure_time"].asInt();
    preset.colorAutoWhiteBalance      = root["color_auto_white_balance"].asBool();
    preset.colorWhiteBalance          = root["color_white_balance"].asInt();
    preset.colorGain                  = root["color_gain"].asInt();
    preset.colorContrast              = root["color_contrast"].asInt();
    preset.colorSaturation            = root["color_saturation"].asInt();
    preset.colorSharpness             = root["color_sharpness"].asInt();
    preset.colorBrightness            = root["color_brightness"].asInt();
    preset.colorPowerLineFrequency    = root["color_power_line_frequency"].asInt();

    loadCustomPreset(presetName, preset);

    if(customPresets_.find(presetName) == customPresets_.end()) {
        availablePresets_.emplace_back(presetName);
    }
    customPresets_[presetName] = preset;
}

Json::Value MegaPresetManager::exportSettingsAsPresetJsonValue(const std::string &presetName) {
    storeCurrentParamsAsCustomPreset(presetName);
    auto iter = customPresets_.find(presetName);
    if(iter == customPresets_.end()) {
        throw std::invalid_argument("Invalid preset name: " + presetName);
    }
    auto &preset = iter->second;

    Json::Value root;
    root["color_auto_exposure"]          = preset.colorAutoExposure;
    root["color_exposure_time"]          = preset.colorExposureTime;
    root["color_auto_white_balance"]     = preset.colorAutoWhiteBalance;
    root["color_white_balance"]          = preset.colorWhiteBalance;
    root["color_gain"]                   = preset.colorGain;
    root["color_contrast"]               = preset.colorContrast;
    root["color_saturation"]             = preset.colorSaturation;
    root["color_sharpness"]              = preset.colorSharpness;
    root["color_brightness"]             = preset.colorBrightness;
    root["color_power_line_frequency"]   = preset.colorPowerLineFrequency;

    return root;
}

const std::vector<uint8_t> &MegaPresetManager::exportSettingsAsPresetJsonData(const std::string &presetName) {
    auto                      root = exportSettingsAsPresetJsonValue(presetName);
    Json::StreamWriterBuilder builder;
    builder.settings_["enableYAMLCompatibility"] = true;
    builder.settings_["dropNullPlaceholders"]    = true;
    std::ostringstream oss;
    builder.newStreamWriter()->write(root, &oss);
    tmpJsonData_.clear();
    auto str = oss.str();
    std::copy(str.begin(), str.end(), std::back_inserter(tmpJsonData_));
    return tmpJsonData_;
}

void MegaPresetManager::exportSettingsAsPresetJsonFile(const std::string &filePath) {
    auto root = exportSettingsAsPresetJsonValue(filePath);

    std::ofstream             ofs(filePath);
    Json::StreamWriterBuilder builder;
    // builder.settings_["indentation"]             = "    ";
    builder.settings_["enableYAMLCompatibility"] = true;
    builder.settings_["dropNullPlaceholders"]    = true;
    auto writer                                  = builder.newStreamWriter();
    writer->write(root, &ofs);
}

void MegaPresetManager::fetchPreset() {
    // clear data
    // availablePresets_.clear();
    // currentPreset_.clear();
    // tmpJsonData_.clear();
    // customPresets_.clear();

    // storeCurrentParamsAsCustomPreset("Custom");
}

template <typename T> void setPropertyValue(IDevice *dev, uint32_t propertyId, T value) {
    // get and release property server on this scope to avoid handle device resource lock for an extended duration
    auto propServer = dev->getPropertyServer();
    return propServer->setPropertyValueT<T>(propertyId, value);
}

void MegaPresetManager::loadCustomPreset(const std::string &presetName, const MegaPreset &preset) {

    auto owner = getOwner();

    setPropertyValue(owner, OB_PROP_COLOR_AUTO_EXPOSURE_BOOL, (bool)preset.colorAutoExposure);
    if(!preset.colorAutoExposure) {
        setPropertyValue(owner, OB_PROP_COLOR_EXPOSURE_INT, preset.colorExposureTime);
        setPropertyValue(owner, OB_PROP_COLOR_GAIN_INT, preset.colorGain);
    }
    setPropertyValue(owner, OB_PROP_COLOR_AUTO_WHITE_BALANCE_BOOL, (bool)preset.colorAutoWhiteBalance);
    if(!preset.colorAutoWhiteBalance) {
        setPropertyValue(owner, OB_PROP_COLOR_WHITE_BALANCE_INT, preset.colorWhiteBalance);
    }
    setPropertyValue(owner, OB_PROP_COLOR_CONTRAST_INT, preset.colorContrast);
    setPropertyValue(owner, OB_PROP_COLOR_SATURATION_INT, preset.colorSaturation);
    setPropertyValue(owner, OB_PROP_COLOR_SHARPNESS_INT, preset.colorSharpness);
    setPropertyValue(owner, OB_PROP_COLOR_BRIGHTNESS_INT, preset.colorBrightness);
    setPropertyValue(owner, OB_PROP_COLOR_POWER_LINE_FREQUENCY_INT, (int)preset.colorPowerLineFrequency);

    currentPreset_ = presetName;
}

template <typename T> T getPropertyValue(IDevice *dev, uint32_t propertyId) {
    // get and release property server on this scope to avoid handle device resource lock for an extended duration
    auto propServer = dev->getPropertyServer();
    return propServer->getPropertyValueT<T>(propertyId);
}

void MegaPresetManager::storeCurrentParamsAsCustomPreset(const std::string &presetName) {
    MegaPreset preset{};
    auto       owner = getOwner();

    preset.colorAutoExposure          = getPropertyValue<bool>(owner, OB_PROP_COLOR_AUTO_EXPOSURE_BOOL);
    preset.colorExposureTime          = getPropertyValue<int>(owner, OB_PROP_COLOR_EXPOSURE_INT);
    preset.colorAutoWhiteBalance      = getPropertyValue<bool>(owner, OB_PROP_COLOR_AUTO_WHITE_BALANCE_BOOL);
    preset.colorWhiteBalance          = getPropertyValue<int>(owner, OB_PROP_COLOR_WHITE_BALANCE_INT);
    preset.colorGain                  = getPropertyValue<int>(owner, OB_PROP_COLOR_GAIN_INT);
    preset.colorContrast              = getPropertyValue<int>(owner, OB_PROP_COLOR_CONTRAST_INT);
    preset.colorSaturation            = getPropertyValue<int>(owner, OB_PROP_COLOR_SATURATION_INT);
    preset.colorSharpness             = getPropertyValue<int>(owner, OB_PROP_COLOR_SHARPNESS_INT);
    preset.colorBrightness            = getPropertyValue<int>(owner, OB_PROP_COLOR_BRIGHTNESS_INT);
    preset.colorPowerLineFrequency    = getPropertyValue<int>(owner, OB_PROP_COLOR_POWER_LINE_FREQUENCY_INT);


    if(customPresets_.find(presetName) == customPresets_.end()) {
        availablePresets_.emplace_back(presetName);
    }
    customPresets_[presetName] = preset;
}

}  // namespace libobsensor
