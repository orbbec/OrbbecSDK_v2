// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "common/test_fixtures.hpp"
#include <libobsensor/ObSensor.hpp>
#include <libobsensor/h/Utils.h>
#include "hw_test_helpers.hpp"

#include <atomic>
#include <cmath>
#include <cstring>
#include <mutex>
#include <string>

using namespace hw_test;

class TC_CPP_21_Firmware : public DeviceTest {};

TEST_F(TC_CPP_21_Firmware, TC_CPP_21_01_global_timestamp) {
    bool supported = device_->isGlobalTimestampSupported();
    if(supported) {
        ScopeGuard guard([&] { device_->enableGlobalTimestamp(false); });
        ASSERT_NO_THROW(device_->enableGlobalTimestamp(true));
        ASSERT_NO_THROW(device_->enableGlobalTimestamp(false));
    }
}

TEST_F(TC_CPP_21_Firmware, TC_CPP_21_02_device_state) {
    ASSERT_NO_THROW(device_->getDeviceState());
}

TEST_F(TC_CPP_21_Firmware, TC_CPP_21_03_state_change_callback) {
    ASSERT_NO_THROW(device_->setDeviceStateChangedCallback([](OBDeviceState, const char *) {}));
}

TEST_F(TC_CPP_21_Firmware, TC_CPP_21_04_heartbeat) {
    HeartbeatDisableGuard guard(device_);
    ASSERT_NO_THROW(device_->enableHeartbeat(true));
}

TEST_F(TC_CPP_21_Firmware, TC_CPP_21_05_calibration_param_list) {
    auto paramList = device_->getCalibrationCameraParamList();
    ASSERT_NE(paramList, nullptr);
    auto count = paramList->getCount();
    EXPECT_GT(count, 0u);

    for(uint32_t i = 0; i < count; i++) {
        auto param = paramList->getCameraParam(i);
        EXPECT_TRUE(std::isfinite(param.depthIntrinsic.fx));
        EXPECT_TRUE(std::isfinite(param.depthIntrinsic.fy));
        EXPECT_TRUE(std::isfinite(param.depthIntrinsic.cx));
        EXPECT_TRUE(std::isfinite(param.depthIntrinsic.cy));
        EXPECT_TRUE(std::isfinite(param.rgbIntrinsic.fx));
        EXPECT_TRUE(std::isfinite(param.rgbIntrinsic.fy));
    }
}

TEST_F(TC_CPP_21_Firmware, TC_CPP_21_06_reboot) {
    auto sn = std::string(devInfo_->getSerialNumber());

    std::atomic<bool> reconnected{ false };
    auto              cbId = ctx_->registerDeviceChangedCallback([&](std::shared_ptr<ob::DeviceList> /*removed*/, std::shared_ptr<ob::DeviceList> added) {
        if(added && added->getCount() > 0)
            reconnected = true;
    });

    device_->reboot();
    device_.reset();
    devInfo_.reset();

    constexpr int kMaxWaitMs = 30000;
    constexpr int kPollMs    = 500;
    for(int elapsed = 0; !reconnected.load() && elapsed < kMaxWaitMs; elapsed += kPollMs) {
        std::this_thread::sleep_for(std::chrono::milliseconds(kPollMs));
    }
    ctx_->unregisterDeviceChangedCallback(cbId);
    ASSERT_TRUE(reconnected.load()) << "Device did not reconnect within 30 s after reboot";

    auto devList = ctx_->queryDeviceList();
    ASSERT_NE(devList, nullptr);
    device_ = devList->getDeviceBySN(sn.c_str());
    ASSERT_NE(device_, nullptr) << "Device SN " << sn << " not found after reboot";
    devInfo_ = device_->getDeviceInfo();
    ASSERT_NE(devInfo_, nullptr);
    EXPECT_STREQ(devInfo_->getSerialNumber(), sn.c_str());
}

