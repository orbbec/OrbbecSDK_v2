// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include <libobsensor/ObSensor.h>

#include "utils.hpp"
#include "utils_opencv.hpp"

#include <thread>
#include <atomic>
#include <fstream>
#include <ctime>
#include <iomanip>
#include <sstream>
#if defined(_WIN32)
#include <direct.h>
#else
#include <sys/stat.h>
#endif

bool              quit_program      = false;  // Flag to signal the program to quit
std::atomic<bool> capture_requested = { false };  // Flag to trigger depth frame capture

static void createDirectories(const std::string &path) {
    for(size_t i = 1; i <= path.size(); ++i) {
        if(i == path.size() || path[i] == '/' || path[i] == '\\') {
            std::string subPath = path.substr(0, i);
#if defined(_WIN32)
            _mkdir(subPath.c_str());
#else
            mkdir(subPath.c_str(), 0755);
#endif
        }
    }
}

// Save a depth frame as a raw binary file
static void saveDepthRaw(const std::string &filePath, std::shared_ptr<const ob::Frame> frame) {
    if(!frame) {
        std::cerr << "SaveDepthRaw: null frame, skip." << std::endl;
        return;
    }
    std::ofstream ofs(filePath, std::ios::binary);
    if(!ofs.is_open()) {
        std::cerr << "SaveDepthRaw: failed to open " << filePath << std::endl;
        return;
    }
    ofs.write(reinterpret_cast<const char *>(frame->getData()), frame->getDataSize());
    std::cout << "Saved: " << filePath << " (" << frame->getDataSize() << " bytes)" << std::endl;
}

void printFiltersInfo(const std::vector<std::shared_ptr<ob::Filter>> &filterList) {
    std::cout << filterList.size() << " post processing filters recommended:" << std::endl;
    for(auto &filter: filterList) {
        std::cout << " - " << filter->getName() << ": " << (filter->isEnabled() ? "enabled" : "disabled") << std::endl;
        auto configSchemaVec = filter->getConfigSchemaVec();

        // Print the config schema for each filter
        for(auto &configSchema: configSchemaVec) {
            std::cout << "    - {" << configSchema.name << ", " << configSchema.type << ", " << configSchema.min << ", " << configSchema.max << ", "
                      << configSchema.step << ", " << configSchema.def << ", " << configSchema.desc << "}" << std::endl;
        }
        filter->enable(false);  // Disable the filter
    }
}

