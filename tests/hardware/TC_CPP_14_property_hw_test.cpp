// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "common/test_fixtures.hpp"
#include <libobsensor/ObSensor.hpp>
#include <libobsensor/h/Utils.h>
#include "hw_test_helpers.hpp"

#include <cstring>
#include <string>

using namespace hw_test;

class TC_CPP_14_Property_HW : public DeviceTest {};

TEST_F(TC_CPP_14_Property_HW, TC_CPP_14_01_property_enum) {
    int count = device_->getSupportedPropertyCount();
    EXPECT_GT(count, 0);
    for(int i = 0; i < count; i++) {
        auto item = device_->getSupportedProperty(i);
        EXPECT_NE(item.id, 0);
    }
}

TEST_F(TC_CPP_14_Property_HW, TC_CPP_14_02_bool_property) {
    if(device_->isPropertySupported(OB_PROP_LASER_BOOL, OB_PERMISSION_READ_WRITE)) {
        BoolPropertyGuard guard(device_, OB_PROP_LASER_BOOL);
        bool              original = device_->getBoolProperty(OB_PROP_LASER_BOOL);
        device_->setBoolProperty(OB_PROP_LASER_BOOL, !original);
        bool flipped = device_->getBoolProperty(OB_PROP_LASER_BOOL);
        EXPECT_EQ(flipped, !original) << "Read-after-write mismatch for OB_PROP_LASER_BOOL";
    }
}

TEST_F(TC_CPP_14_Property_HW, TC_CPP_14_03_int_property_range) {
    if(device_->isPropertySupported(OB_PROP_DEPTH_EXPOSURE_INT, OB_PERMISSION_READ_WRITE)) {
        auto range = device_->getIntPropertyRange(OB_PROP_DEPTH_EXPOSURE_INT);
        EXPECT_LT(range.min, range.max);
        EXPECT_GT(range.step, 0u);

        int cur = device_->getIntProperty(OB_PROP_DEPTH_EXPOSURE_INT);
        EXPECT_GE(cur, (int32_t)range.min);
        EXPECT_LE(cur, (int32_t)range.max);
    }
}

TEST_F(TC_CPP_14_Property_HW, TC_CPP_14_04_float_property_range) {
    if(!device_->isPropertySupported(OB_PROP_DEPTH_UNIT_FLEXIBLE_ADJUSTMENT_FLOAT, OB_PERMISSION_READ)) {
        GTEST_SKIP() << "OB_PROP_DEPTH_UNIT_FLEXIBLE_ADJUSTMENT_FLOAT not supported on this device";
    }

    auto range = device_->getFloatPropertyRange(OB_PROP_DEPTH_UNIT_FLEXIBLE_ADJUSTMENT_FLOAT);
    EXPECT_LT(range.min, range.max);
    EXPECT_GT(range.step, 0.0f);
    float cur = device_->getFloatProperty(OB_PROP_DEPTH_UNIT_FLEXIBLE_ADJUSTMENT_FLOAT);
    EXPECT_GE(cur, range.min);
    EXPECT_LE(cur, range.max);
}

