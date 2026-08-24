// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "common/test_fixtures.hpp"
#include <libobsensor/ObSensor.hpp>
#include <libobsensor/h/Utils.h>
#include "hw_test_helpers.hpp"

#include <cmath>
#include <cstring>
#include <string>

using namespace hw_test;

class TC_CPP_25_DataStruct_HW : public PipelineTest {};

TEST_F(TC_CPP_25_DataStruct_HW, TC_CPP_25_01_intrinsic_extrinsic) {
    auto config = std::make_shared<ob::Config>();
    config->enableStream(OB_STREAM_DEPTH);
    config->enableStream(OB_STREAM_COLOR);

    auto calib = pipeline_->getCalibrationParam(config);

    EXPECT_TRUE(std::isfinite(calib.intrinsics[OB_SENSOR_DEPTH].fx));
    EXPECT_TRUE(std::isfinite(calib.intrinsics[OB_SENSOR_DEPTH].fy));
    EXPECT_TRUE(std::isfinite(calib.intrinsics[OB_SENSOR_DEPTH].cx));
    EXPECT_TRUE(std::isfinite(calib.intrinsics[OB_SENSOR_DEPTH].cy));
    EXPECT_TRUE(std::isfinite(calib.intrinsics[OB_SENSOR_COLOR].fx));
    EXPECT_TRUE(std::isfinite(calib.intrinsics[OB_SENSOR_COLOR].fy));
    EXPECT_TRUE(std::isfinite(calib.intrinsics[OB_SENSOR_COLOR].cx));
    EXPECT_TRUE(std::isfinite(calib.intrinsics[OB_SENSOR_COLOR].cy));

    EXPECT_TRUE(std::isfinite(calib.distortion[OB_SENSOR_DEPTH].k1));
    EXPECT_TRUE(std::isfinite(calib.distortion[OB_SENSOR_DEPTH].k2));
    EXPECT_TRUE(std::isfinite(calib.distortion[OB_SENSOR_DEPTH].k3));
    EXPECT_TRUE(std::isfinite(calib.distortion[OB_SENSOR_DEPTH].k4));
    EXPECT_TRUE(std::isfinite(calib.distortion[OB_SENSOR_DEPTH].k5));
    EXPECT_TRUE(std::isfinite(calib.distortion[OB_SENSOR_DEPTH].k6));
    EXPECT_TRUE(std::isfinite(calib.distortion[OB_SENSOR_DEPTH].p1));
    EXPECT_TRUE(std::isfinite(calib.distortion[OB_SENSOR_DEPTH].p2));
    EXPECT_TRUE(std::isfinite(calib.distortion[OB_SENSOR_COLOR].k1));
    EXPECT_TRUE(std::isfinite(calib.distortion[OB_SENSOR_COLOR].k2));
    EXPECT_TRUE(std::isfinite(calib.distortion[OB_SENSOR_COLOR].k3));
    EXPECT_TRUE(std::isfinite(calib.distortion[OB_SENSOR_COLOR].k4));
    EXPECT_TRUE(std::isfinite(calib.distortion[OB_SENSOR_COLOR].k5));
    EXPECT_TRUE(std::isfinite(calib.distortion[OB_SENSOR_COLOR].k6));
    EXPECT_TRUE(std::isfinite(calib.distortion[OB_SENSOR_COLOR].p1));
    EXPECT_TRUE(std::isfinite(calib.distortion[OB_SENSOR_COLOR].p2));

    auto &rot = calib.extrinsics[OB_SENSOR_DEPTH][OB_SENSOR_COLOR].rot;
    float det = rot[0] * (rot[4] * rot[8] - rot[5] * rot[7]) - rot[1] * (rot[3] * rot[8] - rot[5] * rot[6]) + rot[2] * (rot[3] * rot[7] - rot[4] * rot[6]);
    EXPECT_NEAR(std::abs(det), 1.0f, 0.01f) << "Extrinsic rotation matrix should be orthogonal (det approximately equal ±1)";

    auto &trans = calib.extrinsics[OB_SENSOR_DEPTH][OB_SENSOR_COLOR].trans;
    for(int i = 0; i < 3; i++) {
        EXPECT_TRUE(std::isfinite(trans[i]));
    }
}

