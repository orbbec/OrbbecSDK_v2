# Benchmark Tool

This tool is used to measure the performance of the OrbbecSDK_v2. It can be used to measure the frame rate of the camera, the latency of the camera, and the processing time of the SDK.

## Building the Benchmark Tool
### Step 1: Clone the repository
```bash
git clone https://github.com/orbbec/OrbbecSDK_v2.git
```

### Step 2: Open the tool building options
Navigate to the `cmake` folder in the root directory of the repository and open the `option.cmake` file.
```CMake
option(OB_BUILD_TOOLS "Build tools" ON)
``` 

### Step 3: Build the SDK
You can build the SDK by following the instructions in the [build guide](docs/tutorial/building_orbbec_sdk.md).


## Usage
The current benchmark tool runs four test cases by default for the following streams: depth, color, left IR, and right IR streams, D2C, depth point cloud, and color point cloud. Each test case runs for **five minutes**.

After the benchmark tool finishes, a `summary.csv` file will be generated in the same directory as the executable. This file contains the average results of the four test cases, which will look something like this:
```csv
Config: Enable depth, color, ir | Total average
Average Cpu Usage(%), Average Memory Usage(MB)
34.1462,55.9516
Config: Enable software d2c | Total average
Average Cpu Usage(%), Average Memory Usage(MB)
63.2846,69.4766
Config: Enable point cloud | Total average
Average Cpu Usage(%), Average Memory Usage(MB)
90.7615,96.832
Config: Enable rgb point cloud | Total average
Average Cpu Usage(%), Average Memory Usage(MB)
115.731,128.32
```

## Modifying Test Cases
If you wish to modify the test time for each group of test cases, follow these steps:
1. Locate the `config/PerformanceConfig.hpp` file in the benchmark project.
2. Modify the `RECONDING_TIME_SECONDS` value, for example:
```cpp
#define RECONDING_TIME_SECONDS 60 * 3 // Modified to 3 minutes
```

If you would like to modify the test cases, follow these steps:
1. Locate the `config/PerformanceConfig.hpp` file in the benchmark project.
2. Modify the `updateConfigHandlers_` array, for example:
```cpp
updateConfigHandlers_ = {
    [](std::shared_ptr<DeviceResource>& deviceResource) -> std::string {}, // test 1
    [](std::shared_ptr<DeviceResource>& deviceResource) -> std::string {}, // test 2
    // Add your code here, for example:
    [](std::shared_ptr<DeviceResource>& deviceResource) -> std::string {
        // Summary.csv title
        std::string msg = "Enable depth[848*480/15fps Y16], color[848*480/15fps BGR]";
        std::cout << msg << std::endl;

        auto config = std::make_shared<ob::Config>();
        config->enableVideoStream(OB_SENSOR_DEPTH, 848, 480, 15, OB_FORMAT_Y16);
        config->enableVideoStream(OB_SENSOR_COLOR, 848, 480, 15, OB_FORMAT_BGR);

        deviceResource->startStream(config);
        return msg;
    },
};
```
After modifying the test case, recompile the benchmark project and run the benchmark program again. You will see the results of the newly added test case.

## Test Filtering Effects
If you would like to test the effect of certain filters, follow these steps to modify the benchmark code.
Example: Enable `Spatial Filtering`
1. Add a `spatial_filter_` member variable in `src/DeviceResource.hpp`:
```cpp
class DeviceResource {
public:
    // Other methods...

private:
    // Other members...

    // Post-processing filters
    std::shared_ptr<ob::SpatialAdvancedFilter> spatial_filter_;
};
```

2. Initialize the filter in the `DeviceResource` constructor in `src/DeviceResource.cpp`:
```cpp
DeviceResource::DeviceResource(std::shared_ptr<ob::Device> device) {
    // Initialize the spatial filter
    spatial_filter_ = std::make_shared<ob::SpatialAdvancedFilter>();
}
```

3. Modify the `startStream` function in `src/DeviceResource.cpp` to process the frames:
```cpp
void DeviceResource::startStream(std::shared_ptr<ob::Config> config) {
    pipeline_->start(config, [this](std::shared_ptr<ob::FrameSet> frameset) {
        if(frameset == nullptr) {
            return;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        frames_ = frameset;

        // ====================
        // your code here
        if(spatial_filter_) {
            frames_ = spatial_filter_->process(frames_);
        }
        // your code here
        // ====================
        
        // Note: It is not recommended to handle d2c or point cloud consuming operations in the callbacks set by the pipeline.
        if(is_align_filter_enabled_ && align_filter_) {
            frames_ = align_filter_->process(frames_);
        }

        if(is_point_cloud_filter_enabled_ && point_cloud_filter_) {
            frames_ = align_filter_->process(frames_);
            point_cloud_filter_->process(frames_);
        }
    });
}
```

## Note
1. This benchmark is designed for testing the **G330** series cameras. If you are using a different camera, it may cause some issues (e.g., non-G330 devices do not support hardware noiseremoval). You can modify the code in this project according to your needs..
2. The benchmark tool includes two methods: `enableSwNoiseRemoveFilter` and `enableHwNoiseRemoveFilter`. For G330 devices, the OrbbecSDK internally enables a software filter for noise removal by default. You can call `enableSwNoiseRemoveFilter` to enable or disable software noise removal. If you're using the latest firmware (1.4.1 firmware or later), hardware noise removal is supported, yout can disable the software filter by calling `enableHwNoiseRemoveFilter(false)`. Disabling the software filter and enabling hardware noise removal can improve the performance of the camera.

