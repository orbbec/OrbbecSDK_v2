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

void CSVFile::writeSystemInfo(const std::string &timestamp, float cpuUsage, float memoryUsage) {
    csvFile_ << timestamp << "," << cpuUsage << "," << memoryUsage << "\n";
    csvFile_.flush();
}

void CSVFile::writeSystemInfos(const std::vector<SystemInfo> &systemInfos) {
    for(const auto &info: systemInfos) {
        writeSystemInfo(info.time, info.cpuUsage, info.memUsage);
    }
}

void CSVFile::writeAverageSystemInfo(float cpuUsage, float memoryUsage) {
    csvFile_ << cpuUsage << "," << memoryUsage << "\n";
    csvFile_.flush();
}

void CSVFile::writeTitle(const std::string &title) {
    csvFile_ << title << "\n";
    csvFile_.flush();
}