TEST_F(TC_CPP_25_DataStruct_HW, TC_CPP_25_02_camera_calib_param) {
    auto config = std::make_shared<ob::Config>();
    config->enableStream(OB_STREAM_DEPTH);
    config->enableStream(OB_STREAM_COLOR);
    pipeline_->start(config);
    waitForFramesetWithRetry(pipeline_);

    auto param = pipeline_->getCameraParam();

    EXPECT_GT(param.depthIntrinsic.fx, 0.0f);
    EXPECT_GT(param.depthIntrinsic.fy, 0.0f);
    EXPECT_GT(param.depthIntrinsic.cx, 0.0f);
    EXPECT_GT(param.depthIntrinsic.cy, 0.0f);

    EXPECT_GT(param.rgbIntrinsic.fx, 0.0f);
    EXPECT_GT(param.rgbIntrinsic.fy, 0.0f);

    for(int i = 0; i < 3; ++i) {
        EXPECT_TRUE(std::isfinite(param.transform.trans[i]));
    }

    pipeline_->stop();
}

TEST_F(TC_CPP_25_DataStruct_HW, TC_CPP_25_03_imu_intrinsic) {
    auto accel = tryGetSensor(device_, OB_SENSOR_ACCEL);
    if(!accel)
        GTEST_SKIP() << "No accel sensor";

    auto profiles = accel->getStreamProfileList();
    if(!profiles || profiles->getCount() == 0)
        GTEST_SKIP() << "No accel profiles available";
    auto ap = profiles->getProfile(0)->as<ob::AccelStreamProfile>();
    if(!ap)
        GTEST_SKIP() << "First accel profile could not be cast to AccelStreamProfile";
    auto intrinsic = ap->getIntrinsic();

    EXPECT_TRUE(std::isfinite(intrinsic.noiseDensity));
    EXPECT_TRUE(std::isfinite(intrinsic.randomWalk));
    EXPECT_TRUE(std::isfinite(intrinsic.referenceTemp));
    for(int i = 0; i < 3; i++) {
        EXPECT_TRUE(std::isfinite(intrinsic.bias[i]));
        EXPECT_TRUE(std::isfinite(intrinsic.gravity[i]));
    }
    for(int i = 0; i < 9; i++) {
        EXPECT_TRUE(std::isfinite(intrinsic.scaleMisalignment[i]));
        EXPECT_TRUE(std::isfinite(intrinsic.tempSlope[i]));
    }

    auto gyro = tryGetSensor(device_, OB_SENSOR_GYRO);
    if(gyro) {
        auto gProfiles = gyro->getStreamProfileList();
        if(gProfiles && gProfiles->getCount() > 0) {
            auto gp = gProfiles->getProfile(0)->as<ob::GyroStreamProfile>();
            if(!gp)
                GTEST_SKIP() << "First gyro profile could not be cast to GyroStreamProfile";
            auto gIntrinsic = gp->getIntrinsic();

            EXPECT_TRUE(std::isfinite(gIntrinsic.noiseDensity));
            EXPECT_TRUE(std::isfinite(gIntrinsic.randomWalk));
            EXPECT_TRUE(std::isfinite(gIntrinsic.referenceTemp));
            for(int i = 0; i < 3; i++) {
                EXPECT_TRUE(std::isfinite(gIntrinsic.bias[i]));
            }
            for(int i = 0; i < 9; i++) {
                EXPECT_TRUE(std::isfinite(gIntrinsic.scaleMisalignment[i]));
                EXPECT_TRUE(std::isfinite(gIntrinsic.tempSlope[i]));
            }
        }
    }
}

TEST_F(TC_CPP_25_DataStruct_HW, TC_CPP_25_04_device_temperature) {
    if(!device_->isPropertySupported(OB_STRUCT_DEVICE_TEMPERATURE, OB_PERMISSION_READ)) {
        GTEST_SKIP() << "Device temperature not supported";
    }

    OBDeviceTemperature temp     = {};
    uint32_t            dataSize = static_cast<uint32_t>(sizeof(temp));
    device_->getStructuredData(OB_STRUCT_DEVICE_TEMPERATURE, reinterpret_cast<uint8_t *>(&temp), &dataSize);

    auto checkTemp = [](float val, const char *name) {
        if(val == 0.0f || !std::isfinite(val)) {
            std::cout << "[INFO] " << name << " not available (value=" << val << ")" << std::endl;
            return;
        }
        EXPECT_GE(val, 0.0f) << name << " out of range";
        EXPECT_LE(val, 100.0f) << name << " out of range";
    };
    checkTemp(temp.ldmTemp, "ldmTemp");
    checkTemp(temp.mainBoardTemp, "mainBoardTemp");
    checkTemp(temp.tecTemp, "tecTemp");
    checkTemp(temp.imuTemp, "imuTemp");
    checkTemp(temp.rgbTemp, "rgbTemp");
    checkTemp(temp.irLeftTemp, "irLeftTemp");
    checkTemp(temp.irRightTemp, "irRightTemp");
    checkTemp(temp.chipTopTemp, "chipTopTemp");
    checkTemp(temp.chipBottomTemp, "chipBottomTemp");
}
