# C++ Sample: 5.wrapper.pcl

This project provides a set of examples and utilities for working with the Orbbec SDK and PCL (Point Cloud Library). It includes various modules for processing depth, color, and point cloud data.

## Project Structure

The project is organized into several subdirectories, each corresponding to different functionalities:

- [Pcl](./pcl/README.md): Contains PCL-related functionality for processing point clouds and depth data.
- [Pcl-Color](./pcl_color/README.md): Contains PCL-related functionality specifically for handling color point clouds.

## Building the Project

### Step 1: Install PCL
More information on installing PCL can be found [here](https://github.com/PointCloudLibrary/pcl).

If you are using windows, you can modify the `PCL_DIR` value in the `CMakeLists.txt` file.
```CMake
if(OB_BUILD_PCL)
    # set(PCL_DIR "user/lib/PCL 1.12.1/cmake")
    message(STATUS "- Building PCL examples")
    add_subdirectory(pcl)
    add_subdirectory(pcl_color)
endif()
```
Note: If you need to run it, you may need to copy the dependent dlls to the same directory as the executable file.

### Step 2: Configure the Project

Find the following option and set it to ON. You can find this option in the `option.cmake` file under the `cmake` folder.
```CMake
option(OB_BUILD_PCL "Build Point Cloud Library examples" ON)
```

### Step 3: Build the OrbbecSDK
You can follow the [build guide](../../docs/tutorial/building_orbbec_sdk.md) file for more information on building the Orbbec SDK.

