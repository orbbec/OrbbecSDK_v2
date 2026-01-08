// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "G330PresetManager.hpp"
#include "property/InternalProperty.hpp"
#include "InternalTypes.hpp"
#include "exception/ObException.hpp"
#include "utils/Utils.hpp"
#include "IDepthWorkModeManager.hpp"
#include "preset/PresetDefinitions.hpp"

#include <json/json.h>

namespace libobsensor {

G330PresetManager::G330PresetManager(IDevice *owner) : DeviceComponentBase(owner) {
    auto depthWorkModeManager = owner->getComponentT<IDepthWorkModeManager>(OB_DEV_COMPONENT_DEPTH_WORK_MODE_MANAGER);
    auto depthWorkModeList    = depthWorkModeManager->getDepthWorkModeList();

    for(auto &mode: depthWorkModeList) {
        availablePresets_.emplace_back(mode.name);
    }

    if(availablePresets_.size() > 0) {
        currentPresetName_ = availablePresets_[0];
        depthWorkModeManager->switchDepthWorkMode(currentPresetName_.c_str());
    }

    if(!owner->isPlaybackDevice()) {
        auto                  propServer = owner->getPropertyServer();
        std::vector<uint32_t> propertyIds{ OB_PROP_LASER_CONTROL_INT,
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
                                           OB_PROP_FRAME_INTERLEAVE_ENABLE_BOOL,
                                           OB_PROP_FRAME_INTERLEAVE_CONFIG_INDEX_INT,
                                           OB_PROP_IR_AE_MAX_EXPOSURE_INT,
                                           OB_STRUCT_DEPTH_AE_ROI,
                                           OB_STRUCT_COLOR_AE_ROI,
                                           OB_PROP_DISPARITY_TO_DEPTH_BOOL,
                                           OB_PROP_SDK_DISPARITY_TO_DEPTH_BOOL,
                                           OB_PROP_DEPTH_NOISE_REMOVAL_FILTER_BOOL,
                                           OB_PROP_DEPTH_NOISE_REMOVAL_FILTER_MAX_DIFF_INT,
                                           OB_PROP_DEPTH_NOISE_REMOVAL_FILTER_MAX_SPECKLE_SIZE_INT,
                                           OB_PROP_HW_NOISE_REMOVE_FILTER_ENABLE_BOOL,
                                           OB_PROP_HW_NOISE_REMOVE_FILTER_THRESHOLD_FLOAT,
                                           OB_PROP_DEPTH_AUTO_EXPOSURE_PRIORITY_INT,
                                           OB_PROP_DEPTH_UNIT_FLEXIBLE_ADJUSTMENT_FLOAT,
                                           OB_PROP_LDP_BOOL,
                                           OB_PROP_DEPTH_FLIP_BOOL,
                                           OB_PROP_DEPTH_MIRROR_BOOL,
                                           OB_PROP_DEPTH_ROTATE_INT,
                                           OB_PROP_COLOR_AUTO_EXPOSURE_PRIORITY_INT,
                                           OB_PROP_COLOR_DENOISING_LEVEL_INT,
                                           OB_PROP_COLOR_AE_MAX_EXPOSURE_INT,
                                           OB_PROP_COLOR_FLIP_BOOL,
                                           OB_PROP_COLOR_MIRROR_BOOL,
                                           OB_PROP_COLOR_ROTATE_INT,
                                           OB_PROP_IR_FLIP_BOOL,
                                           OB_PROP_IR_MIRROR_BOOL,
                                           OB_PROP_IR_ROTATE_INT,
                                           OB_PROP_IR_RIGHT_FLIP_BOOL,
                                           OB_PROP_IR_RIGHT_MIRROR_BOOL,
                                           OB_PROP_IR_RIGHT_ROTATE_INT };
        propServer->registerAccessCallback(propertyIds, [&](uint32_t, const uint8_t *, size_t, PropertyOperationType operationType) {
            if(operationType == PROP_OP_WRITE) {
                currentPresetName_ = kCustomPresetName;
            }
        });
        storeCurrentParamsAsCustomPreset(kCustomPresetName);
    }
}

void G330PresetManager::loadPreset(const std::string &presetName) {
    if(std::find(availablePresets_.begin(), availablePresets_.end(), presetName) == availablePresets_.end()) {
        THROW_INVALID_PARAM_EXCEPTION("Invalid preset name: " + presetName);
    }

    // store current parameters to kCustomPresetName
    if(currentPresetName_ == kCustomPresetName) {
        storeCurrentParamsAsCustomPreset(kCustomPresetName);
    }

    auto iter = customPresets_.find(presetName);
    if(iter != customPresets_.end()) {
        // Load custom preset
        loadCustomPreset(iter->first, iter->second);
    }
    else {
        auto owner                = getOwner();
        auto depthWorkModeManager = owner->getComponentT<IDepthWorkModeManager>(OB_DEV_COMPONENT_DEPTH_WORK_MODE_MANAGER);

        depthWorkModeManager->switchDepthWorkMode(presetName.c_str());
        currentPresetName_ = presetName;
    }
}

const std::string &G330PresetManager::getCurrentPresetName() const {
    return currentPresetName_;
}

const std::vector<std::string> &G330PresetManager::getAvailablePresetList() const {
    return availablePresets_;
}

void G330PresetManager::loadPresetFromJsonData(const std::string &presetName, const std::vector<uint8_t> &jsonData) {
    Json::Value  root;
    Json::Reader reader;
    if(!reader.parse(std::string((const char *)jsonData.data(), jsonData.size()), root)) {
        THROW_INVALID_PARAM_EXCEPTION("Invalid JSON data");
    }
    // store current parameters to kCustomPresetName
    if(currentPresetName_ == kCustomPresetName) {
        storeCurrentParamsAsCustomPreset(kCustomPresetName);
    }
    loadPresetFromJsonValue(presetName, root);
}

void G330PresetManager::loadPresetFromJsonFile(const std::string &filePath) {
    Json::Value   root;
    std::ifstream ifs(filePath);
    ifs >> root;
    // store current parameters to kCustomPresetName
    if(currentPresetName_ == kCustomPresetName) {
        storeCurrentParamsAsCustomPreset(kCustomPresetName);
    }
    loadPresetFromJsonValue(filePath, root);
}

std::shared_ptr<IPresetEngine> G330PresetManager::getPresetEngine(const Json::Value &root) {
    if(root.isObject() && root.isMember(kApiVersion)) {
        return getCurrentPresetEngine();
    }
    if(presetEngineV1_ == nullptr) {
        presetEngineV1_ = std::make_shared<G330PresetEngineV1>(getOwner());
        presetEngineV1_->init();
    }
    return presetEngineV1_;
}

std::shared_ptr<IPresetEngine> G330PresetManager::getCurrentPresetEngine() {
    if(presetEngine_ == nullptr) {
        presetEngine_ = std::make_shared<G330PresetEngine>(getOwner());
        presetEngine_->init();
    }
    return presetEngine_;
}

void G330PresetManager::loadPresetFromJsonValue(const std::string &presetName, const Json::Value &root) {
    loadCustomPreset(presetName, root);

    if(!getOwner()->isPlaybackDevice()) {
        if(customPresets_.find(presetName) == customPresets_.end()) {
            availablePresets_.emplace_back(presetName);
        }
    }
    customPresets_[presetName] = root;
}

Json::Value G330PresetManager::exportSettingsAsPresetJsonValue(const std::string &presetName) {
    storeCurrentParamsAsCustomPreset(presetName);
    auto iter = customPresets_.find(presetName);
    if(iter == customPresets_.end()) {
        THROW_INVALID_PARAM_EXCEPTION("Invalid preset name: " + presetName);
    }
    return iter->second;
}

const std::vector<uint8_t> &G330PresetManager::exportSettingsAsPresetJsonData(const std::string &presetName) {
    auto                      root = exportSettingsAsPresetJsonValue(presetName);
    Json::StreamWriterBuilder builder;
    builder.settings_["indentation"]             = "  ";
    builder.settings_["enableYAMLCompatibility"] = true;
    builder.settings_["dropNullPlaceholders"]    = false;  // Keep null for nullvalue
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
    builder.settings_["indentation"]             = "  ";
    builder.settings_["enableYAMLCompatibility"] = true;
    builder.settings_["dropNullPlaceholders"]    = false;  // Keep null for nullvalue
    auto writer                                  = builder.newStreamWriter();
    writer->write(root, &ofs);
}

void G330PresetManager::fetchPreset() {
    auto owner                = getOwner();
    auto depthWorkModeManager = owner->getComponentT<IDepthWorkModeManager>(OB_DEV_COMPONENT_DEPTH_WORK_MODE_MANAGER);

    // refetch list
    depthWorkModeManager->fetchDepthWorkModeList();

    // clear data
    availablePresets_.clear();
    currentPresetName_.clear();
    tmpJsonData_.clear();
    customPresets_.clear();

    auto depthWorkModeList = depthWorkModeManager->getDepthWorkModeList();
    for(auto &mode: depthWorkModeList) {
        availablePresets_.emplace_back(mode.name);
    }

    if(availablePresets_.size() > 0) {
        currentPresetName_ = availablePresets_[0];
        depthWorkModeManager->switchDepthWorkMode(currentPresetName_.c_str());
    }
    storeCurrentParamsAsCustomPreset(kCustomPresetName);
}

void G330PresetManager::loadCustomPreset(const std::string &presetName, const Json::Value &preset) {
    auto presetEngine = getPresetEngine(preset);
    presetEngine->importJson(preset);

    currentPresetName_ = presetName;
}

void G330PresetManager::storeCurrentParamsAsCustomPreset(const std::string &presetName) {
    auto presetEngine = getCurrentPresetEngine();
    auto data         = presetEngine->exportJson();

    if(!getOwner()->isPlaybackDevice()) {
        if(customPresets_.find(presetName) == customPresets_.end()) {
            availablePresets_.emplace_back(presetName);
        }
    }
    customPresets_[presetName] = data;
}

}  // namespace libobsensor
