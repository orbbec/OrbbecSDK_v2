// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "DeviceResource.hpp"
#include "SystemInfosManager.hpp"

#include <chrono>
#include <iostream>
#include <vector>

class PerformanceTester {
public:
    PerformanceTester(std::shared_ptr<ob::DeviceList> devList);
    ~PerformanceTester();

    void startTesting();
    void writeSystemInfosToFile(const std::string &configName, const std::string &fileName);

private:
    std::vector<std::shared_ptr<DeviceResource>> deviceResources_;
    std::shared_ptr<SystemInfosManager>          systemInfosManager_;

    CSVFile summary_;
};