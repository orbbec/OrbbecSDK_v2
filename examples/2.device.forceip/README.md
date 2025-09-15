# C++ Sample: 2.device.forceip

## Overview
This sample demonstrates how to use the SDK context class to query connected devices, configure the network IP of a selected device using the ForceIP command (as defined by the GigE Vision standard)

### Knowledge
The Context class serves as the entry point to the SDK. It provides functionality to:
1. Query connected device lists
2. Modify network configurations for the selected device

## Code Overview

1. Query device list and select a device

    ```cpp
    // Create a Context object to interact with Orbbec devices
    ob::Context context;
    // Query the list of connected devices
    auto deviceList = context.queryDeviceList();
    // Select a device to operate
    uint32_t selectedIndex;
    auto     res = selectDevice(deviceList, selectedIndex);
    ```

2. Get new IP configuration from user input

    ```cpp
    OBNetIpConfig config = getIPConfig();
    ```

3. Change the selected device IP configuration and print the result of the operation.

    ```cpp
    res = context.forceIp(deviceList->getUid(deviceNumber), config);
    if(res) {
        std::cout << "The new IP configuration has been successfully applied to the device." << std::endl;
    }
    else {
        std::cout << "Failed to apply the new IP configuration." << std::endl;
    }
    ```

## Run Sample
Device list:
Enter your choice: 
Please enter the network configuration information:
Enter IP address:
Enter Subnet Mask:
Enter Gateway address:
The new IP configuration has been successfully applied to the device.

### Result
![result](/docs/resource/forceip.jpg)
