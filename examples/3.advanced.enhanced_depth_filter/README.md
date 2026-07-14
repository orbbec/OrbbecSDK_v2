# C++ Sample: 3.advanced.enhanced_depth_filter

## Overview

This sample demonstrates how to use `EnhancedDepthFilter` to improve an aligned depth frame and display the original and processed frames for comparison.

### Knowledge

Pipeline is a pipeline for processing data streams, providing multi-channel stream configuration, switching, frame aggregation, and frame synchronization functions.

Frameset is a combination of different types of Frames.

`Align` aligns the depth frame to the color frame before enhanced depth processing.

### Attentions

> This sample is only supported on NVIDIA Jetson platforms (Linux ARM64).
>
> The connected device must support license authorization and have a valid license for `EnhancedDepthFilter`.
>
> By default, the filter loads the model file located next to the filter library. Use `--model <model_file_path>` or `-m <model_file_path>` to specify another model file.

## Code overview

### 1. Check license authorization

Get the device, verify that it supports license authorization, and print its license information.

```cpp
auto device = pipe.getDevice();

if(!device->isLicenseAuthorizationSupported()) {
    std::cout << "device does not support license authorization, exit." << std::endl;
    return EXIT_FAILURE;
}

auto licenseInfo = device->readLicenseInfo();
std::cout << "license info:" << licenseInfo << std::endl;
```

### 2. Create the filters

Create an `Align` filter to align depth to color and create the `EnhancedDepthFilter` with the optional model path.

```cpp
auto alignFilter         = std::make_shared<ob::Align>(OB_STREAM_COLOR);
auto enhancedDepthFilter = std::make_shared<ob::EnhancedDepthFilter>(device, modelPath);
```

### 3. Configure and start the pipeline

Enable color and depth streams at 640 x 480, and require all enabled frame types in each frameset.

```cpp
auto config = std::make_shared<ob::Config>();
config->enableVideoStream(OB_STREAM_COLOR, 640, 480, OB_FPS_ANY, OB_FORMAT_RGB);
config->enableVideoStream(OB_STREAM_DEPTH, 640, 480, OB_FPS_ANY, OB_FORMAT_Y16);
config->setFrameAggregateOutputMode(OB_FRAME_AGGREGATE_OUTPUT_ALL_TYPE_FRAME_REQUIRE);
pipe.start(config);
```

### 4. Align and process the frames

Align each frameset, apply enhanced depth processing, and obtain the processed color, depth, and optional confidence frames.

```cpp
auto aligned = alignFilter->process(frameset);
if(!aligned) {
    continue;
}

auto processed = enhancedDepthFilter->process(aligned);
if(!processed || !processed->is<ob::FrameSet>()) {
    continue;
}

auto processedFrameset = processed->as<ob::FrameSet>();
auto colorFrame        = processedFrameset->getColorFrame();
auto depthFrame        = processedFrameset->getDepthFrame();
auto confidenceFrame   = processedFrameset->getFrame(OB_FRAME_CONFIDENCE);
```

### 5. Display the results

Display the original color and depth frames alongside the processed frames.

```cpp
win.pushFramesToView(originFrames, 0);
win.pushFramesToView(processedFrames, 1);
```

## Run Sample

Run with the default model:

```bash
./ob_enhanced_depth_filter
```

Run with a specified model file:

```bash
./ob_enhanced_depth_filter --model <model_file_path>
```

The short option `-m` is also supported. Use `-h` or `--help` to display command-line help.

Press the `Esc` key in the window to exit the program.
