// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include <libobsensor/ObSensor.hpp>

#include "utils.hpp"

#include <iostream>
#include <fstream>
#include <string>
#include <memory>
#include <algorithm>
#include <cctype>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <thread>
#include <unordered_set>

// Constants
namespace {
constexpr uint32_t kMaxReconnectTimeoutMs = 15000;
constexpr uint32_t kPollIntervalMs        = 1000;
}  // namespace

struct CmdArgs {
    bool        listOnly = false;
    std::string firmwarePath;
    std::string serial;
};

void printUsage(bool full);
bool parseArguments(int argc, char *argv[], CmdArgs &args);
void firmwareUpdateCallback(OBFwUpdateState state, const char *message, uint8_t percent);
void printDeviceList(std::vector<std::shared_ptr<ob::Device>> devices);
void updateFirmware(std::shared_ptr<ob::Device> device, const std::string &firmwarePath);

bool firstCall           = true;
bool needReupdate        = false;
bool ansiEscapeSupported = false;

int main(int argc, char *argv[]) try {
    CmdArgs args{};
    if(!parseArguments(argc, argv, args)) {
        return -1;
    }

    ansiEscapeSupported = ob_smpl::supportAnsiEscape();

    // Create a context to access the connected devices
    std::shared_ptr<ob::Context>             context = std::make_shared<ob::Context>();
    std::vector<std::shared_ptr<ob::Device>> devices{};

#if defined(__linux__)
    // On Linux, it is recommended to use the libuvc backend for device access as v4l2 is not always reliable on some systems for firmware update.
    context->setUvcBackendType(OB_UVC_BACKEND_TYPE_LIBUVC);
#endif

    // Get connected devices from the context
    std::shared_ptr<ob::DeviceList> deviceList = context->queryDeviceList();
    uint32_t                        count      = deviceList->getCount();
    for(uint32_t i = 0; i < count; ++i) {
        try {
            devices.push_back(deviceList->getDevice(i));
        }
        catch(ob::Error &e) {
            std::cerr << "Failed to get device, ignoring it." << std::endl;
            std::cerr << "Error message: " << e.what() << " (status: " << e.getStatus() << ")" << std::endl;
        }
    }
    if(devices.empty()) {
        std::cout << "\nThere are no connected devices." << std::endl;
        return -1;
    }
    if(args.listOnly) {
        printDeviceList(devices);
        return 0;
    }

    std::shared_ptr<ob::Device>     targetDevice;
    std::shared_ptr<ob::DeviceInfo> devInfo;
    if(args.serial.empty()) {
        if(devices.size() > 1) {
            std::cout << "\nError: multiple devices found.";
            std::cout << "\n       Please specify the serial number number with -s." << std::endl;
            printDeviceList(devices);
            return -1;
        }
        // get the first device
        targetDevice = devices[0];
        devInfo      = targetDevice->getDeviceInfo();
        args.serial  = devInfo->getSerialNumber();
    }
    else {
        // find device by sn
        for(auto device: devices) {
            auto        info = device->getDeviceInfo();
            std::string sn   = info->getSerialNumber();
            if(sn == args.serial) {
                targetDevice = device;
                devInfo      = info;
                break;
            }
        }
        if(targetDevice == nullptr) {
            std::cout << "\nError: device with serial number \"" << args.serial << "\" not found." << std::endl;
            return -1;
        }
    }

    std::cout << "\nTarget device:" << std::endl;
    std::cout << "> " << devInfo->getName() << " | SN: " << devInfo->getSerialNumber() << " | Firmware version: " << devInfo->getFirmwareVersion()
              << " | Connection type: " << devInfo->getConnectionType() << std::endl;
    std::cout << "Target firmware file:" << std::endl;
    std::cout << "> " << args.firmwarePath << std::endl;

    // register callback for device change
    std::mutex                      mutex;
    std::condition_variable         cv;
    std::unordered_set<std::string> onlineDevices;

    context->setDeviceChangedCallback([&](std::shared_ptr<ob::DeviceList> removedList, std::shared_ptr<ob::DeviceList> addedList) {
        (void)removedList;
        if(!addedList) {
            return;
        }
        std::lock_guard<std::mutex> lock(mutex);
        for(uint32_t i = 0; i < addedList->getCount(); ++i) {
            try {
                auto dev = addedList->getDevice(i);
                auto sn  = dev->getDeviceInfo()->getSerialNumber();
                onlineDevices.insert(sn);
            }
            catch(...) {
                // ignore
            }
        }
        cv.notify_one();
    });

    // update synchronously
    updateFirmware(targetDevice, args.firmwarePath);
    if(needReupdate) {
        // some devices require another update after a reboot
        std::cout << "Firmware update completed but not finalized. The device will reboot now and then perform the second update..." << std::endl;
        targetDevice->reboot();
        targetDevice = nullptr;

        std::cout << "Waiting for device to reconnect..." << std::endl;

        // wait for device to come back online
        const uint32_t maxWaitTimeMs  = kMaxReconnectTimeoutMs;
        const uint32_t pollIntervalMs = kPollIntervalMs;
        uint32_t       elapsedTime    = 0;
        bool           reconnected    = false;

        while(elapsedTime < maxWaitTimeMs) {
            {
                std::unique_lock<std::mutex> lock(mutex);
                // check if device appeared in callback
                if(onlineDevices.count(args.serial)) {
                    onlineDevices.erase(args.serial);
                    lock.unlock();
                    // verify by querying device list
                    try {
                        auto newDeviceList = context->queryDeviceList();
                        auto newDevice     = newDeviceList->getDeviceBySN(args.serial.c_str());
                        if(newDevice) {
                            std::cout << "\nThe device is online now, starting the second update..." << std::endl;
                            targetDevice = newDevice;
                            reconnected  = true;
                            break;
                        }
                    }
                    catch(...) {
                        // device not found, continue waiting
                    }
                }
                else {
                    // wait for callback or timeout
                    cv.wait_for(lock, std::chrono::milliseconds(pollIntervalMs));
                }
            }

            elapsedTime += pollIntervalMs;
            std::cout << "Waiting for device to reconnect... (" << elapsedTime << "/" << maxWaitTimeMs << " ms)" << std::endl;
        }

        if(!reconnected) {
            std::cout << "\nDevice failed to reconnect within timeout. Firmware update failed." << std::endl;
            std::cout << "You can rerun this program to update the device once it comes back online." << std::endl;
            return -1;
        }

        // update again
        updateFirmware(targetDevice, args.firmwarePath);
    }

    // ok
    std::cout << "Firmware update completed successfully." << std::endl;
    std::cout << "The device will reboot now and the application will exit." << std::endl;
    targetDevice->reboot();

    // clear
    devInfo      = nullptr;
    targetDevice = nullptr;
    deviceList   = nullptr;

    // reset callback before destroying context
    context->setDeviceChangedCallback([](std::shared_ptr<ob::DeviceList>, std::shared_ptr<ob::DeviceList>) {});
    context = nullptr;
    return 0;
}
catch(ob::Error &e) {
    std::cerr << "function:" << e.getFunction() << "\nargs:" << e.getArgs() << "\nmessage:" << e.what() << "\nstatus:" << e.getStatus()
              << "\ntype:" << e.getExceptionType() << std::endl;
    std::cout << "\nPress any key to exit.";
    ob_smpl::waitForKeyPressed();
    exit(EXIT_FAILURE);
}

