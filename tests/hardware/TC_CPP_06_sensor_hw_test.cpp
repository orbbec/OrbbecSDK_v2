// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "common/test_fixtures.hpp"
#include <libobsensor/ObSensor.hpp>
#include <libobsensor/h/Utils.h>
#include "hw_test_helpers.hpp"

#include <atomic>
#include <cstring>
#include <string>
#include <thread>

using namespace hw_test;

class TC_CPP_06_Sensor : public SensorTest {};

TEST_F(TC_CPP_06_Sensor, TC_CPP_06_01_sensor_list_completeness) {
    auto count = sensorList_->getCount();
    ASSERT_GT(count, 0u) << "Sensor list should not be empty";

    for(uint32_t i = 0; i < count; i++) {
        auto sensor = sensorList_->getSensor(i);
        ASSERT_NE(sensor, nullptr) << "getSensor(" << i << ") should return non-null sensor";
        auto type = sensor->getType();
        (void)type;
    }
}

TEST_F(TC_CPP_06_Sensor, TC_CPP_06_02_core_sensors) {
    const OBSensorType coreTypes[] = { OB_SENSOR_DEPTH, OB_SENSOR_COLOR, OB_SENSOR_IR };
    size_t             checked     = 0;

    for(auto type: coreTypes) {
        if(!hasSensorType(sensorList_, type)) {
            continue;
        }

        auto sensor = tryGetSensor(device_, type);
        ASSERT_NE(sensor, nullptr) << "Advertised sensor type " << static_cast<int>(type) << " could not be opened";
        EXPECT_EQ(sensor->getType(), type);
        ++checked;
    }

    EXPECT_GT(checked, 0u) << "No core depth/color/IR sensors are advertised by this device";
}

TEST_F(TC_CPP_06_Sensor, TC_CPP_06_03_imu_sensors) {
    auto accel = tryGetSensor(device_, OB_SENSOR_ACCEL);
    auto gyro  = tryGetSensor(device_, OB_SENSOR_GYRO);
    if(!accel && !gyro) {
        GTEST_SKIP() << "No IMU sensors (accel/gyro) on this device";
    }

    if(accel) {
        EXPECT_EQ(accel->getType(), OB_SENSOR_ACCEL);
        auto accelProfiles = accel->getStreamProfileList();
        EXPECT_NE(accelProfiles, nullptr);
        if(accelProfiles) {
            EXPECT_GT(accelProfiles->getCount(), 0u);
        }
    }
    if(gyro) {
        EXPECT_EQ(gyro->getType(), OB_SENSOR_GYRO);
        auto gyroProfiles = gyro->getStreamProfileList();
        EXPECT_NE(gyroProfiles, nullptr);
        if(gyroProfiles) {
            EXPECT_GT(gyroProfiles->getCount(), 0u);
        }
    }
}

TEST_F(TC_CPP_06_Sensor, TC_CPP_06_04_stereo_ir) {
    auto irLeft  = tryGetSensor(device_, OB_SENSOR_IR_LEFT);
    auto irRight = tryGetSensor(device_, OB_SENSOR_IR_RIGHT);
    if(irLeft) {
        EXPECT_EQ(irLeft->getType(), OB_SENSOR_IR_LEFT);
        auto irLeftProfiles = irLeft->getStreamProfileList();
        EXPECT_NE(irLeftProfiles, nullptr);
        if(irLeftProfiles) {
            EXPECT_GT(irLeftProfiles->getCount(), 0u);
        }
    }
    if(irRight) {
        EXPECT_EQ(irRight->getType(), OB_SENSOR_IR_RIGHT);
        auto irRightProfiles = irRight->getStreamProfileList();
        EXPECT_NE(irRightProfiles, nullptr);
        if(irRightProfiles) {
            EXPECT_GT(irRightProfiles->getCount(), 0u);
        }
    }
}

TEST_F(TC_CPP_06_Sensor, TC_CPP_06_05_invalid_sensor_type) {
    bool threwOrReturnedNull = false;
    try {
        auto badSensor = device_->getSensor(static_cast<OBSensorType>(999));
        if(badSensor == nullptr) {
            threwOrReturnedNull = true;
        }
    }
    catch(const ob::Error &) {
        threwOrReturnedNull = true;
    }
    catch(const std::exception &) {
        threwOrReturnedNull = true;
    }
    EXPECT_TRUE(threwOrReturnedNull) << "Invalid sensor type should throw or return nullptr";

    auto depthSensor = tryGetSensor(device_, OB_SENSOR_DEPTH);
    EXPECT_NE(depthSensor, nullptr);
    if(depthSensor) {
        EXPECT_EQ(depthSensor->getType(), OB_SENSOR_DEPTH);
    }
}

