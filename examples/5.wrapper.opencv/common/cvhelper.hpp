// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include <libobsensor/ObSensor.hpp>
#include <opencv2/opencv.hpp>
#include <memory>
#include <iostream>

class CvHelper {
public:
    CvHelper()  = default;
    ~CvHelper() = default;

public:
    // Helper function to convert depth frame to colorized image
    static cv::Mat colorizeDepth(std::shared_ptr<ob::Frame> depthFrame) {
        cv::Mat rstMat;
        cv::Mat cvtMat;

        // depth frame pixel value multiply scale to get distance in millimeter
        const float scale = depthFrame->as<ob::DepthFrame>()->getValueScale();

        // Get raw depth data from depth frame
        cv::Mat rawMat = frameToMat(depthFrame);

        // normalization to 0-255. 0.032f is 256/8000, to limit the range of depth to 8000mm
        rawMat.convertTo(cvtMat, CV_32F, scale * 0.032f);

        // apply gamma correction to enhance the contrast for near objects
        cv::pow(cvtMat, 0.6f, cvtMat);

        //  convert to 8-bit
        cvtMat.convertTo(cvtMat, CV_8UC1, 10);  // multiplier 10 is to normalize to 0-255 (nearly) after applying gamma correction

        // apply colormap
        cv::applyColorMap(cvtMat, rstMat, cv::COLORMAP_JET);

        return rstMat;
    }

    // Convert frame to cv::Mat
    static cv::Mat frameToMat(std::shared_ptr<ob::Frame> frame) {
        auto           vf     = frame->as<ob::VideoFrame>();
        const uint32_t height = vf->getHeight();
        const uint32_t width  = vf->getWidth();

        if (frame->getFormat() == OB_FORMAT_BGR) {
            return cv::Mat(height, width, CV_8UC3, frame->getData());
        }
        else if(frame->getFormat() == OB_FORMAT_RGB) {
            // The color channel for RGB images in opencv is BGR, so we need to convert it to BGR before returning the cv::Mat
            auto rgbMat = cv::Mat(height, width, CV_8UC3, frame->getData());
            cv::Mat bgrMat;
            cv::cvtColor(rgbMat, bgrMat, cv::COLOR_RGB2BGR);
            return bgrMat;
        }
        else if(frame->getFormat() == OB_FORMAT_Y16) {
            return cv::Mat(height, width, CV_16UC1, frame->getData());
        }

        std::cerr << "Unsupported format: " << frame->getFormat() << std::endl;
        return cv::Mat();
    }
};