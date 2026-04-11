// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace libobsensor {
namespace tools {

struct StreamSpec {
    std::string sensor;      ///< sensor name
    int         width  = 0;  ///< 0 = any
    int         height = 0;  ///< 0 = any
    int         fps    = 0;  ///< 0 = any
    std::string format;      ///< "" = any
};

struct CmdLineConfig {
    int         durationMinutes = 60;
    int         syncIntervalSec = 0;
    bool        showHelp        = false;
    bool        generateConfig  = false;
    std::string generateConfigFile;
    std::string configFile;

    /// Global stream list applied to all devices (empty = auto-select depth+color or ir_left+ir_right)
    std::vector<StreamSpec> streams;

    /// Per-device stream override keyed by serial number; overrides `streams` entirely for matched devices
    std::map<std::string, std::vector<StreamSpec>> deviceOverrides;

    uint64_t getSyncIntervalMs() const {
        return static_cast<uint64_t>(syncIntervalSec) * 1000;
    }

    uint64_t getDurationMs() const {
        return static_cast<uint64_t>(durationMinutes) * 60 * 1000;
    }
};

class CmdLineParser {
public:
    static bool parse(int argc, char *argv[], CmdLineConfig &config);
    static void printUsage(const char *programName);
    static bool generateDefaultConfig(const std::string &outputPath);
};

}  // namespace tools
}  // namespace libobsensor
