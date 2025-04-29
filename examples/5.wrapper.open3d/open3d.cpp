// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include <libobsensor/ObSensor.hpp>

#include "utils.hpp"
#include "open3d/Open3D.h"

#include <string>
#include <vector>
#include <iostream>
#include <memory>
#include <algorithm>

using namespace open3d;
using legacyImage = open3d::geometry::Image;

bool windowsInited = false;
bool isRunning     = true;

int main(void) try {
    // Create a PipeLine with default device.
    auto pipeline = std::make_shared<ob::Pipeline>();

    // Configure which streams to enable or disable for the Pipeline by creating a Config.
    std::shared_ptr<ob::Config> config = std::make_shared<ob::Config>();

    config->enableVideoStream(OB_STREAM_COLOR, OB_WIDTH_ANY, OB_HEIGHT_ANY, OB_FPS_ANY, OB_FORMAT_RGB);
    config->enableVideoStream(OB_STREAM_DEPTH, OB_WIDTH_ANY, OB_HEIGHT_ANY, OB_FPS_ANY, OB_FORMAT_Y16);
    config->setFrameAggregateOutputMode(OB_FRAME_AGGREGATE_OUTPUT_ALL_TYPE_FRAME_REQUIRE);
    pipeline->enableFrameSync();

    // Start the pipeline with config.
    pipeline->start(config);

    open3d::visualization::VisualizerWithKeyCallback colorVis, depthVis;

    colorVis.RegisterKeyCallback(GLFW_KEY_ESCAPE, [&](visualization::Visualizer *vis) {
        (void)vis;
        isRunning = false;
        return true;
    });
    depthVis.RegisterKeyCallback(GLFW_KEY_ESCAPE, [&](visualization::Visualizer *vis) {
        (void)vis;
        isRunning = false;
        return true;
    });

    while(isRunning) {
        // Wait for a frameset to be available.
        auto frameset = pipeline->waitForFrames();
        if(!frameset) {
            continue;
        }

        // Get the color and depth frames from the frameset.
        auto colorFrame = frameset->getFrame(OB_FRAME_COLOR)->as<ob::ColorFrame>();
        auto depthFrame = frameset->getFrame(OB_FRAME_DEPTH)->as<ob::DepthFrame>();

        // Create an RGBD image in Open3D tensor format.
        std::shared_ptr<t::geometry::RGBDImage> preRgbd = std::make_shared<t::geometry::RGBDImage>();

        preRgbd->color_ =
            core::Tensor(static_cast<const uint8_t *>(colorFrame->getData()), { colorFrame->getHeight(), colorFrame->getWidth(), 3 }, core::Dtype::UInt8);
        preRgbd->depth_ =
            core::Tensor(reinterpret_cast<const uint16_t *>(depthFrame->getData()), { depthFrame->getHeight(), depthFrame->getWidth() }, core::Dtype::UInt16);

        // Convert the RGBD image to legacy Open3D image format.
        auto imRgbd = preRgbd->ToLegacy();

        auto colorImage = std::shared_ptr<legacyImage>(&imRgbd.color_, [](legacyImage *) {});
        auto depthImage = std::shared_ptr<legacyImage>(&imRgbd.depth_, [](legacyImage *) {});

        if(!windowsInited) {
            if(!colorVis.CreateVisualizerWindow("Color", 1280, 720) || !colorVis.AddGeometry(colorImage)) {
                return 0;
            }

            if(!depthVis.CreateVisualizerWindow("Depth", 1280, 720) || !depthVis.AddGeometry(depthImage)) {
                return 0;
            }
            windowsInited = true;
        }
        else {
            colorVis.UpdateGeometry(colorImage);
            depthVis.UpdateGeometry(depthImage);
        }
        depthVis.PollEvents();
        depthVis.UpdateRender();

        colorVis.PollEvents();
        colorVis.UpdateRender();
    }

    // Stop the PipeLine, no frame data will be generated.
    pipeline->stop();

    return 0;
}
catch(ob::Error &e) {
    std::cerr << "function:" << e.getFunction() << "\nargs:" << e.getArgs() << "\nmessage:" << e.what() << "\ntype:" << e.getExceptionType() << std::endl;
    std::cout << "\nPress any key to exit.";
    ob_smpl::waitForKeyPressed();
    exit(EXIT_FAILURE);
}
