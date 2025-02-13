// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "G330PresetManager.hpp"
#include "property/InternalProperty.hpp"
#include "InternalTypes.hpp"
#include "exception/ObException.hpp"
#include "utils/Utils.hpp"
#include "G330DepthWorkModeManager.hpp"

#include <json/json.h>

namespace libobsensor {

G330PresetManager::G330PresetManager(IDevice *owner) : DeviceComponentBase(owner) {
    auto depthWorkModeManager = owner->getComponentT<G330DepthWorkModeManager>(OB_DEV_COMPONENT_DEPTH_WORK_MODE_MANAGER);
    auto depthWorkModeList    = depthWorkModeManager->getDepthWorkModeList();

    for(auto &mode: depthWorkModeList) {
        availablePresets_.emplace_back(mode.name);
    }

    if(availablePresets_.size() > 1) {
        currentPreset_ = availablePresets_[0];
        depthWorkModeManager->switchDepthWorkMode(currentPreset_.c_str());
    }

    auto propServer = owner->getPropertyServer();

    propServer->registerAccessCallback(
        {
            OB_PROP_LASER_CONTROL_INT,
            OB_PROP_LASER_POWER_LEVEL_CONTROL_INT,
            OB_PROP_DEPTH_AUTO_EXPOSURE_BOOL,
            OB_PROP_IR_AUTO_EXPOSURE_BOOL,
            OB_PROP_DEPTH_EXPOSURE_INT,
            OB_PROP_IR_EXPOSURE_INT,
            OB_PROP_DEPTH_GAIN_INT,
            OB_PROP_IR_GAIN_INT,
            OB_PROP_IR_BRIGHTNESS_INT,
            OB_PROP_COLOR_AUTO_EXPOSURE_BOOL,
            OB_PROP_COLOR_EXPOSURE_INT,
            OB_PROP_COLOR_AUTO_WHITE_BALANCE_BOOL,
            OB_PROP_COLOR_WHITE_BALANCE_INT,
            OB_PROP_COLOR_GAIN_INT,
            OB_PROP_COLOR_CONTRAST_INT,
            OB_PROP_COLOR_SATURATION_INT,
            OB_PROP_COLOR_SHARPNESS_INT,
            OB_PROP_COLOR_BRIGHTNESS_INT,
            OB_PROP_COLOR_HUE_INT,
            OB_PROP_COLOR_GAMMA_INT,
            OB_PROP_COLOR_BACKLIGHT_COMPENSATION_INT,
            OB_PROP_COLOR_POWER_LINE_FREQUENCY_INT,
        },
        [&](uint32_t, const uint8_t *, size_t, PropertyOperationType operationType) {
            if(operationType == PROP_OP_WRITE) {
                currentPreset_ = "Custom";
            }
        });
    storeCurrentParamsAsCustomPreset("Custom");
}

void G330PresetManager::loadPreset(const std::string &presetName) {
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
        auto owner                = getOwner();
        auto depthWorkModeManager = owner->getComponentT<G330DepthWorkModeManager>(OB_DEV_COMPONENT_DEPTH_WORK_MODE_MANAGER);

        depthWorkModeManager->switchDepthWorkMode(presetName.c_str());
        currentPreset_ = presetName;
    }
}

const std::string &G330PresetManager::getCurrentPresetName() const {
    return currentPreset_;
}

const std::vector<std::string> &G330PresetManager::getAvailablePresetList() const {
    return availablePresets_;
}

