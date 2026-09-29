// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "FramePairingManager.hpp"
#include "PipelineHolder.hpp"
#include "utils.hpp"
#include "utils/cJSON.h"
#include "utils_opencv.hpp"
#include <libobsensor/ObSensor.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <thread>
#include <unordered_map>
#include <vector>

#if defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
#include <direct.h>
#define MKDIR(path) _mkdir(path)
#else
#include <unistd.h>
#define MKDIR(path) ::mkdir(path, 0755)
#endif

#define KEY_ESC 27

constexpr const char *CONFIG_FILE = "./MultiDeviceSyncConfig.json";

struct DeviceConfigInfo {
    std::string             deviceSN;
    OBMultiDeviceSyncConfig syncConfig;
    StreamProfileRequest    depthProfile;
    StreamProfileRequest    colorProfile;
};

struct LatchedTs {
    uint64_t frame;
    uint64_t global;
    uint64_t system;
};

ob::Context                                    context;
std::vector<std::shared_ptr<ob::Device>>       streamDevList;
std::vector<std::shared_ptr<ob::Device>>       configDevList;
std::vector<std::shared_ptr<DeviceConfigInfo>> deviceConfigList;
std::vector<std::shared_ptr<PipelineHolder>>   pipelineHolderList;
static bool                                    gHeartbeatEnable = false;

static std::atomic<bool> quitStreamPreview(false);
static std::atomic<bool> sigintFlag(false);

bool                  loadConfigFile();
int                   configMultiDeviceSync();
void                  startDeviceStreams(const std::vector<std::shared_ptr<ob::Device>> &devices, int startIndex);
std::string           OBSyncModeToString(const OBMultiDeviceSyncMode syncMode);
OBMultiDeviceSyncMode stringToOBSyncMode(const std::string &modeString);
std::string           readFileContent(const char *filePath);
int                   strcmpNocase(const char *str0, const char *str1);
void                  handleKeyPress(int key);
int                   testMultiDeviceSync(bool headless);
void                  onSigint(int);

int main(int argc, char *argv[]) try {
    bool                      headless = false;
    int                       choice;
    int                       exitValue      = 0;
    constexpr std::streamsize maxInputIgnore = 10000;

    for(int i = 1; i < argc; i++) {
        if(std::string(argv[i]) == "--headless") {
            headless = true;
        }
    }

    std::signal(SIGINT, onSigint);

    while(true) {
        std::cout << "\n--------------------------------------------------\n";
        std::cout << "Please select options: \n";
        std::cout << " 0 --> config devices sync mode. \n";
        std::cout << " 1 --> start stream \n";
        std::cout << "--------------------------------------------------\n";
        std::cout << "Please select input: ";
        if(!(std::cin >> choice)) {
            if(std::cin.eof()) {
                std::cout << "Input stream closed (EOF), exiting." << std::endl;
                break;
            }
            std::cin.clear();
            std::cin.ignore(maxInputIgnore, '\n');
            std::cout << "Invalid input. Please enter a number [0~1]" << std::endl;
            continue;
        }
        std::cout << std::endl;

        switch(choice) {
        case 0:
            exitValue = configMultiDeviceSync();
            if(exitValue == 0) {
                std::cout << "Config MultiDeviceSync Success. \n" << std::endl;
                exitValue = testMultiDeviceSync(headless);
            }
            break;
        case 1:
            std::cout << "\nStart Devices video stream." << std::endl;
            exitValue = testMultiDeviceSync(headless);
            break;
        default:
            break;
        }

        if(exitValue == 0) {
            break;
        }
    }
    return exitValue;
}
catch(ob::Error &e) {
    std::cerr << "function:" << e.getName() << "\nargs:" << e.getArgs() << "\nmessage:" << e.what() << "\nstatus:" << e.getStatus()
              << "\ntype:" << e.getExceptionType() << std::endl;
    std::cout << "\nPress any key to exit.";
    ob_smpl::waitForKeyPressed();
    exit(EXIT_FAILURE);
}
catch(std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    exit(EXIT_FAILURE);
}

