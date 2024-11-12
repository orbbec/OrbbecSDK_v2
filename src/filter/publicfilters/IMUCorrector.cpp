// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "IMUCorrector.hpp"
#include "exception/ObException.hpp"
#include "logger/LoggerInterval.hpp"
#include "frame/FrameFactory.hpp"
#include "stream/StreamProfile.hpp"
#include "InternalTypes.hpp"

namespace libobsensor {

OBIMUCalibrateParams IMUCorrector::parserIMUCalibParamRaw(uint8_t *data, uint32_t size) {
    auto                 singleIMUParamSize = sizeof(OBSingleIMUParams);
    uint8_t              paramCount         = static_cast<uint8_t>(size / singleIMUParamSize);
    OBIMUCalibrateParams params{};

    params.validNum = (paramCount > 3) ? 3 : paramCount;

    for(int i = 0; i < params.validNum; i++) {
        auto singleParam          = *((OBSingleIMUParams *)(data + (i * singleIMUParamSize)));
        params.singleIMUParams[i] = singleParam;
    }
    return params;
}

OBIMUCalibrateParams IMUCorrector::getDefaultImuCalibParam() {
    auto generateDefaultParams = []() {
        OBIMUCalibrateParams params;
        const double         IdentityMatrix3x3[9]  = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
        const double         IdentityMatrix4x4[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
        memset(&params, 0, sizeof(OBIMUCalibrateParams));
        memcpy(params.singleIMUParams[0].imu_to_cam_extrinsics, IdentityMatrix4x4, sizeof(IdentityMatrix4x4));
        memcpy(params.singleIMUParams[0].body_to_gyroscope, IdentityMatrix3x3, sizeof(IdentityMatrix3x3));
        memcpy(params.singleIMUParams[0].acc_to_gyro_factor, IdentityMatrix3x3, sizeof(IdentityMatrix3x3));
        memcpy(params.singleIMUParams[0].acc.scaleMisalignment, IdentityMatrix3x3, sizeof(IdentityMatrix3x3));
        memcpy(params.singleIMUParams[0].gyro.scaleMisalignment, IdentityMatrix3x3, sizeof(IdentityMatrix3x3));

        return params;
    };
    static OBIMUCalibrateParams params = generateDefaultParams();
    return params;
}

IMUCorrector::IMUCorrector() {}

void IMUCorrector::updateConfig(std::vector<std::string> &params) {
    if(params.size() != 0) {
        throw unsupported_operation_exception("IMUCorrector update config error: unsupported operation.");
    }
}

const std::string &IMUCorrector::getConfigSchema() const {
    static const std::string schema = "";
    return schema;
}

std::shared_ptr<Frame> IMUCorrector::process(std::shared_ptr<const Frame> frame) {
    if(frame == nullptr) {
        return nullptr;
    }

    auto newFrame = FrameFactory::createFrameFromOtherFrame(frame, true);
    if(!frame->is<FrameSet>()) {
        return newFrame;
    }

    auto frameSet   = newFrame->as<FrameSet>();
    auto accelFrame = frameSet->getFrame(OB_FRAME_ACCEL);
    if(accelFrame) {
        auto sp        = accelFrame->getStreamProfile();
        auto accelSp   = sp->as<AccelStreamProfile>();
        auto intrinsic = accelSp->getIntrinsic();

        auto frameData   = (AccelFrame::Data *)accelFrame->getData();
        frameData->value = correctAccel(frameData->value, &intrinsic);
    }

    auto gyroFrame = frameSet->getFrame(OB_FRAME_GYRO);
    if(gyroFrame) {
        auto sp        = gyroFrame->getStreamProfile();
        auto gyroSp    = sp->as<GyroStreamProfile>();
        auto intrinsic = gyroSp->getIntrinsic();

        auto frameData   = (GyroFrame::Data *)gyroFrame->getData();
        frameData->value = correctGyro(frameData->value, &intrinsic);
    }

    return newFrame;
}

OBAccelValue IMUCorrector::correctAccel(const OBAccelValue &accelValue, OBAccelIntrinsic *intrinsic) {
    double M_acc[3][3];
    double bias_acc[3];

    for(int i = 0; i < 3; i++) {
        M_acc[i][0] = intrinsic->scaleMisalignment[3 * i];
        M_acc[i][1] = intrinsic->scaleMisalignment[3 * i + 1];
        M_acc[i][2] = intrinsic->scaleMisalignment[3 * i + 2];
    }

    bias_acc[0] = intrinsic->bias[0];
    bias_acc[1] = intrinsic->bias[1];
    bias_acc[2] = intrinsic->bias[2];

    double correctedAccel[3];
    for(int i = 0; i < 3; i++) {
        correctedAccel[i] =
            M_acc[i][0] * (accelValue.x - bias_acc[0]) + M_acc[i][1] * (accelValue.y - bias_acc[1]) + M_acc[i][2] * (accelValue.z - bias_acc[2]);
    }

    return { static_cast<float>(correctedAccel[0]), static_cast<float>(correctedAccel[1]), static_cast<float>(correctedAccel[2]) };
}

OBGyroValue IMUCorrector::correctGyro(const OBGyroValue &gyroValue, OBGyroIntrinsic *intrinsic) {
    double M_gyro[3][3];
    double bias_gyro[3];

    for(int i = 0; i < 3; i++) {
        M_gyro[i][0] = intrinsic->scaleMisalignment[3 * i];
        M_gyro[i][1] = intrinsic->scaleMisalignment[3 * i + 1];
        M_gyro[i][2] = intrinsic->scaleMisalignment[3 * i + 2];
    }

    bias_gyro[0] = intrinsic->bias[0];
    bias_gyro[1] = intrinsic->bias[1];
    bias_gyro[2] = intrinsic->bias[2];

    double correctedGyro[3];
    for(int i = 0; i < 3; i++) {
        correctedGyro[i] =
            M_gyro[i][0] * (gyroValue.x - bias_gyro[0]) + M_gyro[i][1] * (gyroValue.y - bias_gyro[1]) + M_gyro[i][2] * (gyroValue.z - bias_gyro[2]);
    }

    return { static_cast<float>(correctedGyro[0]), static_cast<float>(correctedGyro[1]), static_cast<float>(correctedGyro[2]) };
}

}  // namespace libobsensor
