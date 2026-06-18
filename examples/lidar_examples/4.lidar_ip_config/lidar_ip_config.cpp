// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

// LiDAR IP Configuration Tool
// This tool allows setting IP address and subnet mask on Pulsar SL450/ME450 LiDAR devices

#include <libobsensor/ObSensor.hpp>
#include "utils.hpp"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include <cstring>
#include <array>

// Property IDs are defined in libobsensor/h/Property.h:
// - OB_RAW_DATA_LIDAR_IP_ADDRESS (8000)
// - OB_RAW_DATA_LIDAR_SUBNET_MASK (8003)
// - OB_PROP_LIDAR_APPLY_CONFIGS_INT (8005)
// - OB_PROP_REBOOT_DEVICE_BOOL (57)

// Parse IP string (e.g., "192.168.1.100") into 4 bytes
static bool parseIpString(const std::string &str, uint8_t *out) {
    if(str.empty()) {
        return false;
    }

    try {
        std::istringstream ss(str);
        std::string        token;
        int                count = 0;
        while(std::getline(ss, token, '.')) {
            if(count >= 4) {
                return false;
            }
            for(char c : token) {
                if(!isdigit(c)) {
                    return false;
                }
            }

            int val = std::stoi(token);
            if(val < 0 || val > 255) {
                return false;
            }
            out[count++] = static_cast<uint8_t>(val);
        }
        return count == 4;
    }
    catch(const std::exception &e) {
        (void)e;
    }
    return false;
}

// Format 4 bytes as IP string
static std::string formatIpString(const uint8_t *ip) {
    std::ostringstream oss;
    oss << static_cast<int>(ip[0]) << "."
        << static_cast<int>(ip[1]) << "."
        << static_cast<int>(ip[2]) << "."
        << static_cast<int>(ip[3]);
    return oss.str();
}

// Select a LiDAR device from the list
static std::shared_ptr<ob::Device> selectLiDARDevice(std::shared_ptr<ob::DeviceList> deviceList) {
    std::vector<uint32_t> lidarIndices;

    std::cout << "\nLiDAR Device List:" << std::endl;
    std::cout << "------------------------------------------------------------------------" << std::endl;

    uint32_t devCount = deviceList->getCount();
    for(uint32_t i = 0; i < devCount; i++) {
        // Check connection type - we need Ethernet devices
        std::string connType = deviceList->getConnectionType(i);

        // Create device temporarily to check if it's a LiDAR
        try {
            auto tempDevice = deviceList->getDevice(i);
            if(ob_smpl::isLiDARDevice(tempDevice)) {
                std::cout << lidarIndices.size() << ". Name: " << deviceList->getName(i)
                          << ", SN: " << deviceList->getSerialNumber(i)
                          << ", Connection: " << connType;

                if(connType == "Ethernet") {
                    std::cout << ", IP: " << deviceList->getIpAddress(i);
                }
                std::cout << std::endl;

                lidarIndices.push_back(i);
            }
        }
        catch(const std::exception &e) {
            // Skip devices that can't be opened
            continue;
        }
    }

    if(lidarIndices.empty()) {
        std::cout << "No LiDAR devices found!" << std::endl;
        return nullptr;
    }

    std::cout << "------------------------------------------------------------------------" << std::endl;
    std::cout << "Select a device (0-" << (lidarIndices.size() - 1) << "): ";

    uint32_t selection;
    while(true) {
        std::cin >> selection;
        if(std::cin.fail() || selection >= lidarIndices.size()) {
            std::cin.clear();
            std::cin.ignore(10000, '\n');
            std::cout << "Invalid selection. Please enter 0-" << (lidarIndices.size() - 1) << ": ";
            continue;
        }
        break;
    }
    std::cin.ignore(10000, '\n');  // Clear remaining input

    return deviceList->getDevice(lidarIndices[selection]);
}

// Get current IP address from device
static bool getCurrentIp(std::shared_ptr<ob::Device> device, uint8_t *ip) {
    try {
        uint8_t  data[16] = {0};
        uint32_t dataSize = sizeof(data);
        device->getStructuredData(OB_RAW_DATA_LIDAR_IP_ADDRESS, data, &dataSize);
        if(dataSize >= 4) {
            memcpy(ip, data, 4);
            return true;
        }
    }
    catch(const std::exception &e) {
        std::cerr << "Failed to get current IP: " << e.what() << std::endl;
    }
    return false;
}

