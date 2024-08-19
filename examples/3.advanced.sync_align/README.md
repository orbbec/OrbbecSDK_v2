# C++ Sample: 3.advanced.sync_align

## Overview

Use the SDK interface to demonstrate the synchronization and alignment of sensor data streams, display the aligned image, and exit the program using the ESC_KEY key.

### Knowledge

Pipeline is a pipeline for processing data streams, providing multi-channel stream configuration, switching, frame aggregation, and frame synchronization functions

Frameset is a combination of different types of Frames

win is used to display the frame data.

## Code overview

1. Creates an ob::Config object

    ```cpp
        // Configure which streams to enable or disable for the Pipeline by creating a Config
    auto config = std::make_shared<ob::Config>();

    // enable depth and color streams with specified format
    config->enableVideoStream(OB_STREAM_DEPTH, OB_WIDTH_ANY, OB_HEIGHT_ANY, OB_FPS_ANY, OB_FORMAT_Y16);
    config->enableVideoStream(OB_STREAM_COLOR, OB_WIDTH_ANY, OB_HEIGHT_ANY, OB_FPS_ANY, OB_FORMAT_RGB);

    // set the frame aggregate output mode to ensure all types of frames are included in the output frameset
    config->setFrameAggregateOutputMode(OB_FRAME_AGGREGATE_OUTPUT_ALL_TYPE_FRAME_REQUIRE);
    ```

2. Create a pipeline with the configuration

    ```cpp
    // Create a pipeline with default device to manage stream
    auto pipe = std::make_shared<ob::Pipeline>();

    // Start the pipeline with config
    pipe->start(config);
    ```

3. Set alignment mode

    ```cpp
    // Create a filter to align depth frame to color frame
    auto depth2colorAlign = std::make_shared<ob::Align>(OB_STREAM_COLOR);

    // create a filter to align color frame to depth frame
    auto color2depthAlign = std::make_shared<ob::Align>(OB_STREAM_DEPTH);
    ```

4. Set the callback function for the Align Filter to display the aligned frames in the window

    ```cpp
    depth2colorAlign->setCallBack([&win](std::shared_ptr<ob::Frame> frame) { win.pushFramesToView(frame); });
    color2depthAlign->setCallBack([&win](std::shared_ptr<ob::Frame> frame) { win.pushFramesToView(frame); });
   ```

5. Get frame data

    ```cpp
        auto frameSet = pipe->waitForFrameset(100);
    ```

6. Perform alignment processing

    ```cpp
     // Get filter according to the align mode
        std::shared_ptr<ob::Filter> alignFilter = depth2colorAlign;
        if(align_mode % 2 == 1) {
            alignFilter = color2depthAlign;
        }

        // push the frameset to the Align Filter to align the frames.
        // The frameset will be processed in an internal thread, and the resulting frames will be asynchronously output via the callback function.
        alignFilter->pushFrame(frameSet);
    ```

7. Stop pipeline

    ```cpp
        pipe.stop();
    ```

## Run Sample

Press the Esc key in the window to exit the program.
'T': Switch Align Mode.
'F': Toggle Synchronization.
'+/-': Adjust Transparency

### Result

![image](/docs/resource/SyncAlign.png)
