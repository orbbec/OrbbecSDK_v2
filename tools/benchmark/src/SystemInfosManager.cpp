// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "SystemInfosManager.hpp"

SystemInfosManager::~SystemInfosManager() {
    stopCpuMonitoring();
    data_.clear();
}

void SystemInfosManager::startCpuMonitoring() {
    data_.clear();
    running_          = true;
    monitoringThread_ = std::thread(&SystemInfosManager::monitoringLoop, this);
}

void SystemInfosManager::stopCpuMonitoring() {
    running_ = false;

    if(monitoringThread_.joinable()) {
        monitoringThread_.join();
    }
}

std::string SystemInfosManager::getCurrentTimeHMS() {
    // Get current time as time_point
    auto now = std::chrono::system_clock::now();

    // Convert to time_t for formatting
    std::time_t now_time_t = std::chrono::system_clock::to_time_t(now);

    // Convert to tm structure for local time
    std::tm now_tm;
#ifdef _WIN32
    localtime_s(&now_tm, &now_time_t);  // Thread-safe on Windows
#else
    localtime_r(&now_time_t, &now_tm);  // Thread-safe on POSIX
#endif

    // Format the time into a stringstream (HH:MM:SS)
    std::ostringstream oss;
    oss << std::put_time(&now_tm, "%H:%M:%S");

    return oss.str();
}

#if(defined(_WIN32) || defined(_WIN64))
uint64_t SystemInfosManager::convertTimeFormat(const FILETIME *ftime) {
    LARGE_INTEGER li;

    li.LowPart  = ftime->dwLowDateTime;
    li.HighPart = ftime->dwHighDateTime;
    return li.QuadPart;
}

float SystemInfosManager::getCpuUsage() {
    // Static variables to store previous system and process times
    static int64_t last_time        = 0;
    static int64_t last_system_time = 0;

    FILETIME now;
    FILETIME creation_time;
    FILETIME exit_time;
    FILETIME kernel_time;
    FILETIME user_time;
    int64_t  system_time;
    int64_t  time;
    int64_t  system_time_delta;
    int64_t  time_delta;

    // get cpu num
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    int cpu_num = info.dwNumberOfProcessors;

    float cpu_ratio = 0.0;

    // use GetCurrentProcess() can get current process and no need to close handle
    HANDLE process = GetCurrentProcess();

    // get now time
    GetSystemTimeAsFileTime(&now);

    if(!GetProcessTimes(process, &creation_time, &exit_time, &kernel_time, &user_time)) {
        // We don't assert here because in some cases (such as in the Task Manager)
        // we may call this function on a process that has just exited but we have
        // not yet received the notification.
        printf("GetCpuUsageRatio GetProcessTimes failed\n");
        return 0.0;
    }

    // should handle the multiple cpu num
    system_time = (convertTimeFormat(&kernel_time) + convertTimeFormat(&user_time)) / cpu_num;
    time        = convertTimeFormat(&now);

    if((last_system_time == 0) || (last_time == 0)) {
        // First call, just set the last values.
        last_system_time = system_time;
        last_time        = time;
        return 0.0;
    }

    system_time_delta = system_time - last_system_time;
    time_delta        = time - last_time;

    if(time_delta == 0) {
        printf("GetCpuUsageRatio time_delta is 0, error\n");
        return 0.0;
    }

    // We add time_delta / 2 so the result is rounded.
    cpu_ratio = (float)((static_cast<float>(system_time_delta) * 100.0 + static_cast<float>(time_delta) / 2.0) / static_cast<float>(time_delta));  // the % unit
    last_system_time = system_time;
    last_time        = time;

    return cpu_ratio;
}

float SystemInfosManager::getMemoryUsage() {
    PROCESS_MEMORY_COUNTERS memCounters;
    // Retrieve memory information for the current process
    if(GetProcessMemoryInfo(GetCurrentProcess(), &memCounters, sizeof(memCounters))) {
        // WorkingSetSize is the current size of the working set in bytes
        float memoryUsageMB = static_cast<float>(memCounters.WorkingSetSize) / (1024.0f * 1024.0f);

        return memoryUsageMB;
    }
    else {
        std::cerr << "Failed to get process memory info." << std::endl;

        return -1.0f;
    }
}

#elif defined(__linux__)

float SystemInfosManager::getCpuUsage() {
    auto               pid = getpid();
    std::ostringstream command;

    command << "top -bn1 -p " << pid << "| grep " << pid << " | awk '{print $9}'";

    auto deleter = [](FILE *file) {
        if(file) {
            pclose(file);
        }
    };
    std::unique_ptr<FILE, decltype(deleter)> pipe(popen(command.str().c_str(), "r"), deleter);
    if(!pipe) {
        std::cerr << "Failed to run command" << std::endl;
        return -1.0f;
    }

    char buffer[128];
    if(fgets(buffer, sizeof(buffer), pipe.get()) != nullptr) {
        return std::strtof(buffer, nullptr);
    }

    std::cerr << "Failed to read CPU usage" << std::endl;
    return -1.0f;
}

float SystemInfosManager::getMemoryUsage() {
    std::ifstream status_file("/proc/self/status");
    if(!status_file.is_open()) {
        std::cerr << "Error opening status file" << std::endl;
        return -1;
    }

    std::string line;
    int64_t     rss = -1;
    while(std::getline(status_file, line)) {
        std::istringstream line_stream(line);
        std::string        key;
        line_stream >> key;

        if(key == "VmRSS:") {
            line_stream >> rss;
            break;
        }
    }

    if(rss == -1) {
        std::cerr << "VmRSS field not found" << std::endl;
    }
    return rss / 1024.0f;  // to MB
}
#endif

const std::vector<SystemInfo> &SystemInfosManager::getData() const {
    return data_;
}

void SystemInfosManager::monitoringLoop() {
    while(running_) {
        float       cpuUsage    = getCpuUsage();
        float       memUsage    = getMemoryUsage();
        std::string currentTime = getCurrentTimeHMS();

        std::cout << "CPU usage: " << std::fixed << std::setprecision(2) << cpuUsage << "% | Memory usage: " << memUsage << "MB" << std::endl;
        std::cout.unsetf(std::ios::fixed);

        data_.emplace_back(currentTime, cpuUsage, memUsage);

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}