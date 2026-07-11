// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "SystemInfosManager.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <regex>

namespace {

std::string trim(const std::string &value) {
    const auto begin = value.find_first_not_of(" \t\r\n");
    if(begin == std::string::npos) {
        return "";
    }
    const auto end = value.find_last_not_of(" \t\r\n");
    return value.substr(begin, end - begin + 1);
}

bool runCommand(const std::string &command, std::string &output) {
#if defined(_WIN32) || defined(_WIN64)
    auto pipe = _popen(command.c_str(), "r");
#else
    auto pipe = popen(command.c_str(), "r");
#endif
    if(!pipe) {
        return false;
    }

    output.clear();
    char buffer[256] = { 0 };
    while(fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
    }

#if defined(_WIN32) || defined(_WIN64)
    _pclose(pipe);
#else
    pclose(pipe);
#endif
    return true;
}

bool tryParseFloat(const std::string &text, float &value) {
    auto trimmed = trim(text);
    if(trimmed.empty()) {
        return false;
    }

    char *end = nullptr;
    errno     = 0;
    value     = std::strtof(trimmed.c_str(), &end);
    return end != trimmed.c_str() && errno == 0;
}

bool tryParseInt64(const std::string &text, int64_t &value) {
    auto trimmed = trim(text);
    if(trimmed.empty()) {
        return false;
    }

    char *end = nullptr;
    errno     = 0;
    value     = std::strtoll(trimmed.c_str(), &end, 10);
    return end != trimmed.c_str() && errno == 0;
}

float clampPercentage(float value) {
    if(value < 0.0f) {
        return -1.0f;
    }
    return (std::max)(0.0f, (std::min)(100.0f, value));
}

float normalizeDevfreqLoad(float value) {
    if(value < 0.0f) {
        return -1.0f;
    }
    if(value > 100.0f) {
        value /= 10.0f;
    }
    return clampPercentage(value);
}

float bytesToMB(double bytes) {
    if(bytes < 0.0) {
        return -1.0f;
    }
    return static_cast<float>(bytes / (1024.0 * 1024.0));
}

std::vector<std::string> split(const std::string &text, char delimiter) {
    std::vector<std::string> result;
    std::stringstream        stream(text);
    std::string              item;
    while(std::getline(stream, item, delimiter)) {
        result.emplace_back(trim(item));
    }
    return result;
}

bool extractRegexFloat(const std::string &text, const std::string &pattern, float &value);

GpuInfo queryNvidiaSmiGpuInfo() {
    GpuInfo     gpuInfo;
    std::string output;

    if(!runCommand("nvidia-smi --query-gpu=utilization.gpu,memory.used --format=csv,noheader,nounits 2>/dev/null | head -n 1", output)) {
        return gpuInfo;
    }

    auto columns = split(output, ',');
    if(columns.size() < 2) {
        return gpuInfo;
    }

    float usage      = -1.0f;
    float usedMemory = -1.0f;
    if(tryParseFloat(columns[0], usage)) {
        gpuInfo.usage = clampPercentage(usage);
    }
    if(tryParseFloat(columns[1], usedMemory)) {
        gpuInfo.memoryUsedMB = usedMemory;
    }

    return gpuInfo;
}

float queryDrmGpuBusyPercent() {
    std::string output;
    float       usage = -1.0f;
    if(runCommand("cat /sys/class/drm/card*/device/gpu_busy_percent 2>/dev/null | head -n 1", output) && tryParseFloat(output, usage)) {
        return clampPercentage(usage);
    }
    return -1.0f;
}

float queryDevfreqGpuLoadPercent() {
    constexpr int  kSampleCount    = 6;
    constexpr auto kSampleInterval = std::chrono::milliseconds(20);

    float maxUsage = -1.0f;
    for(int i = 0; i < kSampleCount; ++i) {
        std::string output;
        float       usage = -1.0f;
        if(runCommand("cat /sys/class/devfreq/*gpu*/device/load /sys/class/devfreq/*gpu*/load /sys/devices/gpu.0/load 2>/dev/null | head -n 1", output)
           && tryParseFloat(output, usage)) {
            maxUsage = (std::max)(maxUsage, normalizeDevfreqLoad(usage));
            if(maxUsage >= 100.0f) {
                break;
            }
        }

        if(i + 1 < kSampleCount) {
            std::this_thread::sleep_for(kSampleInterval);
        }
    }

    return maxUsage;
}

float queryDrmGtBusyPercent() {
    std::string output;
    float       usage = -1.0f;
    if(runCommand("cat /sys/class/drm/card*/gt_busy_percent /sys/class/drm/card*/device/gt_busy_percent 2>/dev/null | head -n 1", output)
       && tryParseFloat(output, usage)) {
        return clampPercentage(usage);
    }
    return -1.0f;
}

float queryDrmVramUsage() {
    std::string output;
    int64_t     usedBytes = 0;
    if(runCommand("cat /sys/class/drm/card*/device/mem_info_vram_used 2>/dev/null | head -n 1", output) && tryParseInt64(output, usedBytes)) {
        return bytesToMB(static_cast<double>(usedBytes));
    }
    return -1.0f;
}

float queryDrmLmemUsage() {
    std::string output;
    int64_t     usedBytes = 0;
    if(runCommand("cat /sys/class/drm/card*/device/lmem_used_bytes 2>/dev/null | head -n 1", output) && tryParseInt64(output, usedBytes)) {
        return bytesToMB(static_cast<double>(usedBytes));
    }
    return -1.0f;
}

GpuInfo queryTegraStatsGpuInfo() {
    GpuInfo     gpuInfo;
    std::string output;

    if(!runCommand("timeout 2 tegrastats --interval 1000 2>/dev/null | head -n 1", output)) {
        return gpuInfo;
    }

    float usage = -1.0f;
    if(extractRegexFloat(output, R"(GR3D_FREQ\s+([0-9]+)%)", usage)) {
        gpuInfo.usage = clampPercentage(usage);
    }

    return gpuInfo;
}

bool extractRegexFloat(const std::string &text, const std::string &pattern, float &value) {
    std::smatch match;
    if(!std::regex_search(text, match, std::regex(pattern)) || match.size() < 2) {
        return false;
    }
    return tryParseFloat(match[1].str(), value);
}

#if defined(__APPLE__)
bool extractRegexInt64(const std::string &text, const std::string &pattern, int64_t &value) {
    std::smatch match;
    if(!std::regex_search(text, match, std::regex(pattern)) || match.size() < 2) {
        return false;
    }
    return tryParseInt64(match[1].str(), value);
}

bool extractRegexMemorySizeBytes(const std::string &text, const std::string &pattern, int64_t &value) {
    std::smatch match;
    if(!std::regex_search(text, match, std::regex(pattern)) || match.size() < 3) {
        return false;
    }

    float numeric = 0.0f;
    if(!tryParseFloat(match[1].str(), numeric)) {
        return false;
    }

    auto unit = trim(match[2].str());
    std::transform(unit.begin(), unit.end(), unit.begin(), ::toupper);
    if(unit == "GB") {
        value = static_cast<int64_t>(numeric * 1024.0f * 1024.0f * 1024.0f);
        return true;
    }
    if(unit == "MB") {
        value = static_cast<int64_t>(numeric * 1024.0f * 1024.0f);
        return true;
    }
    if(unit == "KB") {
        value = static_cast<int64_t>(numeric * 1024.0f);
        return true;
    }
    if(unit == "B") {
        value = static_cast<int64_t>(numeric);
        return true;
    }
    return false;
}
#endif

}  // namespace

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
#if defined(__GNUC__) && (__GNUC__ >= 5)
    std::ostringstream oss;
    oss << std::put_time(&now_tm, "%H:%M:%S");
    return oss.str();
