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

}  // namespace libobsensor
