// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include <fstream>
#include <string>
#include <sstream>
#include <vector>

struct SystemInfo {
    SystemInfo(const std::string &time, float cpuUsage, float memUsage) :
        time(time), cpuUsage(cpuUsage), memUsage(memUsage) {}

    std::string time;
    float       cpuUsage;
    float       memUsage;
};

class CSVFile {
public:
    CSVFile() = default;
    ~CSVFile();

    void open(const std::string &fileName);
    void close();

    bool isOpen() const;

    void writeSystemInfo(const std::string &timestamp, float cpuUsage, float memoryUsage);
    void writeAverageSystemInfo(float cpuUsage, float memoryUsage);
    void writeSystemInfos(const std::vector<SystemInfo> &systemInfos);
    void writeTitle(const std::string &title);

private:
    std::ofstream csvFile_;
};