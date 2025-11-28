// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include <libobsensor/ObSensor.hpp>
#include <libobsensor/hpp/Utils.hpp>

#include "utils.hpp"

int main(void) try {
    // Create a pipeline.
    ob::Pipeline pipe;

    // Get the device from pipeline.
    auto device = pipe.getDevice();

    // Check LiDAR device
    if(!ob_smpl::isLiDARDevice(device)) {
        std::cout << "Invalid device, please connect a LiDAR device!" << std::endl;
        return -1;
    }

    // Start the pipeline with default config.
    // Modify the default configuration by the configuration file: "*SDKConfig.xml"
    pipe.start();

    std::cout << "LiDAR stream is started!" << std::endl;
    std::cout << "Press R or r to create LiDAR PointCloud and save to ply file! " << std::endl;
    std::cout << "Press ESC to exit! " << std::endl;

    while(true) {
        // Wait for user input
        auto key = ob_smpl::waitForKeyPressed();
        if(key == ESC_KEY) {
            break;
        }

        // Press 'r' or 'R' to save LiDAR point cloud to ply file
        if(key == 'r' || key == 'R') {
            std::cout << "Save LiDAR PointCloud to ply file, this will take some time..." << std::endl;

            // Wait for frameSet from the pipeline, the default timeout is 1000ms.
            std::shared_ptr<ob::FrameSet> frameset = pipe.waitForFrameset();
            if(!frameset) {
                std::cout << "No frame data, please try again!" << std::endl;
                continue;
            }

            // Get LiDAR point cloud frame
            auto frame = frameset->getFrame(OB_FRAME_LIDAR_POINTS);
            if(!frame) {
                std::cout << "No LiDAR frame found!" << std::endl;
                continue;
            }

            // Save point cloud data to ply file
            if(!ob::PointCloudHelper::saveLiDARPointcloudToPly("LiDARPoints.ply", frame->as<ob::LiDARPointsFrame>(), false)) {
                std::cout << "Failed to save LiDARPoints.ply" << std::endl;
                continue;
            }

            std::cout << "LiDARPoints.ply Saved" << std::endl;
        }
    }

    // Stop the Pipeline, no frame data will be generated
    pipe.stop();

    return 0;
}
catch(ob::Error &e) {
    std::cerr << "function:" << e.getFunction() << "\nargs:" << e.getArgs() << "\nmessage:" << e.what() << "\ntype:" << e.getExceptionType() << std::endl;
    std::cout << "\nPress any key to exit.";
    ob_smpl::waitForKeyPressed();
    exit(EXIT_FAILURE);
}
