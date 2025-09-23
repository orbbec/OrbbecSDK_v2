# C Sample: 1.c_lidar_stream

## Overview

This C language sample demonstrates how to configure and stream LiDAR and IMU sensor data using the Orbbec SDK C API, providing interactive sensor selection and real-time data monitoring.

### Knowledge

- **C API Streaming** uses callback-based frame processing similar to C++ but with manual memory management
- **Interactive Configuration** allows users to select sensors and stream profiles at runtime
- **Multi-sensor Support** handles LiDAR point cloud, accelerometer, and gyroscope data
- **Frame Processing** includes coordinate conversion and valid point counting

## Code overview

1. **Initialize pipeline and configuration with frame aggregation**

    ```c
    ob_pipeline *pipe = ob_create_pipeline(&error);
    ob_device *device = ob_pipeline_get_device(pipe, &error);
    ob_config *config = ob_create_config(&error);
    
    ob_config_set_frame_aggregate_output_mode(config, OB_FRAME_AGGREGATE_OUTPUT_ALL_TYPE_FRAME_REQUIRE, &error);
    ob_config_disable_all_stream(config, &error);
    ```

2. **Set/Get property**

    ```c
    // Get property
    ob_device_get_structured_data(device, OB_RAW_DATA_LIDAR_SERIAL_NUMBER, data, &data_size, &error);
    // Set property
    ob_device_set_int_property(device, OB_PROP_LIDAR_TAIL_FILTER_LEVEL_INT, 0, &error);
    ```


3. **Interactive sensor and stream profile selection**

    ```c
    select_sensors_and_streams(device, config);
    ob_pipeline_start_with_callback(pipe, config, frame_callback, NULL, &error);
    ```

4. **Frame callback with multi-sensor data processing**

    ```c
    void frame_callback(ob_frame *frameset, void *user_data) {
        uint32_t count = ob_frameset_get_count(frameset, &error);
        
        for(uint32_t i = 0; i < count; i++) {
            ob_frame *frame = ob_frameset_get_frame_by_index(frameset, i, &error);
            ob_frame_type frame_type = ob_frame_get_type(frame, &error);
            
            switch(frame_type) {
            case OB_FRAME_LIDAR_POINTS:
                print_lidar_point_cloud_info(frame);
                break;
            case OB_FRAME_ACCEL:
                // Process accelerometer data
                break;
            case OB_FRAME_GYRO:
                // Process gyroscope data
                break;
            }
            
            ob_delete_frame(frame, &error);
        }
        
        ob_delete_frame(frameset, &error);
    }
    ```

5. **LiDAR point cloud processing with coordinate conversion**

    ```c
    void print_lidar_point_cloud_info(ob_frame *point_cloud_frame) {
        ob_format point_cloud_type = ob_frame_get_format(point_cloud_frame, &error);
        
        if(point_cloud_type == OB_FORMAT_LIDAR_SPHERE_POINT) {
            // Spherical to Cartesian conversion
            float x = (float)(distance * cos(theta_rad) * cos(phi_rad));
            float y = (float)(distance * sin(theta_rad) * cos(phi_rad));
            float z = (float)(distance * sin(phi_rad));
        }
        else if(point_cloud_type == OB_FORMAT_LIDAR_SCAN) {
            // Polar to Cartesian conversion
            float x = (float)(distance * cos(angle_rad));
            float y = (float)(distance * sin(angle_rad));
        }
        
        printf("valid point count = %u\n", valid_point_count);
    }
    ```

## Run Sample

### Operation Instructions

1. **Sensor Selection**: Program displays available sensors and prompts for selection
2. **Stream Configuration**: Choose from available stream profiles for each sensor
3. **Real-time Monitoring**: Data from selected sensors is displayed every 50 frames
4. **Exit**: Press **ESC** key to stop streaming and exit

### Interactive Features

- **Sensor List**: Displays all available sensors with type information
- **Stream Profiles**: Shows available formats and parameters for each sensor
- **Flexible Selection**: Choose individual sensors or enable all available
- **Input Validation**: Ensures valid selections with proper error handling

### Data Processing

- **LiDAR Point Cloud**: Handles multiple formats (spherical, Cartesian, scan) with coordinate conversion
- **IMU Data**: Processes accelerometer and gyroscope readings with temperature data
- **Valid Point Counting**: Filters and counts valid 3D points in point cloud data
- **Frame Statistics**: Displays frame indices, timestamps, and format information

### Result

The program provides a comprehensive C-based streaming solution that:

- Enables interactive sensor configuration without code modification
- Processes multiple sensor types simultaneously with proper synchronization
- Demonstrates coordinate system conversions for different LiDAR data formats
- Maintains C API best practices with proper error handling and memory management
- Offers real-time monitoring of sensor data quality and performance

This sample serves as a foundation for building C applications that require flexible sensor configuration and real-time data processing capabilities.

```shell
------------------------------------------------------------------------
Current Device: name: Orbbec_LiDAR_ME450, vid: 0x2bc5, pid: 0x1302, uid: 0x20:4b:5e:13:64:30
LiDAR SN: T0HW8510001

Sensor list:
 - 0. sensor type: Accel
 - 1. sensor type: Gyro
 - 2. sensor type: LiDAR
 - 3. all sensors

Select a sensor to enable (Input index or 'q' to exit): 3
Stream profile list for sensor Accel:
 - 0. format: ACCEL,  fps: 50_HZ
 - 1. format: ACCEL,  fps: 25_HZ
 - 2. format: ACCEL,  fps: 100_HZ
 - 3. format: ACCEL,  fps: 200_HZ
 - 4. format: ACCEL,  fps: 500_HZ
 - 5. format: ACCEL,  fps: 1_KHZ

Select a stream profile to enable (Input index or 'q' to exit): 0
Stream profile list for sensor Gyro:
 - 0. format: GYRO,  fps: 50_HZ
 - 1. format: GYRO,  fps: 25_HZ
 - 2. format: GYRO,  fps: 100_HZ
 - 3. format: GYRO,  fps: 200_HZ
 - 4. format: GYRO,  fps: 500_HZ
 - 5. format: GYRO,  fps: 1_KHZ

Select a stream profile to enable (Input index or 'q' to exit): 0
Stream profile list for sensor LiDAR:
 - 0. format: LIDAR_SPHERE_POINT,  scan rate: 20HZ
 - 1. format: LIDAR_POINT,  scan rate: 20HZ
 - 2. format: LIDAR_SPHERE_POINT,  scan rate: 15HZ
 - 3. format: LIDAR_POINT,  scan rate: 15HZ
 - 4. format: LIDAR_SPHERE_POINT,  scan rate: 10HZ
 - 5. format: LIDAR_POINT,  scan rate: 10HZ

Select a stream profile to enable (Input index or 'q' to exit): 0
Streams have been started!
Press ESC to exit!
LiDAR PointCloud Frame:
{
  frame index: 1
  tsp = 1758875521154717
  format = 36
  valid point count = 6577
}

Accel Frame:
{
  frame index = 5
  tsp = 1758875521162752
  temperature = 0.00 C
  x = -0.007080 m/s^2
  y = -0.007202 m/s^2
  z = -1.018921 m/s^2
}

Gyro Frame:
{
  frame index = 5
  tsp = 1758875521162752
  temperature = 0.00 C
  x = -0.137329 rad/s
  y = -2.471924 rad/s
  z = 0.808716 rad/s
}
```