TEST_F(TC_CPP_14_Property_HW, TC_CPP_14_05_structured_data) {
    if(!device_->isPropertySupported(OB_STRUCT_DEPTH_AE_ROI, OB_PERMISSION_READ_WRITE)) {
        GTEST_SKIP() << "OB_STRUCT_DEPTH_AE_ROI not fully supported (read+write)";
    }

    OBRegionOfInterest orig     = {};
    uint32_t           dataSize = sizeof(orig);
    ASSERT_NO_THROW(device_->getStructuredData(OB_STRUCT_DEPTH_AE_ROI, reinterpret_cast<uint8_t *>(&orig), &dataSize));

    ScopeGuard restore([&] {
        try {
            device_->setStructuredData(OB_STRUCT_DEPTH_AE_ROI, reinterpret_cast<const uint8_t *>(&orig), static_cast<uint32_t>(sizeof(orig)));
        }
        catch(...) {
        }
    });

    OBRegionOfInterest writeRoi = orig;
    writeRoi.x0_left            = static_cast<int16_t>(orig.x0_left + 10);
    writeRoi.y0_top             = static_cast<int16_t>(orig.y0_top + 10);
    writeRoi.x1_right           = static_cast<int16_t>(orig.x1_right - 10);
    writeRoi.y1_bottom          = static_cast<int16_t>(orig.y1_bottom - 10);

    ASSERT_NO_THROW(device_->setStructuredData(OB_STRUCT_DEPTH_AE_ROI, reinterpret_cast<const uint8_t *>(&writeRoi), static_cast<uint32_t>(sizeof(writeRoi))));

    OBRegionOfInterest readRoi = {};
    dataSize                   = sizeof(readRoi);
    ASSERT_NO_THROW(device_->getStructuredData(OB_STRUCT_DEPTH_AE_ROI, reinterpret_cast<uint8_t *>(&readRoi), &dataSize));

    EXPECT_EQ(readRoi.x0_left, writeRoi.x0_left);
    EXPECT_EQ(readRoi.y0_top, writeRoi.y0_top);
    EXPECT_EQ(readRoi.x1_right, writeRoi.x1_right);
    EXPECT_EQ(readRoi.y1_bottom, writeRoi.y1_bottom);
}

TEST_F(TC_CPP_14_Property_HW, TC_CPP_14_06_raw_data) {
    int  propCount    = device_->getSupportedPropertyCount();
    bool foundRawProp = false;
    int  rawPropCount = 0;

    for(int i = 0; i < propCount; ++i) {
        auto item = device_->getSupportedProperty(static_cast<uint32_t>(i));
        EXPECT_NE(item.id, 0);
        if(item.type == OB_STRUCT_PROPERTY) {
            foundRawProp = true;
            rawPropCount++;
            EXPECT_TRUE(item.permission & OB_PERMISSION_READ || item.permission & OB_PERMISSION_READ_WRITE)
                << "Raw data property " << item.id << " should have at least READ permission";
        }
    }

    if(!foundRawProp) {
        GTEST_SKIP() << "No raw data properties found on this device";
    }
    std::cout << "[14_06] Found " << rawPropCount << " raw data properties" << std::endl;
}

TEST_F(TC_CPP_14_Property_HW, TC_CPP_14_07_laser_control) {
    if(device_->isPropertySupported(OB_PROP_LASER_BOOL, OB_PERMISSION_READ_WRITE)) {
        BoolPropertyGuard guard(device_, OB_PROP_LASER_BOOL);
        bool              original = device_->getBoolProperty(OB_PROP_LASER_BOOL);
        device_->setBoolProperty(OB_PROP_LASER_BOOL, !original);
        bool afterSet = device_->getBoolProperty(OB_PROP_LASER_BOOL);
        EXPECT_EQ(afterSet, !original);
    }
    if(device_->isPropertySupported(OB_PROP_LDP_BOOL, OB_PERMISSION_READ_WRITE)) {
        BoolPropertyGuard guard(device_, OB_PROP_LDP_BOOL);
        bool              original = device_->getBoolProperty(OB_PROP_LDP_BOOL);
        device_->setBoolProperty(OB_PROP_LDP_BOOL, !original);
        bool afterSet = device_->getBoolProperty(OB_PROP_LDP_BOOL);
        EXPECT_EQ(afterSet, !original);
    }
    if(device_->isPropertySupported(OB_PROP_FLOOD_BOOL, OB_PERMISSION_READ_WRITE)) {
        BoolPropertyGuard guard(device_, OB_PROP_FLOOD_BOOL);
        bool              original = device_->getBoolProperty(OB_PROP_FLOOD_BOOL);
        device_->setBoolProperty(OB_PROP_FLOOD_BOOL, !original);
        bool afterSet = device_->getBoolProperty(OB_PROP_FLOOD_BOOL);
        EXPECT_EQ(afterSet, !original);
    }
    if(device_->isPropertySupported(OB_PROP_LASER_POWER_LEVEL_CONTROL_INT, OB_PERMISSION_READ_WRITE)) {
        auto range = device_->getIntPropertyRange(OB_PROP_LASER_POWER_LEVEL_CONTROL_INT);
        if(range.min < range.max) {
            IntPropertyGuard guard(device_, OB_PROP_LASER_POWER_LEVEL_CONTROL_INT);
            int              original = device_->getIntProperty(OB_PROP_LASER_POWER_LEVEL_CONTROL_INT);
            device_->setIntProperty(OB_PROP_LASER_POWER_LEVEL_CONTROL_INT, range.max);
            int afterSet = device_->getIntProperty(OB_PROP_LASER_POWER_LEVEL_CONTROL_INT);
            EXPECT_EQ(afterSet, range.max);
        }
    }
}

