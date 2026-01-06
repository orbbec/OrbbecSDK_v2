// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "PlaybackPresetManager.hpp"
#include "property/InternalProperty.hpp"
#include "InternalTypes.hpp"
#include "exception/ObException.hpp"
#include "utils/Utils.hpp"
#include "PlaybackDepthWorkModeManager.hpp"
#include <json/json.h>

namespace libobsensor {

PlaybackPresetManager::PlaybackPresetManager(IDevice *owner, std::shared_ptr<IPresetManager> delegate) : DeviceComponentBase(owner), delegate_(delegate) {}

void PlaybackPresetManager::loadPreset(const std::string &presetName) {
    utils::unusedVar(presetName);
    LOG_DEBUG("Playback Device: unsupported loadPreset() called with name: {}", presetName);
}

const std::string &PlaybackPresetManager::getCurrentPresetName() const {
    return delegate_->getCurrentPresetName();
}

const std::vector<std::string> &PlaybackPresetManager::getAvailablePresetList() const {
    return delegate_->getAvailablePresetList();
}

void PlaybackPresetManager::loadPresetFromJsonData(const std::string &presetName, const std::vector<uint8_t> &jsonData) {
    utils::unusedVar(presetName);
    utils::unusedVar(jsonData);
    LOG_DEBUG("Playback Device: unsupported loadPresetFromJsonData() called with name: {}", presetName);
}

void PlaybackPresetManager::loadPresetFromJsonFile(const std::string &filePath) {
    utils::unusedVar(filePath);
    LOG_DEBUG("Playback Device: unsupported loadPresetFromJsonFile() called with file: {}", filePath);
}

const std::vector<uint8_t> &PlaybackPresetManager::exportSettingsAsPresetJsonData(const std::string &presetName) {
    return delegate_->exportSettingsAsPresetJsonData(presetName);
}

void PlaybackPresetManager::exportSettingsAsPresetJsonFile(const std::string &filePath) {
    return delegate_->exportSettingsAsPresetJsonFile(filePath);
}

void PlaybackPresetManager::fetchPreset() {
    LOG_DEBUG("Playback Device: unsupported fetchPreset()");
}

}  // namespace libobsensor