TEST_F(TC_CPP_06_Sensor, TC_CPP_06_06_sensor_type_consistency) {
    ASSERT_GT(sensorList_->getCount(), 0u);

    for(uint32_t i = 0; i < sensorList_->getCount(); ++i) {
        auto type   = sensorList_->getSensorType(i);
        auto sensor = tryGetSensor(device_, type);
        ASSERT_NE(sensor, nullptr) << "Failed to reopen advertised sensor type " << static_cast<int>(type);
        EXPECT_EQ(sensor->getType(), type);
    }
}

TEST_F(TC_CPP_06_Sensor, TC_CPP_06_07_sensor_callback_stream) {
    auto depthSensor = tryGetSensor(device_, OB_SENSOR_DEPTH);
    ASSERT_NE(depthSensor, nullptr);

    auto profileList = depthSensor->getStreamProfileList();
    ASSERT_NE(profileList, nullptr);
    ASSERT_GT(profileList->getCount(), 0u);

    auto profile = profileList->getProfile(0);

    std::atomic<int>   frameCount{ 0 };
    ScopedSensorStream guard(depthSensor);
    depthSensor->start(profile, [&frameCount](std::shared_ptr<ob::Frame>) { frameCount++; });

    std::this_thread::sleep_for(std::chrono::seconds(2));
    EXPECT_GT(frameCount.load(), 0) << "No frames received via sensor callback";
}

TEST_F(TC_CPP_06_Sensor, TC_CPP_06_08_sensor_repeated_start_stop) {
    auto depthSensor = tryGetSensor(device_, OB_SENSOR_DEPTH);
    ASSERT_NE(depthSensor, nullptr);
    auto profileList = depthSensor->getStreamProfileList();
    auto profile     = profileList->getProfile(0);

    for(int i = 0; i < 5; i++) {
        std::atomic<int>   frameCount{ 0 };
        ScopedSensorStream guard(depthSensor);
        depthSensor->start(profile, [&frameCount](std::shared_ptr<ob::Frame>) { frameCount++; });

        std::this_thread::sleep_for(std::chrono::seconds(1));
        EXPECT_GT(frameCount.load(), 0) << "No frames received in cycle " << (i + 1);
    }
}

TEST_F(TC_CPP_06_Sensor, TC_CPP_06_09_sensor_after_reboot) {
    if(!isDestructiveAllowed()) {
        GTEST_SKIP() << "Destructive tests not enabled in config";
    }
    auto sn = std::string(devInfo_->getSerialNumber());

    std::atomic<bool> reconnected{ false };
    auto              cbId = ctx_->registerDeviceChangedCallback([&](std::shared_ptr<ob::DeviceList> /*removed*/, std::shared_ptr<ob::DeviceList> added) {
        if(added && added->getCount() > 0) {
            reconnected = true;
        }
    });

    device_->reboot();
    device_.reset();
    devInfo_.reset();
    sensorList_.reset();

    constexpr int kMaxWaitMs = 30000;
    constexpr int kPollMs    = 500;
    for(int elapsed = 0; !reconnected.load() && elapsed < kMaxWaitMs; elapsed += kPollMs) {
        std::this_thread::sleep_for(std::chrono::milliseconds(kPollMs));
    }
    ctx_->unregisterDeviceChangedCallback(cbId);
    ASSERT_TRUE(reconnected.load()) << "Device did not reconnect within 30 s after reboot";

    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    auto devList = ctx_->queryDeviceList();
    ASSERT_NE(devList, nullptr);
    device_ = devList->getDeviceBySN(sn.c_str());
    ASSERT_NE(device_, nullptr) << "Device SN " << sn << " not found in device list after reboot";

    auto sensors = device_->getSensorList();
    ASSERT_NE(sensors, nullptr);
    ASSERT_GT(sensors->getCount(), 0u) << "No sensors available after reboot";

    auto depthSensor = tryGetSensor(device_, OB_SENSOR_DEPTH);
    ASSERT_NE(depthSensor, nullptr) << "Depth sensor unavailable after reboot";
    auto profiles = depthSensor->getStreamProfileList();
    ASSERT_NE(profiles, nullptr);
    ASSERT_GT(profiles->getCount(), 0u);

    std::atomic<int> frameCount{ 0 };
    depthSensor->start(profiles->getProfile(0), [&frameCount](std::shared_ptr<ob::Frame>) { frameCount++; });
    std::this_thread::sleep_for(std::chrono::seconds(3));
    depthSensor->stop();

    EXPECT_GT(frameCount.load(), 0) << "No depth frames received after device reboot";
}