void G330PresetManager::loadPresetFromJsonData(const std::string &presetName, const std::vector<uint8_t> &jsonData) {
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

void G330PresetManager::loadPresetFromJsonFile(const std::string &filePath) {
    Json::Value   root;
    std::ifstream ifs(filePath);
    ifs >> root;
    // store current parameters to  "Custom"
    if(currentPreset_ == "Custom") {
        storeCurrentParamsAsCustomPreset("Custom");
    }
    loadPresetFromJsonValue(filePath, root);
}

void G330PresetManager::loadPresetFromJsonValue(const std::string &presetName, const Json::Value &root) {
    G330Preset preset;
    preset.depthWorkMode              = root["depth_alg_mode"].asString();
    preset.laserState                 = root["laser_state"].asInt();
    preset.laserPowerLevel            = root["laser_power_level"].asInt();
    preset.depthAutoExposure          = root["depth_auto_exposure"].asBool();
    preset.depthExposureTime          = root["depth_exposure_time"].asInt();
    preset.depthGain                  = root["depth_gain"].asInt();
    preset.depthBrightness            = root["target_mean_intensity"].asInt();
    preset.colorAutoExposure          = root["color_auto_exposure"].asBool();
    preset.colorExposureTime          = root["color_exposure_time"].asInt();
    preset.colorAutoWhiteBalance      = root["color_auto_white_balance"].asBool();
    preset.colorWhiteBalance          = root["color_white_balance"].asInt();
    preset.colorGain                  = root["color_gain"].asInt();
    preset.colorContrast              = root["color_contrast"].asInt();
    preset.colorSaturation            = root["color_saturation"].asInt();
    preset.colorSharpness             = root["color_sharpness"].asInt();
    preset.colorBrightness            = root["color_brightness"].asInt();
    preset.colorHue                   = root["color_hue"].asInt();
    preset.colorGamma                 = root["color_gamma"].asInt();
    preset.colorBacklightCompensation = root["color_backlight_compensation"].asBool();
    preset.colorPowerLineFrequency    = root["color_power_line_frequency"].asInt();

    loadCustomPreset(presetName, preset);

    if(customPresets_.find(presetName) == customPresets_.end()) {
        availablePresets_.emplace_back(presetName);
    }
    customPresets_[presetName] = preset;
}

Json::Value G330PresetManager::exportSettingsAsPresetJsonValue(const std::string &presetName) {
    storeCurrentParamsAsCustomPreset(presetName);
    auto iter = customPresets_.find(presetName);
    if(iter == customPresets_.end()) {
        throw std::invalid_argument("Invalid preset name: " + presetName);
    }
    auto &preset = iter->second;

    Json::Value root;
    root["depth_alg_mode"]               = preset.depthWorkMode;
    root["laser_state"]                  = preset.laserState;
    root["laser_power_level"]            = preset.laserPowerLevel;
    root["depth_auto_exposure"]          = preset.depthAutoExposure;
    root["depth_exposure_time"]          = preset.depthExposureTime;
    root["depth_gain"]                   = preset.depthGain;
    root["target_mean_intensity"]        = preset.depthBrightness;
    root["color_auto_exposure"]          = preset.colorAutoExposure;
    root["color_exposure_time"]          = preset.colorExposureTime;
    root["color_auto_white_balance"]     = preset.colorAutoWhiteBalance;
    root["color_white_balance"]          = preset.colorWhiteBalance;
    root["color_gain"]                   = preset.colorGain;
    root["color_contrast"]               = preset.colorContrast;
    root["color_saturation"]             = preset.colorSaturation;
    root["color_sharpness"]              = preset.colorSharpness;
    root["color_brightness"]             = preset.colorBrightness;
    root["color_hue"]                    = preset.colorHue;
    root["color_gamma"]                  = preset.colorGamma;
    root["color_backlight_compensation"] = preset.colorBacklightCompensation;
    root["color_power_line_frequency"]   = preset.colorPowerLineFrequency;

    return root;
}

const std::vector<uint8_t> &G330PresetManager::exportSettingsAsPresetJsonData(const std::string &presetName) {
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

void G330PresetManager::exportSettingsAsPresetJsonFile(const std::string &filePath) {
    auto root = exportSettingsAsPresetJsonValue(filePath);

    std::ofstream             ofs(filePath);
    Json::StreamWriterBuilder builder;
    // builder.settings_["indentation"]             = "    ";
    builder.settings_["enableYAMLCompatibility"] = true;
    builder.settings_["dropNullPlaceholders"]    = true;
    auto writer                                  = builder.newStreamWriter();
    writer->write(root, &ofs);
}

void G330PresetManager::fetchPreset() {
    auto owner                = getOwner();
    auto depthWorkModeManager = owner->getComponentT<G330DepthWorkModeManager>(OB_DEV_COMPONENT_DEPTH_WORK_MODE_MANAGER);

    // refetch list
    depthWorkModeManager->fetchDepthWorkModeList();

    // clear data
    availablePresets_.clear();
    currentPreset_.clear();
    tmpJsonData_.clear();
    customPresets_.clear();

    auto depthWorkModeList = depthWorkModeManager->getDepthWorkModeList();
    for(auto &mode: depthWorkModeList) {
        availablePresets_.emplace_back(mode.name);
    }

    if(availablePresets_.size() > 1) {
        currentPreset_ = availablePresets_[0];
        depthWorkModeManager->switchDepthWorkMode(currentPreset_.c_str());
    }
    storeCurrentParamsAsCustomPreset("Custom");
}

template <typename T> void setPropertyValue(IDevice *dev, uint32_t propertyId, T value) {
    // get and release property server on this scope to avoid handle device resource lock for an extended duration
    auto propServer = dev->getPropertyServer();
    return propServer->setPropertyValueT<T>(propertyId, value);
}

void G330PresetManager::loadCustomPreset(const std::string &presetName, const G330Preset &preset) {

    auto owner = getOwner();

    {
        auto depthWorkModeManager = owner->getComponentT<G330DepthWorkModeManager>(OB_DEV_COMPONENT_DEPTH_WORK_MODE_MANAGER);
        depthWorkModeManager->switchDepthWorkMode(preset.depthWorkMode.c_str());
    }

    setPropertyValue(owner, OB_PROP_LASER_CONTROL_INT, preset.laserState);
    setPropertyValue(owner, OB_PROP_LASER_POWER_LEVEL_CONTROL_INT, preset.laserPowerLevel);
    setPropertyValue(owner, OB_PROP_IR_EXPOSURE_INT, preset.depthExposureTime);
    setPropertyValue(owner, OB_PROP_IR_GAIN_INT, preset.depthGain);
    setPropertyValue(owner, OB_PROP_DEPTH_AUTO_EXPOSURE_BOOL, (bool)preset.depthAutoExposure);
    setPropertyValue(owner, OB_PROP_IR_BRIGHTNESS_INT, preset.depthBrightness);
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
    setPropertyValue(owner, OB_PROP_COLOR_HUE_INT, preset.colorHue);
    setPropertyValue(owner, OB_PROP_COLOR_GAMMA_INT, preset.colorGamma);
    setPropertyValue(owner, OB_PROP_COLOR_BACKLIGHT_COMPENSATION_INT, (bool)preset.colorBacklightCompensation);
    setPropertyValue(owner, OB_PROP_COLOR_POWER_LINE_FREQUENCY_INT, (int)preset.colorPowerLineFrequency);

    currentPreset_ = presetName;
}

template <typename T> T getPropertyValue(IDevice *dev, uint32_t propertyId) {
    // get and release property server on this scope to avoid handle device resource lock for an extended duration
    auto propServer = dev->getPropertyServer();
    return propServer->getPropertyValueT<T>(propertyId);
}

void G330PresetManager::storeCurrentParamsAsCustomPreset(const std::string &presetName) {
    G330Preset preset;
    auto       owner = getOwner();

    preset.laserState                 = getPropertyValue<int>(owner, OB_PROP_LASER_CONTROL_INT);
    preset.laserPowerLevel            = getPropertyValue<int>(owner, OB_PROP_LASER_POWER_LEVEL_CONTROL_INT);
    preset.depthAutoExposure          = getPropertyValue<bool>(owner, OB_PROP_DEPTH_AUTO_EXPOSURE_BOOL);
    preset.depthExposureTime          = getPropertyValue<int>(owner, OB_PROP_IR_EXPOSURE_INT);
    preset.depthGain                  = getPropertyValue<int>(owner, OB_PROP_IR_GAIN_INT);
    preset.depthBrightness            = getPropertyValue<int>(owner, OB_PROP_IR_BRIGHTNESS_INT);
    preset.colorAutoExposure          = getPropertyValue<bool>(owner, OB_PROP_COLOR_AUTO_EXPOSURE_BOOL);
    preset.colorExposureTime          = getPropertyValue<int>(owner, OB_PROP_COLOR_EXPOSURE_INT);
    preset.colorAutoWhiteBalance      = getPropertyValue<bool>(owner, OB_PROP_COLOR_AUTO_WHITE_BALANCE_BOOL);
    preset.colorWhiteBalance          = getPropertyValue<int>(owner, OB_PROP_COLOR_WHITE_BALANCE_INT);
    preset.colorGain                  = getPropertyValue<int>(owner, OB_PROP_COLOR_GAIN_INT);
    preset.colorContrast              = getPropertyValue<int>(owner, OB_PROP_COLOR_CONTRAST_INT);
    preset.colorSaturation            = getPropertyValue<int>(owner, OB_PROP_COLOR_SATURATION_INT);
    preset.colorSharpness             = getPropertyValue<int>(owner, OB_PROP_COLOR_SHARPNESS_INT);
    preset.colorBrightness            = getPropertyValue<int>(owner, OB_PROP_COLOR_BRIGHTNESS_INT);
    preset.colorHue                   = getPropertyValue<int>(owner, OB_PROP_COLOR_HUE_INT);
    preset.colorGamma                 = getPropertyValue<int>(owner, OB_PROP_COLOR_GAMMA_INT);
    preset.colorBacklightCompensation = getPropertyValue<bool>(owner, OB_PROP_COLOR_BACKLIGHT_COMPENSATION_INT);
    preset.colorPowerLineFrequency    = getPropertyValue<int>(owner, OB_PROP_COLOR_POWER_LINE_FREQUENCY_INT);

    {
        auto depthWorkModeManager = owner->getComponentT<G330DepthWorkModeManager>(OB_DEV_COMPONENT_DEPTH_WORK_MODE_MANAGER);
        preset.depthWorkMode      = depthWorkModeManager->getCurrentDepthWorkMode().name;
    }

    if(customPresets_.find(presetName) == customPresets_.end()) {
        availablePresets_.emplace_back(presetName);
    }
    customPresets_[presetName] = preset;
}

}  // namespace libobsensor
