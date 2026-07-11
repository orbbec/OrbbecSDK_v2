// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "CsvFile.hpp"

CSVFile::~CSVFile() {
    close();
}

void CSVFile::open(const std::string &fileName) {
    close();
    csvFile_.open(fileName);
}

void CSVFile::close() {
    if(csvFile_.is_open()) {
        csvFile_.close();
    }
}

bool CSVFile::isOpen() const {
    return csvFile_.is_open();
}

void CSVFile::writeSystemInfo(const std::string &timestamp, float cpuUsage, float memoryUsage, float gpuUsage, float gpuMemoryUsedMB) {
    csvFile_ << timestamp << "," << cpuUsage << "," << memoryUsage << "," << gpuUsage << ",";
    if(gpuMemoryUsedMB >= 0.0f) {
        csvFile_ << gpuMemoryUsedMB;
    }
    else {
        csvFile_ << "N/A";
    }
    csvFile_ << "\n";
    csvFile_.flush();
}

void CSVFile::writeSystemInfos(const std::vector<SystemInfo> &systemInfos) {
    for(const auto &info: systemInfos) {
        writeSystemInfo(info.time, info.cpuUsage, info.memUsage, info.gpuUsage, info.gpuMemUsedMB);
    }
}

void CSVFile::writeAverageSystemInfo(const std::string &config, float cpuUsage, float memoryUsage, float gpuUsage, float gpuMemoryUsedMB) {
    csvFile_ << config << "," << cpuUsage << "," << memoryUsage << "," << gpuUsage << ",";
    if(gpuMemoryUsedMB >= 0.0f) {
        csvFile_ << gpuMemoryUsedMB;
    }
    else {
        csvFile_ << "N/A";
    }
    csvFile_ << "\n";
    csvFile_.flush();
}

void CSVFile::writeTitle(const std::string &title) {
    csvFile_ << title << "\n";
    csvFile_.flush();
}