// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "cmdline_parser.hpp"
#include <fstream>
#include <iostream>
#include <json/json.h>

namespace libobsensor {
namespace tools {

static StreamSpec parseStreamSpec(const Json::Value &obj) {
    StreamSpec spec;
    spec.sensor = obj.get("sensor", "").asString();
    spec.width  = obj.get("width", 0).asInt();
    spec.height = obj.get("height", 0).asInt();
    spec.fps    = obj.get("fps", 0).asInt();
    spec.format = obj.get("format", "").asString();
    return spec;
}

static bool loadConfigFile(const std::string &path, CmdLineConfig &config, bool overrideDuration, bool overrideSyncInterval) {
    std::ifstream ifs(path);
    if(!ifs.is_open()) {
        std::cerr << "Error: Cannot open config file: " << path << "\n";
        return false;
    }

    Json::Value             root;
    Json::CharReaderBuilder builder;
    std::string             errs;
    if(!Json::parseFromStream(builder, ifs, &root, &errs)) {
        std::cerr << "Error: Failed to parse config file: " << errs << "\n";
        return false;
    }

    if(!overrideDuration && root.isMember("duration")) {
        int val = root["duration"].asInt();
        if(val > 0) {
            config.durationMinutes = val;
        }
    }
    if(!overrideSyncInterval && root.isMember("syncInterval")) {
        int val = root["syncInterval"].asInt();
        if(val >= 0) {
            config.syncIntervalSec = val;
        }
    }

    if(root.isMember("streams")) {
        for(const auto &s: root["streams"]) {
            config.streams.push_back(parseStreamSpec(s));
        }
    }

    if(root.isMember("deviceOverrides")) {
        const auto &overrides = root["deviceOverrides"];
        for(const auto &serial: overrides.getMemberNames()) {
            std::vector<StreamSpec> specs;
            if(overrides[serial].isMember("streams")) {
                for(const auto &s: overrides[serial]["streams"]) {
                    specs.push_back(parseStreamSpec(s));
                }
            }
            config.deviceOverrides[serial] = specs;
        }
    }

    return true;
}

void CmdLineParser::printUsage(const char *programName) {
    std::cout << "Usage: " << programName << " [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  -t, --time <minutes>          Tracking duration in minutes (default: 60)\n";
    std::cout << "  -i, --sync-interval <seconds> Device clock sync interval in seconds (default: 0)\n";
    std::cout << "                                0 = sync device clock once at start\n";
    std::cout << "                                >0 = periodic device clock sync every N seconds\n";
    std::cout << "  -c, --config <file>           Path to JSON configuration file\n";
    std::cout << "  -g, --generate-config <file>  Write a default JSON config to <file> and exit\n";
    std::cout << "  -h, --help                    Show this help message\n";
    std::cout << "\nExamples:\n";
    std::cout << "  " << programName << "                             # 1 hour tracking, auto-select streams\n";
    std::cout << "  " << programName << " -t 30                       # 30 minutes tracking\n";
    std::cout << "  " << programName << " -t 60 -i 60                 # 1 hour with 60s sync interval\n";
    std::cout << "  " << programName << " -c config.json              # load JSON config file\n";
    std::cout << "  " << programName << " -g config.json              # generate default JSON config\n";
    std::cout << "\nTip: use -g to export a default config file, then edit it to customise streams.\n";
}

bool CmdLineParser::generateDefaultConfig(const std::string &outputPath) {
    std::ofstream ofs(outputPath);
    if(!ofs.is_open()) {
        std::cerr << "Error: Cannot write config file: " << outputPath << "\n";
        return false;
    }
    ofs << "{\n";
    ofs << "  \"duration\": 60,\n";
    ofs << "  \"syncInterval\": 0,\n";
    ofs << "  \"streams\": [\n";
    ofs << "    { \"sensor\": \"depth\", \"width\": 0, \"height\": 0, \"fps\": 0, \"format\": \"\" },\n";
    ofs << "    { \"sensor\": \"color\", \"width\": 0, \"height\": 0, \"fps\": 0, \"format\": \"\" }\n";
    ofs << "  ],\n";
    ofs << "  \"deviceOverrides\": {\n";
    ofs << "    \"<serial_number>\": {\n";
    ofs << "      \"streams\": [\n";
    ofs << "        { \"sensor\": \"depth\", \"width\": 0, \"height\": 0, \"fps\": 0, \"format\": \"\" }\n";
    ofs << "      ]\n";
    ofs << "    }\n";
    ofs << "  }\n";
    ofs << "}\n";
    std::cout << "Default config written to: " << outputPath << "\n";
    return true;
}

bool CmdLineParser::parse(int argc, char *argv[], CmdLineConfig &config) {
    bool        overrideDuration     = false;
    bool        overrideSyncInterval = false;
    std::string configFile;

    for(int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if(arg == "-h" || arg == "--help") {
            config.showHelp = true;
            return true;
        }
        else if((arg == "-t" || arg == "--time") && i + 1 < argc) {
            try {
                int val = std::stoi(argv[++i]);
                if(val <= 0) {
                    std::cerr << "Error: Tracking duration must be positive.\n";
                    return false;
                }
                config.durationMinutes = val;
                overrideDuration       = true;
            }
            catch(...) {
                std::cerr << "Error: Invalid tracking duration.\n";
                return false;
            }
        }
        else if((arg == "-i" || arg == "--sync-interval") && i + 1 < argc) {
            try {
                int val = std::stoi(argv[++i]);
                if(val < 0) {
                    std::cerr << "Error: Sync interval cannot be negative.\n";
                    return false;
                }
                config.syncIntervalSec = val;
                overrideSyncInterval   = true;
            }
            catch(...) {
                std::cerr << "Error: Invalid sync interval.\n";
                return false;
            }
        }
        else if((arg == "-c" || arg == "--config") && i + 1 < argc) {
            configFile = argv[++i];
        }
        else if((arg == "-g" || arg == "--generate-config") && i + 1 < argc) {
            config.generateConfig     = true;
            config.generateConfigFile = argv[++i];
            return true;
        }
        else {
            std::cerr << "Error: Unknown or incomplete argument: " << arg << "\n";
            return false;
        }
    }

    if(!configFile.empty()) {
        config.configFile = configFile;
        if(!loadConfigFile(configFile, config, overrideDuration, overrideSyncInterval)) {
            return false;
        }
    }

    return true;
}

}  // namespace tools
}  // namespace libobsensor