#else
    char buf[32] = { 0 };
    std::strftime(buf, sizeof(buf), "%H:%M:%S", &now_tm);
    return std::string(buf);
#endif
}

#if (defined(_WIN32) || defined(_WIN64))
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

GpuInfo SystemInfosManager::getGpuInfo() {
    GpuInfo     gpuInfo;
    std::string output;

    runCommand(
        R"(powershell -NoProfile -Command "$value = -1; try { $samples = (Get-Counter '\GPU Engine(*)\Utilization Percentage').CounterSamples | Where-Object { $_.InstanceName -match 'engtype_3D' }; if($samples) { $value = ($samples | Measure-Object -Property CookedValue -Sum).Sum; if($value -gt 100) { $value = 100 } } } catch {} ; Write-Output $value")",
        output);
    tryParseFloat(output, gpuInfo.usage);
    gpuInfo.usage = clampPercentage(gpuInfo.usage);

    runCommand(
        R"(powershell -NoProfile -Command "$value = -1; try { $used = (Get-CimInstance Win32_PerfFormattedData_GPUPerformanceCounters_GPUAdapterMemory | Measure-Object -Property DedicatedUsage -Sum).Sum; if($null -ne $used) { $value = [math]::Round($used / 1MB, 2) } } catch {} ; Write-Output $value")",
        output);
    tryParseFloat(output, gpuInfo.memoryUsedMB);

    return gpuInfo;
}