TEST_F(TC_CPP_14_Property_HW, TC_CPP_14_08_depth_control) {
    if(device_->isPropertySupported(OB_PROP_DEPTH_MIRROR_BOOL, OB_PERMISSION_READ_WRITE)) {
        BoolPropertyGuard guard(device_, OB_PROP_DEPTH_MIRROR_BOOL);
        bool              original = device_->getBoolProperty(OB_PROP_DEPTH_MIRROR_BOOL);
        device_->setBoolProperty(OB_PROP_DEPTH_MIRROR_BOOL, !original);
        bool afterSet = device_->getBoolProperty(OB_PROP_DEPTH_MIRROR_BOOL);
        EXPECT_EQ(afterSet, !original);
    }
    if(device_->isPropertySupported(OB_PROP_DEPTH_FLIP_BOOL, OB_PERMISSION_READ_WRITE)) {
        BoolPropertyGuard guard(device_, OB_PROP_DEPTH_FLIP_BOOL);
        bool              original = device_->getBoolProperty(OB_PROP_DEPTH_FLIP_BOOL);
        device_->setBoolProperty(OB_PROP_DEPTH_FLIP_BOOL, !original);
        bool afterSet = device_->getBoolProperty(OB_PROP_DEPTH_FLIP_BOOL);
        EXPECT_EQ(afterSet, !original);
    }
    if(device_->isPropertySupported(OB_PROP_DEPTH_AUTO_EXPOSURE_BOOL, OB_PERMISSION_READ_WRITE)) {
        BoolPropertyGuard guard(device_, OB_PROP_DEPTH_AUTO_EXPOSURE_BOOL);
        bool              original = device_->getBoolProperty(OB_PROP_DEPTH_AUTO_EXPOSURE_BOOL);
        device_->setBoolProperty(OB_PROP_DEPTH_AUTO_EXPOSURE_BOOL, !original);
        bool afterSet = device_->getBoolProperty(OB_PROP_DEPTH_AUTO_EXPOSURE_BOOL);
        EXPECT_EQ(afterSet, !original);
    }
    if(device_->isPropertySupported(OB_PROP_DEPTH_EXPOSURE_INT, OB_PERMISSION_READ_WRITE)) {
        auto range = device_->getIntPropertyRange(OB_PROP_DEPTH_EXPOSURE_INT);
        if(range.min < range.max) {
            IntPropertyGuard guard(device_, OB_PROP_DEPTH_EXPOSURE_INT);
            int              original = device_->getIntProperty(OB_PROP_DEPTH_EXPOSURE_INT);
            device_->setIntProperty(OB_PROP_DEPTH_EXPOSURE_INT, range.max);
            int afterSet = device_->getIntProperty(OB_PROP_DEPTH_EXPOSURE_INT);
            EXPECT_EQ(afterSet, range.max);
        }
    }
    if(device_->isPropertySupported(OB_PROP_DEPTH_GAIN_INT, OB_PERMISSION_READ_WRITE)) {
        auto range = device_->getIntPropertyRange(OB_PROP_DEPTH_GAIN_INT);
        if(range.min < range.max) {
            IntPropertyGuard guard(device_, OB_PROP_DEPTH_GAIN_INT);
            int              original = device_->getIntProperty(OB_PROP_DEPTH_GAIN_INT);
            device_->setIntProperty(OB_PROP_DEPTH_GAIN_INT, range.max);
            int afterSet = device_->getIntProperty(OB_PROP_DEPTH_GAIN_INT);
            EXPECT_EQ(afterSet, range.max);
        }
    }
}

