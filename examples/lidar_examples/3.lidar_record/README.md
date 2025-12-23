# C++ Sample: 3.lidar_record

## Overview

This sample demonstrates how to record LiDAR and IMU sensor data to a bag file format, providing real-time data capture with pause/resume functionality for flexible recording sessions.

### Knowledge

- **Bag File Format** is a container format for storing sensor data streams
- **RecordDevice** handles the recording process and file management

## Code overview

1. **Initialize context and device selection**

    ```cpp
    ob::Context context;
    auto deviceList = context.queryDeviceList();
    auto device = selectDevice(deviceList);
    ```

2. **Frame counting with thread-safe monitoring**

    ```cpp
    std::mutex                      frameMutex;
    std::map<OBFrameType, uint64_t> frameCountMap;
    pipe->start(config, [&](std::shared_ptr<ob::FrameSet> frameSet) {
        if(frameSet == nullptr) {
            return;
        }

        std::lock_guard<std::mutex> lock(frameMutex);
        auto                        count = frameSet->getCount();
        for(uint32_t i = 0; i < count; i++) {
            auto frame = frameSet->getFrameByIndex(i);
            if(frame) {
                auto type = frame->getType();
                frameCountMap[type]++;
            }
        }
    });
    ```

3. **Recording setup**

    ```cpp
    // Initialize recording with user-specified filename
    auto recordDevice = std::make_shared<ob::RecordDevice>(device, filePath);
    ```

4. **Real-time FPS calculation and display**

    ```cpp
    std::cout << "Recording... Current FPS: ";
    for(const auto &item: tempCountMap) {
        auto  name = ob::TypeHelper::convertOBFrameTypeToString(item.first);
        float rate = item.second / (duration / 1000.0f);

        std::cout << std::fixed << std::setprecision(2) << std::showpoint;
        std::cout << seperate << name << "=" << rate;
        seperate = ", ";
    }
    std::cout << std::endl;
    ```

5. **Safe recording termination**

    ```cpp
    do {
        auto key = ob_smpl::waitForKeyPressed(50);
        if(key == ESC_KEY || key == 'q' || key == 'Q') {
            break;  // Safe exit
        }
    } while(true);
    
    pipe->stop();
    recordDevice = nullptr;  // Flush and save
    ```

## Run Sample

### Operation Instructions

1. **Start Program**: Enter the desired output filename with `.bag` extension
2. **Automatic Setup**: Program validates LiDAR device and enables all sensors
3. **Real-time Monitoring**: Watch FPS statistics for each sensor type
4. **Safe Exit**: Use **ESC**, **Q**, or **q** to stop recording properly

### Safety Features

- **Device Validation**: Ensures compatible LiDAR device before recording
- **Proper Termination**: Prevents file corruption through controlled shutdown
- **Thread Protection**: Prevents data races in frame counting
- **File Flushing**: Ensures all data is written before program exit

### Monitoring Capabilities

- **Multi-sensor FPS**: Tracks frame rates for LiDAR and IMU sensors separately
- **Dynamic Update**: Adjusts monitoring interval based on data availability
- **Real-time Feedback**: Immediate visual confirmation of recording status

### Result

The program creates a complete sensor data recording with:

- **Comprehensive Data Capture**: All available sensor streams are recorded simultaneously
- **Performance Metrics**: Real-time FPS monitoring helps assess recording quality
- **Safe File Handling**: Proper termination ensures usable bag files
- **Professional Workflow**: Suitable for production data collection and testing

```shell
Device list:
0. name: LiDAR ME450, vid: 0x2bc5, pid: 0x1302, uid: 0x20:4b:5e:00:43:09, sn: T0H6851001Z
1. name: LiDAR ME450, vid: 0x2bc5, pid: 0x1302, uid: 0x20:4b:5e:13:64:30, sn: T0H6851000Z
Select a device: 1

------------------------------------------------------------------------
Please enter the output filename (with .bag extension) and press Enter to start recording: test.bag
Streams and recorder have started!
Press ESC, 'q', or 'Q' to stop recording and exit safely.
IMPORTANT: Always use ESC/q/Q to stop! Otherwise, the bag file will be corrupted and unplayable.

Recording... Current FPS: Accel=50.14, Gyro=50.14, LiDAR Points=19.87
Recording... Current FPS: Accel=49.93, Gyro=49.93, LiDAR Points=20.36
Recording... Current FPS: Accel=50.46, Gyro=50.46, LiDAR Points=20.38
Recording... Current FPS: Accel=49.95, Gyro=49.95, LiDAR Points=20.37
Recording... Current FPS: Accel=49.95, Gyro=49.95, LiDAR Points=20.37
Recording... Current FPS: Accel=48.98, Gyro=48.98, LiDAR Points=19.99
Recording... Current FPS: Accel=50.41, Gyro=50.41, LiDAR Points=20.36
Recording... Current FPS: Accel=50.41, Gyro=50.41, LiDAR Points=20.36
```