// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include <libobsensor/ObSensor.h>

#include "utils.hpp"
#include "utils_opencv.hpp"

#include <mutex>
#include <thread>

bool getRosbagPath(std::string &rosbagPath);

int main(void) try {
    std::string filePath;
    // Get valid .bag file path from user input
    getRosbagPath(filePath);

    // Create a playback device with a Rosbag file
    std::shared_ptr<ob::PlaybackDevice> playback = std::make_shared<ob::PlaybackDevice>(filePath);
    // Create a pipeline with the playback device
    std::shared_ptr<ob::Pipeline> pipe = std::make_shared<ob::Pipeline>(playback);

    // Enable all recording streams from the playback device
    std::shared_ptr<ob::Config> config     = std::make_shared<ob::Config>();
    auto                        sensorList = playback->getSensorList();
    for(uint32_t i = 0; i < sensorList->getCount(); i++) {
        auto sensorType = sensorList->getSensorType(i);

        config->enableStream(sensorType);
    }

    // Start the pipeline with the config
    pipe->start(config);

    ob_smpl::CVWindow win("Playback", 1280, 720, ob_smpl::ARRANGE_GRID);
    while(win.run()) {
        auto frameSet = pipe->waitForFrames();
        win.pushFramesToView(frameSet);

        // Handle playback loop when reaching end
        if(playback->getPosition() == playback->getDuration()) {
            // Restart the pipeline if the playback has reached the end of the file
            pipe->stop();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            pipe->start(config);
        }
    }

    pipe->stop();
    return 0;
}
catch(ob::Error &e) {
    std::cerr << "function:" << e.getFunction() << "\nargs:" << e.getArgs() << "\nmessage:" << e.what() << "\ntype:" << e.getExceptionType() << std::endl;
    std::cout << "\nPress any key to exit.";
    ob_smpl::waitForKeyPressed();
    exit(EXIT_FAILURE);
}

bool getRosbagPath(std::string &rosbagPath) {
    while(true) {
        std::cout << "Please input the path of the Rosbag file (.bag) to playback: \n";
        std::cout << "Path: ";
        std::string input;
        std::getline(std::cin, input);

        // Remove leading and trailing whitespaces
        input.erase(std::find_if(input.rbegin(), input.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), input.end());

        // Remove leading and trailing quotes
        if(!input.empty() && input.front() == '\'' && input.back() == '\'') {
            input = input.substr(1, input.size() - 2);
        }

        // Validate .bag extension
        if(input.size() > 4 && input.substr(input.size() - 4) == ".bag") {
            rosbagPath = input;
            std::cout << "Playback file confirmed: " << rosbagPath << "\n\n";
            return true;
        }

        std::cout << "Invalid file format. Please provide a .bag file.\n\n";
    }
}