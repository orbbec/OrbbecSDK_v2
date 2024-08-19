# C++ Sample: 1.stream.depth

## Overview

Use the SDK interface to obtain the depth stream of the camera and display it in the window

### Knowledge

DepthFrame can obtain relevant information about the depth

## code overview

1. After instantiating a pipeline, configure the output color stream and open the video stream

    ```cpp
           // Create a pipeline with default device.
        ob::Pipeline pipe;
        
        // By creating config to configure which streams to enable or disable for the pipeline, here the depth stream will be enabled.
        std::shared_ptr<ob::Config> config = std::make_shared<ob::Config>();

        // Enable depth stream.
        config->enableVideoStream(OB_STREAM_DEPTH);

        // Start the pipeline with config.
        pipe.start(config);

        // Create a window for rendering, and set the resolution of the window.
        ob_smpl::CVWindow win("Depth");
    ```

2. Calculate the distance from the center pixel to the opposite side from the acquired Y16 depth stream format and display it in the window. The distance is refreshed every 30 frames

    ```cpp
    // Get the depth Frame form depthFrameRaw.
        auto depthFrame = depthFrameRaw->as<ob::DepthFrame>();
        // for Y16 format depth frame, print the distance of the center pixel every 30 frames.
        if(depthFrame->getIndex() % 30 == 0 && depthFrame->getFormat() == OB_FORMAT_Y16) {
            uint32_t        width  = depthFrame->getWidth();
            uint32_t        height = depthFrame->getHeight();
            float           scale  = depthFrame->getValueScale();
            const uint16_t *data   = reinterpret_cast<const uint16_t *>(depthFrame->getData());

            // pixel value multiplied by scale is the actual distance value in millimeters.
            float centerDistance = data[width * height / 2 + width / 2] * scale;

            // // attention: if the distance is 0, it means that the depth camera cannot detect the object (may be out of detection range).
            win.addLog("Facing an object at a distance of " + ob_smpl::toString(centerDistance, 3) + " mm. ");
        }
    ```

## Run Sample

Moving the camera can obtain the change in the distance across the center pixel
Press the Esc key in the window to exit the program.

### Result

![image](/docs/resource/depth.png)
