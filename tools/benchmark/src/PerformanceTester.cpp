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
        std::cout << "[" << i << "] "
                  << "Device: " << dev->getDeviceInfo()->getName();
        std::cout << " | SN: " << dev->getDeviceInfo()->getSerialNumber();
        std::cout << " | Firmware version: " << dev->getDeviceInfo()->getFirmwareVersion() << std::endl;
        deviceResources_.emplace_back(std::make_shared<DeviceResource>(dev));
    }
    systemInfosManager_ = std::make_shared<SystemInfosManager>();
    summary_.open("summary.csv");
    summary_.writeTitle(
        "Config, Average Process CPU Usage (%), Average Process Memory Used (MB), Average System GPU Usage (%), Average System Graphics Memory Used (MB)");
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

        uint64_t totalReceivedFrames  = 0;
        uint64_t totalAlignFrames     = 0;
        uint64_t totalEnhancedFrames  = 0;
        uint64_t totalCompletedFrames = 0;
        for(auto &deviceResource: deviceResources_) {
            auto frameStats = deviceResource->getFrameStats();
            totalReceivedFrames += frameStats.receivedFrameCount;
            totalAlignFrames += frameStats.alignProcessedFrameCount;
            totalEnhancedFrames += frameStats.enhancedProcessedFrameCount;
            totalCompletedFrames += frameStats.completedFrameCount;
        }

        auto seconds = static_cast<double>(RECONDING_TIME_SECONDS);
        std::cout << "Frame throughput summary | config: " << configName << " | received: " << totalReceivedFrames << " ("
                  << totalReceivedFrames / seconds << " fps)"
                  << " | align: " << totalAlignFrames << " (" << totalAlignFrames / seconds << " fps)"
                  << " | enhanced: " << totalEnhancedFrames << " (" << totalEnhancedFrames / seconds << " fps)"
                  << " | completed: " << totalCompletedFrames << " (" << totalCompletedFrames / seconds << " fps)" << std::endl;

        ++cnt;

        writeSystemInfosToFile(configName, fileName);
    }
}

void PerformanceTester::writeSystemInfosToFile(const std::string &configName, const std::string &fileName) {
    float       totalCpuUsage     = 0.0f;
    float       totalMemoryUsage  = 0.0f;
    float       totalGpuUsage     = 0.0f;
    float       totalGpuMemUsedMB = 0.0f;
    uint64_t    validGpuMemCnt    = 0;
    uint64_t    totalCnt          = 0;
    std::string name              = "\"" + configName + "\"";

    CSVFile file;
    file.open(fileName);
    file.writeTitle(name);
    file.writeTitle("Time,Process CPU Usage (%),Process Memory Used (MB),System GPU Usage (%),System Graphics Memory Used (MB)");
    for(auto &info: systemInfosManager_->getData()) {
        file.writeSystemInfo(info.time, info.cpuUsage, info.memUsage, info.gpuUsage, info.gpuMemUsedMB);
        totalCpuUsage += info.cpuUsage;
        totalMemoryUsage += info.memUsage;
        totalGpuUsage += info.gpuUsage;
        if(info.gpuMemUsedMB >= 0.0f) {
            totalGpuMemUsedMB += info.gpuMemUsedMB;
            ++validGpuMemCnt;
        }
        ++totalCnt;
    }
    file.close();

    float averageGpuMemUsedMB = -1.0f;
    if(validGpuMemCnt > 0) {
        averageGpuMemUsedMB = totalGpuMemUsedMB / validGpuMemCnt;
    }

    summary_.writeAverageSystemInfo(name, totalCpuUsage / totalCnt, totalMemoryUsage / totalCnt, totalGpuUsage / totalCnt, averageGpuMemUsedMB);
}