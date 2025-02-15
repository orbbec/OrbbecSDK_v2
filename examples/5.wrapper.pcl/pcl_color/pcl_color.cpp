// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include <libobsensor/ObSensor.hpp>

#include "utils.hpp"

#include <pcl/io/pcd_io.h>
#include <pcl/point_types.h>
#include <pcl/point_types.h>
#include <pcl/point_cloud.h>
#include <pcl/filters/passthrough.h>
#include <pcl/visualization/cloud_viewer.h>
#include <pcl/visualization/pcl_visualizer.h>

#include <thread>
#include <fstream>
#include <iostream>

#include <cmath>

pcl::PointCloud<pcl::PointXYZRGB>::Ptr frameToPCL(std::shared_ptr<ob::Frame> pointsFrame, std::shared_ptr<ob::Frame> colorFrame) {
    pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud(new pcl::PointCloud<pcl::PointXYZRGB>);

    uint32_t      pointsSize = pointsFrame->dataSize() / sizeof(OBColorPoint);
    OBColorPoint *point      = (OBColorPoint *)pointsFrame->data();

    cloud->width  = colorFrame->as<ob::VideoFrame>()->width();
    cloud->height = colorFrame->as<ob::VideoFrame>()->height();
    cloud->points.resize(pointsSize);

    for(uint32_t i = 0; i < pointsSize; i++) {
        cloud->points[i].x = point->x;
        cloud->points[i].y = point->y;
        cloud->points[i].z = point->z;
        cloud->points[i].r = point->r;
        cloud->points[i].g = point->g;
        cloud->points[i].b = point->b;

        point++;
    }

    return cloud;
}

void loadPCDFile() {
    // Generate object to store cloud in .pcd file
    pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloudView(new pcl::PointCloud<pcl::PointXYZRGB>);

    pcl::io::loadPCDFile("./output.pcd", *cloudView);  // Load .pcd File

    std::shared_ptr<pcl::visualization::PCLVisualizer> viewer(new pcl::visualization::PCLVisualizer("RGB Cloud"));

    // Set background of viewer to black
    viewer->setBackgroundColor(0, 0, 0);
    // Add generated point cloud and identify with string "Cloud"
    viewer->addPointCloud<pcl::PointXYZRGB>(cloudView, "Cloud");
    // Default size for rendered points
    viewer->setPointCloudRenderingProperties(pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 1, "Cloud");
    // Viewer Properties
    viewer->initCameraParameters();  // Camera Parameters for ease of viewing

    viewer->spin();  // Allow user to rotate point cloud and view it
}

int main(void) try {
    auto config = std::make_shared<ob::Config>();

    // enable depth and color streams with specified format
    config->enableVideoStream(OB_STREAM_DEPTH, OB_WIDTH_ANY, OB_HEIGHT_ANY, OB_FPS_ANY, OB_FORMAT_Y16);
    config->enableVideoStream(OB_STREAM_COLOR, OB_WIDTH_ANY, OB_HEIGHT_ANY, OB_FPS_ANY, OB_FORMAT_RGB);

    // set frame aggregate output mode to all type frame require. therefor, the output frameset will contain all type of frames
    config->setFrameAggregateOutputMode(OB_FRAME_AGGREGATE_OUTPUT_ALL_TYPE_FRAME_REQUIRE);

    // create pipeline to manage the streams
    auto pipeline = std::make_shared<ob::Pipeline>();

    // Enable frame synchronization to ensure depth frame and color frame on output frameset are synchronized.
    pipeline->enableFrameSync();

    // Start pipeline with config
    pipeline->start(config);

    // Drop several frames for auto-exposure
    for(int i = 0; i < 10; ++i) {
        auto lost = pipeline->waitForFrames();
    }

    // Wait for frames to arrive
    std::cout << "Waiting for frames..." << std::endl;
    auto frameset   = pipeline->waitForFrames();
    std::cout << "Frames arrive!" << std::endl << std::endl;
    auto colorFrame = frameset->getFrame(OB_FRAME_COLOR);

    pipeline->stop();

    std::cout << "Wait for PointCloud Filter to generate PointCloud Frame..." << std::endl;
    // Create a point cloud Filter, which will be used to generate pointcloud frame from depth and color frames.
    auto pointCloudFilter = std::make_shared<ob::PointCloudFilter>();
    // Create a align filter, which will be used to align depth frame to color frame.
    auto alignFilter      = std::make_shared<ob::Align>(OB_STREAM_COLOR);

    // Align depth frame to color frame.
    auto aliggnFrameset = alignFilter->process(frameset);

    // set to create RGBD point cloud format (will be effective only if color frame and depth frame are contained in the frameset)
    pointCloudFilter->setCreatePointFormat(OB_FORMAT_RGB_POINT);
    auto result = pointCloudFilter->process(aliggnFrameset);

    auto pclPoints = frameToPCL(result, colorFrame);
    std::cout << "PointCloud Filter generated PointCloud Frame!" << std::endl << std::endl;

    std::cout << "It may cost some times to save and load the point cloud, please wait..." << std::endl;
    // Save generated point cloud to .pcd file
    pcl::io::savePCDFileASCII("./output.pcd", *pclPoints);

    // Load generated point cloud from .pcd file
    loadPCDFile();

    // 3D Point Cloud Visualization using PCLVisualizer
    //pcl::visualization::PCLVisualizer vis2("Color Point Cloud");
    //vis2.addPointCloud(pclPoints);
    //vis2.spin();

    exit(EXIT_SUCCESS);
}
catch(ob::Error &e) {
    std::cerr << "function:" << e.getFunction() << "\nargs:" << e.getArgs() << "\nmessage:" << e.what() << "\ntype:" << e.getExceptionType() << std::endl;
    std::cout << "\nPress any key to exit.";
    ob_smpl::waitForKeyPressed();
    exit(EXIT_FAILURE);
}