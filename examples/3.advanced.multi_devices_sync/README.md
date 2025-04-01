# C++ Sample: 3.advanced.multi_devices_sync

## Overview

Function description: Demonstrate multi devices synchronization operation,This sample supports network devices, USB devices, and GMSL devices (such as Gemini 335lg). 

Note that network devices and USB devices must be connected to a sync hub(via the 8-pin port), while GMSL devices can connect via the 8-pin port or through multi-device sync via GMSL2 /FAKRA, Gemini 335lg multi device sync please refer [this document](https://www.orbbec.com/docs/gemini-335lg-hardware-synchronization/).


## Code overview

### 1.Configure multi device synchronization

```cpp
    configMultiDeviceSync();
```

### 2.Distinguishing secondary devices

```cpp
    streamDevList.clear();
    // Query the list of connected devices
    auto devList = context.queryDeviceList();
  
    // Get the number of connected devices
    int devCount = devList->deviceCount();
    for(int i = 0; i < devCount; i++) {
        streamDevList.push_back(devList->getDevice(i));
    }
  
    if(streamDevList.empty()) {
        std::cerr << "Device list is empty. please check device connection state" << std::endl;
        return -1;
    }
  
    // traverse the device list and create the device
    std::vector<std::shared_ptr<ob::Device>> primary_devices;
    std::vector<std::shared_ptr<ob::Device>> secondary_devices;
    for(auto dev: streamDevList) {
        auto config = dev->getMultiDeviceSyncConfig();
        if(config.syncMode == OB_MULTI_DEVICE_SYNC_MODE_PRIMARY) {
            primary_devices.push_back(dev);
        }
        else {
            secondary_devices.push_back(dev);
        }
    }
```

### 3.Enable secondary devices

```cpp
    std::cout << "Secondary devices start..." << std::endl;
    int deviceIndex = 0;  // Sencondary device display first
    for(auto itr = secondary_devices.begin(); itr != secondary_devices.end(); itr++) {
        auto depthHolder = createPipelineHolder(*itr, OB_SENSOR_DEPTH, deviceIndex);
        pipelineHolderList.push_back(depthHolder);
        startStream(depthHolder);
  
        auto colorHolder = createPipelineHolder(*itr, OB_SENSOR_COLOR, deviceIndex);
        pipelineHolderList.push_back(colorHolder);
        startStream(colorHolder);
  
        deviceIndex++;
    }
```

### 4.Enable Primary device


```cpp
    std::cout << "Primary device start..." << std::endl;
    deviceIndex = secondary_devices.size();  // Primary device display after primary devices.
    for(auto itr = primary_devices.begin(); itr != primary_devices.end(); itr++) {
        auto depthHolder = createPipelineHolder(*itr, OB_SENSOR_DEPTH, deviceIndex);
        startStream(depthHolder);
        pipelineHolderList.push_back(depthHolder);
  
        auto colorHolder = createPipelineHolder(*itr, OB_SENSOR_COLOR, deviceIndex);
        startStream(colorHolder);
        pipelineHolderList.push_back(colorHolder);
  
        deviceIndex++;
    }
```

### 5.Set software synchronization interval time

```cpp
    // Start the multi-device time synchronization function
    context.enableDeviceClockSync(60000);  
```

### 6.Conduct multi device testing

```cpp
    testMultiDeviceSync();
```

### 7.Close data stream

```cpp
    // close data stream
    for(auto itr = pipelineHolderList.begin(); itr != pipelineHolderList.end(); itr++) {
        stopStream(*itr);
    }
```

### 8.Triggering Mode

#### Software Triggering Mode

Set the device synchronization mode to `OB_MULTI_DEVICE_SYNC_MODE_SOFTWARE_TRIGGERING` after opening the stream, and the device will wait for the trigger signal (command) sent by the upper layer after opening the stream. The number of frames to be triggered for triggering mode can be configured through `framesPerTrigger`. The method for triggering images:

```c++
auto multiDeviceSyncConfig = dev->getMultiDeviceSyncConfig();
if(multiDeviceSyncConfig.syncMode == OB_MULTI_DEVICE_SYNC_MODE_SOFTWARE_TRIGGERING)
{
    dev->triggerCapture();
}
```

*Press `t` in the render window to trigger a capture once.*

## Configuration file parameter description
*Notes："The configuration parameters for multi-device Sync may vary between different devices. Please refer to the [Multi-device Sync documentation](https://www.orbbec.com/docs-general/set-up-cameras-for-external-synchronization_v1-2/)*

config file :  MultiDeviceSyncConfig.json

```
{
    "version": "1.0.0",
    "configTime": "2023/01/01",
    "devices": [
        {
            "sn": "CP2194200060", //device serial number
            "syncConfig": {
                "syncMode": "OB_MULTI_DEVICE_SYNC_MODE_PRIMARY",     // sync mode
                "depthDelayUs": 0,                                   //Configure depth trigger delay, unit: microseconds
                "colorDelayUs": 0,                                   //Configure color trigger delay, unit: microseconds
                "trigger2ImageDelayUs": 0,                           //Configure trigger image delay, unit: microseconds
                "triggerOutEnable": true,                            //Configure trigger signal output enable.
                "triggerOutDelayUs": 0,                              //Configure trigger signal output delay, unit: microsecond
                "framesPerTrigger": 1                                //Configure the number of frames captured by each trigger in the trigger mode
            }
        },
        {
            "sn": "CP0Y8420004K",
            "syncConfig": {
                "syncMode": "OB_MULTI_DEVICE_SYNC_MODE_SECONDARY",
                "depthDelayUs": 0,
                "colorDelayUs": 0,
                "trigger2ImageDelayUs": 0,
                "triggerOutEnable": true,
                "triggerOutDelayUs": 0,
                "framesPerTrigger": 1
            }
        }
    ]
}
```

## GMSL Multi devices Sync

###  Method 1:Multi-device sync via 8-pin port
When using 8-pin port for multi-device synchronization, in order to ensure the quality of the synchronization signal, it is necessary to use it together with a multi-device sync hub,please refer [Multi-device Sync documentation](https://www.orbbec.com/docs-general/set-up-cameras-for-external-synchronization_v1-2/).

via 8-pin port, GMSL multi devices sync is the same as that for USB devices, and the supported synchronization modes are also the same.

### Method 2: Multi-device sync via GMSL2/FAKRA

GMSL Multi devices Sync please refer [this document](https://www.orbbec.com/docs/gemini-335lg-hardware-synchronization/),There are two usage methods:

The first is to set all devices as OB_MULTI_DEVICE_SYNC_MODE_SECONDARY mode and synchronize them through PWM triggering. 

The second is to set all devices as OB_MULTI_DEVICE_SYNC_MODE_HARDWARE_TRIGGERING mode and synchronize them through PWM triggering. 


PWM triggering please refer ob_multi_devices_sync_gmsltrigger sample.

* Notes: To make the multi devices sync sample simple and versatile, the PWM trigger has been separated into its own sample. GMSL2/FAKRA requires running two samples for testing. If you are developing your own application, you can combine these two functionalities into a single application.


## Run Sample

```cpp
$ ./ob_multi_devices_sync

--------------------------------------------------
Please select options: 
 0 --> config devices sync mode. 
 1 --> start stream 
--------------------------------------------------
Please select input: 0


0: Configure sync mode and start stream

1: Start stream: If the parameters for multi device sync mode have been configured, you can start the stream directly.

