// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "src/cmdline_parser.hpp"
#include "src/timestamp_collector.hpp"
#include "src/utils.hpp"
#include <libobsensor/ObSensor.hpp>

#include <iostream>
#include <vector>
#include <memory>
#include <thread>
#include <chrono>

using namespace libobsensor::tools;
const char *appName = "timestamp_tracker";

int main(int argc, char *argv[]) try {
    CmdLineConfig config;
    if(!CmdLineParser::parse(argc, argv, config)) {
        CmdLineParser::printUsage(appName);
        return EXIT_FAILURE;
    }

    if(config.showHelp) {
        CmdLineParser::printUsage(appName);
        return 0;
    }

    if(config.generateConfig) {
        return CmdLineParser::generateDefaultConfig(config.generateConfigFile) ? 0 : EXIT_FAILURE;
    }

    std::cout << "================================================" << std::endl;
    std::cout << "Timestamp Tracker Tool" << std::endl;
    std::cout << "================================================" << std::endl;
    std::cout << "Duration: " << config.durationMinutes << " minutes" << std::endl;
    std::cout << "Sync interval: " << config.syncIntervalSec << " seconds (" << (config.syncIntervalSec == 0 ? "once" : "periodic") << ")" << std::endl;
    if(!config.configFile.empty()) {
        std::cout << "Config file: " << config.configFile << std::endl;
    }
    std::cout << std::endl;

    ob::Context ctx;

    if(config.syncIntervalSec > 0) {
        uint64_t intervalMs = config.getSyncIntervalMs();
        std::cout << "Enabling periodic time sync (interval: " << config.syncIntervalSec << "s)" << std::endl;
        ctx.enableDeviceClockSync(intervalMs);
    }

    auto     deviceList  = ctx.queryDeviceList();
    uint32_t deviceCount = deviceList->getCount();

    if(deviceCount == 0) {
        std::cerr << "Error: No devices found!" << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "Found " << deviceCount << " device(s)" << std::endl;
    std::cout << std::endl;

    std::vector<std::shared_ptr<TimestampCollector>> collectors;

    for(uint32_t i = 0; i < deviceCount; ++i) {
        auto device    = deviceList->getDevice(i);
        auto collector = std::make_shared<TimestampCollector>(device);

        std::cout << "[" << (i + 1) << "/" << deviceCount << "] ";
        if(!collector->start(config)) {
            std::cerr << "Failed to start collector for device " << i << std::endl;
            continue;
        }

        collectors.push_back(std::move(collector));
        std::cout << std::endl;
    }

    if(collectors.empty()) {
        std::cerr << "Error: No devices could be started!" << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "================================================" << std::endl;
    std::cout << "Collectting started on " << collectors.size() << " device(s)" << std::endl;
    std::cout << "================================================" << std::endl;

    // Wait for specified duration or until ESC is pressed.
    // Use steady_clock so NTP adjustments to the system clock do not affect the duration.
    auto     startTime  = std::chrono::steady_clock::now();
    uint64_t durationMs = config.getDurationMs();
    bool     userQuit   = false;

    std::cout << "Press ESC to stop early." << std::endl;

    while(true) {
        auto elapsed = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime).count());
        if(elapsed >= durationMs) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if(isEscPressed()) {
            userQuit = true;
            break;
        }
    }

    std::cout << std::endl;
    if(userQuit) {
        std::cout << "ESC pressed. Stopping early..." << std::endl;
    }
    else {
        std::cout << "Collectting duration reached. Stopping..." << std::endl;
    }
    std::cout << "Stopping all collectors..." << std::endl;

    for(auto &collector: collectors) {
        collector->stop();
    }

    std::cout << std::endl;
    std::cout << "================================================" << std::endl;
    std::cout << "Collecting complete!" << std::endl;
    for(const auto &collector: collectors) {
        auto files = collector->getOpenedFiles();
        if(files.empty()) {
            continue;
        }
        std::cout << "  " << collector->getDeviceInfoStr() << ":" << std::endl;
        for(const auto &f: files) {
            std::cout << "    " << f << std::endl;
        }
    }
    std::cout << "================================================" << std::endl;

    return 0;
}
catch(ob::Error &e) {
    std::cerr << "function:" << e.getFunction() << "\nargs:" << e.getArgs() << "\nmessage:" << e.what() << "\nstatus:" << e.getStatus()
              << "\ntype:" << e.getExceptionType() << std::endl;
    std::cout << "\nPress any key to exit.";
    getchar();
    exit(EXIT_FAILURE);
}
