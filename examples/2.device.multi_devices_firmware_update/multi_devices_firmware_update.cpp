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

bool getFirmwarePathFromCommandLine(int argc, char **argv, std::string &firmwarePath);
void firmwareUpdateCallback(OBFwUpdateState state, const char *message, uint8_t percent);
void printDeviceList();

bool firstCall     = true;
bool finalSuccess  = false;
bool finalMismatch = false;
bool finalFailure  = false;

std::vector<std::shared_ptr<ob::Device>> totalDevices{};
std::vector<std::shared_ptr<ob::Device>> successDevices{};
std::vector<std::shared_ptr<ob::Device>> misMatchDevices{};
std::vector<std::shared_ptr<ob::Device>> failedDevices{};

int main(int argc, char *argv[]) try {
    std::string firmwarePath;
    if(!getFirmwarePathFromCommandLine(argc, argv, firmwarePath)) {
        std::cout << "Press any key to exit..." << std::endl;
        exit(EXIT_FAILURE);
    }

    // Create a context to access the devices
    std::shared_ptr<ob::Context> context = std::make_shared<ob::Context>();

#if defined(__linux__)
    // On Linux, it is recommended to use the libuvc backend for device access as v4l2 is not always reliable on some systems for firmware update.
    context->setUvcBackendType(OB_UVC_BACKEND_TYPE_LIBUVC);
#endif

    // Query the device list
    std::shared_ptr<ob::DeviceList> deviceList = context->queryDeviceList();
    if(deviceList->getCount() == 0) {
        std::cout << "No device found. Please connect a device first!" << std::endl;
        std::cout << "Press any key to exit..." << std::endl;
        ob_smpl::waitForKeyPressed();
        exit(EXIT_FAILURE);
    }

    // Get all devices
    for(uint32_t i = 0; i < deviceList->getCount(); ++i) {
        totalDevices.push_back(deviceList->getDevice(i));
    }
    printDeviceList();

    for(uint32_t i = 0; i < totalDevices.size(); ++i) {
        firstCall     = true;
        finalSuccess  = false;
        finalMismatch = false;
        finalFailure  = false;

        try {
            std::cout << "\nUpgrading device: " << i + 1 << "/" << totalDevices.size() 
                      << " - " << totalDevices[i]->getDeviceInfo()->getName() << std::endl;

            // Upgrade each device with async set to false for synchronous calls.
            // You can set a callback function to retrieve the device's upgrade progress and related information in real time.
            totalDevices[i]->updateFirmware(firmwarePath.c_str(), firmwareUpdateCallback, false);
        }
        catch(ob::Error &e) {
            // Unexpected situations, such as device disconnection, will typically throw an exception.
            // Note that common issues like verification failures are usually reported through the callback status.
            std::cerr << "function:" << e.getFunction() << "\nargs:" << e.getArgs() << "\nmessage:" << e.what() << "\ntype:" << e.getExceptionType()
                      << std::endl;
            finalFailure = true;
        }

        if(finalSuccess) {
            successDevices.push_back(totalDevices[i]);
        }
        else if(finalMismatch) {
            misMatchDevices.push_back(totalDevices[i]);
        }
        else if(finalFailure) {
            failedDevices.push_back(totalDevices[i]);
        }
    }

    std::cout << "\nUpgrade Summary:\n";
    std::cout << "==================================================\n";

    std::cout << "Success (" << successDevices.size() << "):\n";
    for(const auto &device: successDevices) {
        std::cout << "  - Name: " << device->getDeviceInfo()->getName() << std::endl;
    }

    std::cout << "\nMismatch (" << misMatchDevices.size() << "):\n";
    for(const auto &device: misMatchDevices) {
        std::cout << "  - Name: " << device->getDeviceInfo()->getName() << std::endl;
    }
    if (misMatchDevices.size() > 0) {
        std::cout << "Please check use the correct firmware version and retry the upgrade." << std::endl;
    }

    std::cout << "\nFailure (" << failedDevices.size() << "):\n";
    for(const auto &device: failedDevices) {
        std::cout << "  - Name: " << device->getDeviceInfo()->getName() << std::endl;
    }

    std::cout << "\nUpgrade process completed. Please reboot devices manually." << std::endl;
    std::cout << "Press any key to exit..." << std::endl;
    ob_smpl::waitForKeyPressed();
    return 0;
}
catch(ob::Error &e) {
    std::cerr << "function:" << e.getFunction() << "\nargs:" << e.getArgs() << "\nmessage:" << e.what() << "\ntype:" << e.getExceptionType() << std::endl;
    std::cout << "\nPress any key to exit.";
    ob_smpl::waitForKeyPressed();
    exit(EXIT_FAILURE);
}

