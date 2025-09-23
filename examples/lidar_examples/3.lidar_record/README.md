# C++ Sample: 3.lidar_record

## Overview

This sample demonstrates how to record LiDAR and IMU sensor data to a bag file format, providing real-time data capture with pause/resume functionality for flexible recording sessions.

### Knowledge

- **Bag File Format** is a container format for storing sensor data streams
- **RecordDevice** handles the recording process and file management

## Code overview

1. **Initialize recording setup with user-specified filename**

    ```cpp
    std::string filePath;
    std::getline(std::cin, filePath);
    
    auto context = std::make_shared<ob::Context>();
    auto device = deviceList->getDevice(0);
    auto pipe = std::make_shared<ob::Pipeline>(device);
    ```

2. **Configure and enable all available sensors**

    ```cpp
    std::shared_ptr<ob::Config> config = std::make_shared<ob::Config>();
    auto sensorList = device->getSensorList();
    for(uint32_t i = 0; i < sensorList->getCount(); i++) {
        auto sensorType = sensorList->getSensorType(i);
        config->enableStream(sensorType);
    }
    ```

3. **Start pipeline with frame monitoring callback**

    ```cpp
    pipe->start(config, [&](std::shared_ptr<ob::FrameSet> frameSet) {
        // Display frame information every 20 frames
        if(frameCount % 20 == 0) {
            std::cout << "frame index: " << frame->getIndex() 
                      << ", format: " << ob::TypeHelper::convertOBFormatTypeToString(frame->getFormat()) << std::endl;
        }
        frameCount++;
    });
    ```

4. **Initialize recording device and implement control loop**

    ```cpp
    auto recordDevice = std::make_shared<ob::RecordDevice>(device, filePath);
    
    while(true) {
        auto key = ob_smpl::waitForKeyPressed(1);
        if(key == ESC_KEY) {
            break;  // Exit recording
        }
        else if(key == 'p' || key == 'P') {
            // Toggle pause/resume recording
            if(isPaused) {
                recordDevice->resume();
            } else {
                recordDevice->pause();
            }
        }
    }
    ```

## Run Sample

### Operation Instructions

1. **Start Program**: Enter the desired output filename with `.bag` extension when prompted
2. **Real-time Monitoring**: Program displays frame information every 20 frames during recording
3. **Control Options**:
   - Press **P** to pause/resume recording
   - Press **ESC** to stop recording and exit

### Recording Features

- **Automatic Sensor Detection**: All available sensors are automatically enabled
- **Pause/Resume**: Flexible recording control without stopping the pipeline
- **Proper Cleanup**: Automatic file flushing when recording stops

### Result

The program creates a bag file containing all sensor data streams, which can be used for:
- Offline analysis and processing
- Algorithm development and testing
- Data replay and visualization
- Benchmarking and performance analysis

The recorded file maintains the original sensor data format and timing information for accurate reproduction of the capture session.

```shell
Please enter the output filename (with .bag extension) and press Enter to start recording: LiDAR.bag
frame index: 1, tsp: 1758807585890018, format: ACCEL
frame index: 1, tsp: 1758807585890018, format: GYRO
frame index: 21, tsp: 1758807586309769, format: ACCEL
frame index: 21, tsp: 1758807586309769, format: GYRO
frame index: 41, tsp: 1758807586729837, format: ACCEL
frame index: 41, tsp: 1758807586729837, format: GYRO
frame index: 17, tsp: 1758807586736121, format: LIDAR_SPHERE_POINT
Recording paused.
frame index: 61, tsp: 1758807587149993, format: ACCEL
frame index: 61, tsp: 1758807587149993, format: GYRO
frame index: 81, tsp: 1758807587570051, format: ACCEL
frame index: 81, tsp: 1758807587570051, format: GYRO
frame index: 42, tsp: 1758807587986180, format: LIDAR_SPHERE_POINT
frame index: 101, tsp: 1758807587989474, format: ACCEL
frame index: 101, tsp: 1758807587989474, format: GYRO
Recording resumed.
frame index: 121, tsp: 1758807588409878, format: ACCEL
frame index: 121, tsp: 1758807588409878, format: GYRO
frame index: 141, tsp: 1758807588829471, format: ACCEL
frame index: 141, tsp: 1758807588829471, format: GYRO
frame index: 59, tsp: 1758807588836453, format: LIDAR_SPHERE_POINT
frame index: 161, tsp: 1758807589249518, format: ACCEL
frame index: 161, tsp: 1758807589249518, format: GYRO
frame index: 181, tsp: 1758807589669535, format: ACCEL
frame index: 181, tsp: 1758807589669535, format: GYRO
frame index: 84, tsp: 1758807590086408, format: LIDAR_SPHERE_POINT
```