TEST_F(TC_CPP_14_Property_HW, TC_CPP_14_09_color_control) {
    if(device_->isPropertySupported(OB_PROP_COLOR_MIRROR_BOOL, OB_PERMISSION_READ_WRITE)) {
        BoolPropertyGuard guard(device_, OB_PROP_COLOR_MIRROR_BOOL);
        bool              original = device_->getBoolProperty(OB_PROP_COLOR_MIRROR_BOOL);
        device_->setBoolProperty(OB_PROP_COLOR_MIRROR_BOOL, !original);
        bool afterSet = device_->getBoolProperty(OB_PROP_COLOR_MIRROR_BOOL);
        EXPECT_EQ(afterSet, !original);
    }
    if(device_->isPropertySupported(OB_PROP_COLOR_FLIP_BOOL, OB_PERMISSION_READ_WRITE)) {
        BoolPropertyGuard guard(device_, OB_PROP_COLOR_FLIP_BOOL);
        bool              original = device_->getBoolProperty(OB_PROP_COLOR_FLIP_BOOL);
        device_->setBoolProperty(OB_PROP_COLOR_FLIP_BOOL, !original);
        bool afterSet = device_->getBoolProperty(OB_PROP_COLOR_FLIP_BOOL);
        EXPECT_EQ(afterSet, !original);
    }
    if(device_->isPropertySupported(OB_PROP_COLOR_AUTO_EXPOSURE_BOOL, OB_PERMISSION_READ_WRITE)) {
        BoolPropertyGuard guard(device_, OB_PROP_COLOR_AUTO_EXPOSURE_BOOL);
        bool              original = device_->getBoolProperty(OB_PROP_COLOR_AUTO_EXPOSURE_BOOL);
        device_->setBoolProperty(OB_PROP_COLOR_AUTO_EXPOSURE_BOOL, !original);
        bool afterSet = device_->getBoolProperty(OB_PROP_COLOR_AUTO_EXPOSURE_BOOL);
        EXPECT_EQ(afterSet, !original);
    }
    if(device_->isPropertySupported(OB_PROP_COLOR_EXPOSURE_INT, OB_PERMISSION_READ_WRITE)) {
        auto range = device_->getIntPropertyRange(OB_PROP_COLOR_EXPOSURE_INT);
        if(range.min < range.max) {
            IntPropertyGuard guard(device_, OB_PROP_COLOR_EXPOSURE_INT);
            int              original = device_->getIntProperty(OB_PROP_COLOR_EXPOSURE_INT);
            device_->setIntProperty(OB_PROP_COLOR_EXPOSURE_INT, range.max);
            int afterSet = device_->getIntProperty(OB_PROP_COLOR_EXPOSURE_INT);
            EXPECT_EQ(afterSet, range.max);
        }
    }
    if(device_->isPropertySupported(OB_PROP_COLOR_GAIN_INT, OB_PERMISSION_READ_WRITE)) {
        auto range = device_->getIntPropertyRange(OB_PROP_COLOR_GAIN_INT);
        if(range.min < range.max) {
            IntPropertyGuard guard(device_, OB_PROP_COLOR_GAIN_INT);
            int              original = device_->getIntProperty(OB_PROP_COLOR_GAIN_INT);
            device_->setIntProperty(OB_PROP_COLOR_GAIN_INT, range.max);
            int afterSet = device_->getIntProperty(OB_PROP_COLOR_GAIN_INT);
            EXPECT_EQ(afterSet, range.max);
        }
    }
}

