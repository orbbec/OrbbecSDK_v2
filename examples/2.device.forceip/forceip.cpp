// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include <libobsensor/ObSensor.hpp>
#include "utils.hpp"

#include <thread>
#include <string>
#include <vector>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <iostream>

static bool parseIpString(const std::string &Str, uint8_t *out) {
    if(Str.empty()) {
        return false;
    }

    try {
        std::istringstream ss(Str);
        std::string        token;
        int                count = 0;
        while(std::getline(ss, token, '.')) {
            if(count > 4) {
                return false;
            }
            for(char c: token) {
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
        // error
        (void)e;
    }
    return false;
}

static bool selectDevice(std::shared_ptr<ob::DeviceList> deviceList, uint32_t &selectedIndex) {
    selectedIndex = static_cast<uint32_t>(-1);

    auto devCount = deviceList->getCount();
    if(devCount == 0) {
        std::cout << "No devices found." << std::endl;
        return false;
    }

    std::vector<uint32_t> indexList;
    uint32_t              count = 0;

    std::cout << "Ethernet device list:" << std::endl;
    for(uint32_t i = 0; i < devCount; i++) {
        std::string DeviceConnectType = deviceList->getConnectionType(i);
        if(DeviceConnectType != "Ethernet") {
            continue;
        }
        std::cout << count << ". Name: " << deviceList->getName(i) << ", Serial Number: " << deviceList->getSerialNumber(i)
                  << ", MAC: " << deviceList->getUid(i) << std::dec << ", IP: " << deviceList->getIpAddress(i)
                  << ", Subnet Mask: " << deviceList->getSubnetMask(i) << ", Gateway: " << deviceList->getGateway(i) << std::endl;
        indexList.push_back(i);
        count++;
    }
    if(indexList.empty()) {
        std::cout << "No network devices found." << std::endl;
        return false;
    }

    uint32_t index;
    do {
        std::cout << "Enter your choice: ";
        std::cin >> index;
        if(std::cin.fail()) {
            std::cin.clear();
            std::cin.ignore();
            std::cout << "Invalid input, please enter a number." << std::endl;
            continue;
        }

        if(index >= indexList.size()) {
            std::cout << "Invalid input, please enter a valid index number." << std::endl;
            continue;
        }
        selectedIndex = indexList[index];
        return true;
    } while(true);
    return false;
}

static OBNetIpConfig getIPConfig() {
    OBNetIpConfig cfg;
    std::string   val;
    uint8_t       address[4];
    uint8_t       mask[4];
    uint8_t       gateway[4];

    std::cout << "Please enter the network configuration information:" << std::endl;    
    std::cout << "Enter IP address:" << std::endl;
    while(std::cin >> val) {
        if(parseIpString(val, address)) {
            break;
        }
        std::cout << "Invalid format." << std::endl;
        std::cout << "Enter IP address:" << std::endl;
    }

    std::cout << "Enter Subnet Mask:" << std::endl;
    while(std::cin >> val) {
        if(parseIpString(val, mask)) {
            break;
        }
        std::cout << "Invalid format." << std::endl;
        std::cout << "Enter Subnet Mask:" << std::endl;
    }

    std::cout << "Enter Gateway address:" << std::endl;
    while(std::cin >> val) {
        if(parseIpString(val, gateway)) {
            break;
        }
        std::cout << "Invalid format." << std::endl;
        std::cout << "Enter Gateway address:" << std::endl;
    }

    cfg.dhcp = 0;
    for(int i = 0; i < 4; ++i) {
        cfg.address[i] = address[i];
        cfg.gateway[i] = gateway[i];
        cfg.mask[i]    = mask[i];
    }
    return cfg;
}

int main(void) try {
    // Create a Context object to interact with Orbbec devices
    ob::Context context;
    // Query the list of connected devices
    auto deviceList = context.queryDeviceList();
    // Select a device to operate
    uint32_t selectedIndex;
    auto     res = selectDevice(deviceList, selectedIndex);
    if(res) {
        // Get the new IP configuration from user input
        OBNetIpConfig config = getIPConfig();

        // Change device IP configuration
        res = context.forceIp(deviceList->getUid(selectedIndex), config);
        if(res) {
            std::cout << "The new IP configuration has been successfully applied to the device." << std::endl;
        }
        else {
            std::cout << "Failed to apply the new IP configuration." << std::endl;
        }
    }

    std::cout << "\nPress any key to exit.";
    ob_smpl::waitForKeyPressed();
    return 0;
}
catch(ob::Error &e) {
    std::cerr << "Function: " << e.getFunction() << "\nArguments: " << e.getArgs() << "\nMessage: " << e.what() << "\nException Type: " << e.getExceptionType()
              << std::endl;
    std::cout << "\nPress any key to exit.";
    ob_smpl::waitForKeyPressed();
    exit(EXIT_FAILURE);
}