TEST_F(TC_CPP_21_Firmware, TC_CPP_21_07_firmware_update) {
    if(!isDestructiveAllowed()) {
        GTEST_SKIP() << "Destructive tests not enabled in config";
    }
    if(!hasFirmware()) {
        GTEST_SKIP() << "No firmware file - set resource.firmwarePath in config";
    }
    ASSERT_TRUE(rebootAndReconnect(ctx_, cfg_.deviceSerial(), device_, devInfo_)) << "Device did not reconnect after reboot";
    const std::string &fwPath = ENV().firmwarePath();

    std::atomic<OBFwUpdateState> lastState{ STAT_START };
    std::atomic<uint8_t>         lastPercent{ 0 };
    std::mutex                   mtx;
    std::condition_variable      cv;
    bool                         done = false;
    std::string                  lastMsg;

    ob::Device::DeviceFwUpdateCallback cb = [&](OBFwUpdateState state, const char *msg, uint8_t percent) {
        lastState   = state;
        lastPercent = percent;
        if(state == STAT_DONE || state < 0 /* any ERR_ value */) {
            std::lock_guard<std::mutex> lk(mtx);
            if(msg)
                lastMsg = msg;
            done = true;
            cv.notify_all();
        }
    };

    ASSERT_NO_THROW(device_->updateFirmware(fwPath.c_str(), cb, /*async=*/true));

    {
        std::unique_lock<std::mutex> lk(mtx);
        cv.wait_for(lk, std::chrono::minutes(3), [&] { return done; });
    }

    EXPECT_TRUE(done) << "Firmware update did not complete within 3 minutes";
    if(done && (lastState.load() == ERR_MISMATCH || lastState.load() == ERR_UNSUPPORT_DEV)) {
        GTEST_SKIP() << "Firmware not compatible with connected device; state=" << static_cast<int>(lastState.load()) << ", msg: " << lastMsg;
    }
    EXPECT_EQ(lastState.load(), STAT_DONE) << "Firmware update ended with state " << static_cast<int>(lastState.load()) << ", msg: " << lastMsg;
    if(lastState.load() == STAT_DONE) {
        EXPECT_EQ(lastPercent.load(), 100u);
    }
}

TEST_F(TC_CPP_21_Firmware, TC_CPP_21_08_update_depth_presets) {
    if(!isDestructiveAllowed()) {
        GTEST_SKIP() << "Destructive tests not enabled in config";
    }
    if(!hasDepthPreset()) {
        GTEST_SKIP() << "No depth preset file - set resource.depthPresetPath in config";
    }
    ASSERT_TRUE(rebootAndReconnect(ctx_, cfg_.deviceSerial(), device_, devInfo_)) << "Device did not reconnect after reboot";
    const std::string &presetPathStr = ENV().depthPresetPath();

    char pathList[1][OB_PATH_MAX]{};
    std::strncpy(pathList[0], presetPathStr.c_str(), OB_PATH_MAX - 1);

    std::atomic<OBFwUpdateState> lastState{ STAT_START };
    std::mutex                   mtx;
    std::condition_variable      cv;
    bool                         done = false;

    ob::Device::DeviceFwUpdateCallback cb = [&](OBFwUpdateState state, const char * /*msg*/, uint8_t /*percent*/) {
        lastState = state;
        if(state == STAT_DONE || state < 0 /* any ERR_ value */) {
            std::lock_guard<std::mutex> lk(mtx);
            done = true;
            cv.notify_all();
        }
    };

    ASSERT_NO_THROW(device_->updateOptionalDepthPresets(pathList, 1, cb));

    {
        std::unique_lock<std::mutex> lk(mtx);
        cv.wait_for(lk, std::chrono::minutes(2), [&] { return done; });
    }

    EXPECT_TRUE(done) << "Depth preset update did not complete within 2 minutes";
    if(done && (lastState.load() == ERR_MISMATCH || lastState.load() == ERR_UNSUPPORT_DEV)) {
        GTEST_SKIP() << "Depth preset resource is not compatible with the connected device; state=" << static_cast<int>(lastState.load());
    }
    EXPECT_EQ(lastState.load(), STAT_DONE) << "Depth preset update ended with state " << static_cast<int>(lastState.load());
}