TEST_F(TC_CPP_14_Property_HW, TC_CPP_14_10_ir_control) {
    if(device_->isPropertySupported(OB_PROP_IR_MIRROR_BOOL, OB_PERMISSION_READ_WRITE)) {
        BoolPropertyGuard guard(device_, OB_PROP_IR_MIRROR_BOOL);
        bool              original = device_->getBoolProperty(OB_PROP_IR_MIRROR_BOOL);
        device_->setBoolProperty(OB_PROP_IR_MIRROR_BOOL, !original);
        bool afterSet = device_->getBoolProperty(OB_PROP_IR_MIRROR_BOOL);
        EXPECT_EQ(afterSet, !original);
    }
    if(device_->isPropertySupported(OB_PROP_IR_FLIP_BOOL, OB_PERMISSION_READ_WRITE)) {
        BoolPropertyGuard guard(device_, OB_PROP_IR_FLIP_BOOL);
        bool              original = device_->getBoolProperty(OB_PROP_IR_FLIP_BOOL);
        device_->setBoolProperty(OB_PROP_IR_FLIP_BOOL, !original);
        bool afterSet = device_->getBoolProperty(OB_PROP_IR_FLIP_BOOL);
        EXPECT_EQ(afterSet, !original);
    }
    if(device_->isPropertySupported(OB_PROP_IR_EXPOSURE_INT, OB_PERMISSION_READ_WRITE)) {
        auto range = device_->getIntPropertyRange(OB_PROP_IR_EXPOSURE_INT);
        if(range.min < range.max) {
            IntPropertyGuard guard(device_, OB_PROP_IR_EXPOSURE_INT);
            int              original = device_->getIntProperty(OB_PROP_IR_EXPOSURE_INT);
            device_->setIntProperty(OB_PROP_IR_EXPOSURE_INT, range.max);
            int afterSet = device_->getIntProperty(OB_PROP_IR_EXPOSURE_INT);
            EXPECT_EQ(afterSet, range.max);
        }
    }
}

TEST_F(TC_CPP_14_Property_HW, TC_CPP_14_11_device_management) {
    if(device_->isPropertySupported(OB_PROP_INDICATOR_LIGHT_BOOL, OB_PERMISSION_READ_WRITE)) {
        BoolPropertyGuard guard(device_, OB_PROP_INDICATOR_LIGHT_BOOL);
        bool              original = device_->getBoolProperty(OB_PROP_INDICATOR_LIGHT_BOOL);
        device_->setBoolProperty(OB_PROP_INDICATOR_LIGHT_BOOL, !original);
        bool afterSet = device_->getBoolProperty(OB_PROP_INDICATOR_LIGHT_BOOL);
        EXPECT_EQ(afterSet, !original);
    }
    if(device_->isPropertySupported(OB_PROP_FAN_WORK_MODE_INT, OB_PERMISSION_READ_WRITE)) {
        auto range = device_->getIntPropertyRange(OB_PROP_FAN_WORK_MODE_INT);
        if(range.min < range.max) {
            IntPropertyGuard guard(device_, OB_PROP_FAN_WORK_MODE_INT);
            int              original = device_->getIntProperty(OB_PROP_FAN_WORK_MODE_INT);
            device_->setIntProperty(OB_PROP_FAN_WORK_MODE_INT, range.max);
            int afterSet = device_->getIntProperty(OB_PROP_FAN_WORK_MODE_INT);
            EXPECT_EQ(afterSet, range.max);
        }
    }
}

