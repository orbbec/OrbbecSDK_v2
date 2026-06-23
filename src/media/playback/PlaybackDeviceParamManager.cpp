// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "PlaybackDeviceParamManager.hpp"
#include "stream/StreamIntrinsicsManager.hpp"
#include "property/InternalProperty.hpp"
#include <algorithm>

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

const OpenNIFrameProcessParam &PlaybackDeviceParamManager::getOpenNIFrameProcessParam() const {
    return frameProcessParam_;
}

void PlaybackDeviceParamManager::bindDisparityParam(std::vector<std::shared_ptr<const StreamProfile>> streamProfileList) {
    const auto &dispParam    = getDisparityParam();
    auto        intrinsicMgr = StreamIntrinsicsManager::getInstance();
    for(const auto &sp: streamProfileList) {
        if(!sp->is<DisparityBasedStreamProfile>()) {
            continue;
        }
        intrinsicMgr->registerDisparityBasedStreamDisparityParam(sp, dispParam);
    }
}

bool PlaybackDeviceParamManager::getPreProcessParam(uint16_t colorWidth, uint16_t colorHeight, OBD2CPreProcessParam &param) const {
    // d2cColorPreProcessProfileList_ is parallel to the first N hardware-D2C entries of d2cProfileList_
    // by index (software D2C entries appended later have no corresponding pre-process profile).
    // Iterating only up to the smaller of the two sizes is therefore both correct and safe.
    // Returns false if d2cColorPreProcessProfileList_ is empty (old recording or non-DaBaiA device),
    // which causes Pipeline::checkHardwareD2CConfig() to skip setPreProcessParam() — same as before.
    auto count = std::min(d2cProfileList_.size(), d2cColorPreProcessProfileList_.size());
    for(size_t i = 0; i < count; i++) {
        if(d2cProfileList_[i].colorWidth == colorWidth && d2cProfileList_[i].colorHeight == colorHeight) {
            param = d2cColorPreProcessProfileList_[i].preProcessParam;
            return true;
        }
    }
    return false;
}

void PlaybackDeviceParamManager::initDeviceParams() {
    std::vector<uint8_t> rawData   = port_->getRecordedStructData(OB_RAW_DATA_D2C_ALIGN_SUPPORT_PROFILE_LIST);
    auto itemCount = rawData.size() / sizeof(OBD2CProfile);
    for(uint32_t i = 0; i < itemCount; i++) {
        auto item = reinterpret_cast<OBD2CProfile *>(rawData.data() + i * sizeof(OBD2CProfile));
        d2cProfileList_.push_back(*item);
    }

    // Load the hardware D2C color pre-process profile list written by RecordDevice.
    // Parallel to the first N hardware-D2C entries of d2cProfileList_ by index.
    // For recordings made before this fix or for non-DaBaiA devices the property is absent:
    // getRecordedStructData() returns an empty vector, leaving d2cColorPreProcessProfileList_ empty,
    // and getPreProcessParam() then returns false — preserving backward compatibility.
    rawData.clear();
    rawData   = port_->getRecordedStructData(OB_RAW_DATA_D2C_ALIGN_COLOR_PRE_PROCESS_PROFILE_LIST);
    itemCount = rawData.size() / sizeof(OBD2CColorPreProcessProfile);
    for(uint32_t i = 0; i < itemCount; i++) {
        auto item = reinterpret_cast<OBD2CColorPreProcessProfile *>(rawData.data() + i * sizeof(OBD2CColorPreProcessProfile));
        d2cColorPreProcessProfileList_.push_back(*item);
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

    rawData.clear();
    rawData = port_->getRecordedStructData(OB_OPENNI_DEPTH_PROCESSOR_PARAM);
    if(!rawData.empty()) {
        frameProcessParam_ = *(reinterpret_cast<OpenNIFrameProcessParam *>(rawData.data()));
    }
}


}  // namespace libobsensor