void filterControl(const std::vector<std::shared_ptr<ob::Filter>> &filterList) {
    auto printHelp = [&]() {
        std::cout << "Available commands:" << std::endl;
        std::cout << "- Enter `[Filter]` to list the config values for the filter" << std::endl;
        std::cout << "- Enter `[Filter] on` or `[Filter] off` to enable/disable the filter" << std::endl;
        std::cout << "- Enter `[Filter] list` to list the config schema for the filter" << std::endl;
        std::cout << "- Enter `[Filter] [Config]` to show the config values for the filter" << std::endl;
        std::cout << "- Enter `[Filter] [Config] [Value]` to set a config value" << std::endl;
        std::cout << "- Enter `L`or `l` to list all available filters" << std::endl;
        std::cout << "- Enter `H` or `h` to print this help message" << std::endl;
        std::cout << "- Enter `C` or `c` to capture depth frames (before/after processing) as .raw files" << std::endl;
        std::cout << "- Enter `Q` or `q` to quit" << std::endl;
    };
    printHelp();
    while(!quit_program) {
        std::cout << "---------------------------" << std::endl;
        std::cout << "Enter your input (h for help): ";

        std::string input;
        std::getline(std::cin, input);
        if(input == "q" || input == "Q") {
            quit_program = true;
            break;
        }
        else if(input == "l" || input == "L") {
            printFiltersInfo(filterList);
            continue;
        }
        else if(input == "h" || input == "H") {
            printHelp();
            continue;
        }
        else if(input == "c" || input == "C") {
            capture_requested = true;
            std::cout << "Capture requested, saving depth frames to Output/Processing/ ..." << std::endl;
            continue;
        }

        // Parse the input
        std::vector<std::string> tokens;
        std::istringstream       iss(input);
        for(std::string token; iss >> token;) {
            tokens.push_back(token);
        }
        if(tokens.empty()) {
            continue;
        }

        bool foundFilter = false;
        for(auto &filter: filterList) {
            if(filter->getName() == tokens[0]) {
                foundFilter = true;
                if(tokens.size() == 1) {  // print list of configs for the filter
                    auto configSchemaVec = filter->getConfigSchemaVec();
                    std::cout << "Config values for " << filter->getName() << ":" << std::endl;
                    for(auto &configSchema: configSchemaVec) {
                        std::cout << " - " << configSchema.name << ": " << filter->getConfigValue(configSchema.name) << std::endl;
                    }
                }
                else if(tokens.size() == 2 && (tokens[1] == "on" || tokens[1] == "off")) {  // Enable/disable the filter
                    filter->enable(tokens[1] == "on");
                    std::cout << "Success: Filter " << filter->getName() << " is now " << (filter->isEnabled() ? "enabled" : "disabled") << std::endl;
                }
                else if(tokens.size() == 2 && tokens[1] == "list") {  // List the config values for the filter
                    auto configSchemaVec = filter->getConfigSchemaVec();
                    std::cout << "Config schema for " << filter->getName() << ":" << std::endl;
                    for(auto &configSchema: configSchemaVec) {
                        std::cout << " - {" << configSchema.name << ", " << configSchema.type << ", " << configSchema.min << ", " << configSchema.max << ", "
                                  << configSchema.step << ", " << configSchema.def << ", " << configSchema.desc << "}" << std::endl;
                    }
                }
                else if(tokens.size() == 2) {  // Print the config schema for the filter
                    auto configSchemaVec = filter->getConfigSchemaVec();
                    bool foundConfig     = false;
                    for(auto &configSchema: configSchemaVec) {
                        if(configSchema.name == tokens[1]) {
                            foundConfig = true;
                            std::cout << "Config values for " << filter->getName() << "@" << configSchema.name << ":"
                                      << filter->getConfigValue(configSchema.name) << std::endl;
                            break;
                        }
                    }
                    if(!foundConfig) {
                        std::cerr << "Error: Config " << tokens[1] << " not found for filter " << filter->getName() << std::endl;
                    }
                }
                else if(tokens.size() == 3) {  // Set a config value
                    try {
                        double value = std::stod(tokens[2]);
                        filter->setConfigValue(tokens[1], value);
                    }
                    catch(const std::exception &e) {
                        std::cerr << "Error: " << e.what() << std::endl;
                        continue;
                    }
                    std::cout << "Success: Config value of " << tokens[1] << " for filter " << filter->getName() << " is set to " << tokens[2] << std::endl;
                }
                break;
            }
        }
        if(!foundFilter) {
            std::cerr << "Error: Filter " << tokens[0] << " not found" << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

int main() try {
    // Create a pipeline with default device
    ob::Pipeline pipe;

    // Get the device and sensor, and get the list of recommended filters for the sensor
    auto device     = pipe.getDevice();
    auto sensor     = device->getSensor(OB_SENSOR_DEPTH);
    auto filterList = sensor->createRecommendedFilters();

    // Print the recommended filters
    printFiltersInfo(filterList);

    // Create a config with depth stream enabled
    std::shared_ptr<ob::Config> config = std::make_shared<ob::Config>();
    config->enableStream(OB_STREAM_DEPTH);

    // Start the pipeline with config
    pipe.start(config);

    // Start the filter control loop on sub thread
    std::thread filterControlThread(filterControl, filterList);
    filterControlThread.detach();

    // Create a window for rendering, and set the resolution of the window
    ob_smpl::CVWindow win("PostProcessing", 1280, 720, ob_smpl::ARRANGE_ONE_ROW);

    while(win.run() && !quit_program) {
        // Wait for up to 1000ms for a frameset in blocking mode.
        auto frameSet = pipe.waitForFrameset(1000);
        if(frameSet == nullptr) {
            continue;
        }

        // Get the depth frame from the frameset
        auto depthFrameRaw = frameSet->getFrame(OB_FRAME_DEPTH);
        if(!depthFrameRaw) {
            continue;
        }

        auto processedFrame = depthFrameRaw;
        // Apply the recommended filters to the depth frame
        for(auto &filter: filterList) {
            if(filter->isEnabled()) {  // Only apply enabled filters
                processedFrame = filter->process(processedFrame);
            }
        }

        // Check if a capture was requested (triggered by pressing 'C' in the console)
        if(capture_requested.exchange(false)) {
            const std::string outDir = "Output/Process";
            createDirectories(outDir);

            auto depthFrame = depthFrameRaw->as<ob::DepthFrame>();
            saveDepthRaw(outDir + "/depth_before_" + std::to_string(depthFrame->getWidth()) + "x" + std::to_string(depthFrame->height()) + "_"
                             + std::to_string(depthFrame->timeStamp()) + ".raw",
                         depthFrame);
            saveDepthRaw(outDir + "/depth_after_" + std::to_string(depthFrame->width()) + "x" + std::to_string(depthFrame->height()) + "_"
                             + std::to_string(depthFrame->timeStamp()) + ".raw",
                         processedFrame);
        }

        // Push the frames to the window for showing
        // Due to processedFrame type is same as the depthFrame, we should push it with different group id.
        win.pushFramesToView(depthFrameRaw, 0);
        win.pushFramesToView(processedFrame, 1);
    }

    // Stop the pipeline
    pipe.stop();

    quit_program = true;

    return 0;
}
catch(ob::Error &e) {
    std::cerr << "function:" << e.getFunction() << "\nargs:" << e.getArgs() << "\nmessage:" << e.what() << "\ntype:" << e.getExceptionType() << std::endl;
    std::cout << "\nPress any key to exit.";
    ob_smpl::waitForKeyPressed();
    exit(EXIT_FAILURE);
}