void printUsage(bool brief) {
    std::cout << "Usage:" << std::endl;
    std::cout << "   ob_device_firmware_update [-h] [-l] [-f] [-s]\n" << std::endl;

    if(brief) {
        return;
    }

    std::cout << "Where:\n"
                 "   -h,  --help\n"
                 "       Displays usage information and exits.\n\n"
                 "   -l,  --list_devices\n"
                 "       List all available devices.\n\n"
                 "   -f <path>,  --file <path>\n"
                 "       Path of the firmware image file (.bin or .img).\n\n"
                 "   -s <string>,  --serial_number <string>\n"
                 "       The serial number of the device to be update, this is mandatory if\n"
                 "       more than one device is connected.\n\n"
                 "Examples:\n"
                 "   ob_device_firmware_update -l\n"
                 "   ob_device_firmware_update -f firmware.bin\n"
                 "   ob_device_firmware_update -s CP1234567890 -f firmware.bin\n";
    std::cout << std::endl;
}

bool parseArguments(int argc, char *argv[], CmdArgs &args) {
    enum class errState { DEFAULT, MISS, REPEAT };
    auto showErr = [](const std::string &arg, errState state) {
        std::cout << "Invalid argument: " << arg << std::endl;
        switch(state) {
        case errState::MISS:
            std::cout << "    Missing a value for this argument!" << std::endl;
            break;
        case errState::REPEAT:
            std::cout << "    Argument already set!" << std::endl;
            break;
        default:
            break;
        }
        std::cout << std::endl;
        printUsage(true);
        std::cout << "For complete usage and help type: ob_device_firmware_update --help" << std::endl;
    };

    if(argc == 2) {
        std::string arg = argv[1];
    }
    // others
    for(int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if(arg == "-h" || arg == "--help") {
            // help
            printUsage(false);
            return false;
        }
        else if(arg == "-l" || arg == "--list_devices") {
            // list
            args.listOnly = true;
            return true;
        }
        else if(arg == "-f" || arg == "--file") {
            auto argName = "-f (--file)";
            if(!args.firmwarePath.empty()) {
                showErr(argName, errState::REPEAT);
                return false;
            }
            if(i + 1 < argc) {
                args.firmwarePath = argv[++i];
                if(args.firmwarePath[0] == '-') {
                    showErr(argName, errState::DEFAULT);
                    return false;
                }
                continue;
            }
            else {
                showErr(argName, errState::MISS);
                return false;
            }
        }
        else if(arg == "-s" || arg == "--serial_number") {
            auto argName = "-s (--serial_number)";
            if(!args.serial.empty()) {
                showErr(argName, errState::REPEAT);
                return false;
            }
            if(i + 1 < argc) {
                args.serial = argv[++i];
                if(args.serial[0] == '-') {
                    showErr(argName, errState::DEFAULT);
                    return false;
                }
                continue;
            }
            else {
                showErr(argName, errState::MISS);
                return false;
            }
        }

        // others
        showErr(arg, errState::DEFAULT);
        return false;
    }

    if(argc == 1 || args.firmwarePath.empty()) {
        std::cout << "Nothing to do, run again with -h for help.\n" << std::endl;
        args.listOnly = true;
        return true;
    }
    return true;
}

