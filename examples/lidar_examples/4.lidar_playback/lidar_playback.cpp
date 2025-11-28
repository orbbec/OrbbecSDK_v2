// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include <libobsensor/ObSensor.hpp>
#include "utils.hpp"

#include <mutex>
#include <thread>
#include <atomic>
#include <algorithm>
#include <condition_variable>
#include <cctype>

bool getRosbagPath(std::string &rosbagPath);

int main(void) try {
    std::atomic<bool> exited(false);
    std::string       filePath;
    // Get valid .bag file path from user input
    getRosbagPath(filePath);

    // Create a playback device with a Rosbag file
    std::shared_ptr<ob::PlaybackDevice> playback = std::make_shared<ob::PlaybackDevice>(filePath);
    // Create a pipeline with the playback device
    std::shared_ptr<ob::Pipeline> pipe = std::make_shared<ob::Pipeline>(playback);
    // Enable all recording streams from the playback device
    std::shared_ptr<ob::Config> config = std::make_shared<ob::Config>();
    std::cout << "Duration: " << playback->getDuration() << std::endl;

    uint32_t frameCount    = 0;
    auto     frameCallback = [&](std::shared_ptr<ob::FrameSet> frameSet) {
        if(frameCount % 20 == 0) {
            for(uint32_t i = 0; i < frameSet->getCount(); i++) {
                auto frame = frameSet->getFrame(i);
                if(!frame) {
                    continue;
                }

                std::cout << "frame index: " << frame->getIndex() << ", tsp: " << frame->getTimeStampUs()
                          << ", format: " << ob::TypeHelper::convertOBFormatTypeToString(frame->getFormat()) << std::endl;
            }
        }
        frameCount++;
    };


    std::mutex                    replayMutex;
    std::condition_variable       replayCv;
    std::atomic<OBPlaybackStatus> playStatus(OB_PLAYBACK_STOPPED);

    // Set playback status change callback, when the playback stops, start the pipeline again with the same config
    playback->setPlaybackStatusChangeCallback([&](OBPlaybackStatus status) {
        playStatus = status;
        replayCv.notify_all();
    });

    auto sensorList = playback->getSensorList();
    for(uint32_t i = 0; i < sensorList->getCount(); i++) {
        auto sensorType = sensorList->getSensorType(i);

        config->enableStream(sensorType);
    }
    config->setFrameAggregateOutputMode(OB_FRAME_AGGREGATE_OUTPUT_ANY_SITUATION);

    // Start the pipeline with the config
    pipe->start(config, frameCallback);

    auto replayThread = std::thread([&] {
        do {
            std::unique_lock<std::mutex> lock(replayMutex);
            replayCv.wait(lock, [&]() { return (exited.load() || playStatus.load() == OB_PLAYBACK_STOPPED); });
            if(exited) {
                break;
            }
            if(playStatus == OB_PLAYBACK_STOPPED) {
                pipe->stop();
                // wait 1s and play again
                replayCv.wait_for(lock, std::chrono::milliseconds(1000), [&]() { return exited.load(); });
                if(exited) {
                    break;
                }
                playStatus = OB_PLAYBACK_UNKNOWN;
                std::cout << "Replay again" << std::endl;
                pipe->start(config, frameCallback);
            }
        } while(!exited);
        pipe->stop();
        std::cout << "Replay monitor thread exists" << std::endl;
    });

    while(true) {
        auto key = ob_smpl::waitForKeyPressed(1);
        if(key == ESC_KEY) {  // Esc key to exit.
            break;
        }

        if(key == 'p' || key == 'P') {  // 'p' or 'P' key to pause/resume playback.
            if(playback->getPlaybackStatus() == OB_PLAYBACK_PLAYING) {
                playback->pause();
                std::cout << "Playback paused" << std::endl;
            }
            else if(playback->getPlaybackStatus() == OB_PLAYBACK_PAUSED) {
                playback->resume();
                std::cout << "Playback resumed" << std::endl;
            }
        }
    }

    // stop
    pipe->stop();
    exited = true;
    replayCv.notify_all();
    if(replayThread.joinable()) {
        replayThread.join();
    }

    std::cout << "exit" << std::endl;
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

        if(!input.empty() && input.front() == '\"' && input.back() == '\"') {
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