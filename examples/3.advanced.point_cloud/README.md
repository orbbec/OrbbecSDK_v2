# C++ Sample: 3.advanced.point_clout

## Overview

Connect the device to open the stream, generate a depth point cloud or RGBD point cloud and save it as a ply format file, and exit the program through the ESC\_KEY key

### Knowledge

Pipeline is a pipeline for processing data streams, providing multi-channel stream configuration, switching, frame aggregation, and frame synchronization functions

## Code overview

1. Get the pipeline.

    ```cpp
        // create config to configure the pipeline streams
        auto config = std::make_shared<ob::Config>();

        // enable depth and color streams with specified format
        config->enableVideoStream(OB_STREAM_DEPTH, OB_WIDTH_ANY, OB_HEIGHT_ANY, OB_FPS_ANY, OB_FORMAT_Y16);
        config->enableVideoStream(OB_STREAM_COLOR, OB_WIDTH_ANY, OB_HEIGHT_ANY, OB_FPS_ANY, OB_FORMAT_RGB);

        // set frame aggregate output mode to all type frame require. therefor, the output frameset will contain all type of frames
        config->setFrameAggregateOutputMode(OB_FRAME_AGGREGATE_OUTPUT_ALL_TYPE_FRAME_REQUIRE);

        // create pipeline to manage the streams
        auto pipeline = std::make_shared<ob::Pipeline>();
    ```

2. Enable frame synchronization and start pipeline with config

    ```cpp
        // Start pipeline with config
        pipeline->start(config);

        // Create a point cloud Filter, which will be used to generate pointcloud frame from depth and color frames.
        auto pointCloud = std::make_shared<ob::PointCloudFilter>();
    ```

3. Create a Align Filter.

    ```cpp
        auto align = std::make_shared<ob::Align>(OB_STREAM_COLOR); // align depth frame to color frame
    ```

4. Get the frameset from pipeline.

    ```cpp
    auto frameset = pipeline->waitForFrameset(1000);
    ```

5. Create RGBD PointCloud.

    ```cpp
        // align depth frame to color frame
        auto alignedFrameset = align->process(frameset);

        // set to create RGB point cloud format (will be effective only if color frame and depth frame are contained in the frameset)
        pointCloud->setCreatePointFormat(OB_FORMAT_RGB_POINT);

        // process the frameset to generate point cloud frame
        std::shared_ptr<ob::Frame> frame = pointCloud->process(alignedFrameset);
    ```

6. create Depth PointCloud

    ```cpp
            // set to create depth point cloud format
        auto alignedFrameset = align->process(frameset);

        // set to create point cloud format
        pointCloud->setCreatePointFormat(OB_FORMAT_POINT);

        // process the frameset to generate point cloud frame (pass into a single depth frame to process is also valid)
        std::shared_ptr<ob::Frame> frame = pointCloud->process(alignedFrameset);
    ```

7. Stop pipeline

    ```cpp
    pipeline->stop();
    ```

## Run Sample

Press R or r to create RGBD PointCloud and save to ply file!  
Press D or d to create Depth PointCloud and save to ply file!

Press ESC to exit!

### Result

![image](/docs/resource/point_cloud.png)