TEST_F(TC_CPP_14_Property_HW, TC_CPP_14_12_timing_sync) {
    auto origConfig = device_->getTimestampResetConfig();

    ScopeGuard restoreConfig([&] {
        try {
            device_->setTimestampResetConfig(origConfig);
        }
        catch(...) {
        }
    });

    OBDeviceTimestampResetConfig testConfig = origConfig;
    testConfig.enable                       = !origConfig.enable;

    ASSERT_NO_THROW(device_->setTimestampResetConfig(testConfig));
    auto readBack = device_->getTimestampResetConfig();
    EXPECT_EQ(readBack.enable, testConfig.enable);

    try {
        device_->timestampReset();
    }
    catch(const ob::Error &e) {
        GTEST_SKIP() << "timestampReset not supported: " << e.what();
    }
}

TEST_F(TC_CPP_14_Property_HW, TC_CPP_14_13_hdr_interleave) {
    if(device_->isPropertySupported(OB_PROP_HDR_MERGE_BOOL, OB_PERMISSION_READ_WRITE)) {
        BoolPropertyGuard guard(device_, OB_PROP_HDR_MERGE_BOOL);
        bool              original = device_->getBoolProperty(OB_PROP_HDR_MERGE_BOOL);
        device_->setBoolProperty(OB_PROP_HDR_MERGE_BOOL, !original);
        bool afterSet = device_->getBoolProperty(OB_PROP_HDR_MERGE_BOOL);
        EXPECT_EQ(afterSet, !original);
    }
    if(device_->isPropertySupported(OB_PROP_FRAME_INTERLEAVE_ENABLE_BOOL, OB_PERMISSION_READ_WRITE)) {
        BoolPropertyGuard guard(device_, OB_PROP_FRAME_INTERLEAVE_ENABLE_BOOL);
        bool              original = device_->getBoolProperty(OB_PROP_FRAME_INTERLEAVE_ENABLE_BOOL);
        device_->setBoolProperty(OB_PROP_FRAME_INTERLEAVE_ENABLE_BOOL, !original);
        bool afterSet = device_->getBoolProperty(OB_PROP_FRAME_INTERLEAVE_ENABLE_BOOL);
        EXPECT_EQ(afterSet, !original);
    }
}

TEST_F(TC_CPP_14_Property_HW, TC_CPP_14_14_structured_read_group) {
    int successCount = 0;

    if(device_->isPropertySupported(OB_STRUCT_DEVICE_TEMPERATURE, OB_PERMISSION_READ)) {
        OBDeviceTemperature temp     = {};
        uint32_t            dataSize = static_cast<uint32_t>(sizeof(temp));
        ASSERT_NO_THROW(device_->getStructuredData(OB_STRUCT_DEVICE_TEMPERATURE, reinterpret_cast<uint8_t *>(&temp), &dataSize));
        EXPECT_GE(temp.chipTopTemp, 0.0f);
        EXPECT_LE(temp.chipTopTemp, 80.0f);
        successCount++;
    }

    if(device_->isPropertySupported(OB_STRUCT_BASELINE_CALIBRATION_PARAM, OB_PERMISSION_READ)) {
        OBBaselineCalibrationParam param    = {};
        uint32_t                   dataSize = static_cast<uint32_t>(sizeof(param));
        ASSERT_NO_THROW(device_->getStructuredData(OB_STRUCT_BASELINE_CALIBRATION_PARAM, reinterpret_cast<uint8_t *>(&param), &dataSize));
        EXPECT_GT(param.baseline, 0.0f);
        EXPECT_GE(param.zpd, 0.0f);
        successCount++;
    }

    if(device_->isPropertySupported(OB_STRUCT_MULTI_DEVICE_SYNC_CONFIG, OB_PERMISSION_READ)) {
        OBMultiDeviceSyncConfig config   = {};
        uint32_t                dataSize = sizeof(config);
        try {
            device_->getStructuredData(OB_STRUCT_MULTI_DEVICE_SYNC_CONFIG, reinterpret_cast<uint8_t *>(&config), &dataSize);
            EXPECT_GE(config.syncMode, 0);
            successCount++;
        }
        catch(const ob::Error &) {
        }
    }

    if(device_->isPropertySupported(OB_STRUCT_DEVICE_SERIAL_NUMBER, OB_PERMISSION_READ)) {
        OBSerialNumber serial   = {};
        uint32_t       dataSize = sizeof(serial);
        ASSERT_NO_THROW(device_->getStructuredData(OB_STRUCT_DEVICE_SERIAL_NUMBER, reinterpret_cast<uint8_t *>(&serial), &dataSize));
        EXPECT_GT(std::strlen(serial.numberStr), 0u);
        successCount++;
    }

    if(successCount == 0) {
        GTEST_SKIP() << "No structured data properties are supported for reading";
    }
}

