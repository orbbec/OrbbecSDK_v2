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
            std::cerr << "Error message: " << e.what() << std::endl;
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
    std::shared_ptr<ob::Device> deviceAfterReboot;
    std::mutex                  mutex;
    std::condition_variable     cv;

    context->setDeviceChangedCallback([&](std::shared_ptr<ob::DeviceList> removedList, std::shared_ptr<ob::DeviceList> addedList) {
        (void)removedList;
        std::shared_ptr<ob::Device> device;
        try {
            device = addedList->getDeviceBySN(args.serial.c_str());
            {
                std::lock_guard<std::mutex> lock(mutex);
                deviceAfterReboot = device;
            }
            cv.notify_one();
        }
        catch(ob::Error &e) {
            (void)e;
        }
    });

    // update synchronously
    updateFirmware(targetDevice, args.firmwarePath);
    if(needReupdate) {
        // some devices require another update after a reboot
        std::cout << "Firmware update completed but not finalized. The device will reboot now and then perform the second update..." << std::endl;
        targetDevice->reboot();
        targetDevice = nullptr;

        // wait the device arrival
        do {
            {
                std::unique_lock<std::mutex> lock(mutex);
                cv.wait_for(lock, std::chrono::milliseconds(5000), [&]() { return deviceAfterReboot != nullptr; });
            }
            if(deviceAfterReboot == nullptr) {
                std::string input;
                std::cout << "\nThe device may not have come back online yet." << std::endl;
                std::cout << " - Press 'y' or 'Y' to restart the update if you are sure it's online." << std::endl;
                std::cout << " - Press any other key to continue waiting for it to come back online." << std::endl;
                std::cout << " - Press ESC 'q' or 'Q' to quit (you can re-run this program to update again)." << std::endl;
                auto key = ob_smpl::waitForKeyPressed(30000);
                if(key == 0 || key == ESC_KEY || key == 'q' || key == 'Q') {
                    std::cout << "\nExiting now. You can rerun this program to update the device once it comes back online." << std::endl;
                    return 0;
                }
                else if(key == 'y' || key == 'Y') {
                    std::unique_lock<std::mutex> lock(mutex);
                    if(deviceAfterReboot != nullptr) {
                        std::cout << "\nThe device is online now, start to the second update now";
                        targetDevice = deviceAfterReboot;
                        break;
                    }
                    // get device again
                    deviceList = context->queryDeviceList();
                    try {
                        deviceAfterReboot = deviceList->getDeviceBySN(args.serial.c_str());
                    }
                    catch(ob::Error &e) {
                        (void)e;
                    }
                    if(deviceAfterReboot == nullptr) {
                        std::cout << "\nThe device is still offline. You can rerun this program to update it once the device is back online." << std::endl;
                        return -1;
                    }
                    targetDevice = deviceAfterReboot;
                    break;
                }
                std::cout << std::endl;
                continue;
            }
            // ok
            std::cout << "\nThe device is online now, start to the second update now";
            targetDevice = deviceAfterReboot;
            break;
        } while(1);
        // update again
        updateFirmware(targetDevice, args.firmwarePath);
    }

    // ok
    std::cout << "Firmware update completed successfully." << std::endl;
    std::cout << "The device will reboot now and the application will exit." << std::endl;
    targetDevice->reboot();

    // clear
    devInfo           = nullptr;
    deviceAfterReboot = nullptr;
    targetDevice      = nullptr;
    deviceList        = nullptr;
    context           = nullptr;
    return 0;
}
catch(ob::Error &e) {
    std::cerr << "function:" << e.getFunction() << "\nargs:" << e.getArgs() << "\nmessage:" << e.what() << "\ntype:" << e.getExceptionType() << std::endl;
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
        std::cerr << "       Error message: " << e.what() << std::endl;
        std::cout << "\nPress any key to exit." << std::endl;
        ob_smpl::waitForKeyPressed();
        exit(EXIT_FAILURE);
    }
    std::cout << std::endl;  // separate
}
