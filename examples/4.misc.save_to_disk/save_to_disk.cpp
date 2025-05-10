// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include <libobsensor/ObSensor.hpp>

#include "utils.hpp"
#include "utils_opencv.hpp"

#include <iostream>

void saveDepthFrame(const std::shared_ptr<ob::DepthFrame> depthFrame, uint32_t frameIndex);
void saveColorFrame(const std::shared_ptr<ob::ColorFrame> colorFrame, uint32_t frameIndex);

int main() try {
    // Create a pipeline.
    std::shared_ptr<ob::Pipeline> pipeline = std::make_shared<ob::Pipeline>();

    // Create a config and enable the depth and color streams.
    std::shared_ptr<ob::Config> config = std::make_shared<ob::Config>();
    config->enableStream(OB_STREAM_COLOR);
    config->enableStream(OB_STREAM_DEPTH);
    // Set the frame aggregate output mode to all type frame require.
    config->setFrameAggregateOutputMode(OB_FRAME_AGGREGATE_OUTPUT_ALL_TYPE_FRAME_REQUIRE);

    uint32_t frameIndex = 0;
    // Create a format converter filter.
    auto formatConverter = std::make_shared<ob::FormatConvertFilter>();

    // Start the pipeline.
    pipeline->start(config);

    // Drop several frames
    for(int i = 0; i < 15; ++i) {
        auto lost = pipeline->waitForFrameset(100);
    }

    while(true) {
        // Wait for frameSet from the pipeline.
        std::shared_ptr<ob::FrameSet> frameSet = pipeline->waitForFrameset(100);

        if(!frameSet) {
            std::cout << "No frames received in 100ms..." << std::endl;
            continue;
        }

        if (++frameIndex >= 5) {
            std::cout << "The demo is over, please press ESC to exit manually!" << std::endl;
            break;
        }

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

        saveDepthFrame(depthFrame, frameIndex);
        saveColorFrame(colorFrame, frameIndex);
    }

    // Stop the Pipeline, no frame data will be generated
    pipeline->stop();

    std::cout << "Press any key to exit." << std::endl;
    ob_smpl::waitForKeyPressed();

    return 0;
}
catch(ob::Error &e) {
    std::cerr << "function:" << e.getFunction() << "\nargs:" << e.getArgs() << "\nmessage:" << e.what() << "\ntype:" << e.getExceptionType() << std::endl;
    std::cout << "\nPress any key to exit.";
    ob_smpl::waitForKeyPressed();
    exit(EXIT_FAILURE);
}

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
    std::cout << "Color saved:" << colorName << std::endl;
}