int configMultiDeviceSync() {
    try {
        configDevList.clear();
        streamDevList.clear();
        if(!loadConfigFile()) {
            std::cout << "load config failed" << std::endl;
            return -1;
        }

        if(deviceConfigList.empty()) {
            std::cout << "DeviceConfigList is empty. please check config file: " << CONFIG_FILE << std::endl;
            return -1;
        }

        auto devList  = context.queryDeviceList();
        int  devCount = devList->deviceCount();
        for(int i = 0; i < devCount; i++) {
            configDevList.push_back(devList->getDevice(i));
        }

        if(configDevList.empty()) {
            std::cerr << "Device list is empty. please check device connection state" << std::endl;
            return -1;
        }

        int notFoundCount = 0;
        for(const auto &config: deviceConfigList) {
            auto it = std::find_if(configDevList.begin(), configDevList.end(), [&](const std::shared_ptr<ob::Device> &device) {
                auto sn = device->getDeviceInfo()->serialNumber();
                return strcmpNocase(sn, config->deviceSN.c_str()) == 0;
            });
            if(it == configDevList.end()) {
                std::cerr << "ERROR: Device SN " << config->deviceSN << " not found in connected devices!" << std::endl;
                notFoundCount++;
                continue;
            }

            auto device = (*it);
            device->setMultiDeviceSyncConfig(config->syncConfig);

            auto curConfig = device->getMultiDeviceSyncConfig();
            std::cout << "Sync config for SN " << config->deviceSN << ":" << std::endl;
            std::cout << "  syncMode: " << OBSyncModeToString(curConfig.syncMode) << std::endl;
            std::cout << "  depthDelayUs: " << (int)curConfig.depthDelayUs << std::endl;
            std::cout << "  colorDelayUs: " << (int)curConfig.colorDelayUs << std::endl;
            std::cout << "  trigger2ImageDelayUs: " << (int)curConfig.trigger2ImageDelayUs << std::endl;
            std::cout << "  triggerOutEnable: " << (curConfig.triggerOutEnable ? "true" : "false") << std::endl;
            std::cout << "  triggerOutDelayUs: " << (int)curConfig.triggerOutDelayUs << std::endl;
            std::cout << "  framesPerTrigger: " << (int)curConfig.framesPerTrigger << std::endl;

            streamDevList.push_back(device);
        }

        if(notFoundCount > 0) {
            std::cerr << notFoundCount << " device(s) not found. All devices must be connected." << std::endl;
            return -1;
        }

        return 0;
    }
    catch(ob::Error &e) {
        std::cerr << "function:" << e.getName() << "\nargs:" << e.getArgs() << "\nmessage:" << e.what() << "\nstatus:" << e.getStatus()
                  << "\ntype:" << e.getExceptionType() << std::endl;
        return -1;
    }
}

void startDeviceStreams(const std::vector<std::shared_ptr<ob::Device>> &devices, int startIndex) {
    for(auto &dev: devices) {
        auto holder   = std::make_shared<PipelineHolder>(dev, startIndex);
        auto sn       = dev->getDeviceInfo()->serialNumber();
        auto configIt = std::find_if(deviceConfigList.begin(), deviceConfigList.end(),
                                     [&](const std::shared_ptr<DeviceConfigInfo> &config) { return strcmpNocase(config->deviceSN.c_str(), sn) == 0; });
        if(configIt != deviceConfigList.end()) {
            holder->setStreamConfig((*configIt)->depthProfile, (*configIt)->colorProfile);
        }
        pipelineHolderList.push_back(holder);
        startIndex++;
    }
}

void handleKeyPress(int key) {
    if(key == 't' || key == 'T') {
        // Query each device's persisted sync config so manual trigger also works
        // when streams were started via menu 1 (config list not loaded this run).
        std::vector<std::shared_ptr<ob::Device>> swTriggerDevs;
        for(auto &dev: streamDevList) {
            try {
                if(dev->getMultiDeviceSyncConfig().syncMode == OB_MULTI_DEVICE_SYNC_MODE_SOFTWARE_TRIGGERING) {
                    swTriggerDevs.push_back(dev);
                }
            }
            catch(ob::Error &e) {
                std::cerr << "getMultiDeviceSyncConfig failed: " << e.what() << std::endl;
            }
        }
        if(swTriggerDevs.empty()) {
            std::cout << "No SOFTWARE_TRIGGERING device, skip manual trigger." << std::endl;
            return;
        }
        std::cout << "Manual trigger..." << std::endl;
        for(auto &dev: swTriggerDevs) {
            try {
                dev->triggerCapture();
            }
            catch(ob::Error &e) {
                std::cerr << "triggerCapture failed: " << e.what() << std::endl;
            }
        }
    }
    else if(key == 's' || key == 'S') {
        std::cout << "Syncing device clocks..." << std::endl;
        context.enableDeviceClockSync(0);
    }
}

