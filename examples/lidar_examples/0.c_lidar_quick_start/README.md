# C Sample: 0.c_lidar_quik_start

## Overview

This C language sample demonstrates how to quickly obtain LiDAR point cloud data using the SDK C API and save it to a PLY file when triggered by user input.

### Knowledge

- **C API** provides a C-language interface for the SDK functionality
- **Pipeline** manages data streams similar to the C++ version
- **Error Handling** uses `ob_error` structure for comprehensive error reporting
- **Memory Management** requires explicit deletion of frames and pipelines

## Code overview

1. **Initialize pipeline and start with default configuration**

    ```c
    ob_error *error = NULL;
    ob_pipeline *pipe = ob_create_pipeline(&error);
    ob_pipeline_start(pipe, &error);
    ```

2. **Interactive control loop for point cloud capture**

    ```c
    while(true) {
        char key = ob_smpl_wait_for_key_press(10);
        if(key == ESC_KEY) {
            break;
        }

        if(key == 'r' || key == 'R') {
            // Capture and save point cloud
        }
    }
    ```

3. **Capture frameset and extract LiDAR point cloud data**

    ```c
    ob_frame *frameset = ob_pipeline_wait_for_frameset(pipe, 1000, &error);
    ob_frame *frame = ob_frameset_get_frame(frameset, OB_FRAME_LIDAR_POINTS, &error);
    ```

4. **Save point cloud to PLY file with proper cleanup**

    ```c
    bool result = ob_save_pointcloud_to_ply("LiDARPoints.ply", frame, false, false, 50, &error);
    ob_delete_frame(frameset, &error);
    ```

5. **Proper resource cleanup on exit**

    ```c
    ob_pipeline_stop(pipe, &error);
    ob_delete_pipeline(pipe, &error);
    ```

## Run Sample

### Operation Instructions

- Press **R** or **r** key to capture and save LiDAR point cloud data to "LiDARPoints.ply" file
- Press **ESC** key to exit the program

### C API Specific Features

- **Explicit Memory Management**: All created objects must be explicitly deleted
- **Error Handling**: Every function call returns error information through `ob_error` parameter
- **Resource Cleanup**: Proper cleanup sequence ensures no memory leaks
- **Timeout Handling**: Frame waiting includes timeout parameter (1000ms in this sample)

### Result

The program generates "LiDARPoints.ply" file containing 3D point cloud data when the R key is pressed. The C API provides the same functionality as the C++ version but with explicit memory management and error handling typical of C programming patterns.

```shell
LiDAR stream is started!
Press R or r to create LiDAR PointCloud and save to ply file!
Press ESC to exit!
Save LiDAR PointCloud to ply file, this will take some time...
LiDARPoints.ply Saved
Save LiDAR PointCloud to ply file, this will take some time...
LiDARPoints.ply Saved
```