void firmwareUpdateCallback(OBFwUpdateState state, const char *message, uint8_t percent) {
    if(firstCall) {
        firstCall = false;
    }
    else {
        if(ansiEscapeSupported) {
            std::cout << "\033[3F";  // Move cursor up 3 lines
        }
    }

    if(ansiEscapeSupported) {
        std::cout << "\033[K";  // Clear the current line
    }
    std::cout << "Progress: " << static_cast<uint32_t>(percent) << "%" << std::endl;

    if(ansiEscapeSupported) {
        std::cout << "\033[K";
    }
    std::cout << "Status  : ";
    switch(state) {
    case STAT_VERIFY_SUCCESS:
        std::cout << "Image file verification success" << std::endl;
        break;
    case STAT_FILE_TRANSFER:
        std::cout << "File transfer in progress" << std::endl;
        break;
    case STAT_DONE:
        std::cout << "Update completed" << std::endl;
        break;
    case STAT_DONE_REBOOT_AND_REUPDATE:
        needReupdate = true;
        std::cout << "Update completed" << std::endl;
        break;
    case STAT_IN_PROGRESS:
        std::cout << "Upgrade in progress" << std::endl;
        break;
    case STAT_START:
        std::cout << "Starting the upgrade" << std::endl;
        break;
    case STAT_VERIFY_IMAGE:
        std::cout << "Verifying image file" << std::endl;
        break;
    default:
        std::cout << "Unknown status or error" << std::endl;
        break;
    }

    if(ansiEscapeSupported) {
        std::cout << "\033[K";
    }
    std::cout << "Message : " << message << std::endl << std::flush;
}

void printDeviceList(std::vector<std::shared_ptr<ob::Device>> devices) {
    std::cout << std::endl;  // separate
    if(devices.empty()) {
        std::cout << "There are no connected devices." << std::endl;
        return;
    }

    std::cout << "Connected devices:" << std::endl;
    uint32_t i = 0;
    for(auto device: devices) {
        auto info = device->getDeviceInfo();
        std::cout << "[" << i++ << "] " << "Device: " << info->getName();
        std::cout << " | SN: " << info->getSerialNumber();
        std::cout << " | Firmware version: " << info->getFirmwareVersion() << std::endl;
    }
}

void updateFirmware(std::shared_ptr<ob::Device> device, const std::string &firmwarePath) {
    std::cout << "\nUpgrading device firmware, please wait for a moment...\n" << std::endl;
    try {
        // Set async to false to synchronously block and wait for the device firmware upgrade to complete.
        needReupdate = false;
        firstCall    = true;
        device->updateFirmware(firmwarePath.c_str(), firmwareUpdateCallback, false);
    }
    catch(ob::Error &e) {
        // If the update fails, will throw an exception.
        std::cout << std::endl;
        std::cerr << "Error: The upgrade was interrupted! An error occurred! " << std::endl;
        std::cerr << "       Error message: " << e.what() << " (status: " << e.getStatus() << ")" << std::endl;
        std::cout << "\nPress any key to exit." << std::endl;
        ob_smpl::waitForKeyPressed();
        exit(EXIT_FAILURE);
    }
    std::cout << std::endl;  // separate
}
