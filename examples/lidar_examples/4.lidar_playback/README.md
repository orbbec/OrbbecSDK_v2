# C++ Sample: 4.lidar_playback

## Overview

This sample demonstrates how to play back recorded LiDAR and IMU sensor data from bag files, providing full control over playback including pause/resume functionality and automatic replay capabilities.

### Knowledge

- **PlaybackDevice** handles reading and streaming data from recorded bag files
- **Playback Status** tracks the current state of playback (playing, paused, stopped)
- **Frame Callback** processes replayed frames similarly to live data
- **Automatic Replay** enables continuous playback loops

## Code overview

1. **Initialize playback device with user-specified bag file**

    ```cpp
    std::string filePath;
    getRosbagPath(filePath);
    
    auto playback = std::make_shared<ob::PlaybackDevice>(filePath);
    auto pipe = std::make_shared<ob::Pipeline>(playback);
    ```

2. **Configure playback with automatic replay capability**

    ```cpp
    playback->setPlaybackStatusChangeCallback([&](OBPlaybackStatus status) {
        playStatus = status;
        replayCv.notify_all();
    });
    ```

3. **Enable all recorded streams and start playback**

    ```cpp
    auto sensorList = playback->getSensorList();
    for(uint32_t i = 0; i < sensorList->getCount(); i++) {
        auto sensorType = sensorList->getSensorType(i);
        config->enableStream(sensorType);
    }
    
    pipe->start(config, frameCallback);
    ```

4. **Frame monitoring callback with playback control**

    ```cpp
    auto frameCallback = [&](std::shared_ptr<ob::FrameSet> frameSet) {
        if(frameCount % 20 == 0) {
            std::cout << "frame index: " << frame->getIndex() 
                      << ", format: " << ob::TypeHelper::convertOBFormatTypeToString(frame->getFormat()) << std::endl;
        }
        frameCount++;
    };
    ```

5. **Interactive playback control loop**

    ```cpp
    while(true) {
        auto key = ob_smpl::waitForKeyPressed(1);
        if(key == ESC_KEY) {
            break;  // Exit playback
        }
        else if(key == 'p' || key == 'P') {
            // Toggle pause/resume playback
            if(playback->getPlaybackStatus() == OB_PLAYBACK_PLAYING) {
                playback->pause();
            } else {
                playback->resume();
            }
        }
    }
    ```

## Run Sample

### Operation Instructions

1. **Start Program**: Enter the path to a valid `.bag` file when prompted
2. **Playback Information**: Program displays file duration and starts playback
3. **Control Options**:
   - Press **P** to pause/resume playback
   - Press **ESC** to stop playback and exit
   - When playback completes, it automatically restarts from the beginning

### Playback Features

- **File Validation**: Ensures provided file has correct `.bag` extension
- **Real-time Monitoring**: Displays frame information every 20 frames during playback
- **Seamless Looping**: Automatically restarts when playback reaches end
- **Status Awareness**: Tracks and responds to playback state changes

### Result

The program provides a complete playback solution that:
- Recreates the original sensor data stream with timing fidelity
- Maintains frame metadata including indices, timestamps, and formats
- Offers interactive control for analysis and debugging
- Supports continuous operation for testing and demonstration purposes

This enables thorough analysis of recorded sensor data with the same processing pipeline used for live data, making it ideal for algorithm development and data validation.

```shell
Please input the path of the Rosbag file (.bag) to playback:
Path: LiDAR
Invalid file format. Please provide a .bag file.

Please input the path of the Rosbag file (.bag) to playback:
Path: LiDAR.bag
Playback file confirmed: LiDAR.bag

Duration: 5669
frame index: 2, tsp: 1758807585910889, format: ACCEL
frame index: 2, tsp: 1758807585910889, format: GYRO
frame index: 22, tsp: 1758807586330468, format: ACCEL
frame index: 22, tsp: 1758807586330468, format: GYRO
frame index: 9, tsp: 1758807586336156, format: LIDAR_SPHERE_POINT
frame index: 42, tsp: 1758807586750593, format: ACCEL
frame index: 42, tsp: 1758807586750593, format: GYRO
Playback paused
Playback resumed
frame index: 130, tsp: 1758807588598443, format: ACCEL
frame index: 130, tsp: 1758807588598443, format: GYRO
frame index: 150, tsp: 1758807589018829, format: ACCEL
frame index: 150, tsp: 1758807589018829, format: GYRO
frame index: 71, tsp: 1758807589436271, format: LIDAR_SPHERE_POINT
Replay monitor thread exists
exit
```

