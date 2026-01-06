// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include <memory>
#include "PlaybackFrameInterleaveManager.hpp"
#include "exception/ObException.hpp"

namespace libobsensor {

PlaybackFrameInterleaveManager::PlaybackFrameInterleaveManager(std::shared_ptr<IFrameInterleaveManager> frameInterleaveManager)
    : DeviceComponentBase(nullptr), deviceFrameInterleaveManager_(frameInterleaveManager) {}

PlaybackFrameInterleaveManager::~PlaybackFrameInterleaveManager() {}

void PlaybackFrameInterleaveManager::loadFrameInterleave(const std::string &frameInterleaveName) {
    (void)frameInterleaveName;
    THROW_UNSUPPORTED_OPERATION_EXCEPTION("Playback device doesn't support loadFrameInterleave");
}

const std::vector<std::string> &PlaybackFrameInterleaveManager::getAvailableFrameInterleaveList() const {
    return deviceFrameInterleaveManager_->getAvailableFrameInterleaveList();
}

const std::string &PlaybackFrameInterleaveManager::getCurrentFrameInterleaveName() const {
    return deviceFrameInterleaveManager_->getCurrentFrameInterleaveName();
}

int32_t PlaybackFrameInterleaveManager::getParamCount() const {
    return deviceFrameInterleaveManager_->getParamCount();
}

const FrameInterleaveParam &PlaybackFrameInterleaveManager::getParam(const std::string &frameInterleaveName, int32_t index) const {
    return deviceFrameInterleaveManager_->getParam(frameInterleaveName, index);
}

void PlaybackFrameInterleaveManager::updateParam(const std::string &frameInterleaveName, const FrameInterleaveParam &param, int32_t index) {
    utils::unusedVar(frameInterleaveName);
    utils::unusedVar(param);
    utils::unusedVar(index);
    THROW_UNSUPPORTED_OPERATION_EXCEPTION("Playback device doesn't support updateParam");
}

}  // namespace libobsensor