#elif defined(__linux__)

void SystemInfosManager::probeGpuBackends() {
    if(gpuBackendProbed_) {
        return;
    }

    auto nvidiaSmiGpuInfo = queryNvidiaSmiGpuInfo();
    if(nvidiaSmiGpuInfo.usage >= 0.0f) {
        gpuUsageBackend_ = GpuUsageBackend::NvidiaSmi;
    }
    if(nvidiaSmiGpuInfo.memoryUsedMB >= 0.0f) {
        gpuMemoryBackend_ = GpuMemoryBackend::NvidiaSmi;
    }

    if(gpuUsageBackend_ == GpuUsageBackend::Unknown && queryDevfreqGpuLoadPercent() >= 0.0f) {
        gpuUsageBackend_ = GpuUsageBackend::DevfreqLoad;
    }
    if(gpuUsageBackend_ == GpuUsageBackend::Unknown && queryDrmGpuBusyPercent() >= 0.0f) {
        gpuUsageBackend_ = GpuUsageBackend::DrmGpuBusyPercent;
    }
    if(gpuUsageBackend_ == GpuUsageBackend::Unknown && queryDrmGtBusyPercent() >= 0.0f) {
        gpuUsageBackend_ = GpuUsageBackend::DrmGtBusyPercent;
    }

    if(gpuMemoryBackend_ == GpuMemoryBackend::Unknown && queryDrmVramUsage() >= 0.0f) {
        gpuMemoryBackend_ = GpuMemoryBackend::DrmVram;
    }
    if(gpuMemoryBackend_ == GpuMemoryBackend::Unknown && queryDrmLmemUsage() >= 0.0f) {
        gpuMemoryBackend_ = GpuMemoryBackend::DrmLmem;
    }

    if(gpuUsageBackend_ == GpuUsageBackend::Unknown || gpuMemoryBackend_ == GpuMemoryBackend::Unknown) {
        auto tegraGpuInfo = queryTegraStatsGpuInfo();
        if(gpuUsageBackend_ == GpuUsageBackend::Unknown && tegraGpuInfo.usage >= 0.0f) {
            gpuUsageBackend_ = GpuUsageBackend::TegraStats;
        }
    }

    if(gpuUsageBackend_ == GpuUsageBackend::Unknown) {
        gpuUsageBackend_ = GpuUsageBackend::Unavailable;
    }
    if(gpuMemoryBackend_ == GpuMemoryBackend::Unknown) {
        gpuMemoryBackend_ = GpuMemoryBackend::Unavailable;
    }

    gpuBackendProbed_ = true;
}

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

GpuInfo SystemInfosManager::getGpuInfo() {
    GpuInfo gpuInfo;

    probeGpuBackends();

    if(gpuUsageBackend_ == GpuUsageBackend::NvidiaSmi || gpuMemoryBackend_ == GpuMemoryBackend::NvidiaSmi) {
        auto nvidiaSmiGpuInfo = queryNvidiaSmiGpuInfo();
        if(gpuUsageBackend_ == GpuUsageBackend::NvidiaSmi) {
            gpuInfo.usage = nvidiaSmiGpuInfo.usage;
        }
        if(gpuMemoryBackend_ == GpuMemoryBackend::NvidiaSmi) {
            gpuInfo.memoryUsedMB = nvidiaSmiGpuInfo.memoryUsedMB;
        }
    }

    switch(gpuUsageBackend_) {
    case GpuUsageBackend::DevfreqLoad:
        gpuInfo.usage = queryDevfreqGpuLoadPercent();
        break;
    case GpuUsageBackend::DrmGpuBusyPercent:
        gpuInfo.usage = queryDrmGpuBusyPercent();
        break;
    case GpuUsageBackend::DrmGtBusyPercent:
        gpuInfo.usage = queryDrmGtBusyPercent();
        break;
    case GpuUsageBackend::TegraStats:
        if(gpuInfo.usage < 0.0f) {
            gpuInfo.usage = queryTegraStatsGpuInfo().usage;
        }
        break;
    default:
        break;
    }

    switch(gpuMemoryBackend_) {
    case GpuMemoryBackend::DrmVram:
        gpuInfo.memoryUsedMB = queryDrmVramUsage();
        break;
    case GpuMemoryBackend::DrmLmem:
        gpuInfo.memoryUsedMB = queryDrmLmemUsage();
        break;
    default:
        break;
    }

    return gpuInfo;
}

#elif defined(__APPLE__)

