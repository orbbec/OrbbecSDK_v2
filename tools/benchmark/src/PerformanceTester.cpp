// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "PerformanceTester.hpp"
#include "config/PerformanceConfig.hpp"

#include <thread>

PerformanceTester::PerformanceTester(std::shared_ptr<ob::DeviceList> devList) {
    int devCount = devList->getCount();
    std::cout << "Found " << devCount << " device(s):" << std::endl;
    for(int i = 0; i < devCount; i++) {
        auto dev = devList->getDevice(i);
        std::cout << "[" << i << "] " << "Device: " << dev->getDeviceInfo()->getName();
        std::cout << " | SN: " << dev->getDeviceInfo()->getSerialNumber();
        std::cout << " | Firmware version: " << dev->getDeviceInfo()->getFirmwareVersion() << std::endl;
        deviceResources_.emplace_back(std::make_shared<DeviceResource>(dev));
    }
    systemInfosManager_ = std::make_shared<SystemInfosManager>();
    summary_.open("summary.csv");
}

PerformanceTester::~PerformanceTester() {
    summary_.close();
}

void PerformanceTester::startTesting() {
    int cnt = 0;
    for(auto &updateConfigHandler: updateConfigHandlers_) {
        std::string configName;
        std::string fileName;
        fileName = std::to_string(cnt) + ".csv";

        for(auto &deviceResource: deviceResources_) {
            // Update Config and restart streaming
            configName = updateConfigHandler(deviceResource);
            std::cout << "Config updated to " << configName << std::endl;
        }
        // Wait for streaming stablization
        std::this_thread::sleep_for(std::chrono::seconds(2));

        systemInfosManager_->startCpuMonitoring();

        // Wait for cpu monitoring and log data
        std::this_thread::sleep_for(std::chrono::seconds(RECONDING_TIME_SECONDS));

        systemInfosManager_->stopCpuMonitoring();
        for(auto &deviceResource: deviceResources_) {
            deviceResource->stopStream();
        }
        ++cnt;

        writeSystemInfosToFile(configName, fileName);
    }
}

void PerformanceTester::writeSystemInfosToFile(const std::string &configName, const std::string &fileName) {
    float    totalCpuUsage    = 0.0f;
    float    totalMemoryUsage = 0.0f;
    uint64_t totalCnt         = 0;

    CSVFile file;
    file.open(fileName);
    file.writeTitle(configName);
    file.writeTitle("Time,CPU Usage,Memory Usage");
    for(auto &info: systemInfosManager_->getData()) {
        file.writeSystemInfo(info.time, info.cpuUsage, info.memUsage);
        totalCpuUsage += info.cpuUsage;
        totalMemoryUsage += info.memUsage;
        ++totalCnt;
    }
    file.close();

    summary_.writeTitle(std::string("Config: " + configName + " | Total average"));
    summary_.writeTitle("Average Cpu Usage(%), Average Memory Usage(MB)");
    summary_.writeAverageSystemInfo(totalCpuUsage / totalCnt, totalMemoryUsage / totalCnt);
}