void firmwareUpdateCallback(OBFwUpdateState state, const char *message, uint8_t percent) {
    if(firstCall) {
        firstCall = !firstCall;
    }
    else {
        std::cout << "\033[3F";  // Move cursor up 3 lines
    }

    std::cout << "\033[K";  // Clear the current line
    std::cout << "Progress: " << static_cast<uint32_t>(percent) << "%" << std::endl;

    std::cout << "\033[K";
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
    case STAT_IN_PROGRESS:
        std::cout << "Upgrade in progress" << std::endl;
        break;
    case STAT_START:
        std::cout << "Starting the upgrade" << std::endl;
        break;
    case STAT_VERIFY_IMAGE:
        std::cout << "Verifying image file" << std::endl;
        break;
    case ERR_MISMATCH:
        std::cout << "Mismatch between device and image file" << std::endl;
        break;
    default:
        std::cout << "Unknown status or error" << std::endl;
        break;
    }

    std::cout << "\033[K";
    std::cout << "Message : " << message << std::endl << std::flush;

    if(state == STAT_DONE) {
        finalSuccess = true;
        finalFailure = false;
    }
    else if(state == ERR_MISMATCH) {
        // If the device's firmware version does not match the image file, the callback status will be ERR_MISMATCH.
        finalMismatch = true;
    }
    else if(state < 0) {
        // While state < 0, it means an error occurred.
        finalFailure = true;
    }
}

bool getFirmwarePathFromCommandLine(int argc, char **argv, std::string &firmwarePath) {
    if(argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <firmware_file_path>" << std::endl;
        std::cerr << "Example: " << argv[0] << " /path/to/firmware.bin" << std::endl;
        return false;
    }

    std::vector<std::string> validExtensions = { ".bin", ".img" };
    firmwarePath                             = argv[1];

    if(firmwarePath.size() > 4) {
        std::string extension = firmwarePath.substr(firmwarePath.size() - 4);

        auto result = std::find_if(validExtensions.begin(), validExtensions.end(),
                                   [extension](const std::string &validExtension) { return extension == validExtension; });
        if(result != validExtensions.end()) {
            std::cout << "Firmware file confirmed: " << firmwarePath << std::endl << std::endl;
            return true;
        }
    }

    std::cout << "Invalid input file: Please provide a valid firmware file, supported formats: ";
    for(const auto &ext: validExtensions) {
        std::cout << ext << " ";
    }
    std::cout << std::endl;
    return false;
}

void printDeviceList() {
    std::cout << "Devices found:" << std::endl;
    std::cout << "--------------------------------------------------------------------------------\n";
    for(uint32_t i = 0; i < totalDevices.size(); ++i) {
        std::cout << "[" << i << "] " << "Device: " << totalDevices[i]->getDeviceInfo()->getName();
        std::cout << " | SN: " << totalDevices[i]->getDeviceInfo()->getSerialNumber();
        std::cout << " | Firmware version: " << totalDevices[i]->getDeviceInfo()->getFirmwareVersion() << std::endl;
    }
    std::cout << "---------------------------------------------------------------------------------\n";
}
