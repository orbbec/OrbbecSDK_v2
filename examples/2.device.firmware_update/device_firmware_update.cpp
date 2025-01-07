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

void firmwareUpdateCallback(OBFwUpdateState state, const char *message, uint8_t percent);
bool getFirmwarePath(std::string &firmwarePath);
bool selectDevice(int &deviceIndex);
void printDeviceList();

bool                                     firstCall = true;
std::vector<std::shared_ptr<ob::Device>> devices{};

int main() try {
    // Create a context to access the connected devices
    std::shared_ptr<ob::Context> context = std::make_shared<ob::Context>();

#if defined(__linux__)
    // On Linux, it is recommended to use the libuvc backend for device access as v4l2 is not always reliable on some systems for firmware update.
    context->setUvcBackendType(OB_UVC_BACKEND_TYPE_LIBUVC);
#endif

    // Get connected devices from the context
    std::shared_ptr<ob::DeviceList> deviceList = context->queryDeviceList();
    if(deviceList->getCount() == 0) {
        std::cout << "No device found. Please connect a device first!" << std::endl;
        std::cout << "Press any key to exit..." << std::endl;
        ob_smpl::waitForKeyPressed();
        return 0;
    }

    for(uint32_t i = 0; i < deviceList->getCount(); ++i) {
        devices.push_back(deviceList->getDevice(i));
    }
    std::cout << "Devices found:" << std::endl;
    printDeviceList();

    while(true) {
        firstCall       = true;
        int deviceIndex = -1;

        if(!selectDevice(deviceIndex)) {
            break;
        }

        std::string firmwarePath;
        if(!getFirmwarePath(firmwarePath)) {
            break;
        }

        std::cout << "Upgrading device firmware, please wait...\n\n";
        try {
            // Set async to false to synchronously block and wait for the device firmware upgrade to complete.
            devices[deviceIndex]->updateFirmware(firmwarePath.c_str(), firmwareUpdateCallback, false);
        }
        catch(ob::Error &e) {
            // If the update fails, will throw an exception.
            std::cerr << "\nThe upgrade was interrupted! An error occurred! " << std::endl;
            std::cerr << "Error message: " << e.what() << std::endl;
            std::cout << "Press any key to exit." << std::endl;
            ob_smpl::waitForKeyPressed();
            break;
        }

        std::string input;
        std::cout << "Enter 'Q' or 'q' to quit, or any other key to continue: ";
        std::getline(std::cin, input);
        if(input == "Q" || input == "q") {
            break;
        }
    }
}
catch(ob::Error &e) {
    std::cerr << "function:" << e.getFunction() << "\nargs:" << e.getArgs() << "\nmessage:" << e.what() << "\ntype:" << e.getExceptionType() << std::endl;
    std::cout << "\nPress any key to exit.";
    ob_smpl::waitForKeyPressed();
    exit(EXIT_FAILURE);
}void firmwareUpdateCallback(OBFwUpdateState state, const char *message, uint8_t percent)

 {
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
    default:
        std::cout << "Unknown status or error" << std::endl;
        break;
    }

    std::cout << "\033[K";
    std::cout << "Message : " << message << std::endl << std::flush;
}

bool getFirmwarePath(std::string &firmwarePath) {
    std::cout << "Please input the path of the firmware file (.bin) to be updated:" << std::endl;
    std::cout << "(Enter 'Q' or 'q' to quit): " << std::endl;
    std::cout << "Path: ";
    std::string input;
    std::getline(std::cin, input);

    if(input == "Q" || input == "q") {
        exit(EXIT_SUCCESS);
    }

    // Remove leading and trailing whitespaces
    input.erase(std::find_if(input.rbegin(), input.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), input.end());

    // Remove leading and trailing quotes
    if(!input.empty() && input.front() == '\'' && input.back() == '\'') {
        input = input.substr(1, input.size() - 2);
    }

    if(input.size() > 4 && input.substr(input.size() - 4) == ".bin") {
        firmwarePath = input;
        std::cout << "Firmware file confirmed: " << firmwarePath << std::endl << std::endl;
        return true;
    }

    std::cout << "Invalid file format. Please provide a .bin file." << std::endl << std::endl;
    return getFirmwarePath(firmwarePath);
}

void printDeviceList() {
    std::cout << "--------------------------------------------------------------------------------\n";
    for(uint32_t i = 0; i < devices.size(); ++i) {
        std::cout << "[" << i << "] " << "Device: " << devices[i]->getDeviceInfo()->getName();
        std::cout << " | SN: " << devices[i]->getDeviceInfo()->getSerialNumber();
        std::cout << " | Firmware version: " << devices[i]->getDeviceInfo()->getFirmwareVersion() << std::endl;
    }
    std::cout << "---------------------------------------------------------------------------------\n";
}

bool selectDevice(int &deviceIndex) {
    std::string input;
    while(true) {
        std::cout << "Please select a device to update the firmware, enter 'l' to list devices, or enter 'q' to quit: " << std::endl;
        std::cout << "Device index: ";
        std::getline(std::cin, input);

        if(input == "Q" || input == "q") {
            return false;
        }

        if(input == "l" || input == "L") {
            printDeviceList();
            continue;
        }

        try {
            deviceIndex = std::stoi(input);
            if(deviceIndex < 0 || deviceIndex >= static_cast<int>(devices.size())) {
                std::cout << "Invalid input, please enter a valid index number." << std::endl;
                continue;
            }
            std::cout << std::endl;
            break;
        }
        catch(...) {
            std::cout << "Invalid input, please enter a valid index number." << std::endl;
            continue;
        }
    }
    return true;
}