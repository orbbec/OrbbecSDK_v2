// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "PlaybackDepthWorkModeManager.hpp"
#include "utils/Utils.hpp"
#include "property/InternalProperty.hpp"
#include <memory>

namespace libobsensor {
PlaybackDepthWorkModeManager::PlaybackDepthWorkModeManager(IDevice *owner, std::shared_ptr<PlaybackDevicePort> port) : DeviceComponentBase(owner), port_(port) {
    currentItem_ = {};
    fetchDepthWorkModeList();
}

std::vector<DepthWorkModeItem> PlaybackDepthWorkModeManager::getDepthWorkModeList() const {
    return { currentItem_ };
}

const DepthWorkModeItem &PlaybackDepthWorkModeManager::getCurrentDepthWorkMode() const {
    return currentItem_;
}

void PlaybackDepthWorkModeManager::fetchDepthWorkModeList() {
    auto v2Data = port_->getRecordedStructData(OB_STRUCT_CURRENT_DEPTH_ALG_MODE_V2);
    if(v2Data.size() == sizeof(OBDepthWorkModeV2_Internal)) {
        memcpy(&currentItem_.mode, v2Data.data(), sizeof(OBDepthWorkModeV2_Internal));
        uint32_t exposedVersion = (currentItem_.mode.optionCode & CUSTOM_DEPTH_MODE_TAG) ? currentItem_.mode.version : 0;
        currentItem_.version    = utils::string::versionToString(exposedVersion);
        return;
    }

    auto data = port_->getRecordedStructData(OB_STRUCT_CURRENT_DEPTH_ALG_MODE);

    if(data.size() != sizeof(OBDepthWorkMode_Internal)) {
        LOG_WARN("Playback Device: Invalid depth work mode data size, expected: {}, actual: {}", sizeof(OBDepthWorkMode_Internal), data.size());
        return;
    }

    OBDepthWorkMode_Internal mode{};
    memcpy(&mode, data.data(), sizeof(OBDepthWorkMode_Internal));
    memcpy(&currentItem_.mode, &mode, sizeof(OBDepthWorkMode_Internal));
    currentItem_.version.clear();
}

void PlaybackDepthWorkModeManager::switchDepthWorkMode(const std::string &name, const std::string &version) {
    utils::unusedVar(version);
    LOG_DEBUG("Playback Device: unsupported switchDepthWorkMode() called with name: {}", name);
    return;
}
}  // namespace libobsensor