void shutdownStreams() {
    gTimestampBuffer.setRecording(false);
    gTimestampBuffer.stopBackgroundFlush();
    gTimestampBuffer.release();

    std::cout << "\n========== Device Info (Stop) ==========" << std::endl;
    for(auto &dev: streamDevList) {
        try {
            auto info = dev->getDeviceInfo();
            std::cout << "  SN: " << info->serialNumber() << ", Name: " << info->name() << std::endl;
        }
        catch(const ob::Error &) {
        }
    }
    std::cout << "========================================\n" << std::endl;

    for(auto &holder: pipelineHolderList) {
        holder->stopStream();
    }
    pipelineHolderList.clear();
    streamDevList.clear();
    configDevList.clear();
    deviceConfigList.clear();
}

bool isSameMoment(const std::vector<LatchedTs> &ts, int64_t halfGapUs, bool useGlobal) {
    if(halfGapUs <= 0 || ts.empty()) {
        return false;
    }
    int64_t ref    = 0;
    bool    hasRef = false;
    for(auto &t: ts) {
        uint64_t v = useGlobal ? t.global : t.frame;
        if(v != 0) {
            ref    = (int64_t)v;
            hasRef = true;
            break;
        }
    }
    if(!hasRef) {
        return false;
    }
    for(auto &t: ts) {
        uint64_t v = useGlobal ? t.global : t.frame;
        if(v == 0) {
            return false;
        }
        int64_t d = (int64_t)v - ref;
        if(d < 0) {
            d = -d;
        }
        if(d > halfGapUs) {
            return false;
        }
    }
    return true;
}

static bool sameLatchedTs(const std::vector<LatchedTs> &a, const std::vector<LatchedTs> &b) {
    if(a.size() != b.size()) {
        return false;
    }
    for(size_t i = 0; i < a.size(); ++i) {
        if(a[i].frame != b[i].frame || a[i].global != b[i].global || a[i].system != b[i].system) {
            return false;
        }
    }
    return true;
}

void printSyncMonitor(const std::vector<LatchedTs> &depthTs, const std::vector<LatchedTs> &colorTs, int64_t halfGapUs, bool useGlobal,
                      std::vector<LatchedTs> &lastPrintedDepth, std::vector<LatchedTs> &lastPrintedColor) {
    bool colorOk = isSameMoment(colorTs, halfGapUs, useGlobal);
    bool depthOk = isSameMoment(depthTs, halfGapUs, useGlobal);
    if(!colorOk || !depthOk) {
        return;
    }
    // Skip printing when no new frames arrived since the last print (e.g. waiting for the next software trigger)
    if(sameLatchedTs(depthTs, lastPrintedDepth) && sameLatchedTs(colorTs, lastPrintedColor)) {
        return;
    }
    std::cout << "=================================================" << std::endl;
    size_t n = depthTs.size();
    for(size_t i = 0; i < n; ++i) {
        if(i < depthTs.size()) {
            std::cout << "Device#" << i << ", "
                      << " depth(us) "
                      << ", frame timestamp=" << depthTs[i].frame << ","
                      << "global timestamp = " << depthTs[i].global << ","
                      << "system timestamp = " << depthTs[i].system << std::endl;
        }
        if(i < colorTs.size()) {
            std::cout << "Device#" << i << ", "
                      << " color(us) "
                      << ", frame timestamp=" << colorTs[i].frame << ","
                      << "global timestamp = " << colorTs[i].global << ","
                      << "system timestamp = " << colorTs[i].system << std::endl;
        }
    }
    lastPrintedDepth = depthTs;
    lastPrintedColor = colorTs;
}

