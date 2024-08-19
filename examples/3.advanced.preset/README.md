# C++ Sample: 3.advanced.preset

## Overview

Use the SDK interface to set and get the preset value.

### Knowledge

Pipeline is a pipeline for processing data streams, providing multi-channel stream configuration, switching, frame aggregation, and frame synchronization functions

## Code overview

1. Get the pipeline and device.

    ```cpp
        // Create a pipeline with default device.
        ob::Pipeline pipe;
        // Get the device from the pipeline.
        std::shared_ptr<ob::Device> device = pipe.getDevice();
    ```

2. Get preset list from device.

    ```cpp
    std::shared_ptr<ob::DevicePresetList> presetLists = device->getAvailablePresetList();
    ```

3. Get preset value from device.

    ```cpp
        // Print current preset name.
        std::cout << "Current PresetName: " << device->getCurrentPresetName() << std::endl;
    ```

4. Set preset value to device.

    ```cpp
        // Load preset.
        device->loadPreset(presetName);
    ```

5. Stop pipeline

    ```cpp
        pipe.stop();
    ```

## Run Sample

Press the button according to the interface prompts

### Result

![image](/docs/resource/preset.png)