// Get current subnet mask from device
static bool getCurrentSubnet(std::shared_ptr<ob::Device> device, uint8_t *mask) {
    try {
        uint8_t  data[16] = {0};
        uint32_t dataSize = sizeof(data);
        device->getStructuredData(OB_RAW_DATA_LIDAR_SUBNET_MASK, data, &dataSize);
        if(dataSize >= 4) {
            memcpy(mask, data, 4);
            return true;
        }
    }
    catch(const std::exception &e) {
        std::cerr << "Failed to get current subnet mask: " << e.what() << std::endl;
    }
    return false;
}

// Set IP address on device
static bool setIpAddress(std::shared_ptr<ob::Device> device, const uint8_t *ip) {
    try {
        device->setStructuredData(OB_RAW_DATA_LIDAR_IP_ADDRESS, ip, 4);
        return true;
    }
    catch(const std::exception &e) {
        std::cerr << "Failed to set IP address: " << e.what() << std::endl;
    }
    return false;
}

// Set subnet mask on device
static bool setSubnetMask(std::shared_ptr<ob::Device> device, const uint8_t *mask) {
    try {
        device->setStructuredData(OB_RAW_DATA_LIDAR_SUBNET_MASK, mask, 4);
        return true;
    }
    catch(const std::exception &e) {
        std::cerr << "Failed to set subnet mask: " << e.what() << std::endl;
    }
    return false;
}

// Apply configuration changes
static bool applyConfig(std::shared_ptr<ob::Device> device) {
    try {
        device->setIntProperty(OB_PROP_LIDAR_APPLY_CONFIGS_INT, 1);
        return true;
    }
    catch(const std::exception &e) {
        std::cerr << "Failed to apply config: " << e.what() << std::endl;
    }
    return false;
}

// Reboot device
static bool rebootDevice(std::shared_ptr<ob::Device> device) {
    try {
        device->setBoolProperty(OB_PROP_REBOOT_DEVICE_BOOL, true);
        return true;
    }
    catch(const std::exception &e) {
        std::cerr << "Failed to reboot device: " << e.what() << std::endl;
    }
    return false;
}

// Prompt user for IP address
static bool promptForIp(const std::string &prompt, const std::string &currentValue, uint8_t *out) {
    std::cout << prompt << " [current: " << currentValue << "] (press Enter to keep current): ";
    std::string input;
    std::getline(std::cin, input);

    if(input.empty()) {
        return false;  // Keep current value
    }

    if(!parseIpString(input, out)) {
        std::cerr << "Invalid IP format. Please use format: xxx.xxx.xxx.xxx" << std::endl;
        return promptForIp(prompt, currentValue, out);  // Retry
    }
    return true;
}

