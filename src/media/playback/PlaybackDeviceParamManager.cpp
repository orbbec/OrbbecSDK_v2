// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "PlaybackDeviceParamManager.hpp"
#include "stream/StreamIntrinsicsManager.hpp"

namespace libobsensor {

PlaybackDeviceParamManager::PlaybackDeviceParamManager(IDevice *owner, std::shared_ptr<PlaybackDevicePort> port) : DeviceComponentBase(owner), port_(port) {
    auto dispSpList = port_->getStreamProfileList(OB_SENSOR_DEPTH);
    if(dispSpList.empty()) {
        LOG_WARN("No disparity stream found on playback device");
        return;
    }

    auto dispSp = dispSpList.front()->as<DisparityBasedStreamProfile>();
    try {
        disparityParam_ = dispSp->getDisparityParam();
    }
    catch(const std::exception &e) {
        LOG_WARN("Playback Device get disparity param failed: {}", e.what());
    }
}

const OBDisparityParam &PlaybackDeviceParamManager::getDisparityParam() const {
    return disparityParam_;
}

void PlaybackDeviceParamManager::bindDisparityParam(std::vector<std::shared_ptr<const StreamProfile>> streamProfileList) {
    auto dispParam    = getDisparityParam();
    auto intrinsicMgr = StreamIntrinsicsManager::getInstance();
    for(const auto &sp: streamProfileList) {
        if(!sp->is<DisparityBasedStreamProfile>()) {
            continue;
        }
        intrinsicMgr->registerDisparityBasedStreamDisparityParam(sp, dispParam);
    }
}

}  // namespace libobsensor