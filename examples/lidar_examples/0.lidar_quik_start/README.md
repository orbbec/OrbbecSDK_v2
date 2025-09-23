# C++ Sample: 0.lidar_quik_start

## Overview

Use the SDK interface to quickly obtain LiDAR point cloud data and save it to a PLY file when triggered by user input.

### Knowledge

- **Pipeline** is a data stream processing pipeline that provides multi-stream configuration, switching, frame aggregation, and frame synchronization functions
- **FrameSet** is a combination of different types of frames
- **LiDAR Point Cloud** contains 3D spatial coordinate data collected by the LiDAR sensor

## Code Overview

1. **Initialize pipeline and start LiDAR stream**
   
    ```cpp
    ob::Pipeline pipe;
    pipe.start();
    ```

2. **Wait for user input to trigger point cloud capture**

    ```cpp
    auto key = ob_smpl::waitForKeyPressed();
    if(key == 'r' || key == 'R') {
        // Capture point cloud
    }
    ```

3. **Capture and save LiDAR point cloud data**

    ```cpp
    auto frameset = pipe.waitForFrameset();
    auto frame = frameset->getFrame(OB_FRAME_LIDAR_POINTS);
    ob::PointCloudHelper::savePointcloudToPly("LiDARPoints.ply", frame, false, false, 50);
    ```

4. **Stop the pipeline**

    ```cpp
    pipe.stop();
    ```

## Run Sample

- Press **R** or **r** key to capture and save LiDAR point cloud data to "LiDARPoints.ply" file
- Press **ESC** key to exit the program

### Result

The program will generate "LiDARPoints.ply" file containing the 3D point cloud data when the R key is pressed. The file can be opened with 3D point cloud visualization software.

```shell
LiDAR stream are started!
Press R or r to create LiDAR PointCloud and save to ply file!
Press ESC to exit!
Save LiDAR PointCloud to ply file, this will take some time...
LiDARPoints.ply Saved
Save LiDAR PointCloud to ply file, this will take some time...
LiDARPoints.ply Saved
```