int testMultiDeviceSync(bool headless) {
    try {
        if(streamDevList.empty()) {
            auto devList  = context.queryDeviceList();
            int  devCount = devList->deviceCount();
            for(int i = 0; i < devCount; i++) {
                streamDevList.push_back(devList->getDevice(i));
            }
        }
        if(streamDevList.empty()) {
            std::cerr << "Device list is empty. Please check device connection state." << std::endl;
            return -1;
        }
        std::cout << "\n========== Device Info (Start) ==========" << std::endl;
        for(auto &dev: streamDevList) {
            auto info = dev->getDeviceInfo();
            std::cout << "  SN: " << info->serialNumber() << ", Name: " << info->name() << ", FW: " << info->firmwareVersion() << std::endl;
        }
        std::cout << "========================================\n" << std::endl;
        quitStreamPreview = false;
        sigintFlag.store(false);

        std::vector<std::shared_ptr<ob::Device>> primaryDevices;
        std::vector<std::shared_ptr<ob::Device>> secondaryDevices;
        for(auto &dev: streamDevList) {
            auto sn       = dev->getDeviceInfo()->serialNumber();
            auto configIt = std::find_if(deviceConfigList.begin(), deviceConfigList.end(),
                                         [&](const std::shared_ptr<DeviceConfigInfo> &config) { return strcmpNocase(config->deviceSN.c_str(), sn) == 0; });
            if(configIt != deviceConfigList.end()) {
                if((*configIt)->syncConfig.syncMode == OB_MULTI_DEVICE_SYNC_MODE_PRIMARY) {
                    primaryDevices.push_back(dev);
                }
                else {
                    secondaryDevices.push_back(dev);
                }
            }
            else {
                secondaryDevices.push_back(dev);
            }
        }

        bool useGlobalTimestamp = true;
        for(const auto &device: streamDevList) {
            if(device->isGlobalTimestampSupported()) {
                device->enableGlobalTimestamp(true);
                std::cout << "Enabled global timestamp for device: " << device->getDeviceInfo()->serialNumber() << std::endl;
            }
            else {
                useGlobalTimestamp = false;
                std::cout << "Global timestamp not supported for device: " << device->getDeviceInfo()->serialNumber() << std::endl;
            }
        }
        std::cout << (useGlobalTimestamp ? "Sync monitor: using global timestamp" : "Sync monitor: using device timestamp") << std::endl;

        for(const auto &device: streamDevList) {
            device->enableHeartbeat(gHeartbeatEnable);
            std::cout << "Heartbeat " << (gHeartbeatEnable ? "enabled" : "disabled") << " for device: " << device->getDeviceInfo()->serialNumber() << std::endl;
        }

        std::cout << "Syncing device clocks..." << std::endl;
        for(const auto &device: streamDevList) {
            try {
                device->timerSyncWithHost();
            }
            catch(const ob::Error &e) {
                std::cerr << "Sync device clock failed: " << e.what() << std::endl;
            }
        }

        // Start secondary devices first so they wait for the primary trigger
        std::cout << "Secondary devices start..." << std::endl;
        startDeviceStreams(secondaryDevices, 0);

        if(!primaryDevices.empty()) {
            std::cout << "Primary device start..." << std::endl;
            startDeviceStreams(primaryDevices, static_cast<int>(secondaryDevices.size()));
        }

        std::vector<std::string> deviceSNs;
        for(auto &h: pipelineHolderList) {
            deviceSNs.push_back(h->getSerialNumber());
        }
        const std::string outputDir = "./output";
        MKDIR(outputDir.c_str());
        gTimestampBuffer.init(deviceSNs, outputDir);

        // Per-device frame exchange buffers: callback writes, main loop reads
        std::vector<std::shared_ptr<ob::FrameSet>> latestFrames(pipelineHolderList.size());
        std::vector<std::mutex>                    frameMutexes(pipelineHolderList.size());

        for(auto &holder: pipelineHolderList) {
            auto h = holder;
            holder->setFrameCallback([h, &latestFrames, &frameMutexes](std::shared_ptr<ob::FrameSet> frameSet) {
                auto dFrame = frameSet->getFrame(OB_FRAME_DEPTH);
                if(dFrame) {
                    int64_t frameNum = -1;
                    if(dFrame->hasMetadata(OB_FRAME_METADATA_TYPE_FRAME_NUMBER)) {
                        frameNum = dFrame->getMetadataValue(OB_FRAME_METADATA_TYPE_FRAME_NUMBER);
                    }
                    gTimestampBuffer.pushDepthFrame(h->getDeviceIndex(), frameNum, dFrame->getIndex(), dFrame->getSystemTimeStampUs(), dFrame->getTimeStampUs(),
                                                    dFrame->getGlobalTimeStampUs());
                }
                auto cFrame = frameSet->getFrame(OB_FRAME_COLOR);
                if(cFrame) {
                    int64_t frameNum = -1;
                    if(cFrame->hasMetadata(OB_FRAME_METADATA_TYPE_FRAME_NUMBER)) {
                        frameNum = cFrame->getMetadataValue(OB_FRAME_METADATA_TYPE_FRAME_NUMBER);
                    }
                    gTimestampBuffer.pushColorFrame(h->getDeviceIndex(), frameNum, cFrame->getIndex(), cFrame->getSystemTimeStampUs(), cFrame->getTimeStampUs(),
                                                    cFrame->getGlobalTimeStampUs());
                }

                // Store latest frame for rendering (thread-safe write)
                size_t renderIdx = static_cast<size_t>(h->getDeviceIndex());
                if(renderIdx < latestFrames.size()) {
                    std::lock_guard<std::mutex> lk(frameMutexes[renderIdx]);
                    latestFrames[renderIdx] = frameSet;
                }
            });
        }

        // Now start all pipelines (callbacks already registered)
        gTimestampBuffer.setRecording(true);
        std::cout << "Starting all device streams..." << std::endl;
        for(auto &holder: pipelineHolderList) {
            holder->startStream();
        }

        gTimestampBuffer.startBackgroundFlush();

        // Latch latest timestamps and detect frame rate from the first color frame
        auto latchTimestamps = [&](std::vector<LatchedTs> &depthTs, std::vector<LatchedTs> &colorTs, uint32_t &streamFps, int64_t &halfGapUs) {
            for(size_t i = 0; i < pipelineHolderList.size(); i++) {
                std::shared_ptr<ob::FrameSet> frameSet;
                {
                    std::lock_guard<std::mutex> lk(frameMutexes[i]);
                    frameSet = latestFrames[i];
                }
                if(!frameSet) {
                    continue;
                }

                auto depthFrame = frameSet->getFrame(OB_FRAME_DEPTH);
                auto colorFrame = frameSet->getFrame(OB_FRAME_COLOR);

                if(depthFrame) {
                    depthTs[i] = LatchedTs{ depthFrame->getTimeStampUs(), depthFrame->getGlobalTimeStampUs(), depthFrame->getSystemTimeStampUs() };
                }
                if(colorFrame) {
                    colorTs[i] = LatchedTs{ colorFrame->getTimeStampUs(), colorFrame->getGlobalTimeStampUs(), colorFrame->getSystemTimeStampUs() };
                }
                if(streamFps == 0 && colorFrame) {
                    auto sp = colorFrame->getStreamProfile();
                    if(sp && sp->is<ob::VideoStreamProfile>()) {
                        streamFps = sp->as<ob::VideoStreamProfile>()->getFps();
                        if(streamFps > 0) {
                            halfGapUs = 1000000LL / streamFps / 2;
                        }
                    }
                }
            }
        };

        std::vector<LatchedTs> depthTs(pipelineHolderList.size());
        std::vector<LatchedTs> colorTs(pipelineHolderList.size());
        std::vector<LatchedTs> lastPrintedDepthTs;
        std::vector<LatchedTs> lastPrintedColorTs;
        auto                   lastSyncLog = std::chrono::steady_clock::now();
        uint32_t               streamFps   = 0;
        int64_t                halfGapUs   = 0;

        if(headless) {
            std::cout << "Headless mode: no preview window. Press Ctrl+C to stop and exit." << std::endl;
            while(!quitStreamPreview.load()) {
                latchTimestamps(depthTs, colorTs, streamFps, halfGapUs);

                auto now = std::chrono::steady_clock::now();
                if(now - lastSyncLog >= std::chrono::seconds(1)) {
                    lastSyncLog = now;
                    printSyncMonitor(depthTs, colorTs, halfGapUs, useGlobalTimestamp, lastPrintedDepthTs, lastPrintedColorTs);
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
        else {
            ob_smpl::CVWindow win("MultiDeviceSyncViewer", 1600, 900, ob_smpl::ARRANGE_GRID);
            win.setKeyPrompt("'S': syncDevicesTime, 'T': software trigger, 'ESC': quit");
            win.setKeyPressedCallback([&](int key) { handleKeyPress(key); });
            win.setShowInfo(true);
            win.setShowSyncTimeInfo(true);

            std::vector<std::shared_ptr<const ob::Frame>> renderFrames;
            while(win.run() && !quitStreamPreview.load()) {
                latchTimestamps(depthTs, colorTs, streamFps, halfGapUs);

                for(size_t i = 0; i < pipelineHolderList.size(); i++) {
                    std::shared_ptr<ob::FrameSet> frameSet;
                    {
                        std::lock_guard<std::mutex> lk(frameMutexes[i]);
                        frameSet = latestFrames[i];
                    }
                    if(!frameSet) {
                        continue;
                    }

                    renderFrames.clear();
                    auto depthFrame = frameSet->getFrame(OB_FRAME_DEPTH);
                    auto colorFrame = frameSet->getFrame(OB_FRAME_COLOR);
                    if(depthFrame) {
                        renderFrames.push_back(depthFrame);
                    }
                    if(colorFrame) {
                        renderFrames.push_back(colorFrame);
                    }
                    if(!renderFrames.empty()) {
                        win.pushFramesToView(renderFrames, static_cast<int>(i + 1));
                    }
                }

                auto now = std::chrono::steady_clock::now();
                if(now - lastSyncLog >= std::chrono::seconds(1)) {
                    lastSyncLog = now;
                    printSyncMonitor(depthTs, colorTs, halfGapUs, useGlobalTimestamp, lastPrintedDepthTs, lastPrintedColorTs);
                }
            }
        }

        quitStreamPreview = true;
        shutdownStreams();
        return 0;
    }
    catch(ob::Error &e) {
        std::cerr << "function:" << e.getName() << "\nargs:" << e.getArgs() << "\nmessage:" << e.what() << "\nstatus:" << e.getStatus()
                  << "\ntype:" << e.getExceptionType() << std::endl;
        shutdownStreams();
        return -1;
    }
}

std::string readFileContent(const char *filePath) {
    std::ostringstream oss;
    std::ifstream      file(filePath, std::fstream::in);
    if(!file.is_open()) {
        std::cerr << "Failed to open file: " << filePath << std::endl;
        return "";
    }
    oss << file.rdbuf();
    file.close();
    return oss.str();
}

bool loadConfigFile() {
    deviceConfigList.clear();
    int                               deviceCount   = 0;
    std::shared_ptr<DeviceConfigInfo> devConfigInfo = nullptr;
    cJSON                            *deviceElem    = nullptr;

    auto content = readFileContent(CONFIG_FILE);
    if(content.empty()) {
        std::cerr << "load config file failed." << std::endl;
        return false;
    }

    cJSON *rootElem = cJSON_Parse(content.c_str());
    if(rootElem == nullptr) {
        const char *errMsg = cJSON_GetErrorPtr();
        if(errMsg) {
            std::cerr << "JSON parse error: " << std::string(errMsg) << std::endl;
        }
        cJSON_Delete(rootElem);
        return false;
    }

    cJSON *heartbeatElem = cJSON_GetObjectItem(rootElem, "heartbeatEnable");
    if(cJSON_IsBool(heartbeatElem)) {
        gHeartbeatEnable = (bool)heartbeatElem->valueint;
    }
    std::cout << "config: heartbeatEnable=" << (gHeartbeatEnable ? "true" : "false") << std::endl;

    cJSON *devicesElem = cJSON_GetObjectItem(rootElem, "devices");
    cJSON_ArrayForEach(deviceElem, devicesElem) {
        devConfigInfo = std::make_shared<DeviceConfigInfo>();
        memset(&devConfigInfo->syncConfig, 0, sizeof(devConfigInfo->syncConfig));
        devConfigInfo->syncConfig.syncMode = OB_MULTI_DEVICE_SYNC_MODE_FREE_RUN;

        cJSON *snElem = cJSON_GetObjectItem(deviceElem, "sn");
        if(cJSON_IsString(snElem) && snElem->valuestring != nullptr) {
            devConfigInfo->deviceSN = std::string(snElem->valuestring);
        }
        cJSON *deviceConfigElem = cJSON_GetObjectItem(deviceElem, "syncConfig");
        if(cJSON_IsObject(deviceConfigElem)) {
            cJSON *numberElem = nullptr;
            cJSON *strElem    = nullptr;
            cJSON *bElem      = nullptr;
            strElem           = cJSON_GetObjectItemCaseSensitive(deviceConfigElem, "syncMode");
            if(cJSON_IsString(strElem) && strElem->valuestring != nullptr) {
                try {
                    devConfigInfo->syncConfig.syncMode = stringToOBSyncMode(strElem->valuestring);
                    std::cout << "config[" << (deviceCount++) << "]: SN=" << std::string(devConfigInfo->deviceSN) << ", mode=" << strElem->valuestring
                              << std::endl;
                }
                catch(const std::invalid_argument &e) {
                    std::cerr << "Invalid syncMode '" << strElem->valuestring << "' for device SN: " << devConfigInfo->deviceSN << ". " << e.what()
                              << std::endl;
                    std::cerr << "Valid modes: OB_MULTI_DEVICE_SYNC_MODE_FREE_RUN / "
                                 "STANDALONE / PRIMARY / SECONDARY / SECONDARY_SYNCED / "
                                 "SOFTWARE_TRIGGERING / HARDWARE_TRIGGERING"
                              << std::endl;
                    cJSON_Delete(rootElem);
                    return false;
                }
            }
            numberElem = cJSON_GetObjectItemCaseSensitive(deviceConfigElem, "depthDelayUs");
            if(cJSON_IsNumber(numberElem)) {
                devConfigInfo->syncConfig.depthDelayUs = numberElem->valueint;
            }
            numberElem = cJSON_GetObjectItemCaseSensitive(deviceConfigElem, "colorDelayUs");
            if(cJSON_IsNumber(numberElem)) {
                devConfigInfo->syncConfig.colorDelayUs = numberElem->valueint;
            }
            numberElem = cJSON_GetObjectItemCaseSensitive(deviceConfigElem, "trigger2ImageDelayUs");
            if(cJSON_IsNumber(numberElem)) {
                devConfigInfo->syncConfig.trigger2ImageDelayUs = numberElem->valueint;
            }
            numberElem = cJSON_GetObjectItemCaseSensitive(deviceConfigElem, "triggerOutDelayUs");
            if(cJSON_IsNumber(numberElem)) {
                devConfigInfo->syncConfig.triggerOutDelayUs = numberElem->valueint;
            }
            bElem = cJSON_GetObjectItemCaseSensitive(deviceConfigElem, "triggerOutEnable");
            if(cJSON_IsBool(bElem)) {
                devConfigInfo->syncConfig.triggerOutEnable = (bool)bElem->valueint;
            }
            bElem = cJSON_GetObjectItemCaseSensitive(deviceConfigElem, "framesPerTrigger");
            if(cJSON_IsNumber(bElem)) {
                devConfigInfo->syncConfig.framesPerTrigger = bElem->valueint;
            }
        }

        cJSON *streamConfigElem = cJSON_GetObjectItem(deviceElem, "streamConfig");
        if(cJSON_IsObject(streamConfigElem)) {
            const char *sensorNames[] = { "depth", "color" };
            for(const char *sensorName: sensorNames) {
                StreamProfileRequest &request    = (strcmp(sensorName, "depth") == 0) ? devConfigInfo->depthProfile : devConfigInfo->colorProfile;
                cJSON                *sensorElem = cJSON_GetObjectItemCaseSensitive(streamConfigElem, sensorName);
                if(!cJSON_IsObject(sensorElem)) {
                    continue;
                }
                cJSON *numberElem = cJSON_GetObjectItemCaseSensitive(sensorElem, "width");
                if(cJSON_IsNumber(numberElem)) {
                    request.width = numberElem->valueint;
                }
                numberElem = cJSON_GetObjectItemCaseSensitive(sensorElem, "height");
                if(cJSON_IsNumber(numberElem)) {
                    request.height = numberElem->valueint;
                }
                numberElem = cJSON_GetObjectItemCaseSensitive(sensorElem, "fps");
                if(cJSON_IsNumber(numberElem)) {
                    request.fps = numberElem->valuedouble;
                }
                cJSON *strElem = cJSON_GetObjectItemCaseSensitive(sensorElem, "format");
                if(cJSON_IsString(strElem) && strElem->valuestring != nullptr) {
                    try {
                        stringToOBFormat(std::string(strElem->valuestring));
                        request.format = std::string(strElem->valuestring);
                    }
                    catch(const std::invalid_argument &e) {
                        std::cerr << "Invalid format '" << strElem->valuestring << "' for device SN: " << devConfigInfo->deviceSN << ". " << e.what()
                                  << std::endl;
                        cJSON_Delete(rootElem);
                        return false;
                    }
                }
            }
        }

        if(OB_MULTI_DEVICE_SYNC_MODE_FREE_RUN != devConfigInfo->syncConfig.syncMode) {
            deviceConfigList.push_back(devConfigInfo);
        }
        else {
            std::cerr << "Invalid sync mode of deviceSN: " << devConfigInfo->deviceSN << std::endl;
        }
        devConfigInfo = nullptr;
    }
    cJSON_Delete(rootElem);
    return true;
}

OBMultiDeviceSyncMode stringToOBSyncMode(const std::string &modeString) {
    static const std::unordered_map<std::string, OBMultiDeviceSyncMode> syncModeMap = {
        { "OB_MULTI_DEVICE_SYNC_MODE_FREE_RUN", OB_MULTI_DEVICE_SYNC_MODE_FREE_RUN },
        { "OB_MULTI_DEVICE_SYNC_MODE_STANDALONE", OB_MULTI_DEVICE_SYNC_MODE_STANDALONE },
        { "OB_MULTI_DEVICE_SYNC_MODE_PRIMARY", OB_MULTI_DEVICE_SYNC_MODE_PRIMARY },
        { "OB_MULTI_DEVICE_SYNC_MODE_SECONDARY", OB_MULTI_DEVICE_SYNC_MODE_SECONDARY },
        { "OB_MULTI_DEVICE_SYNC_MODE_SECONDARY_SYNCED", OB_MULTI_DEVICE_SYNC_MODE_SECONDARY_SYNCED },
        { "OB_MULTI_DEVICE_SYNC_MODE_SOFTWARE_TRIGGERING", OB_MULTI_DEVICE_SYNC_MODE_SOFTWARE_TRIGGERING },
        { "OB_MULTI_DEVICE_SYNC_MODE_HARDWARE_TRIGGERING", OB_MULTI_DEVICE_SYNC_MODE_HARDWARE_TRIGGERING }
    };
    auto it = syncModeMap.find(modeString);
    if(it != syncModeMap.end()) {
        return it->second;
    }
    std::stringstream ss;
    ss << "Unrecognized sync mode: " << modeString;
    throw std::invalid_argument(ss.str());
}

std::string OBSyncModeToString(const OBMultiDeviceSyncMode syncMode) {
    static const std::unordered_map<OBMultiDeviceSyncMode, std::string> modeToStringMap = {
        { OB_MULTI_DEVICE_SYNC_MODE_FREE_RUN, "OB_MULTI_DEVICE_SYNC_MODE_FREE_RUN" },
        { OB_MULTI_DEVICE_SYNC_MODE_STANDALONE, "OB_MULTI_DEVICE_SYNC_MODE_STANDALONE" },
        { OB_MULTI_DEVICE_SYNC_MODE_PRIMARY, "OB_MULTI_DEVICE_SYNC_MODE_PRIMARY" },
        { OB_MULTI_DEVICE_SYNC_MODE_SECONDARY, "OB_MULTI_DEVICE_SYNC_MODE_SECONDARY" },
        { OB_MULTI_DEVICE_SYNC_MODE_SECONDARY_SYNCED, "OB_MULTI_DEVICE_SYNC_MODE_SECONDARY_SYNCED" },
        { OB_MULTI_DEVICE_SYNC_MODE_SOFTWARE_TRIGGERING, "OB_MULTI_DEVICE_SYNC_MODE_SOFTWARE_TRIGGERING" },
        { OB_MULTI_DEVICE_SYNC_MODE_HARDWARE_TRIGGERING, "OB_MULTI_DEVICE_SYNC_MODE_HARDWARE_TRIGGERING" }
    };

    auto it = modeToStringMap.find(syncMode);
    if(it != modeToStringMap.end()) {
        return it->second;
    }
    std::stringstream ss;
    ss << "Unmapped sync mode value: " << static_cast<int>(syncMode) << ". Please check the sync mode value.";
    throw std::invalid_argument(ss.str());
}

int strcmpNocase(const char *str0, const char *str1) {
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
    return _strcmpi(str0, str1);
#else
    return strcasecmp(str0, str1);
#endif
}

void onSigint(int) {
    sigintFlag.store(true);
    quitStreamPreview.store(true);
}
