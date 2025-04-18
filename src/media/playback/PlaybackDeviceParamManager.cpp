// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "PlaybackDeviceParamManager.hpp"
#include "stream/StreamIntrinsicsManager.hpp"
#include "property/InternalProperty.hpp"

namespace libobsensor {

PlaybackDeviceParamManager::PlaybackDeviceParamManager(IDevice *owner, std::shared_ptr<PlaybackDevicePort> port) : DeviceComponentBase(owner), port_(port) {
    initDeviceParams();
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
void PlaybackDeviceParamManager::bindStreamProfileParams(std::vector<std::shared_ptr<const StreamProfile>> streamProfileList) {
    utils::unusedVar((streamProfileList));
}

const std::vector<OBD2CProfile> &PlaybackDeviceParamManager::getD2CProfileList() const {
    return d2cProfileList_;
}

const std::vector<OBCameraParam> &PlaybackDeviceParamManager::getCalibrationCameraParamList() const {
    return cameraParamList_;
}

const OBIMUCalibrateParams &PlaybackDeviceParamManager::getIMUCalibrationParam() const {
    return imuCalibrationParam_;
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

void PlaybackDeviceParamManager::initDeviceParams() {
    std::vector<uint8_t> rawData   = port_->getRecordedStructData(OB_RAW_DATA_D2C_ALIGN_SUPPORT_PROFILE_LIST);
    auto itemCount = rawData.size() / sizeof(OBD2CProfile);
    for(uint32_t i = 0; i < itemCount; i++) {
        auto item = reinterpret_cast<OBD2CProfile *>(rawData.data() + i * sizeof(OBD2CProfile));
        d2cProfileList_.push_back(*item);
    }

    rawData.clear();
    rawData = port_->getRecordedStructData(OB_RAW_DATA_ALIGN_CALIB_PARAM);
    itemCount = rawData.size() / sizeof(OBCameraParam);
    for(uint32_t i = 0; i < itemCount; i++) {
        auto item = reinterpret_cast<OBCameraParam *>(rawData.data() + i * sizeof(OBCameraParam));
        cameraParamList_.push_back(*item);
    }

    rawData.clear();
    rawData = port_->getRecordedStructData(OB_RAW_DATA_IMU_CALIB_PARAM);
    if(!rawData.empty()) {
        imuCalibrationParam_ = *(reinterpret_cast<OBIMUCalibrateParams *>(rawData.data()));
    }
}


}  // namespace libobsensor