int main(void) try {
    std::cout << "========================================" << std::endl;
    std::cout << "  LiDAR IP Configuration Tool" << std::endl;
    std::cout << "  For Pulsar SL450/ME450 devices" << std::endl;
    std::cout << "========================================" << std::endl;

    // Create context
    ob::Context context;

    // Query device list
    auto deviceList = context.queryDeviceList();
    if(deviceList->getCount() == 0) {
        std::cout << "No devices found." << std::endl;
        std::cout << "\nPress any key to exit.";
        ob_smpl::waitForKeyPressed();
        return -1;
    }

    // Select LiDAR device
    auto device = selectLiDARDevice(deviceList);
    if(!device) {
        std::cout << "\nPress any key to exit.";
        ob_smpl::waitForKeyPressed();
        return -1;
    }

    // Get device info
    auto deviceInfo = device->getDeviceInfo();
    std::cout << "\n========================================" << std::endl;
    std::cout << "Selected Device: " << deviceInfo->getName() << std::endl;
    std::cout << "Serial Number:   " << deviceInfo->getSerialNumber() << std::endl;
    std::cout << "Firmware:        " << deviceInfo->getFirmwareVersion() << std::endl;
    std::cout << "========================================" << std::endl;

    // Get current configuration
    uint8_t currentIp[4]   = {0};
    uint8_t currentMask[4] = {0};

    bool hasIp   = getCurrentIp(device, currentIp);
    bool hasMask = getCurrentSubnet(device, currentMask);

    std::string currentIpStr   = hasIp ? formatIpString(currentIp) : "Unknown";
    std::string currentMaskStr = hasMask ? formatIpString(currentMask) : "Unknown";

    std::cout << "\nCurrent Network Configuration:" << std::endl;
    std::cout << "  IP Address:  " << currentIpStr << std::endl;
    std::cout << "  Subnet Mask: " << currentMaskStr << std::endl;
    std::cout << std::endl;

    // Prompt for new configuration
    uint8_t newIp[4]   = {0};
    uint8_t newMask[4] = {0};

    bool changeIp   = promptForIp("Enter new IP address", currentIpStr, newIp);
    bool changeMask = promptForIp("Enter new subnet mask", currentMaskStr, newMask);

    if(!changeIp && !changeMask) {
        std::cout << "\nNo changes requested." << std::endl;
        std::cout << "\nPress any key to exit.";
        ob_smpl::waitForKeyPressed();
        return 0;
    }

    // Confirm changes
    std::cout << "\n========================================" << std::endl;
    std::cout << "Pending Changes:" << std::endl;
    if(changeIp) {
        std::cout << "  IP Address:  " << currentIpStr << " -> " << formatIpString(newIp) << std::endl;
    }
    if(changeMask) {
        std::cout << "  Subnet Mask: " << currentMaskStr << " -> " << formatIpString(newMask) << std::endl;
    }
    std::cout << "========================================" << std::endl;
    std::cout << "\nApply changes? (y/n): ";

    std::string confirm;
    std::getline(std::cin, confirm);
    if(confirm != "y" && confirm != "Y") {
        std::cout << "Changes cancelled." << std::endl;
        std::cout << "\nPress any key to exit.";
        ob_smpl::waitForKeyPressed();
        return 0;
    }

    // Apply changes
    bool success = true;

    if(changeIp) {
        std::cout << "Setting IP address... ";
        if(setIpAddress(device, newIp)) {
            std::cout << "OK" << std::endl;
        }
        else {
            std::cout << "FAILED" << std::endl;
            success = false;
        }
    }

    if(changeMask) {
        std::cout << "Setting subnet mask... ";
        if(setSubnetMask(device, newMask)) {
            std::cout << "OK" << std::endl;
        }
        else {
            std::cout << "FAILED" << std::endl;
            success = false;
        }
    }

    if(success) {
        std::cout << "Applying configuration... ";
        if(applyConfig(device)) {
            std::cout << "OK" << std::endl;
        }
        else {
            std::cout << "FAILED" << std::endl;
            success = false;
        }
    }

    if(success) {
        std::cout << "\nConfiguration applied successfully!" << std::endl;
        std::cout << "\nWould you like to reboot the device now? (y/n): ";
        std::string rebootChoice;
        std::getline(std::cin, rebootChoice);

        if(rebootChoice == "y" || rebootChoice == "Y") {
            std::cout << "Rebooting device... ";
            if(rebootDevice(device)) {
                std::cout << "OK" << std::endl;
                std::cout << "\nDevice is rebooting. Please wait a few seconds and reconnect." << std::endl;
            }
            else {
                std::cout << "FAILED" << std::endl;
                std::cout << "Please manually power cycle the device." << std::endl;
            }
        }
        else {
            std::cout << "\nNote: You may need to reboot the device for changes to take effect." << std::endl;
        }
    }
    else {
        std::cout << "\nConfiguration failed. Please check the device and try again." << std::endl;
    }

    std::cout << "\nPress any key to exit.";
    ob_smpl::waitForKeyPressed();
    return success ? 0 : -1;
}
catch(ob::Error &e) {
    std::cerr << "\nError occurred!" << std::endl;
    std::cerr << "Function: " << e.getFunction() << std::endl;
    std::cerr << "Args: " << e.getArgs() << std::endl;
    std::cerr << "Message: " << e.what() << std::endl;
    std::cerr << "Type: " << e.getExceptionType() << std::endl;
    std::cout << "\nPress any key to exit.";
    ob_smpl::waitForKeyPressed();
    return -1;
}
