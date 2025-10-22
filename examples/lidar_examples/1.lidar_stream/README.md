# C++ Sample: 1.lidar_stream

## Overview

This sample demonstrates how to configure and start LiDAR and IMU sensor streams, process the data frames through a callback function, and display sensor information.

### Knowledge

- **Pipeline** is a data stream processing pipeline that provides multi-stream configuration, switching, frame aggregation, and frame synchronization functions
- **Config** is used to configure which streams to enable and their parameters
- **FrameSet** contains multiple types of sensor data frames
- **Callback mode** allows asynchronous processing of incoming data

## Code Overview

1. **Initialize pipeline and configure streams**

    ```cpp
    ob::Pipeline pipe;
    std::shared_ptr<ob::Config> config = std::make_shared<ob::Config>();
    config->disableAllStream();
    
    // Select and enable desired streams
    selectStreams(device, config);
    config->setFrameAggregateOutputMode(OB_FRAME_AGGREGATE_OUTPUT_ALL_TYPE_FRAME_REQUIRE);
    ```

2. **Set/Get property**

    ```c
    // Get property
    device->getStructuredData(OB_STRUCT_DEVICE_SERIAL_NUMBER, data, &dataSize);
    // Set property
    device->setIntProperty(OB_PROP_LIDAR_TAIL_FILTER_LEVEL_INT, 0);
    ```

3. **Start pipeline with callback function for frame processing**

    ```cpp
    pipe.start(config, [&](std::shared_ptr<ob::FrameSet> frameSet) {
        // Process each frame in the frameset
        for(uint32_t i = 0; i < frameSet->getCount(); i++) {
            auto frame = frameSet->getFrame(i);
            
            // Print frame information every 50 frames
            if(frameCount % 50 == 0) {
                if(frame->getType() == OB_FRAME_LIDAR_POINTS) {
                    auto lidarFrame = frame->as<ob::LiDARPointsFrame>();
                    printLiDARPointCloudInfo(lidarFrame);
                }
                else if(frame->getType() == OB_FRAME_ACCEL) {
                    // Process accelerometer data
                }
                else if(frame->getType() == OB_FRAME_GYRO) {
                    // Process gyroscope data
                }
            }
        }
        frameCount++;
    });
    ```

4. **Stream selection and configuration interface**

    ```cpp
    void selectStreams(std::shared_ptr<ob::Device> device, std::shared_ptr<ob::Config> config) {
        auto selectedSensors = selectSensors(device);
        for(auto &sensor: selectedSensors) {
            auto streamProfileList = sensor->getStreamProfileList();
            // Enable selected stream profile
            config->enableStream(selectedStreamProfile);
        }
    }
    ```

5. **LiDAR point cloud data processing**

    ```cpp
    void printLiDARPointCloudInfo(std::shared_ptr<ob::LiDARPointsFrame> pointCloudFrame) {
        auto pointCloudType = pointCloudFrame->getFormat();
        uint32_t validPointCount = 0;
        
        // Process different point cloud formats
        switch(pointCloudType) {
        case OB_FORMAT_LIDAR_SPHERE_POINT:
            // Process 3D point data
            break;
        case OB_FORMAT_LIDAR_POINT:
            // Process 3D point data
            break;
        case OB_FORMAT_LIDAR_SCAN:
            // Process 2D scan data  
            break;
        }
        
        std::cout << "valid point count = " << validPointCount << std::endl;
    }
    ```

## Run Sample

- The program will display available sensors and stream profiles for selection
- Selected streams will start running and print frame information every 50 frames
- Press **ESC** key to exit the program

### Result

The program outputs LiDAR point cloud information (frame index, timestamp, format, valid point count) and IMU data (acceleration, angular velocity) at regular intervals, demonstrating real-time sensor data acquisition and processing.

```shell
------------------------------------------------------------------------
Current Device: name: Orbbec_LiDAR_ME450, vid: 0x2bc5, pid: 0x1302, uid: 0x20:4b:5e:13:64:30
LiDAR SN: T0HW8510001

Sensor list:
 - 0.sensor type: Accel
 - 1.sensor type: Gyro
 - 2.sensor type: LiDAR
 - 3.all sensors
Select a sensor to enable(input sensor index, '3' to select all sensors):
Stream profile list for sensor: Accel
 - 0.acc rate: 50_HZ
 - 1.acc rate: 25_HZ
 - 2.acc rate: 100_HZ
 - 3.acc rate: 200_HZ
 - 4.acc rate: 500_HZ
 - 5.acc rate: 1_KHZ
Select a stream profile to enable (input stream profile index):
Stream profile list for sensor: Gyro
 - 0.gyro rate: 50_HZ
 - 1.gyro rate: 25_HZ
 - 2.gyro rate: 100_HZ
 - 3.gyro rate: 200_HZ
 - 4.gyro rate: 500_HZ
 - 5.gyro rate: 1_KHZ
Select a stream profile to enable (input stream profile index):
Stream profile list for sensor: LiDAR
 - 0.format: LIDAR_SPHERE_POINT, scan rate: 20HZ
 - 1.format: LIDAR_POINT, scan rate: 20HZ
 - 2.format: LIDAR_SPHERE_POINT, scan rate: 15HZ
 - 3.format: LIDAR_POINT, scan rate: 15HZ
 - 4.format: LIDAR_SPHERE_POINT, scan rate: 10HZ
 - 5.format: LIDAR_POINT, scan rate: 10HZ
Select a stream profile to enable (input stream profile index):
frame index: 4
Accel Frame:
{
  tsp = 1758807027704535
  temperature = 0
  Accel.x = -0.0065918m/s^2
  Accel.y = -0.00622559m/s^2
  Accel.z = -1.01978m/s^2
}

frame index: 4
Gyro Frame:
{
  tsp = 1758807027704535
  temperature = 0
  Gyro.x = -1.51062rad/s
  Gyro.y = -1.93024rad/s
  Gyro.z = 0.747681rad/s
}

frame index: 1
LiDAR PointCloud Frame:
{
  tsp = 1758807027712041
  format = LIDAR_SPHERE_POINT
  valid point count = 6182
}
```

