// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include <libobsensor/ObSensor.hpp>

#include "utils.hpp"

#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/filters/passthrough.h>
#include <pcl/visualization/cloud_viewer.h>
#include <pcl/visualization/pcl_visualizer.h>

#include <thread>
#include <fstream>
#include <iostream>
#include <cmath>

pcl::PointCloud<pcl::PointXYZ>::Ptr frameToPCL(std::shared_ptr<ob::Frame> frame) {
   pcl::PointCloud<pcl::PointXYZ>::Ptr cloud(new pcl::PointCloud<pcl::PointXYZ>);

   uint32_t pointsSize = frame->dataSize() / sizeof(OBPoint);
   OBPoint *point      = (OBPoint *)frame->data();

   cloud->points.resize(pointsSize);

   for(uint32_t i = 0; i < pointsSize; i++) {
       cloud->points[i].x = point->x;
       cloud->points[i].y = point->y;
       cloud->points[i].z = point->z;

       point++;
   }

   return cloud;
}

int main(void) try {
    auto config = std::make_shared<ob::Config>();

    // enable depth and color streams with specified format
    config->enableVideoStream(OB_STREAM_DEPTH, OB_WIDTH_ANY, OB_HEIGHT_ANY, OB_FPS_ANY, OB_FORMAT_Y16);

    // create pipeline to manage the streams
    auto pipeline = std::make_shared<ob::Pipeline>();

    // Start pipeline with config
    pipeline->start(config);

    // Wait for frames to be available
    std::cout << "Waiting for frames arrive..." << std::endl;
    auto frameset    = pipeline->waitForFrames();
    std::cout << "Frames arrive!" << std::endl;

    auto depthFrame = frameset->getFrame(OB_FRAME_DEPTH);

    pipeline->stop();

    // Create a point cloud Filter, which will be used to generate pointcloud frame from depth and color frames.
    auto pointCloudFilter = std::make_shared<ob::PointCloudFilter>();

    // Set the format of the point cloud to be generated.
    pointCloudFilter->setCreatePointFormat(OB_FORMAT_POINT);
    auto result = pointCloudFilter->process(depthFrame);

    // Convert the result frame to pcl point cloud
    auto pclPoint = frameToPCL(result);

    // Using PCL library to visualize the point cloud.
    pcl::visualization::PCLVisualizer vis2("Depth Cloud");
    vis2.addPointCloud(pclPoint);
    vis2.spin();

    exit(EXIT_SUCCESS);
}
catch(ob::Error &e) {
   std::cerr << "function:" << e.getFunction() << "\nargs:" << e.getArgs() << "\nmessage:" << e.what() << "\ntype:" << e.getExceptionType() << std::endl;
   std::cout << "\nPress any key to exit.";
   ob_smpl::waitForKeyPressed();
   exit(EXIT_FAILURE);
}