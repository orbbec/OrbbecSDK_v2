# C++ Sample: 1.stream.color

## Overview

Use the SDK interface to obtain the camera's color stream and display it in the window.

### Knowledge

config is the configuration of the camera
Frameset is a combination of different types of Frames

## Code overview

1. After instantiating a pipeline, configure the output color stream and open the video stream

    ```cpp
        // Create a pipeline with default device.
        ob::Pipeline pipe;

        // Configure which streams to enable or disable for the Pipeline by creating a Config.
        std::shared_ptr<ob::Config> config = std::make_shared<ob::Config>();

        // Enable color video stream.
        config->enableVideoStream(OB_STREAM_COLOR);

        // Start the pipeline with config.
        pipe.start(config);

        // Create a window for rendering and set the resolution of the window.
        ob_smpl::CVWindow win("Color");
    ```

2. After waiting for a while, get the color stream in the frameset and display it in the window

    ```cpp
        while(win.run()) {
        // Wait for up to 100ms for a frameset in blocking mode.
        auto frameSet = pipe.waitForFrameset();
        if(frameSet == nullptr) {
            continue;
        }

        // get color frame from frameset.
        auto colorFrame = frameSet->getFrame(OB_FRAME_COLOR);
        // Render colorFrame.
        win.pushFramesToView(colorFrame);
    }
    ```

## Run Sample

Press the Esc key in the window to exit the program.

### Result

![result](/docs/resource/color.jpg)