float SystemInfosManager::getCpuUsage() {
    auto               pid    = getpid();
    const std::string  header = "usage";
    std::ostringstream command;

    command << "ps -p " << pid << " -o %cpu=" << header;

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

    std::string           result;
    std::array<char, 128> buffer;
    while(fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }

    std::istringstream outStream(result);
    std::string        line;
    float              usage = -1.0f;
    while(std::getline(outStream, line)) {
        size_t pos = line.find(header);

        if(pos != std::string::npos) {
            // header
            continue;
        }
        try {
            usage = std::stof(line);
            return usage;
        }
        catch(const std::exception &e) {
            std::cerr << "Error reading usage: " << e.what() << std::endl;
            break;
        }
    }

    std::cerr << "Failed to read CPU usage" << std::endl;
    return -1.0f;
}

float SystemInfosManager::getMemoryUsage() {
    auto               pid    = getpid();
    const std::string  header = "usage";
    std::ostringstream command;

    command << "ps -p " << pid << " -o rss=" << header;

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

    std::string           result;
    std::array<char, 128> buffer;
    while(fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }

    std::istringstream outStream(result);
    std::string        line;
    float              usage = -1.0f;
    while(std::getline(outStream, line)) {
        size_t pos = line.find(header);

        if(pos != std::string::npos) {
            // header
            continue;
        }
        try {
            usage = std::stof(line);
            return usage / 1024.0f;  // to MB
        }
        catch(const std::exception &e) {
            std::cerr << "Error reading usage: " << e.what() << std::endl;
            break;
        }
    }

    std::cerr << "Failed to read memory usage" << std::endl;
    return -1.0f;
}

GpuInfo SystemInfosManager::getGpuInfo() {
    GpuInfo     gpuInfo;
    std::string output;

    if(!runCommand("ioreg -r -d 1 -w 0 -c IOAccelerator 2>/dev/null", output) || output.empty()) {
        runCommand("ioreg -r -d 1 -w 0 -c AGXAccelerator 2>/dev/null", output);
    }

    if(!output.empty()) {
        float usage = -1.0f;
        if(extractRegexFloat(output, R"("Device Utilization %"\s*=\s*([0-9]+))", usage)
           || extractRegexFloat(output, R"(Device Utilization %\s*=\s*([0-9]+))", usage)
           || extractRegexFloat(output, R"("GPU Core Utilization"\s*=\s*([0-9]+))", usage)) {
            gpuInfo.usage = clampPercentage(usage);
        }

        int64_t usedBytes = 0;
        if(extractRegexInt64(output, R"(vramUsedBytes"?\s*=\s*([0-9]+))", usedBytes)
           || extractRegexInt64(output, R"(inUseVidMemoryBytes"?\s*=\s*([0-9]+))", usedBytes)
           || extractRegexInt64(output, R"(inUseSystemMemoryBytes"?\s*=\s*([0-9]+))", usedBytes)) {
            gpuInfo.memoryUsedMB = bytesToMB(static_cast<double>(usedBytes));
        }
    }

    return gpuInfo;
}

#else

float SystemInfosManager::getCpuUsage() {
    std::cerr << "getMemoryUsage: unsupported os" << std::endl;
    return -1.0f;
}

float SystemInfosManager::getMemoryUsage() {
    std::cerr << "getMemoryUsage: unsupported os" << std::endl;
    return -1.0f;
}

GpuInfo SystemInfosManager::getGpuInfo() {
    std::cerr << "getGpuInfo: unsupported os" << std::endl;
    return GpuInfo();
}

#endif

const std::vector<SystemInfo> &SystemInfosManager::getData() const {
    return data_;
}

void SystemInfosManager::monitoringLoop() {
    while(running_) {
        float       cpuUsage    = getCpuUsage();
        float       memUsage    = getMemoryUsage();
        auto        gpuInfo     = getGpuInfo();
        std::string currentTime = getCurrentTimeHMS();

        std::cout << "Process CPU usage: " << std::fixed << std::setprecision(2) << cpuUsage << "% | Process memory used: " << memUsage << " MB"
                  << " | System GPU usage: " << gpuInfo.usage << "%"
                  << " | System graphics memory used: ";
        if(gpuInfo.memoryUsedMB >= 0.0f) {
            std::cout << gpuInfo.memoryUsedMB << " MB";
        }
        else {
            std::cout << "N/A";
        }
        std::cout << std::endl;
        std::cout.unsetf(std::ios::fixed);

        data_.emplace_back(currentTime, cpuUsage, memUsage, gpuInfo.usage, gpuInfo.memoryUsedMB);

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}
