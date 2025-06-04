// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include <libobsensor/ObSensor.hpp>
#include "libobsensor/hpp/Utils.hpp"
#include "utils.hpp"

#include <fstream>
#include <iostream>

#define KEY_ESC 27
#define KEY_R 82
#define KEY_r 114

int main(void) try {

    // create config to configure the pipeline streams
    auto config = std::make_shared<ob::Config>();

    // enable depth and color streams with specified format
    config->enableVideoStream(OB_STREAM_DEPTH, OB_WIDTH_ANY, OB_HEIGHT_ANY, OB_FPS_ANY, OB_FORMAT_ANY);
    config->enableVideoStream(OB_STREAM_COLOR, OB_WIDTH_ANY, OB_HEIGHT_ANY, OB_FPS_ANY, OB_FORMAT_RGB);

    // set frame aggregate output mode to all type frame require. therefor, the output frameset will contain all type of frames
    config->setFrameAggregateOutputMode(OB_FRAME_AGGREGATE_OUTPUT_ALL_TYPE_FRAME_REQUIRE);

    // create pipeline to manage the streams
    auto pipeline = std::make_shared<ob::Pipeline>();

    // Enable frame synchronization to ensure depth frame and color frame on output frameset are synchronized.
    pipeline->enableFrameSync();

    // Start pipeline with config
    pipeline->start(config);

    // Create a point cloud Filter, which will be used to generate pointcloud frame from depth and color frames.
    auto pointCloud = std::make_shared<ob::PointCloudFilter>();

    // Create a Align Filter, which will be used to align depth frame and color frame.
    auto align = std::make_shared<ob::Align>(OB_STREAM_COLOR);  // align depth frame to color frame

    // operation prompt
    std::cout << "Depth and Color stream are started!" << std::endl;
    std::cout << "Press R or r to create RGBD PointCloud and save to ply file! " << std::endl;
    std::cout << "Press D or d to create Depth PointCloud and save to ply file! " << std::endl;
    std::cout << "Press M or m to create RGBD PointCloud and save to Mesh ply file! " << std::endl;
    std::cout << "Press ESC to exit! " << std::endl;

    while(true) {
        auto key = ob_smpl::waitForKeyPressed();
        if(key == 27) {
            break;
        }

        if(key == 'r' || key == 'R') {
            std::cout << "Save RGBD PointCloud to ply file, this will take some time..." << std::endl;

            std::shared_ptr<ob::FrameSet> frameset = nullptr;
            while(true) {
                frameset = pipeline->waitForFrameset(1000);
                if(frameset) {
                    break;
                }
            }

            // align depth frame to color frame
            auto alignedFrameset = align->process(frameset);

            // set to create RGBD point cloud format (will be effective only if color frame and depth frame are contained in the frameset)
            pointCloud->setCreatePointFormat(OB_FORMAT_RGB_POINT);

            // process the frameset to generate point cloud frame
            std::shared_ptr<ob::Frame> frame = pointCloud->process(alignedFrameset);

            // save point cloud data to ply file
            ob::PointCloudHelper::savePointcloudToPly("RGBPoints.ply", frame, false, false, 50);

            std::cout << "RGBPoints.ply Saved" << std::endl;
        }
        else if(key == 'd' || key == 'D') {
            std::cout << "Save Depth PointCloud to ply file, this will take some time..." << std::endl;

            std::shared_ptr<ob::FrameSet> frameset = nullptr;
            while(true) {
                frameset = pipeline->waitForFrameset(1000);
                if(frameset) {
                    break;
                }
            }

            // set to create depth point cloud format
            auto alignedFrameset = align->process(frameset);

            // set to create point cloud format
            pointCloud->setCreatePointFormat(OB_FORMAT_POINT);

            // process the frameset to generate point cloud frame (pass into a single depth frame to process is also valid)
            std::shared_ptr<ob::Frame> frame = pointCloud->process(alignedFrameset);

            // save point cloud data to ply file
            ob::PointCloudHelper::savePointcloudToPly("DepthPoints.ply", frame, false, false, 50);

            std::cout << "DepthPoints.ply Saved" << std::endl;
        }
        else if(key == 'm' || key == 'M') {
            std::cout << "Save RGBD PointCloud(mesh) to ply file, this will take some time..." << std::endl;

            std::shared_ptr<ob::FrameSet> frameset = nullptr;
            while(true) {
                frameset = pipeline->waitForFrameset(1000);
                if(frameset) {
                    break;
                }
            }

            // align depth frame to color frame
            auto alignedFrameset = align->process(frameset);

            // set to create RGBD point cloud format (will be effective only if color frame and depth frame are contained in the frameset)
            pointCloud->setCreatePointFormat(OB_FORMAT_RGB_POINT);

            // process the frameset to generate point cloud frame
            std::shared_ptr<ob::Frame> frame = pointCloud->process(alignedFrameset);

            ob::PointCloudHelper::savePointcloudToPly("ColorMeshPoints.ply", frame, false, true, 50);
            std::cout << "ColorMeshPoints.ply Saved" << std::endl;
        }
    }
    // stop the pipeline
    pipeline->stop();

    return 0;
}
catch(ob::Error &e) {
    std::cerr << "function:" << e.getFunction() << "\nargs:" << e.getArgs() << "\nmessage:" << e.what() << "\ntype:" << e.getExceptionType() << std::endl;
    std::cout << "\nPress any key to exit.";
    ob_smpl::waitForKeyPressed();
    exit(EXIT_FAILURE);
}
