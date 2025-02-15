# C++ Sample: 5.wrapper.opencv.imshow

## Overview
This example shows how to use OpenCV's `imshow` function to display the camera feed in a window.

## Code Overview

1. Create a `Config` object to enable the depth stream with the desired format.

    ```cpp
    // Configure which streams to enable or disable for the Pipeline by creating a Config
    auto config = std::make_shared<ob::Config>();

    // enable depth stream with specified format
    config->enableVideoStream(OB_STREAM_DEPTH, OB_WIDTH_ANY, OB_HEIGHT_ANY, OB_FPS_ANY, OB_FORMAT_Y16);
    ```
2. Create a `Pipeline` object with the `config` object.

    ```cpp
    // Create a Pipeline with the Config object
    auto pipeline = std::make_shared<ob::Pipeline>();
    pipeline->configure(config);
    ```
3. Colorize the depth frame and display it using OpenCV's `imshow` function.
    ```cpp
    cv::Mat depthVisualized = CvHelper::colorizeDepth(depthFrame);
    cv::imshow("ImageShow", depthVisualized);
    ```
## Run Sample

Press the q or Q key in the window to exit the program.

### Result
![result](/docs/resource/imshow.jpg)
