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

static bool shouldContinue();
static void presetUpdateCallback(bool firstCall, OBFwUpdateState state, const char *message, uint8_t percent);
static bool getPresetPath(std::vector<std::string> &pathList);
static bool selectDevice(std::shared_ptr<ob::Device> &device);
static void printDeviceList();
static bool isPresetSupported(std::shared_ptr<ob::Device> device);
static void printPreset(std::shared_ptr<ob::Device> device);

std::vector<std::shared_ptr<ob::Device>> devices{};

int main() try {
    // Create a context to access the connected devices
    std::shared_ptr<ob::Context> context = std::make_shared<ob::Context>();

#if defined(__linux__)
    // On Linux, it is recommended to use the libuvc backend for device access as v4l2 is not always reliable on some systems for preset update.
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
        bool                        firstCall   = true;
        OBFwUpdateState             updateState = STAT_START;
        std::shared_ptr<ob::Device> device      = nullptr;

        if(!selectDevice(device)) {
            break;
        }

        printPreset(device);

        std::vector<std::string> pathList;
        if(!getPresetPath(pathList)) {
            break;
        }

        uint8_t index                 = 0;
        uint8_t count                 = static_cast<uint8_t>(pathList.size());
        char(*filePaths)[OB_PATH_MAX] = new char[count][OB_PATH_MAX];

        // copy paths
        std::cout << "\nPreset file paths you input: " << std::endl;
        for(const auto &path: pathList) {
            strcpy(filePaths[index++], path.c_str());
            std::cout << "Path " << (uint32_t)index << ": " << path << std::endl;
        }
        std::cout << std::endl;

        std::cout << "Start to update optional depth preset, please wait a moment...\n\n";
        try {
            device->updateOptionalDepthPresets(filePaths, count, [&updateState, &firstCall](OBFwUpdateState state, const char *message, uint8_t percent) {
                updateState = state;
                presetUpdateCallback(firstCall, state, message, percent);
                firstCall = false;
            });
            delete[] filePaths;
            filePaths = nullptr;
        }
        catch(ob::Error &e) {
            // If the update fails, will throw an exception.
            std::cerr << "\nThe update was interrupted! An error occurred! " << std::endl;
            std::cerr << "Error message: " << e.what() << "\n" << std::endl;
            std::cout << "Press any key to exit." << std::endl;
            ob_smpl::waitForKeyPressed();
            delete[] filePaths;
            filePaths = nullptr;
            break;
        }

        std::cout << std::endl;
        if ( updateState == STAT_DONE || updateState == STAT_DONE_WITH_DUPLICATES) {
            // success
            std::cout << "After updating the preset: " << std::endl;
            printPreset(device);
        }

        if(!shouldContinue()) {
            break;
        }
    }
}
catch(ob::Error &e) {
    std::cerr << "function:" << e.getFunction() << "\nargs:" << e.getArgs() << "\nmessage:" << e.what() << "\ntype:" << e.getExceptionType() << std::endl;
    std::cout << "\nPress any key to exit.";
    ob_smpl::waitForKeyPressed();
    exit(EXIT_FAILURE);
}

static bool shouldContinue() {
    std::string input;
    std::cout << "Enter 'Q' or 'q' to quit, or any other key to continue: ";
    std::getline(std::cin, input);
    if(input == "Q" || input == "q") {
        return false;
    }
    return true;
}

static void presetUpdateCallback(bool firstCall, OBFwUpdateState state, const char *message, uint8_t percent) {
    if(!firstCall) {
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
    case STAT_DONE_WITH_DUPLICATES:
        std::cout << "Update completed, duplicated presets have been ignored" << std::endl;
        break;
    case STAT_IN_PROGRESS:
        std::cout << "Update in progress" << std::endl;
        break;
    case STAT_START:
        std::cout << "Starting the update" << std::endl;
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

static bool getPresetPath(std::vector<std::string> &pathList) {
    std::cout << "Please input the file paths of the optional depth preset file (.bin):" << std::endl;
    std::cout << " - Press 'Enter' to finish this input" << std::endl;
    std::cout << " - Press 'Q' or 'q' to exit the program" << std::endl;

    uint8_t count = 0;

    pathList.clear();
    do 
    {
        std::cout << "Enter Path: ";
        std::string input;
        std::getline(std::cin, input);

        if(input == "Q" || input == "q") {
            return false;
        }
        if(input.empty()) {
            if(pathList.size() == 0) {
                std::cout << "You didn't input any file paths" << std::endl;
                if(!shouldContinue()) {
                    return false;
                }
                continue;
            }
            break;
        }

        // Remove leading and trailing whitespaces
        input.erase(std::find_if(input.rbegin(), input.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), input.end());

        // Remove leading and trailing quotes
        if(!input.empty() && input.front() == '\'' && input.back() == '\'') {
            input = input.substr(1, input.size() - 2);
        }

        if(input.size() > 4 && input.substr(input.size() - 4) == ".bin") {
            pathList.push_back(input);
            ++count;
            continue;
        }
        else {
            std::cout << "Invalid file format. Please provide a .bin file." << std::endl << std::endl;
            continue;
        }
    } while(count<10);

    return true;
}

static bool selectDevice(std::shared_ptr<ob::Device>& device) {
    std::string input;
    
    device = nullptr;
    while(true) {
        std::cout << "Please select a device to update the optional depth preset, enter 'l' to list devices, or enter 'q' to quit: " << std::endl;
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
            uint32_t index = std::stoi(input);
            if(index >= static_cast<uint32_t>(devices.size())) {
                std::cout << "Invalid input, please enter a valid index number." << std::endl;
                continue;
            }

            device = devices[index];
            if(!isPresetSupported(device)) {
                std::cerr << "The device you selected does not support preset. Please select another one" << std::endl;
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

static void printDeviceList() {
    std::cout << "--------------------------------------------------------------------------------\n";
    for(uint32_t i = 0; i < devices.size(); ++i) {
        std::cout << "[" << i << "] " << "Device: " << devices[i]->getDeviceInfo()->getName();
        std::cout << " | SN: " << devices[i]->getDeviceInfo()->getSerialNumber();
        std::cout << " | Firmware version: " << devices[i]->getDeviceInfo()->getFirmwareVersion() << std::endl;
    }
    std::cout << "---------------------------------------------------------------------------------\n";
}

static bool isPresetSupported(std::shared_ptr<ob::Device> device) {
    auto presetList = device->getAvailablePresetList();
    if(presetList && presetList->getCount() > 0) {
        return true;
    }
    return false;
}

static void printPreset(std::shared_ptr<ob::Device> device) {
    try {
        auto presetList = device->getAvailablePresetList();
        std::cout << "Preset count: " << presetList->getCount() << std::endl;
        for(uint32_t i = 0; i < presetList->getCount(); ++i) {
            std::cout << "  - " << presetList->getName(i) << std::endl;
        }
        std::cout << "Current preset: " << device->getCurrentPresetName() << "\n" << std::endl;
    }
    catch(ob::Error &e) {
        // If the update fails, will throw an exception.
        std::cerr << "\nThe device doesn't support preset! " << std::endl;
        std::cerr << "error: " << e.what() << "\n" << std::endl;
        return;
    }

    std::string key = "PresetVer";
    if(device->isExtensionInfoExist(key)) {
        std::string value = device->getExtensionInfo(key);
        std::cout << "Preset version: " << value << "\n" << std::endl;
    }
    else {
        std::cout << "PresetVer: n/a\n" << std::endl;
    }
}
