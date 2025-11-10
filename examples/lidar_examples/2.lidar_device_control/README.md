# C++ Sample: 2.lidar_device_control

## Overview

This sample demonstrates how to interactively control LiDAR device properties through a command-line interface, allowing users to get and set various device parameters.

### Knowledge

- **Device Properties** are configurable parameters that control sensor behavior
- **Property Types** include boolean, integer, and float values with specific ranges
- **Permission System** determines which properties can be read or written

## Code overview

1. **Initialize context and device selection**

    ```cpp
    ob::Context context;
    auto deviceList = context.queryDeviceList();
    auto device = selectDevice(deviceList);
    ```

2. **Retrieve and display available properties**

    ```cpp
    std::vector<OBPropertyItem> propertyList = getPropertyList(device);
    printfPropertyList(device, propertyList);
    ```

3. **Interactive property control loop**

    ```cpp
    while(isSelectProperty) {
        std::string choice;
        std::getline(std::cin, choice);
        
        if(choice == "?") {
            printfPropertyList(device, propertyList);
        }
        else {
            // Parse commands like: "0 set 100" or "1 get"
            if(controlVec.at(1) == "get") {
                getPropertyValue(device, propertyItem);
            }
            else {
                setPropertyValue(device, propertyItem, controlVec.at(2));
            }
        }
    }
    ```

4. **Property value manipulation**

    ```cpp
    void setPropertyValue(std::shared_ptr<ob::Device> device, OBPropertyItem propertyItem, std::string strValue) {
        switch(propertyItem.type) {
        case OB_BOOL_PROPERTY:
            device->setBoolProperty(propertyItem.id, bool_value);
            break;
        case OB_INT_PROPERTY:
            device->setIntProperty(propertyItem.id, int_value);
            break;
        case OB_FLOAT_PROPERTY:
            device->setFloatProperty(propertyItem.id, float_value);
            break;
        }
    }
    ```

## Run Sample

### Command Usage

- **`?`** - Display all available properties with their ranges and permissions
- **`[index] get`** - Get current value of specified property
- **`[index] set [value]`** - Set new value for specified property
- **`exit`** - Exit the program

### Result

The program provides an interactive interface to monitor and modify LiDAR device parameters, displaying property information including:
- Property names and IDs
- Read/write permissions  
- Value ranges and types
- Current values when queried

```shell
Device list:
0. name: Orbbec_LiDAR_ME450, vid: 0x2bc5, pid: 0x1302, uid: 0x20:4b:5e:00:43:09, sn: T0H6851001Z
1. name: Orbbec_LiDAR_ME450, vid: 0x2bc5, pid: 0x1302, uid: 0x20:4b:5e:13:64:30, sn: T0H6851000Z
Select a device: 1

------------------------------------------------------------------------
Current Device:  name: Orbbec_LiDAR_ME450, vid: 0x2bc5, pid: 0x1302, uid: 0x20:4b:5e:13:64:30
Input "?" to get all properties.
Input "exit" to exit the program.
?
size: 11

------------------------------------------------------------------------
00. OB_PROP_REBOOT_DEVICE_BOOL(57), permission=_/W, range=Bool value(min:0, max:1, step:1)
01. OB_PROP_LIDAR_PORT_INT(8001), permission=R/W, range=Int value(min:1024, max:65535, step:1)
02. OB_PROP_LIDAR_APPLY_CONFIGS_INT(8005), permission=_/W, range=Int value
03. OB_PROP_LIDAR_TAIL_FILTER_LEVEL_INT(8006), permission=R/W, range=Int value(min:0, max:5, step:1)
04. OB_PROP_LIDAR_MEMS_FOV_SIZE_FLOAT(8007), permission=R/W, range=Float value(min:0.000000, max:60.000000, step:0.010000)
05. OB_PROP_LIDAR_MEMS_FRENQUENCY_FLOAT(8008), permission=R/W, range=Float value(min:0.000000, max:1100.000000, step:0.500000)
06. OB_PROP_LIDAR_WARNING_INFO_INT(8012), permission=R/_, range=Int value(min:0, max:1023, step:1)
07. OB_PROP_LIDAR_MOTOR_SPIN_SPEED_INT(8013), permission=R/_, range=Int value(min:0, max:1200, step:300)
08. OB_PROP_LIDAR_MCU_TEMPERATURE_INT(8014), permission=R/_, range=Int value(min:0, max:10000, step:1)
09. OB_PROP_LIDAR_APD_TEMPERATURE_INT(8015), permission=R/_, range=Int value(min:0, max:10000, step:1)
10. OB_PROP_LIDAR_REPETITIVE_SCAN_MODE_INT(8017), permission=R/W, range=Int value(min:0, max:3, step:1)
------------------------------------------------------------------------
Please select property.(Property control usage: [property number] [set/get] [property value])
1 get
property name:OB_PROP_LIDAR_PORT_INT,get int value:2401
3 set 2
property name:OB_PROP_LIDAR_TAIL_FILTER_LEVEL_INT,set int value:2
3 get
property name:OB_PROP_LIDAR_TAIL_FILTER_LEVEL_INT,get int value:2
exit
```


