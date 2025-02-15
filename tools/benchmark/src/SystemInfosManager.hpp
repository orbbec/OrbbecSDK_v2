// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#if (defined(_WIN32) || defined(_WIN64))
#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <direct.h>
#include <process.h>
#include <tchar.h>
#include <conio.h>
#elif defined(__linux__)
#include <fstream>
#include <string>
#include <unistd.h>
#endif

#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>

#include "CsvFile.hpp"

class SystemInfosManager {
public:
    SystemInfosManager() : running_(false) {};
    ~SystemInfosManager();

    void startCpuMonitoring();
    void stopCpuMonitoring();

    /**
     * @brief Get the current time as a string in the format HH:MM:SS
     *
     * @return The current time as a string in the format HH:MM:SS
     */
    std::string getCurrentTimeHMS();

#if (defined(_WIN32) || defined(_WIN64))
    uint64_t convertTimeFormat(const FILETIME *ftime);

    /**
     * @brief Get the cpu usage of the process as a percentage
     *
     * @return The cpu usage of the process as a percentage
     */
    float getCpuUsage();

    /**
     * @brief Get the memory usage of the process in megabytes
     *
     * @return The memory usage of the process in megabytes
     */
    float getMemoryUsage();

#elif defined(__linux__)
    /**
     * @brief Get the cpu usage of the process as a percentage
     *
     * @return The cpu usage of the process as a percentage
     */
    float getCpuUsage();

    /**
     * @brief Get the memory usage of the process in megabytes
     *
     * @return The memory usage of the process in megabytes
     */
    float getMemoryUsage();
#endif
    const std::vector<SystemInfo>& getData() const;

private:
    void monitoringLoop();

private:
    std::thread       monitoringThread_;
    std::atomic<bool> running_;

    std::vector<SystemInfo> data_;
};