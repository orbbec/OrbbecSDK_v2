// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "DaBaiAAlgParamManager.hpp"
#include "property/InternalProperty.hpp"
#include "DevicePids.hpp"
#include "exception/ObException.hpp"
#include "publicfilters/IMUCorrector.hpp"
#include "G330DepthWorkModeManager.hpp"
#include "sensor/video/DisparityBasedSensor.hpp"
#include <vector>
#include <sstream>
namespace libobsensor {

bool DaBaiAAlgParamManager::findBestMatchedCameraParam(const std::vector<OBCameraParam>                &cameraParamList,
                                                       const std::shared_ptr<const VideoStreamProfile> &profile, OBCameraParam &result) {
    bool found = false;
    // match same resolution
    for(uint32_t i = 0; i < cameraParamList.size(); ++i) {
        if(!calibrationParamValidMap_.empty()) {
            if(!calibrationParamValidMap_[i]) {
                continue;
            }
        }
        const auto &param = cameraParamList[i];

        auto streamType = profile->getType();
        if((streamType == OB_STREAM_DEPTH || streamType == OB_STREAM_IR || streamType == OB_STREAM_IR_LEFT || streamType == OB_STREAM_IR_RIGHT)
           && static_cast<uint32_t>(param.depthIntrinsic.width) == profile->getWidth()
           && static_cast<uint32_t>(param.depthIntrinsic.height) == profile->getHeight()) {
            found  = true;
            result = param;
            break;
        }
        else if(streamType == OB_STREAM_COLOR && static_cast<uint32_t>(param.rgbIntrinsic.width) == profile->getWidth()
                && static_cast<uint32_t>(param.rgbIntrinsic.height) == profile->getHeight()) {
            found  = true;
            result = param;
            break;
        }
    }

    if(!found) {
        // match same ratio
        float ratio = (float)profile->getWidth() / profile->getHeight();
        for(uint32_t i = 0; i < cameraParamList.size(); ++i) {
            if(!calibrationParamValidMap_.empty()) {
                if(!calibrationParamValidMap_[i]) {
                    continue;
                }
            }
            const auto &param = cameraParamList[i];

            auto streamType = profile->getType();
            if((streamType == OB_STREAM_DEPTH || streamType == OB_STREAM_IR || streamType == OB_STREAM_IR_LEFT || streamType == OB_STREAM_IR_RIGHT)
               && (float)param.depthIntrinsic.width / param.depthIntrinsic.height == ratio) {
                found  = true;
                result = param;
                break;
            }
            else if(streamType == OB_STREAM_COLOR && (float)param.rgbIntrinsic.width / param.rgbIntrinsic.height == ratio) {
                found  = true;
                result = param;
                break;
            }
        }
    }

    return found;
}

DaBaiAAlgParamManager::DaBaiAAlgParamManager(IDevice *owner) : DisparityAlgParamManagerBase(owner) {
    auto        depthWorkModeManager = owner->getComponentT<G330DepthWorkModeManager>(OB_DEV_COMPONENT_DEPTH_WORK_MODE_MANAGER);
    auto        currentWorkMode      = depthWorkModeManager->getCurrentDepthWorkMode();
    std::string name(currentWorkMode.name);
    currentDepthAlgMode_ = name;
    fetchParamFromDevice();
    fixD2CParmaList();
    registerBasicExtrinsics();
}

void DaBaiAAlgParamManager::fetchParamFromDevice() {

    try {
        auto owner           = getOwner();
        auto propServer      = owner->getPropertyServer();
        depthCalibParamList_ = propServer->getStructureDataListProtoV1_1_T<OBDepthCalibrationParam, 1>(OB_RAW_DATA_DEPTH_CALIB_PARAM);

        const auto &depthCalib       = depthCalibParamList_.front();
        disparityParam_.baseline     = depthCalib.baseline;
        disparityParam_.zpd          = depthCalib.z0;
        disparityParam_.fx           = depthCalib.focalPix;
        disparityParam_.zpps         = depthCalib.z0 / depthCalib.focalPix;
        disparityParam_.bitSize      = 14;  // low 14 bit
        disparityParam_.dispIntPlace = 8;
        disparityParam_.unit         = depthCalib.unit;
        disparityParam_.dispOffset   = depthCalib.dispOffset;
        disparityParam_.invalidDisp  = depthCalib.invalidDisp;
        disparityParam_.packMode     = OB_DISP_PACK_ORIGINAL_NEW;
        disparityParam_.isDualCamera = true;
    }
    catch(const std::exception &e) {
        LOG_ERROR("Get depth calibration params failed! {}", e.what());
    }

    try {
        auto owner           = getOwner();
        auto propServer      = owner->getPropertyServer();
        auto cameraParamList = propServer->getStructureDataListProtoV1_1_T<OBCameraParam_Internal_V0, 0>(OB_RAW_DATA_ALIGN_CALIB_PARAM);
        for(auto &cameraParam: cameraParamList) {
            OBCameraParam param;
            param.depthIntrinsic = cameraParam.depthIntrinsic;
            param.rgbIntrinsic   = cameraParam.rgbIntrinsic;
            memcpy(&param.depthDistortion, &cameraParam.depthDistortion, sizeof(param.depthDistortion));
            param.depthDistortion.model = OB_DISTORTION_BROWN_CONRADY;
            memcpy(&param.rgbDistortion, &cameraParam.rgbDistortion, sizeof(param.rgbDistortion));
            param.rgbDistortion.model = OB_DISTORTION_BROWN_CONRADY_K6;
            param.transform           = cameraParam.transform;
            param.isMirrored          = false;
            originCalibrationCameraParamList_.emplace_back(param);

            std::stringstream ss;
            ss << param;
            LOG_DEBUG("-{}", ss.str());
        }
    }
    catch(const std::exception &e) {
        LOG_ERROR("Get depth calibration params failed! {}", e.what());
    }

    try {
        auto owner            = getOwner();
        auto propServer       = owner->getPropertyServer();
        originD2cProfileList_ = propServer->getStructureDataListProtoV1_1_T<OBD2CProfile, 0>(OB_RAW_DATA_D2C_ALIGN_SUPPORT_PROFILE_LIST);
    }
    catch(const std::exception &e) {
        LOG_ERROR("Get depth to color profile list failed! {}", e.what());
    }

#if 1
    try {
        auto owner      = getOwner();
        auto propServer = owner->getPropertyServer();
        originD2cColorPreProcessProfileList_ =
            propServer->getStructureDataListProtoV1_1_T<OBD2CColorPreProcessProfile, 0>(OB_RAW_DATA_D2C_ALIGN_COLOR_PRE_PROCESS_PROFILE_LIST);
    }
    catch(const std::exception &e) {
        LOG_ERROR("Get d2c pre process profile list failed,unsupport cmd {}", e.what());
    }
#elif

    //////////////////////////////////////////
    // TODO:
    for(size_t i = 0; i < originD2cProfileList_.size(); ++i) {
        OBD2CColorPreProcessProfile preProcessProfile = { 0 };
        preProcessProfile.preProcessParam.colorScale  = 1;
        originD2cColorPreProcessProfileList_.push_back(preProcessProfile);
    }
    //////////////////////////////////////////
#endif

    d2CProfileListFilter(currentDepthAlgMode_);

    // imu param
    std::vector<uint8_t> data;
    BEGIN_TRY_EXECUTE({
        auto owner      = getOwner();
        auto propServer = owner->getPropertyServer();
        propServer->getRawData(
            OB_RAW_DATA_IMU_CALIB_PARAM,
            [&](OBDataTranState state, OBDataChunk *dataChunk) {
                if(state == DATA_TRAN_STAT_TRANSFERRING) {
                    data.insert(data.end(), dataChunk->data, dataChunk->data + dataChunk->size);
                }
            },
            PROP_ACCESS_INTERNAL);
    })
    CATCH_EXCEPTION_AND_EXECUTE({
        LOG_ERROR("Get imu calibration params failed!");
        data.clear();
    })
    if(!data.empty()) {
        imuCalibParam_ = IMUCorrector::parserIMUCalibParamRaw(data.data(), static_cast<uint32_t>(data.size()));
        LOG_DEBUG("Get imu calibration params success!");
    }
    else {
        LOG_WARN("Get imu calibration params failed! Use default params instead!");
        imuCalibParam_ = IMUCorrector::getDefaultImuCalibParam();
    }
}

void DaBaiAAlgParamManager::reFetchDisparityParams() {
    try {
        auto owner           = getOwner();
        auto propServer      = owner->getPropertyServer();
        depthCalibParamList_ = propServer->getStructureDataListProtoV1_1_T<OBDepthCalibrationParam, 1>(OB_RAW_DATA_DEPTH_CALIB_PARAM);

        const auto &depthCalib       = depthCalibParamList_.front();
        disparityParam_.baseline     = depthCalib.baseline;
        disparityParam_.zpd          = depthCalib.z0;
        disparityParam_.fx           = depthCalib.focalPix;
        disparityParam_.zpps         = depthCalib.z0 / depthCalib.focalPix;
        disparityParam_.bitSize      = 14;  // low 14 bit
        disparityParam_.dispIntPlace = 8;
        disparityParam_.unit         = depthCalib.unit;
        disparityParam_.dispOffset   = depthCalib.dispOffset;
        disparityParam_.invalidDisp  = depthCalib.invalidDisp;
        disparityParam_.packMode     = OB_DISP_PACK_ORIGINAL_NEW;
        disparityParam_.isDualCamera = true;
    }
    catch(const std::exception &e) {
        LOG_ERROR("Get depth calibration params failed! {}", e.what());
    }
}

void DaBaiAAlgParamManager::registerBasicExtrinsics() {
    auto extrinsicMgr              = StreamExtrinsicsManager::getInstance();
    auto depthBasicStreamProfile   = StreamProfileFactory::createVideoStreamProfile(OB_STREAM_DEPTH, OB_FORMAT_ANY, OB_WIDTH_ANY, OB_HEIGHT_ANY, OB_FPS_ANY);
    auto colorBasicStreamProfile   = StreamProfileFactory::createVideoStreamProfile(OB_STREAM_COLOR, OB_FORMAT_ANY, OB_WIDTH_ANY, OB_HEIGHT_ANY, OB_FPS_ANY);
    auto leftIrBasicStreamProfile  = StreamProfileFactory::createVideoStreamProfile(OB_STREAM_IR_LEFT, OB_FORMAT_ANY, OB_WIDTH_ANY, OB_HEIGHT_ANY, OB_FPS_ANY);
    auto rightIrBasicStreamProfile = StreamProfileFactory::createVideoStreamProfile(OB_STREAM_IR_RIGHT, OB_FORMAT_ANY, OB_WIDTH_ANY, OB_HEIGHT_ANY, OB_FPS_ANY);
    auto accelBasicStreamProfile   = StreamProfileFactory::createAccelStreamProfile(OB_ACCEL_FS_2g, OB_SAMPLE_RATE_1_5625_HZ);
    auto gyroBasicStreamProfile    = StreamProfileFactory::createGyroStreamProfile(OB_GYRO_FS_16dps, OB_SAMPLE_RATE_1_5625_HZ);

    if(!originCalibrationCameraParamList_.empty()) {
        auto d2cExtrinsic = originCalibrationCameraParamList_.front().transform;
        extrinsicMgr->registerExtrinsics(depthBasicStreamProfile, colorBasicStreamProfile, d2cExtrinsic);
    }
    extrinsicMgr->registerSameExtrinsics(leftIrBasicStreamProfile, depthBasicStreamProfile);

    if(!depthCalibParamList_.empty()) {
        auto left_to_right     = IdentityExtrinsics;
        left_to_right.trans[0] = -1.0f * depthCalibParamList_.front().baseline * depthCalibParamList_.front().unit;
        extrinsicMgr->registerExtrinsics(leftIrBasicStreamProfile, rightIrBasicStreamProfile, left_to_right);
    }

    const auto &imuCalibParam = getIMUCalibrationParam();
    double      imuExtr[16]   = { 0 };
    memcpy(imuExtr, imuCalibParam.singleIMUParams[0].imu_to_cam_extrinsics, sizeof(imuExtr));

    OBExtrinsic imu_to_depth;
    imu_to_depth.rot[0] = (float)imuExtr[0];
    imu_to_depth.rot[1] = (float)imuExtr[1];

    imu_to_depth.rot[2]   = (float)imuExtr[2];
    imu_to_depth.rot[3]   = (float)imuExtr[4];
    imu_to_depth.rot[4]   = (float)imuExtr[5];
    imu_to_depth.rot[5]   = (float)imuExtr[6];
    imu_to_depth.rot[6]   = (float)imuExtr[8];
    imu_to_depth.rot[7]   = (float)imuExtr[9];
    imu_to_depth.rot[8]   = (float)imuExtr[10];
    imu_to_depth.trans[0] = (float)imuExtr[3];
    imu_to_depth.trans[1] = (float)imuExtr[7];
    imu_to_depth.trans[2] = (float)imuExtr[11];
    extrinsicMgr->registerExtrinsics(accelBasicStreamProfile, depthBasicStreamProfile, imu_to_depth);
    extrinsicMgr->registerSameExtrinsics(gyroBasicStreamProfile, accelBasicStreamProfile);

    basicStreamProfileList_.emplace_back(depthBasicStreamProfile);
    basicStreamProfileList_.emplace_back(colorBasicStreamProfile);
    basicStreamProfileList_.emplace_back(leftIrBasicStreamProfile);
    basicStreamProfileList_.emplace_back(rightIrBasicStreamProfile);
    basicStreamProfileList_.emplace_back(accelBasicStreamProfile);
    basicStreamProfileList_.emplace_back(gyroBasicStreamProfile);
}

typedef struct {
    uint32_t width;
    uint32_t height;
} Resolution;

void DaBaiAAlgParamManager::fixD2CParmaList() {

    for(auto &profile: d2cProfileList_) {
        profile.alignType = ALIGN_D2C_HW;
    }

    calibrationCameraParamList_.clear();

    // save color pre process param
    preProcessCameraParamList_.clear();

    // fix color pre process param
    for(size_t i = 0; i < d2cProfileList_.size(); ++i) {
        // color pre process param
        auto colorPreParam = d2cColorPreProcessProfileList_[i].preProcessParam;
        // d2c profile list
        auto         &profile      = d2cProfileList_[i];
        auto          colorWidth   = profile.colorWidth;
        auto          colorHeight  = profile.colorHeight;
        auto          colorProfile = StreamProfileFactory::createVideoStreamProfile(OB_STREAM_COLOR, OB_FORMAT_UNKNOWN, colorWidth, colorHeight, 30);
        OBCameraParam colorAlignParam;
        if(!findBestMatchedCameraParam(originCalibrationCameraParamList_, colorProfile, colorAlignParam)) {
            continue;
        }

        // TODO: color pre process
        colorAlignParam.rgbIntrinsic.fx = colorAlignParam.rgbIntrinsic.fx / colorPreParam.colorScale;
        colorAlignParam.rgbIntrinsic.fy = colorAlignParam.rgbIntrinsic.fy / colorPreParam.colorScale;
        colorAlignParam.rgbIntrinsic.cx = (colorAlignParam.rgbIntrinsic.cx - colorPreParam.alignLeft) / colorPreParam.colorScale;
        colorAlignParam.rgbIntrinsic.cy = (colorAlignParam.rgbIntrinsic.cy - colorPreParam.alignTop) / colorPreParam.colorScale;

        colorAlignParam.rgbIntrinsic.width  = colorAlignParam.rgbIntrinsic.width;
        colorAlignParam.rgbIntrinsic.height = colorAlignParam.rgbIntrinsic.height;

        preProcessCameraParamList_.push_back(colorAlignParam);
    }

    calibrationCameraParamList_ = preProcessCameraParamList_;

    // fix soft d2c param
    for(size_t i = 0; i < originD2cProfileList_.size(); ++i) {
        auto         &profile      = originD2cProfileList_[i];
        auto          colorWidth   = profile.colorWidth;
        auto          colorHeight  = profile.colorHeight;
        auto          colorProfile = StreamProfileFactory::createVideoStreamProfile(OB_STREAM_COLOR, OB_FORMAT_UNKNOWN, colorWidth, colorHeight, 30);
        OBCameraParam colorAlignParam;
        if(!findBestMatchedCameraParam(preProcessCameraParamList_, colorProfile, colorAlignParam)) {
            continue;
        }

        OBCameraIntrinsic colorIntrinsic = colorAlignParam.rgbIntrinsic;
        float             ratio          = (float)colorProfile->getWidth() / colorIntrinsic.width;
        colorIntrinsic.fx *= ratio;
        colorIntrinsic.fy *= ratio;
        colorIntrinsic.cx *= ratio;
        colorIntrinsic.cy *= ratio;
        colorIntrinsic.width  = static_cast<int16_t>(colorProfile->getWidth());
        colorIntrinsic.height = static_cast<int16_t>((float)colorIntrinsic.height * ratio);

        auto          depthWidth   = profile.depthWidth;
        auto          depthHeight  = profile.depthHeight;
        auto          depthProfile = StreamProfileFactory::createVideoStreamProfile(OB_STREAM_DEPTH, OB_FORMAT_UNKNOWN, depthWidth, depthHeight, 30);
        OBCameraParam depthAlignParam;
        if(!findBestMatchedCameraParam(originCalibrationCameraParamList_, depthProfile, depthAlignParam)) {
            continue;
        }
        OBCameraIntrinsic depthIntrinsic = depthAlignParam.depthIntrinsic;
        ratio                            = (float)depthProfile->getWidth() / depthIntrinsic.width;
        depthIntrinsic.fx *= ratio;
        depthIntrinsic.fy *= ratio;
        depthIntrinsic.cx *= ratio;
        depthIntrinsic.cy *= ratio;
        depthIntrinsic.width  = static_cast<int16_t>(depthProfile->getWidth());
        depthIntrinsic.height = static_cast<int16_t>((float)depthIntrinsic.height * ratio);

        auto index = calibrationCameraParamList_.size();
        calibrationCameraParamList_.push_back({ depthIntrinsic, colorIntrinsic, depthAlignParam.depthDistortion, depthAlignParam.rgbDistortion,
                                                depthAlignParam.transform, depthAlignParam.isMirrored });

        OBD2CProfile d2cProfile;
        d2cProfile.alignType        = ALIGN_D2C_SW;
        d2cProfile.postProcessParam = { 1.0f, 0, 0, 0, 0 };
        d2cProfile.colorWidth       = static_cast<int16_t>(colorProfile->getWidth());
        d2cProfile.colorHeight      = static_cast<int16_t>(colorProfile->getHeight());
        d2cProfile.depthWidth       = static_cast<int16_t>(depthProfile->getWidth());
        d2cProfile.depthHeight      = static_cast<int16_t>(depthProfile->getHeight());
        d2cProfile.paramIndex       = (uint8_t)index;
        d2cProfileList_.push_back(d2cProfile);
    }

    // fix depth intrinsic according to post process param and rgb intrinsic
    for(auto &profile: d2cProfileList_) {
        if(profile.alignType != ALIGN_D2C_HW || profile.paramIndex >= calibrationCameraParamList_.size()) {
            continue;
        }
        const auto &cameraParam = calibrationCameraParamList_.at(profile.paramIndex);

        auto colorProfile = StreamProfileFactory::createVideoStreamProfile(OB_STREAM_COLOR, OB_FORMAT_UNKNOWN, profile.colorWidth, profile.colorHeight, 30);

        OBCameraParam fixedCameraParam = cameraParam;
        OBCameraParam matchedCameraParam;
        if(!findBestMatchedCameraParam(originCalibrationCameraParamList_, colorProfile, matchedCameraParam)) {
            continue;
        }
        fixedCameraParam.rgbIntrinsic = matchedCameraParam.rgbIntrinsic;

        // scale the rgb(target) intrinsic to match the profile
        auto ratio = (float)profile.colorWidth / fixedCameraParam.rgbIntrinsic.width;
        fixedCameraParam.rgbIntrinsic.fx *= ratio;
        fixedCameraParam.rgbIntrinsic.fy *= ratio;
        fixedCameraParam.rgbIntrinsic.cx *= ratio;
        fixedCameraParam.rgbIntrinsic.cy *= ratio;
        fixedCameraParam.rgbIntrinsic.width  = static_cast<int16_t>((float)fixedCameraParam.rgbIntrinsic.width * ratio);
        fixedCameraParam.rgbIntrinsic.height = static_cast<int16_t>((float)fixedCameraParam.rgbIntrinsic.height * ratio);

        // Scale the rgb(target) intrinsic parameters by the same factor as the depth intrinsic scale.
        auto depthRatio = (float)fixedCameraParam.depthIntrinsic.width / profile.depthWidth;
        fixedCameraParam.rgbIntrinsic.fx *= depthRatio;
        fixedCameraParam.rgbIntrinsic.fy *= depthRatio;
        fixedCameraParam.rgbIntrinsic.cx *= depthRatio;
        fixedCameraParam.rgbIntrinsic.cy *= depthRatio;
        fixedCameraParam.rgbIntrinsic.width  = static_cast<int16_t>((float)fixedCameraParam.rgbIntrinsic.width * depthRatio);
        fixedCameraParam.rgbIntrinsic.height = static_cast<int16_t>((float)fixedCameraParam.rgbIntrinsic.height * depthRatio);

        // according to post process param to reverse process the rgb(target) intrinsic
        fixedCameraParam.rgbIntrinsic.fx     = fixedCameraParam.rgbIntrinsic.fx / profile.postProcessParam.depthScale;
        fixedCameraParam.rgbIntrinsic.fy     = fixedCameraParam.rgbIntrinsic.fy / profile.postProcessParam.depthScale;
        fixedCameraParam.rgbIntrinsic.cx     = (fixedCameraParam.rgbIntrinsic.cx - profile.postProcessParam.alignLeft) / profile.postProcessParam.depthScale;
        fixedCameraParam.rgbIntrinsic.cy     = (fixedCameraParam.rgbIntrinsic.cy - profile.postProcessParam.alignTop) / profile.postProcessParam.depthScale;
        fixedCameraParam.rgbIntrinsic.width  = profile.depthWidth;
        fixedCameraParam.rgbIntrinsic.height = profile.depthHeight;

        auto index = calibrationCameraParamList_.size();
        calibrationCameraParamList_.push_back(fixedCameraParam);
        // auto oldProfile    = profile;
        profile.paramIndex = (uint8_t)index;
    }
}

void DaBaiAAlgParamManager::bindIntrinsic(std::vector<std::shared_ptr<const StreamProfile>> streamProfileList) {
    auto intrinsicMgr = StreamIntrinsicsManager::getInstance();
    for(const auto &sp: streamProfileList) {
        if(sp->is<AccelStreamProfile>()) {
            const auto &imuCalibParam = getIMUCalibrationParam();
            intrinsicMgr->registerAccelStreamIntrinsics(sp, imuCalibParam.singleIMUParams[0].acc);
        }
        else if(sp->is<GyroStreamProfile>()) {
            const auto &imuCalibParam = getIMUCalibrationParam();
            intrinsicMgr->registerGyroStreamIntrinsics(sp, imuCalibParam.singleIMUParams[0].gyro);
        }
        else {
            OBCameraIntrinsic  intrinsic  = { 0 };
            OBCameraDistortion distortion = { 0 };
            OBCameraParam      param{};
            auto               vsp = sp->as<VideoStreamProfile>();
            if(!findBestMatchedCameraParam(calibrationCameraParamList_, vsp, param)) {
                // throw libobsensor::unsupported_operation_exception("Can not find matched camera param!");
                continue;
            }
            switch(sp->getType()) {
            case OB_STREAM_COLOR:
                intrinsic  = param.rgbIntrinsic;
                distortion = param.rgbDistortion;
                break;
            case OB_STREAM_DEPTH:
            case OB_STREAM_IR:
            case OB_STREAM_IR_LEFT:
            case OB_STREAM_IR_RIGHT:
                intrinsic  = param.depthIntrinsic;
                distortion = param.depthDistortion;
                break;
            default:
                break;
            }
            auto ratio = (float)vsp->getWidth() / (float)intrinsic.width;
            intrinsic.fx *= ratio;
            intrinsic.fy *= ratio;
            intrinsic.cx *= ratio;
            intrinsic.cy *= ratio;
            intrinsic.width  = static_cast<int16_t>(vsp->getWidth());
            intrinsic.height = static_cast<int16_t>((float)intrinsic.height * ratio);

            intrinsicMgr->registerVideoStreamIntrinsics(sp, intrinsic);
            intrinsicMgr->registerVideoStreamDistortion(sp, distortion);
        }
    }
}

void DaBaiAAlgParamManager::d2CProfileListFilter(const std::string currentDepthAlgMode) {

    if(originD2cProfileList_.empty() || originD2cColorPreProcessProfileList_.empty()) {
        return;
    }

    bool isEnable = false;

    // Check if any profile is enabled
    for(const auto &profile: originD2cProfileList_) {
        if(profile.mixedBits.enableFlag) {
            isEnable = true;
            break;
        }
    }

    if(!isEnable) {
        // If no profiles are enabled, copy all profiles
        d2cProfileList_                = originD2cProfileList_;
        d2cColorPreProcessProfileList_ = originD2cColorPreProcessProfileList_;
        return;
    }

    // Determine work mode value based on currentDepthAlgMode
    uint8_t workModeVal = 0;
    if(std::strcmp(currentDepthAlgMode.c_str(), "Wide") == 0) {
        workModeVal = 1;
    }

    d2cProfileList_.clear();
    d2cColorPreProcessProfileList_.clear();
    // Filter profiles based on enable flag and work mode
    for(size_t i = 0; i < originD2cProfileList_.size(); i++) {
        const auto &profile = originD2cProfileList_[i];
        if(profile.mixedBits.enableFlag && workModeVal == profile.mixedBits.workModeVal) {
            auto fixProfile      = profile;
            fixProfile.alignType = fixProfile.mixedBits.aligntypeVal;
            d2cProfileList_.push_back(fixProfile);
            const auto &preProcessProfile = originD2cColorPreProcessProfileList_[i];
            d2cColorPreProcessProfileList_.push_back(preProcessProfile);
        }
    }

    // update valid calibration camera param
    if(originCalibrationCameraParamList_.size() == 4) {
        if(std::strcmp(currentDepthAlgMode.c_str(), "Wide") == 0) {
            calibrationParamValidMap_[0] = false;
            calibrationParamValidMap_[1] = false;
            calibrationParamValidMap_[2] = true;
            calibrationParamValidMap_[3] = true;
        }
        else if(std::strcmp(currentDepthAlgMode.c_str(), "Default") == 0) {
            calibrationParamValidMap_[0] = true;
            calibrationParamValidMap_[1] = true;
            calibrationParamValidMap_[2] = false;
            calibrationParamValidMap_[3] = false;
        }
    }
}

const std::vector<DeviceComponentId> updateDeviceComponentIds = { OB_DEV_COMPONENT_LEFT_IR_SENSOR, OB_DEV_COMPONENT_RIGHT_IR_SENSOR,
                                                                  OB_DEV_COMPONENT_COLOR_SENSOR };

void DaBaiAAlgParamManager::updateD2CProfileList(const std::string currentDepthAlgMode) {
    if(currentDepthAlgMode == currentDepthAlgMode_) {
        return;
    }

    d2CProfileListFilter(currentDepthAlgMode);

    fixD2CParmaList();

    // update intrinsic
    auto owner                  = getOwner();
    auto depthSensor            = owner->getComponentT<DisparityBasedSensor>(OB_DEV_COMPONENT_DEPTH_SENSOR, false);
    auto depthStreamProfileList = depthSensor->getStreamProfileList();
    bindIntrinsic(depthStreamProfileList);

    for(const auto &componentId: updateDeviceComponentIds) {
        auto sensor            = owner->getComponentT<VideoSensor>(componentId);
        auto streamProfileList = sensor->getStreamProfileList();
        bindIntrinsic(streamProfileList);
    }

    currentDepthAlgMode_ = currentDepthAlgMode;
}

}  // namespace libobsensor
