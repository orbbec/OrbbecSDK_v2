// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "../src/DeviceResource.hpp"

// The start time of each Config, default is 5 minutes
#define RECONDING_TIME_SECONDS 60 * 5  // 5 minutes

std::vector<std::function<std::string(std::shared_ptr<DeviceResource> &)>> updateConfigHandlers_ = {
    // enhanced depth filter + software d2c, color 1280x720 RGB, depth 848x480 Y16, 30 fps
    [](std::shared_ptr<DeviceResource> &deviceResource) -> std::string {
        std::string msg = "Enable enhanced depth filter + software d2c, color[1280x720 RGB], depth[848x480 Y16], 30fps";
        std::cout << msg << std::endl;

        deviceResource->enableHwNoiseRemoveFilter(false);
        deviceResource->enableSwNoiseRemoveFilter(false);
        deviceResource->enableEnhancedDepthFilter(true, 1280, 720);
        deviceResource->enableAlignFilter(true);

        auto config = std::make_shared<ob::Config>();
        config->enableVideoStream(OB_SENSOR_DEPTH, 848, 480, 30, OB_FORMAT_Y16);
        config->enableVideoStream(OB_SENSOR_COLOR, 1280, 720, 30, OB_FORMAT_RGB);
        config->setFrameAggregateOutputMode(OB_FRAME_AGGREGATE_OUTPUT_ALL_TYPE_FRAME_REQUIRE);
        deviceResource->startStream(config);
        return msg;
    },
    // enhanced depth filter + hardware d2c, color 1280x720 RGB, depth 848x480 Y16, 30 fps
    [](std::shared_ptr<DeviceResource> &deviceResource) -> std::string {
        std::string msg = "Enable enhanced depth filter + hardware d2c, color[1280x720 RGB], depth[848x480 Y16], 30fps";
        std::cout << msg << std::endl;

        deviceResource->enableHwNoiseRemoveFilter(false);
        deviceResource->enableSwNoiseRemoveFilter(false);
        deviceResource->enableEnhancedDepthFilter(true, 1280, 720);

        auto config = std::make_shared<ob::Config>();
        config->enableVideoStream(OB_SENSOR_DEPTH, 848, 480, 30, OB_FORMAT_Y16);
        config->enableVideoStream(OB_SENSOR_COLOR, 1280, 720, 30, OB_FORMAT_RGB);
        config->setFrameAggregateOutputMode(OB_FRAME_AGGREGATE_OUTPUT_ALL_TYPE_FRAME_REQUIRE);
        config->setAlignMode(ALIGN_D2C_HW_MODE);
        deviceResource->startStream(config);
        return msg;
    },
    // enhanced depth filter + software d2c, color 640x480 RGB, depth 640x480 Y16, 30 fps
    [](std::shared_ptr<DeviceResource> &deviceResource) -> std::string {
        std::string msg = "Enable enhanced depth filter + software d2c, color[640x480 RGB], depth[640x480 Y16], 30fps";
        std::cout << msg << std::endl;

        deviceResource->enableHwNoiseRemoveFilter(false);
        deviceResource->enableSwNoiseRemoveFilter(false);
        deviceResource->enableEnhancedDepthFilter(true, 640, 480);
        deviceResource->enableAlignFilter(true);

        auto config = std::make_shared<ob::Config>();
        config->enableVideoStream(OB_SENSOR_DEPTH, 640, 480, 30, OB_FORMAT_Y16);
        config->enableVideoStream(OB_SENSOR_COLOR, 640, 480, 30, OB_FORMAT_RGB);
        config->setFrameAggregateOutputMode(OB_FRAME_AGGREGATE_OUTPUT_ALL_TYPE_FRAME_REQUIRE);
        deviceResource->startStream(config);
        return msg;
    },
    // enhanced depth filter + hardware d2c, color 640x480 RGB, depth 640x480 Y16, 30 fps
    [](std::shared_ptr<DeviceResource> &deviceResource) -> std::string {
        std::string msg = "Enable enhanced depth filter + hardware d2c, color[640x480 RGB], depth[640x480 Y16], 30fps";
        std::cout << msg << std::endl;

        deviceResource->enableHwNoiseRemoveFilter(false);
        deviceResource->enableSwNoiseRemoveFilter(false);
        deviceResource->enableEnhancedDepthFilter(true, 640, 480);

        auto config = std::make_shared<ob::Config>();
        config->enableVideoStream(OB_SENSOR_DEPTH, 640, 480, 30, OB_FORMAT_Y16);
        config->enableVideoStream(OB_SENSOR_COLOR, 640, 480, 30, OB_FORMAT_RGB);
        config->setFrameAggregateOutputMode(OB_FRAME_AGGREGATE_OUTPUT_ALL_TYPE_FRAME_REQUIRE);
        config->setAlignMode(ALIGN_D2C_HW_MODE);
        deviceResource->startStream(config);
        return msg;
    },
    // depth, color, ir
    [](std::shared_ptr<DeviceResource> &deviceResource) -> std::string {
        std::string msg = "Enable depth, color, ir";
        std::cout << msg << std::endl;

        deviceResource->enableHwNoiseRemoveFilter(false);
        deviceResource->enableSwNoiseRemoveFilter(false);

        auto config = std::make_shared<ob::Config>();
        config->enableVideoStream(OB_SENSOR_DEPTH, OB_WIDTH_ANY, OB_HEIGHT_ANY, OB_FPS_ANY, OB_FORMAT_ANY);
        config->enableVideoStream(OB_SENSOR_COLOR, OB_WIDTH_ANY, OB_HEIGHT_ANY, OB_FPS_ANY, OB_FORMAT_ANY);
        config->enableVideoStream(OB_SENSOR_IR_LEFT, OB_WIDTH_ANY, OB_HEIGHT_ANY, OB_FPS_ANY, OB_FORMAT_ANY);
        config->enableVideoStream(OB_SENSOR_IR_RIGHT, OB_WIDTH_ANY, OB_HEIGHT_ANY, OB_FPS_ANY, OB_FORMAT_ANY);

        deviceResource->startStream(config);
        return msg;
    },
    // software d2c
    [](std::shared_ptr<DeviceResource> &deviceResource) -> std::string {
        std::string msg = "Enable software d2c";
        std::cout << msg << std::endl;

        deviceResource->enableHwNoiseRemoveFilter(false);
        deviceResource->enableSwNoiseRemoveFilter(false);
        deviceResource->enableAlignFilter(true);

        auto config = std::make_shared<ob::Config>();
        config->enableVideoStream(OB_SENSOR_DEPTH, OB_WIDTH_ANY, OB_HEIGHT_ANY, OB_FPS_ANY, OB_FORMAT_ANY);
        config->enableVideoStream(OB_SENSOR_COLOR, OB_WIDTH_ANY, OB_HEIGHT_ANY, OB_FPS_ANY, OB_FORMAT_ANY);
        config->setFrameAggregateOutputMode(OB_FRAME_AGGREGATE_OUTPUT_ALL_TYPE_FRAME_REQUIRE);

        deviceResource->startStream(config);
        return msg;
    },
    // point cloud
    [](std::shared_ptr<DeviceResource> &deviceResource) -> std::string {
        std::string msg = "Enable point cloud";
        std::cout << msg << std::endl;

        deviceResource->enableHwNoiseRemoveFilter(false);
        deviceResource->enableSwNoiseRemoveFilter(false);
        deviceResource->enablePointCloudFilter(true);

        auto config = std::make_shared<ob::Config>();
        config->enableVideoStream(OB_SENSOR_DEPTH, OB_WIDTH_ANY, OB_HEIGHT_ANY, OB_FPS_ANY, OB_FORMAT_ANY);
        config->enableVideoStream(OB_SENSOR_COLOR, OB_WIDTH_ANY, OB_HEIGHT_ANY, OB_FPS_ANY, OB_FORMAT_ANY);
        config->setFrameAggregateOutputMode(OB_FRAME_AGGREGATE_OUTPUT_ALL_TYPE_FRAME_REQUIRE);

        deviceResource->startStream(config);
        return msg;
    },
    // rgb point cloud
    [](std::shared_ptr<DeviceResource> &deviceResource) -> std::string {
        std::string msg = "Enable rgb point cloud";
        std::cout << msg << std::endl;

        deviceResource->enableHwNoiseRemoveFilter(false);
        deviceResource->enableSwNoiseRemoveFilter(false);
        deviceResource->enableRGBPointCloudFilter(true);

        auto config = std::make_shared<ob::Config>();
        config->enableVideoStream(OB_SENSOR_DEPTH, OB_WIDTH_ANY, OB_HEIGHT_ANY, OB_FPS_ANY, OB_FORMAT_ANY);
        config->enableVideoStream(OB_SENSOR_COLOR, OB_WIDTH_ANY, OB_HEIGHT_ANY, OB_FPS_ANY, OB_FORMAT_ANY);
        config->setFrameAggregateOutputMode(OB_FRAME_AGGREGATE_OUTPUT_ALL_TYPE_FRAME_REQUIRE);

        deviceResource->startStream(config);
        return msg;
    },
};