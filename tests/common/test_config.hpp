// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "test_utils.hpp"
#include "cJSON.h"

#include <iostream>
#include <string>
#include <vector>

class TestConfig {
public:
    static TestConfig &instance() {
        static TestConfig cfg;
        return cfg;
    }

    static void loadFromFile(const std::string &path) {
        instance().loadConfig(path);
    }

    bool allowDestructive() const {
        return allowDestructive_;
    }

    void setAllowDestructive(bool v) {
        allowDestructive_ = v;
    }

    const std::string &deviceSerial() const {
        return deviceSerial_;
    }

    int deviceCount() const {
        return deviceCount_;
    }

    const std::string &bagPath() const {
        return bagPath_;
    }

    const std::vector<std::string> &allBagPaths() const {
        return allBagPaths_;
    }

    const std::string &firmwarePath() const {
        return firmwarePath_;
    }

    bool hasFirmware() const {
        return !firmwarePath_.empty();
    }

    const std::string &depthPresetPath() const {
        return depthPresetPath_;
    }

    bool hasDepthPreset() const {
        return !depthPresetPath_.empty();
    }

    const std::string &netIp() const {
        return netIp_;
    }

    const std::string &netMac() const {
        return netMac_;
    }

    const std::string &netMask() const {
        return netMask_;
    }

    const std::string &netGateway() const {
        return netGateway_;
    }

    const std::string &netOriginalIp() const {
        return netOriginalIp_;
    }

    int perfDurationSec() const {
        return perfDurationSec_;
    }

    int perfCpuThreads() const {
        return perfCpuThreads_;
    }

    int perfIoThreads() const {
        return perfIoThreads_;
    }

    int tsDurationSec() const {
        return tsDurationSec_;
    }

    int tsThreadCount() const {
        return tsThreadCount_;
    }

private:
    TestConfig() {
        loadDefaults();
    }

    void loadDefaults() {
        deviceSerial_.clear();
        deviceCount_      = 1;
        allowDestructive_ = false;
        allBagPaths_.clear();
        bagPath_.clear();
        firmwarePath_.clear();
        depthPresetPath_.clear();
        netIp_.clear();
        netMac_.clear();
        netMask_.clear();
        netGateway_.clear();
        netOriginalIp_.clear();
        perfDurationSec_ = 60;
        perfCpuThreads_  = 0;
        perfIoThreads_   = 2;
        tsDurationSec_   = 10;
        tsThreadCount_   = 4;
    }

    void loadConfig(const std::string &path) {
        loadDefaults();

        std::string actualPath = path;
        if(actualPath.empty()) {
            if(test_utils::fileExists("test_config.json")) {
                actualPath = "test_config.json";
            }
            else {
                std::cout << "[TestConfig] No config file specified, using defaults.\n";
                return;
            }
        }

        if(!test_utils::fileExists(actualPath)) {
            std::cout << "[TestConfig] Config file not found: " << actualPath << ", using defaults.\n";
            return;
        }

        std::string content = test_utils::readFileContent(actualPath);
        if(content.empty()) {
            std::cout << "[TestConfig] Config file is empty or unreadable: " << actualPath << ", using defaults.\n";
            return;
        }

        cJSON *root = cJSON_Parse(content.c_str());
        if(!root) {
            std::cout << "[TestConfig] Failed to parse JSON: " << actualPath << ", using defaults.\n";
            return;
        }

        std::cout << "[TestConfig] Loaded from: " << actualPath << "\n";
        parseConfig(root);
        cJSON_Delete(root);
    }

