# C++ Sample：2.device.firmware_update

## Overview

This sample demonstrates how to use the SDK to update the firmware of a connected device. It includes functions to list connected devices, select a device, and update its firmware.

> Note: This sample are not suiltable for Femto Mega, Femto Mega i, and Femto Bolt devices.
> For these devices, please refer to the this repo:[https://github.com/orbbec/OrbbecFirmware](https://github.com/orbbec/OrbbecFirmware)

### Knowledge

Context is the environment context, the first object created during initialization, which can be used to perform some settings, including but not limited to device status change callbacks, log level settings, etc. Context can access multiple Devices.

Device is the device object, which can be used to obtain the device information, such as the model, serial number, and various sensors.One actual hardware device corresponds to one Device object.

## code overview

1. Initialize the SDK Context: This is necessary to access the connected devices.

    ```c++
        std::shared_ptr<ob::Context> context = std::make_shared<ob::Context>();
    ```
2. List Connected Devices.

    ```c++
        std::shared_ptr<ob::DeviceList> deviceList = context->queryDeviceList();
        for(uint32_t i = 0; i < deviceList->getCount(); ++i) {
            devices.push_back(deviceList->getDevice(i));
        }
    ```
3. Define a Callback Function for Firmware Update Progress.

    You can define a callback function to get the progress of the firmware update. The callback function will be called every time the device updates its progress.

    ```c++
        ob::Device::DeviceFwUpdateCallback firmwareUpdateCallback = [](OBFwUpdateState state, const char *message, uint8_t percent) {
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
        };
    ```

4. Update the Device Firmware.

    After selecting a device, update its firmware by calling the updateFirmware function with the specified callback.

    ```c++
        devices[deviceIndex]->updateFirmware(firmwarePath.c_str(), firmwareUpdateCallback, false);
    ```

### Attention

1. After the firmware update completes, you need to restart the device manually to apply the new firmware. Alternatively, you can use the `reboot()` function to restart the device programmatically.

    ```c++
        device->reboot();
    ```

2. Don't plug out the device during the firmware update process.

3. For linux users, it is recommended to use the `LibUVC` as the backend as the `V4L2` backend may cause some issues on some systems. Switch backend before create device like this:

    ```c++
        context->setUvcBackendType(OB_UVC_BACKEND_TYPE_LIBUVC);
    ```

## Run Sample

Select the device for firmware update and input the path of the firmware file. The SDK will start updating the firmware, and the progress will be displayed on the console.

### Result

![image](../../docs/resource/device_firmware_update.jpg)
