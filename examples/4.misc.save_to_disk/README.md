# C++ Sample: 4.misc.save_to_disk

## Overview

This sample demonstrates how to configure a pipeline to capture color and depth frames, perform necessary format conversions, and save the first 5 valid frames to disk in PNG format. The program discards initial frames to ensure stable stream capture and handles color format conversion for compatibility.

### Knowledge

Pipeline is a pipeline for processing data streams, providing multi-channel stream configuration, switching, frame aggregation, and frame synchronization functions.

Frameset is a combination of different types of Frames.

Metadata is used to describe the various properties and states of a frame.

## Code overview

1. Pipeline Configuration and Initialization.

    ```cpp
    // Create a pipeline.
    std::shared_ptr<ob::Pipeline> pipeline = std::make_shared<ob::Pipeline>();

    // Create a config and enable the depth and color streams.
    std::shared_ptr<ob::Config> config = std::make_shared<ob::Config>();
    config->enableStream(OB_STREAM_COLOR);
    config->enableStream(OB_STREAM_DEPTH);
    // Set the frame aggregate output mode to all type frame require.
    config->setFrameAggregateOutputMode(OB_FRAME_AGGREGATE_OUTPUT_ALL_TYPE_FRAME_REQUIRE);
    ```

2. Get frameSet from pipeline.

    ```cpp
    // Wait for frameSet from the pipeline, the default timeout is 1000ms.
    auto frameSet   = pipe.waitForFrameset();
    ```

3. Convert color images to RGB format and save them.

    ```cpp
    // Get the depth and color frames.
    auto depthFrame = frameSet->getFrame(OB_FRAME_DEPTH)->as<ob::DepthFrame>();
    auto colorFrame = frameSet->getFrame(OB_FRAME_COLOR)->as<ob::ColorFrame>();

    // Convert the color frame to RGB format.
    if(colorFrame->format() != OB_FORMAT_RGB) {
        if(colorFrame->format() == OB_FORMAT_MJPG) {
            formatConverter->setFormatConvertType(FORMAT_MJPG_TO_RGB);
        }
        else if(colorFrame->format() == OB_FORMAT_UYVY) {
            formatConverter->setFormatConvertType(FORMAT_UYVY_TO_RGB);
        }
        else if(colorFrame->format() == OB_FORMAT_YUYV) {
            formatConverter->setFormatConvertType(FORMAT_YUYV_TO_RGB);
        }
        else {
            std::cout << "Color format is not support!" << std::endl;
            continue;
        }
        colorFrame = formatConverter->process(colorFrame)->as<ob::ColorFrame>();
    }
    // Processed the color frames to BGR format, use OpenCV to save to disk.
    formatConverter->setFormatConvertType(FORMAT_RGB_TO_BGR);
    colorFrame = formatConverter->process(colorFrame)->as<ob::ColorFrame>();
    ```

4. Save the first 5 valid frames to disk in PNG format.

    ```cpp
    void saveDepthFrame(const std::shared_ptr<ob::DepthFrame> depthFrame, const uint32_t frameIndex) {
        std::vector<int> params;
        params.push_back(cv::IMWRITE_PNG_COMPRESSION);
        params.push_back(0);
        params.push_back(cv::IMWRITE_PNG_STRATEGY);
        params.push_back(cv::IMWRITE_PNG_STRATEGY_DEFAULT);
        std::string depthName = "Depth_" + std::to_string(depthFrame->width()) + "x" + std::to_string(depthFrame->height()) + "_" + std::to_string(frameIndex) + "_"
                                + std::to_string(depthFrame->timeStamp()) + "ms.png";
        cv::Mat depthMat(depthFrame->height(), depthFrame->width(), CV_16UC1, depthFrame->data());
        cv::imwrite(depthName, depthMat, params);
        std::cout << "Depth saved:" << depthName << std::endl;
    }

    void saveColorFrame(const std::shared_ptr<ob::ColorFrame> colorFrame, const uint32_t frameIndex) {
        std::vector<int> params;
        params.push_back(cv::IMWRITE_PNG_COMPRESSION);
        params.push_back(0);
        params.push_back(cv::IMWRITE_PNG_STRATEGY);
        params.push_back(cv::IMWRITE_PNG_STRATEGY_DEFAULT);
        std::string colorName = "Color_" + std::to_string(colorFrame->width()) + "x" + std::to_string(colorFrame->height()) + "_" + std::to_string(frameIndex) + "_"
                                + std::to_string(colorFrame->timeStamp()) + "ms.png";
        cv::Mat depthMat(colorFrame->height(), colorFrame->width(), CV_8UC3, colorFrame->data());
        cv::imwrite(colorName, depthMat, params);
        std::cout << "Depth saved:" << colorName << std::endl;
    }
    ```

## Run Sample

Press the Any key in the window to exit the program.