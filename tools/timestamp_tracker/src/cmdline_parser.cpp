// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "cmdline_parser.hpp"
#include <cctype>
#include <fstream>
#include <iostream>
#include <json/json.h>

namespace libobsensor {
namespace tools {

namespace {

std::string trim(const std::string &value) {
    size_t begin = 0;
    while(begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin]))) {
        ++begin;
    }

    size_t end = value.size();
    while(end > begin && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }

    return value.substr(begin, end - begin);
}

std::string toLowerCopy(const std::string &value) {
    std::string lowered = value;
    for(auto &ch: lowered) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return lowered;
}

bool parseFpsValue(const Json::Value &value, double &fps, std::string &error) {
    fps = 0.0;
    if(value.isNull()) {
        return true;
    }

    if(value.isDouble() || value.isInt() || value.isUInt()) {
        fps = value.asDouble();
    }
    else if(value.isString()) {
        std::string text = trim(value.asString());
        if(text.empty()) {
            return true;
        }

        const std::string lowered = toLowerCopy(text);
        if(lowered.size() >= 2 && lowered.substr(lowered.size() - 2) == "hz") {
            text = trim(text.substr(0, text.size() - 2));
        }

        if(text.empty()) {
            error = "fps string is empty after removing Hz suffix";
            return false;
        }

        try {
            size_t parsedLength = 0;
            fps                 = std::stod(text, &parsedLength);
            if(parsedLength != text.size()) {
                error = "fps contains unexpected trailing characters";
                return false;
            }
        }
        catch(...) {
            error = "fps must be a number or a string like \"3.125HZ\"";
            return false;
        }
    }
    else {
        error = "fps must be a number or a string like \"3.125HZ\"";
        return false;
    }

    if(fps < 0.0) {
        error = "fps cannot be negative";
        return false;
    }

    return true;
}

bool parseStreamSpec(const Json::Value &obj, StreamSpec &spec, std::string &error) {
    spec.sensor              = obj.get("sensor", "").asString();
    spec.width               = obj.get("width", 0).asInt();
    spec.height              = obj.get("height", 0).asInt();
    spec.format              = obj.get("format", "").asString();
    spec.accelFullScaleRange = obj.get("accelFullScaleRange", "").asString();
    spec.gyroFullScaleRange  = obj.get("gyroFullScaleRange", "").asString();

    if(!parseFpsValue(obj.get("fps", Json::Value()), spec.fps, error)) {
        const std::string sensorName = spec.sensor.empty() ? "<unknown>" : spec.sensor;
        error                        = "Invalid fps for sensor \"" + sensorName + "\": " + error;
        return false;
    }

    return true;
}

}  // namespace

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
            StreamSpec  spec;
            std::string error;
            if(!parseStreamSpec(s, spec, error)) {
                std::cerr << "Error: " << error << "\n";
                return false;
            }
            config.streams.push_back(spec);
        }
    }

    if(root.isMember("deviceOverrides")) {
        const auto &overrides = root["deviceOverrides"];
        for(const auto &serial: overrides.getMemberNames()) {
            std::vector<StreamSpec> specs;
            if(overrides[serial].isMember("streams")) {
                for(const auto &s: overrides[serial]["streams"]) {
                    StreamSpec  spec;
                    std::string error;
                    if(!parseStreamSpec(s, spec, error)) {
                        std::cerr << "Error: " << error << " (device override: " << serial << ")\n";
                        return false;
                    }
                    specs.push_back(spec);
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
    ofs << "    { \"sensor\": \"color\", \"width\": 0, \"height\": 0, \"fps\": 0, \"format\": \"\" },\n";
    ofs << "    { \"sensor\": \"imu\", \"fps\": \"200HZ\", \"accelFullScaleRange\": \"4g\", \"gyroFullScaleRange\": \"1000dps\" }\n";
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
