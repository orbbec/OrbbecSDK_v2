// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "LiDARAlgParamManager.hpp"
#include "stream/StreamIntrinsicsManager.hpp"
#include "stream/StreamExtrinsicsManager.hpp"
#include "stream/StreamProfileFactory.hpp"
#include "stream/StreamProfile.hpp"
#include "property/InternalProperty.hpp"
#include "DevicePids.hpp"
#include "exception/ObException.hpp"
#include "publicfilters/IMUCorrector.hpp"

#include <vector>
#include <sstream>
namespace libobsensor {

LiDARAlgParamManager::LiDARAlgParamManager(IDevice *owner) : AlgParamManagerBase(owner) {
    fetchParamFromDevice();
    registerBasicExtrinsics();
}

void LiDARAlgParamManager::bindIntrinsic(std::vector<std::shared_ptr<const StreamProfile>> streamProfileList) {
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
            throw invalid_value_exception("Unsupported stream profile type for LiDAR device.");
        }
    }
}

void LiDARAlgParamManager::fetchParamFromDevice() {
    // imu param
    std::vector<uint8_t> data;
    BEGIN_TRY_EXECUTE({
        auto owner      = getOwner();
        auto propServer = owner->getPropertyServer();
        data            = propServer->getStructureData(OB_RAW_DATA_IMU_CALIB_PARAM, PROP_ACCESS_INTERNAL);
    })
    CATCH_EXCEPTION_AND_EXECUTE({
        LOG_ERROR("Get imu calibration params failed!");
        data.clear();
    })

    imuCalibParam_ = IMUCorrector::parserIMUCalibParamRaw(data.data(), static_cast<uint32_t>(data.size()));
    if(imuCalibParam_.validNum > 0) {
        LOG_DEBUG("Get imu calibration params success!");
    }
    else {
        LOG_WARN("Get imu calibration params failed! Use default params instead!");
        imuCalibParam_ = IMUCorrector::getDefaultImuCalibParam();
    }
}

void LiDARAlgParamManager::registerBasicExtrinsics() {
    auto extrinsicMgr            = StreamExtrinsicsManager::getInstance();
    auto lidarBasicStreamProfile = StreamProfileFactory::createLiDARStreamProfile(OB_LIDAR_SCAN_ANY, OB_FORMAT_ANY);
    auto accelBasicStreamProfile = StreamProfileFactory::createAccelStreamProfile(OB_ACCEL_FULL_SCALE_RANGE_ANY, OB_ACCEL_SAMPLE_RATE_ANY);
    auto gyroBasicStreamProfile  = StreamProfileFactory::createGyroStreamProfile(OB_GYRO_FULL_SCALE_RANGE_ANY, OB_GYRO_SAMPLE_RATE_ANY);

    const auto &imuCalibParam = getIMUCalibrationParam();
    double      imuExtr[16]   = { 0 };
    memcpy(imuExtr, imuCalibParam.singleIMUParams[0].imu_to_cam_extrinsics, sizeof(imuExtr));

    OBExtrinsic imuToLidar;
    imuToLidar.rot[0]   = (float)imuExtr[0];
    imuToLidar.rot[1]   = (float)imuExtr[1];
    imuToLidar.rot[2]   = (float)imuExtr[2];
    imuToLidar.rot[3]   = (float)imuExtr[4];
    imuToLidar.rot[4]   = (float)imuExtr[5];
    imuToLidar.rot[5]   = (float)imuExtr[6];
    imuToLidar.rot[6]   = (float)imuExtr[8];
    imuToLidar.rot[7]   = (float)imuExtr[9];
    imuToLidar.rot[8]   = (float)imuExtr[10];
    imuToLidar.trans[0] = (float)imuExtr[3];
    imuToLidar.trans[1] = (float)imuExtr[7];
    imuToLidar.trans[2] = (float)imuExtr[11];

    extrinsicMgr->registerExtrinsics(accelBasicStreamProfile, lidarBasicStreamProfile, imuToLidar);
    extrinsicMgr->registerSameExtrinsics(gyroBasicStreamProfile, accelBasicStreamProfile);

    basicStreamProfileList_.emplace_back(lidarBasicStreamProfile);
    basicStreamProfileList_.emplace_back(accelBasicStreamProfile);
    basicStreamProfileList_.emplace_back(gyroBasicStreamProfile);
}

}  // namespace libobsensor