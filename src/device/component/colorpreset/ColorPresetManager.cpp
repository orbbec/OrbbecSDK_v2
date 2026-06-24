// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "ColorPresetManager.hpp"
#include "libobsensor/h/Property.h"
#include "logger/Logger.hpp"

namespace libobsensor {

ColorPresetManager::ColorPresetManager(IDevice *owner, std::map<int32_t, std::string> valueToName)
    : DeviceComponentBase(owner), valueToPresetName_(std::move(valueToName)) {
    for(const auto &entry: valueToPresetName_) {
        presetNameToValue_[entry.second] = entry.first;
        presetList_.push_back(entry.second);
    }

    auto ownerPtr   = getOwner();
    auto propServer = ownerPtr->getPropertyServer();
    auto value      = propServer->getPropertyValueT<int32_t>(OB_PROP_COLOR_PRESET_PRIORITY_INT);
    auto it         = valueToPresetName_.find(value);
    if(it == valueToPresetName_.end()) {
        currentPresetName_                     = "Unknown (" + std::to_string(value) + ")";
        valueToPresetName_[value]              = currentPresetName_;
        presetNameToValue_[currentPresetName_] = value;
        presetList_.push_back(currentPresetName_);
        LOG_WARN("Unknown color preset value {}, using '{}'", value, currentPresetName_);
        return;
    }
    currentPresetName_ = it->second;
}

const std::string &ColorPresetManager::getCurrentName() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return currentPresetName_;
}

void ColorPresetManager::switchPreset(const std::string &name) {
    auto it = presetNameToValue_.find(name);
    if(it == presetNameToValue_.end()) {
        THROW_INVALID_DATA_EXCEPTION("Invalid color preset name: " + name);
    }

    std::lock_guard<std::mutex> lock(mutex_);
    auto                        owner = getOwner();
    if(!owner->isPlaybackDevice()) {
        auto propServer = owner->getPropertyServer();
        propServer->setPropertyValueT<int32_t>(OB_PROP_COLOR_PRESET_PRIORITY_INT, it->second);
        currentPresetName_ = it->first;
    }
    else {
        LOG_WARN("Playback Device: unsupported switchPreset() called with name: {}", name);
    }
}

const std::vector<std::string> &ColorPresetManager::getPresetList() const {
    return presetList_;
}

}  // namespace libobsensor
