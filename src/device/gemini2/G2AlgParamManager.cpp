#include "G2AlgParamManager.hpp"

#include "InternalTypes.hpp"
#include "property/InternalProperty.hpp"
#include "publicfilters/IMUCorrector.hpp"
#include "stream/StreamIntrinsicsManager.hpp"
#include "stream/StreamExtrinsicsManager.hpp"
#include "stream/StreamProfileFactory.hpp"
#include "exception/ObException.hpp"

namespace libobsensor {
G2AlgParamManager::G2AlgParamManager(IDevice *owner) : DisparityAlgParamManagerBase(owner) {
    fetchParamFromDevice();
    registerBasicExtrinsics();
}

void G2AlgParamManager::fetchParamFromDevice() {
    try {
        auto owner                   = getOwner();
        auto propServer              = owner->getPropertyServer();
        depthCalibParamList_         = propServer->getStructureDataListProtoV1_1_T<OBDepthCalibrationParam, 1>(OB_RAW_DATA_DEPTH_CALIB_PARAM);
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
            param.depthDistortion.model = OB_DISTORTION_BROWN_CONRADY_K6;
            memcpy(&param.rgbDistortion, &cameraParam.rgbDistortion, sizeof(param.rgbDistortion));
            param.rgbDistortion.model = OB_DISTORTION_BROWN_CONRADY_K6;
            param.transform           = cameraParam.transform;
            param.isMirrored          = false;
            calibrationCameraParamList_.emplace_back(param);

            std::stringstream ss;
            ss << param;
            LOG_DEBUG("-{}", ss.str());
        }
    }
    catch(const std::exception &e) {
        LOG_ERROR("Get depth calibration params failed! {}", e.what());
    }

    try {
        auto owner      = getOwner();
        auto propServer = owner->getPropertyServer();
        d2cProfileList_ = propServer->getStructureDataListProtoV1_1_T<OBD2CProfile, 0>(OB_RAW_DATA_D2C_ALIGN_SUPPORT_PROFILE_LIST);
    }
    catch(const std::exception &e) {
        LOG_ERROR("Get depth to color profile list failed! {}", e.what());
    }

    // imu param
    std::vector<uint8_t> data;
    try {
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
    }
    catch(...) {
        LOG_ERROR("Get imu calibration params failed!");
        data.clear();
    }

    if(data.empty()) {
        imuCalibParam_ = IMUCorrector::getDefaultImuCalibParam();
    }
    else {
        imuCalibParam_ = IMUCorrector::parserIMUCalibParamRaw(data.data(), static_cast<uint32_t>(data.size()));
        LOG_DEBUG("Get imu calibration params success!");
    }
}
void G2AlgParamManager::registerBasicExtrinsics() {
    auto extrinsicMgr              = StreamExtrinsicsManager::getInstance();
    auto depthBasicStreamProfile   = StreamProfileFactory::createVideoStreamProfile(OB_STREAM_DEPTH, OB_FORMAT_ANY, OB_WIDTH_ANY, OB_HEIGHT_ANY, OB_FPS_ANY);
    auto colorBasicStreamProfile   = StreamProfileFactory::createVideoStreamProfile(OB_STREAM_COLOR, OB_FORMAT_ANY, OB_WIDTH_ANY, OB_HEIGHT_ANY, OB_FPS_ANY);
    auto irBasicStreamProfile      = StreamProfileFactory::createVideoStreamProfile(OB_STREAM_IR, OB_FORMAT_ANY, OB_WIDTH_ANY, OB_HEIGHT_ANY, OB_FPS_ANY);
    auto leftIrBasicStreamProfile  = StreamProfileFactory::createVideoStreamProfile(OB_STREAM_IR_LEFT, OB_FORMAT_ANY, OB_WIDTH_ANY, OB_HEIGHT_ANY, OB_FPS_ANY);
    auto rightIrBasicStreamProfile = StreamProfileFactory::createVideoStreamProfile(OB_STREAM_IR_RIGHT, OB_FORMAT_ANY, OB_WIDTH_ANY, OB_HEIGHT_ANY, OB_FPS_ANY);
    auto accelBasicStreamProfile   = StreamProfileFactory::createAccelStreamProfile(OB_ACCEL_FS_2g, OB_SAMPLE_RATE_1_5625_HZ);
    auto gyroBasicStreamProfile    = StreamProfileFactory::createGyroStreamProfile(OB_GYRO_FS_16dps, OB_SAMPLE_RATE_1_5625_HZ);

    if(!calibrationCameraParamList_.empty()) {
        auto d2cExtrinsic = calibrationCameraParamList_.front().transform;
        bool mirrored     = false;
        {
            auto owner      = getOwner();
            auto propServer = owner->getPropertyServer();
            if(propServer->isPropertySupported(OB_PROP_DEPTH_MIRROR_MODULE_STATUS_BOOL, PROP_OP_READ, PROP_ACCESS_INTERNAL)) {
                mirrored = propServer->getPropertyValueT<bool>(OB_PROP_DEPTH_MIRROR_MODULE_STATUS_BOOL);
            }
        }
        if(mirrored) {
            (d2cExtrinsic.rot)[1] *= -1.0;
            (d2cExtrinsic.rot)[2] *= -1.0;
            (d2cExtrinsic.rot)[3] *= -1.0;
            (d2cExtrinsic.rot)[6] *= -1.0;
            (d2cExtrinsic.trans)[0] *= -1.0;
        }
        extrinsicMgr->registerExtrinsics(depthBasicStreamProfile, colorBasicStreamProfile, d2cExtrinsic);
    }

    extrinsicMgr->registerSameExtrinsics(irBasicStreamProfile, depthBasicStreamProfile);
    extrinsicMgr->registerSameExtrinsics(leftIrBasicStreamProfile, depthBasicStreamProfile);

    if(!depthCalibParamList_.empty()) {
        auto left_to_right     = IdentityExtrinsic;
        left_to_right.trans[0] = -1.0f * depthCalibParamList_.front().baseline * depthCalibParamList_.front().unit;
        extrinsicMgr->registerExtrinsics(leftIrBasicStreamProfile, rightIrBasicStreamProfile, left_to_right);
    }

    double imuExtr[16] = { 0 };
    memcpy(imuExtr, imuCalibParam_.singleIMUParams[0].imu_to_cam_extrinsics, sizeof(imuExtr));

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
    basicStreamProfileList_.emplace_back(irBasicStreamProfile);
    basicStreamProfileList_.emplace_back(leftIrBasicStreamProfile);
    basicStreamProfileList_.emplace_back(rightIrBasicStreamProfile);
    basicStreamProfileList_.emplace_back(accelBasicStreamProfile);
    basicStreamProfileList_.emplace_back(gyroBasicStreamProfile);
}

}  // namespace libobsensor