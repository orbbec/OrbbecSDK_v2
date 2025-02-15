// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "../src/DeviceResource.hpp"

// The start time of each Config, default is 5 minutes
#define RECONDING_TIME_SECONDS 60 * 5  // 5 minutes

std::vector<std::function<std::string(std::shared_ptr<DeviceResource>&)>> updateConfigHandlers_ = {
    // depth, color, ir
    [](std::shared_ptr<DeviceResource>& deviceResource) -> std::string {
        std::string msg = "Enable depth, color, ir";
        std::cout << msg << std::endl;

        auto config = std::make_shared<ob::Config>();
        config->enableVideoStream(OB_SENSOR_DEPTH, OB_WIDTH_ANY, OB_HEIGHT_ANY, OB_FPS_ANY, OB_FORMAT_Y16);
        config->enableVideoStream(OB_SENSOR_COLOR, OB_WIDTH_ANY, OB_HEIGHT_ANY, OB_FPS_ANY, OB_FORMAT_RGB);
        config->enableVideoStream(OB_SENSOR_IR_LEFT, OB_WIDTH_ANY, OB_HEIGHT_ANY, OB_FPS_ANY, OB_FORMAT_Y8);
        config->enableVideoStream(OB_SENSOR_IR_RIGHT, OB_WIDTH_ANY, OB_HEIGHT_ANY, OB_FPS_ANY, OB_FORMAT_Y8);

        deviceResource->startStream(config);
        return msg;
    },
    // software d2c
    [](std::shared_ptr<DeviceResource>& deviceResource) -> std::string {
        std::string msg = "Enable software d2c";
        std::cout << msg << std::endl;

        // Enable hardware noise remove filter
        // Hardware noise remove filter needs the lastest firmware version
        deviceResource->enableHwNoiseRemoveFilter(true);
        deviceResource->enableAlignFilter(true);

        auto config = std::make_shared<ob::Config>();
        config->enableVideoStream(OB_SENSOR_DEPTH, OB_WIDTH_ANY, OB_HEIGHT_ANY, OB_FPS_ANY, OB_FORMAT_Y16);
        config->enableVideoStream(OB_SENSOR_COLOR, OB_WIDTH_ANY, OB_HEIGHT_ANY, OB_FPS_ANY, OB_FORMAT_RGB);
        config->setFrameAggregateOutputMode(OB_FRAME_AGGREGATE_OUTPUT_ALL_TYPE_FRAME_REQUIRE);

        deviceResource->startStream(config);
        return msg;
    },
    // point cloud
    [](std::shared_ptr<DeviceResource>& deviceResource) -> std::string {
        std::string msg = "Enable point cloud";
        std::cout << msg << std::endl;

        deviceResource->enableHwNoiseRemoveFilter(true);
        deviceResource->enablePointCloudFilter(true);

        auto config = std::make_shared<ob::Config>();
        config->enableVideoStream(OB_SENSOR_DEPTH, OB_WIDTH_ANY, OB_HEIGHT_ANY, OB_FPS_ANY, OB_FORMAT_Y16);
        config->enableVideoStream(OB_SENSOR_COLOR, OB_WIDTH_ANY, OB_HEIGHT_ANY, OB_FPS_ANY, OB_FORMAT_RGB);
        config->setFrameAggregateOutputMode(OB_FRAME_AGGREGATE_OUTPUT_ALL_TYPE_FRAME_REQUIRE);

        deviceResource->startStream(config);
        return msg;
    },
    // rgb point cloud
    [](std::shared_ptr<DeviceResource>& deviceResource) -> std::string {
        std::string msg = "Enable rgb point cloud";
        std::cout << msg << std::endl;
        
        deviceResource->enableHwNoiseRemoveFilter(true);
        deviceResource->enableRGBPointCloudFilter(true);

        auto config = std::make_shared<ob::Config>();
        config->enableVideoStream(OB_SENSOR_DEPTH, OB_WIDTH_ANY, OB_HEIGHT_ANY, OB_FPS_ANY, OB_FORMAT_Y16);
        config->enableVideoStream(OB_SENSOR_COLOR, OB_WIDTH_ANY, OB_HEIGHT_ANY, OB_FPS_ANY, OB_FORMAT_RGB);
        config->setFrameAggregateOutputMode(OB_FRAME_AGGREGATE_OUTPUT_ALL_TYPE_FRAME_REQUIRE);

        deviceResource->startStream(config);
        return msg;
    },
};