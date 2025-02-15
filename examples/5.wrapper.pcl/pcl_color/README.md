# C++ Sample: 5.wrapper.pcl_color

## Overview

Use OrbbecSDK to acquire point cloud data and convert the acquired point cloud data to PCL library point cloud data


## Code overview

1. Create an ob::Pipeline object, and get frameSet from pipeline..

    ```cpp
    // create pipeline to manage the streams
    auto pipeline = std::make_shared<ob::Pipeline>();

    // Enable frame synchronization to ensure depth frame and color frame on output frameset are synchronized.
    pipeline->enableFrameSync();

    // Start pipeline with config
    pipeline->start(config);

    // Wait for frames to arrive
    auto frameset   = pipeline->waitForFrames();
    auto colorFrame = frameset->getFrame(OB_FRAME_COLOR);
    ```

2. Create a PointCloud object and convert the depth frame to point cloud data.

    ```cpp
    // Create a point cloud Filter, which will be used to generate pointcloud frame from depth and color frames.
    auto pointCloudFilter = std::make_shared<ob::PointCloudFilter>();
    // Create a align filter, which will be used to align depth frame to color frame.
    auto alignFilter      = std::make_shared<ob::Align>(OB_STREAM_COLOR);

    // Align depth frame to color frame.
    auto aliggnFrameset = alignFilter->process(frameset);

    // set to create RGBD point cloud format (will be effective only if color frame and depth frame are contained in the frameset)
    pointCloudFilter->setCreatePointFormat(OB_FORMAT_RGB_POINT);
    auto result = pointCloudFilter->process(aliggnFrameset);
   ```

3. Convert the point cloud data to PCL library point cloud data.

    ```cpp
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
    ```

4. Render the point cloud data using PCL library.

    ```cpp
    void loadPCDFile() {
        // Generate object to store cloud in .pcd file
        pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloudView(new pcl::PointCloud<pcl::PointXYZRGB>);

        pcl::io::loadPCDFile("./output.pcd", *cloudView);  // Load .pcd File

        std::shared_ptr<pcl::visualization::PCLVisualizer> viewer(new pcl::visualization::PCLVisualizer("Captured Frame"));

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
    ```

## Run Sample

Press the q or Q key in the window to exit the program.

### Result

![image](../../../docs/resource/pcl_color.jpg)
