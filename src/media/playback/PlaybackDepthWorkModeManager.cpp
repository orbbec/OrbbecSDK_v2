// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "PlaybackDepthWorkModeManager.hpp"
#include <memory>

namespace libobsensor {
PlaybackDepthWorkModeManager::PlaybackDepthWorkModeManager(IDevice *owner, std::shared_ptr<PlaybackDevicePort> port)
    : DeviceComponentBase(owner), port_(port), currentDepthWorkMode_({ { 0 }, { 0 }, 0 }) {
    fetchCurrentDepthWorkMode();
}

std::vector<OBDepthWorkMode_Internal> PlaybackDepthWorkModeManager::getDepthWorkModeList() const {
    std::vector<OBDepthWorkMode_Internal> workModes;
    workModes.emplace_back(getCurrentDepthWorkMode());

    return workModes;
}

const OBDepthWorkMode_Internal &PlaybackDepthWorkModeManager::getCurrentDepthWorkMode() const {
    return currentDepthWorkMode_;
}

void PlaybackDepthWorkModeManager::fetchCurrentDepthWorkMode() {
    auto data = port_->getRecordedStructData(OB_STRUCT_CURRENT_DEPTH_ALG_MODE);

    if(data.size() != sizeof(OBDepthWorkMode_Internal)) {
        LOG_WARN("Playback Device: Invalid depth work mode data size, expected: {}, actual: {}", sizeof(OBDepthWorkMode_Internal), data.size());
        return;
    }

    memcpy(&currentDepthWorkMode_, data.data(), sizeof(OBDepthWorkMode_Internal));
}

void PlaybackDepthWorkModeManager::switchDepthWorkMode(const std::string &name) {
    LOG_DEBUG("Playback Device: unsupported switchDepthWorkMode() called with name: {}", name);
    return;
}
}  // namespace libobsensor