TEST_F(TC_CPP_14_Property_HW, TC_CPP_14_15_sdk_level_property) {
    struct BoolPropertyCase {
        OBPropertyID id;
        const char  *name;
    };

    struct IntPropertyCase {
        OBPropertyID id;
        const char  *name;
    };

    const std::vector<BoolPropertyCase> boolProperties = {
        { OB_PROP_HEARTBEAT_BOOL, "OB_PROP_HEARTBEAT_BOOL" },
        { OB_PROP_WATCHDOG_BOOL, "OB_PROP_WATCHDOG_BOOL" },
        { OB_PROP_EXTERNAL_SIGNAL_RESET_BOOL, "OB_PROP_EXTERNAL_SIGNAL_RESET_BOOL" },
        { OB_PROP_TIMER_RESET_TRIGGER_OUT_ENABLE_BOOL, "OB_PROP_TIMER_RESET_TRIGGER_OUT_ENABLE_BOOL" },
    };

    const std::vector<IntPropertyCase> intProperties = {
        { OB_PROP_DEVICE_COMMUNICATION_TYPE_INT, "OB_PROP_DEVICE_COMMUNICATION_TYPE_INT" },
        { OB_PROP_DEVICE_WORK_MODE_INT, "OB_PROP_DEVICE_WORK_MODE_INT" },
        { OB_PROP_USB_POWER_STATE_INT, "OB_PROP_USB_POWER_STATE_INT" },
    };

    int readablePropertyCount = 0;

    for(const auto &property: boolProperties) {
        if(!device_->isPropertySupported(property.id, OB_PERMISSION_READ)) {
            continue;
        }

        EXPECT_NO_THROW({ (void)device_->getBoolProperty(property.id); }) << "Failed to read " << property.name;
        ++readablePropertyCount;
    }

    for(const auto &property: intProperties) {
        if(!device_->isPropertySupported(property.id, OB_PERMISSION_READ)) {
            continue;
        }

        EXPECT_NO_THROW({ (void)device_->getIntProperty(property.id); }) << "Failed to read " << property.name;
        ++readablePropertyCount;
    }

    EXPECT_GT(readablePropertyCount, 0) << "Current device does not expose any readable SDK-level property from the validation set";
}

TEST_F(TC_CPP_14_Property_HW, TC_CPP_14_16_unsupported_property_safe) {
    bool supported = device_->isPropertySupported(static_cast<OBPropertyID>(99999), OB_PERMISSION_READ);
    EXPECT_FALSE(supported);

    if(!supported) {
        EXPECT_THROW(device_->getIntProperty(static_cast<OBPropertyID>(99999)), ob::Error);
    }
}

TEST_F(TC_CPP_14_Property_HW, TC_CPP_14_17_out_of_range_safe) {
    if(device_->isPropertySupported(OB_PROP_DEPTH_EXPOSURE_INT, OB_PERMISSION_WRITE)) {
        IntPropertyGuard guard(device_, OB_PROP_DEPTH_EXPOSURE_INT);
        auto             range = device_->getIntPropertyRange(OB_PROP_DEPTH_EXPOSURE_INT);
        ASSERT_NO_THROW(device_->setIntProperty(OB_PROP_DEPTH_EXPOSURE_INT, range.max + 10000));

        ASSERT_NO_THROW(device_->setIntProperty(OB_PROP_DEPTH_EXPOSURE_INT, range.min - 10000));
    }
}
