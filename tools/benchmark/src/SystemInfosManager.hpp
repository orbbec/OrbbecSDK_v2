// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#if(defined(_WIN32) || defined(_WIN64))
#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <direct.h>
#include <process.h>
#include <tchar.h>
#include <conio.h>
#elif defined(__linux__) || defined(__APPLE__)
#include <fstream>
#include <string>
#include <unistd.h>
#endif

#include <array>
#include <memory>
#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>

#include "CsvFile.hpp"

struct GpuInfo {
    GpuInfo(float usage = -1.0f, float memoryUsedMB = -1.0f) : usage(usage), memoryUsedMB(memoryUsedMB) {}

    float usage;
    float memoryUsedMB;
};

class SystemInfosManager {
public:
    SystemInfosManager() : running_(false){};
    ~SystemInfosManager();

    void startCpuMonitoring();
    void stopCpuMonitoring();

    /**
     * @brief Get the current time as a string in the format HH:MM:SS
     *
     * @return The current time as a string in the format HH:MM:SS
     */
    std::string getCurrentTimeHMS();

#if(defined(_WIN32) || defined(_WIN64))
    uint64_t convertTimeFormat(const FILETIME *ftime);
#endif

    /**
     * @brief Get the CPU usage of the process as a percentage
     *
     * @return The CPU usage of the process as a percentage
     */
    float getCpuUsage();

    /**
     * @brief Get the memory used by the process in megabytes
     *
     * @return The memory used by the process in megabytes
     */
    float getMemoryUsage();

    /**
     * @brief Get the system GPU usage percentage and system graphics memory used in MB.
     *
     * @return The system GPU usage information.
     */
    GpuInfo getGpuInfo();

    const std::vector<SystemInfo> &getData() const;

private:
    void monitoringLoop();

#if defined(__linux__)
    enum class GpuUsageBackend {
        Unknown,
        NvidiaSmi,
        DevfreqLoad,
        DrmGpuBusyPercent,
        DrmGtBusyPercent,
        TegraStats,
        Unavailable,
    };

    enum class GpuMemoryBackend {
        Unknown,
        NvidiaSmi,
        DrmVram,
        DrmLmem,
        TegraStats,
        Unavailable,
    };

    void probeGpuBackends();
#endif

private:
    std::thread       monitoringThread_;
    std::atomic<bool> running_;

    std::vector<SystemInfo> data_;

#if defined(__linux__)
    bool             gpuBackendProbed_ = false;
    GpuUsageBackend  gpuUsageBackend_  = GpuUsageBackend::Unknown;
    GpuMemoryBackend gpuMemoryBackend_ = GpuMemoryBackend::Unknown;
#endif
};