    void parseConfig(cJSON *root) {
        // device
        cJSON *device = cJSON_GetObjectItem(root, "device");
        if(device) {
            cJSON *sn = cJSON_GetObjectItem(device, "serial");
            if(cJSON_IsString(sn) && sn->valuestring && sn->valuestring[0]) {
                deviceSerial_ = sn->valuestring;
            }
            cJSON *count = cJSON_GetObjectItem(device, "count");
            if(cJSON_IsNumber(count)) {
                deviceCount_ = count->valueint > 0 ? count->valueint : 1;
            }
        }

        // destructive
        cJSON *destructive = cJSON_GetObjectItem(root, "destructive");
        if(destructive) {
            cJSON *enabled = cJSON_GetObjectItem(destructive, "enabled");
            if(cJSON_IsBool(enabled)) {
                allowDestructive_ = cJSON_IsTrue(enabled);
            }
        }

        // resource
        cJSON *resource = cJSON_GetObjectItem(root, "resource");
        if(resource) {
            cJSON *bagPaths = cJSON_GetObjectItem(resource, "bagPaths");
            if(cJSON_IsArray(bagPaths)) {
                int n = cJSON_GetArraySize(bagPaths);
                for(int i = 0; i < n; ++i) {
                    cJSON *item = cJSON_GetArrayItem(bagPaths, i);
                    if(cJSON_IsString(item) && item->valuestring && item->valuestring[0]) {
                        allBagPaths_.push_back(item->valuestring);
                    }
                }
            }
            if(!allBagPaths_.empty()) {
                bagPath_ = allBagPaths_.front();
            }
            cJSON *fw = cJSON_GetObjectItem(resource, "firmwarePath");
            if(cJSON_IsString(fw) && fw->valuestring && fw->valuestring[0]) {
                firmwarePath_ = fw->valuestring;
            }
            cJSON *preset = cJSON_GetObjectItem(resource, "depthPresetPath");
            if(cJSON_IsString(preset) && preset->valuestring && preset->valuestring[0]) {
                depthPresetPath_ = preset->valuestring;
            }
        }

        // network
        cJSON *network = cJSON_GetObjectItem(root, "network");
        if(network) {
            cJSON *ip = cJSON_GetObjectItem(network, "ip");
            if(cJSON_IsString(ip) && ip->valuestring && ip->valuestring[0])
                netIp_ = ip->valuestring;
            cJSON *mac = cJSON_GetObjectItem(network, "mac");
            if(cJSON_IsString(mac) && mac->valuestring && mac->valuestring[0])
                netMac_ = mac->valuestring;
            cJSON *mask = cJSON_GetObjectItem(network, "mask");
            if(cJSON_IsString(mask) && mask->valuestring && mask->valuestring[0])
                netMask_ = mask->valuestring;
            cJSON *gw = cJSON_GetObjectItem(network, "gateway");
            if(cJSON_IsString(gw) && gw->valuestring && gw->valuestring[0])
                netGateway_ = gw->valuestring;
            cJSON *orig = cJSON_GetObjectItem(network, "originalIp");
            if(cJSON_IsString(orig) && orig->valuestring && orig->valuestring[0])
                netOriginalIp_ = orig->valuestring;
        }

        // performance
        cJSON *perf = cJSON_GetObjectItem(root, "performance");
        if(perf) {
            cJSON *dur = cJSON_GetObjectItem(perf, "durationSeconds");
            if(cJSON_IsNumber(dur) && dur->valueint > 0)
                perfDurationSec_ = dur->valueint;
            cJSON *cpu = cJSON_GetObjectItem(perf, "cpuThreads");
            if(cJSON_IsNumber(cpu) && cpu->valueint >= 0)
                perfCpuThreads_ = cpu->valueint;
            cJSON *io = cJSON_GetObjectItem(perf, "ioThreads");
            if(cJSON_IsNumber(io) && io->valueint > 0)
                perfIoThreads_ = io->valueint;
        }

        // threadSafety
        cJSON *ts = cJSON_GetObjectItem(root, "threadSafety");
        if(ts) {
            cJSON *dur = cJSON_GetObjectItem(ts, "durationSeconds");
            if(cJSON_IsNumber(dur) && dur->valueint > 0)
                tsDurationSec_ = dur->valueint;
            cJSON *tc = cJSON_GetObjectItem(ts, "threadCount");
            if(cJSON_IsNumber(tc) && tc->valueint > 0)
                tsThreadCount_ = tc->valueint;
        }

        std::cout << "[TestConfig] serial=" << deviceSerial_ << " count=" << deviceCount_ << "\n";
    }

    bool                     allowDestructive_;
    std::string              deviceSerial_;
    int                      deviceCount_;
    std::string              bagPath_;
    std::vector<std::string> allBagPaths_;
    std::string              firmwarePath_;
    std::string              depthPresetPath_;
    std::string              netIp_;
    std::string              netMac_;
    std::string              netMask_;
    std::string              netGateway_;
    std::string              netOriginalIp_;
    int                      perfDurationSec_;
    int                      perfCpuThreads_;
    int                      perfIoThreads_;
    int                      tsDurationSec_;
    int                      tsThreadCount_;
};

#define ENV() TestConfig::instance()
