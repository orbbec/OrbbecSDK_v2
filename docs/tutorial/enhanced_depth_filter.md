# EnhancedDepthFilter

## Overview

The LingBot Enhanced Depth Filter (`EnhancedDepthFilter`) uses color and depth information to improve depth quality by reducing noise, filling depth holes, and refining object edges. The filter is supported by both OrbbecViewer and OrbbecSDK.

- [LingBot Enhanced Depth Filter](https://www.orbbec.com/lingbot-enhanced-depth-filter/)

## Requirements

- NVIDIA Jetson running Linux ARM64;
- a supported Gemini 330 series camera with a valid LingBot-Depth License;
- CUDA Runtime 12;
- TensorRT 10 Runtime;
- the EnhancedDepthFilter extension and matching `model.sm4` from the same OrbbecSDK release.

The filter requires a frameset containing Color and Depth frames after D2C alignment. Both software D2C and hardware D2C are supported.

Supported configurations:

- D2C target resolution: `640x480`, `1280x720`, or `1280x800`;
- color format: `RGB`;
- depth format: `Y10`, `Y11`, `Y12`, `Y14`, `Y16`, or `Z16`.

## License

EnhancedDepthFilter requires a valid LingBot-Depth License. For LicenseTool downloads, License applications, and device activation, see:

- [orbbec/LingBot-Depth-LicenseTool](https://github.com/orbbec/LingBot-Depth-LicenseTool)

## Model Deployment

Download `model.sm4` from the [OrbbecSDK Releases](https://github.com/orbbec/OrbbecSDK_v2/releases). Use the model file published for the same version as OrbbecSDK or OrbbecViewer. GitHub-generated source code archives do not include the model file.

OrbbecViewer and OrbbecSDK use different default model locations.

OrbbecViewer resolves the following path relative to its executable directory:

```text
<OrbbecViewer executable directory>/extensions/filters/enhanced_depth_filter/model.sm4
```

OrbbecSDK loads the model from the directory containing the EnhancedDepthFilter shared library by default:

```text
<EnhancedDepthFilter shared library directory>/model.sm4
```

In the standard SDK release layout, the shared library and model are located in `extensions/filters/enhanced_depth_filter/`. An application can also specify the model path explicitly when creating `ob::EnhancedDepthFilter`.

Keep OrbbecViewer or OrbbecSDK, the filter extension, and `model.sm4` from the same release.

## Use in OrbbecViewer

1. Start OrbbecViewer on NVIDIA Jetson/Linux ARM64 and connect a camera with a valid LingBot-Depth License.
2. Enable the Color and Depth streams.
3. Enable software D2C or hardware D2C alignment.
4. Select a supported stream configuration. For initial verification, use Color `640x480 RGB` and Depth `640x480 Y16`.
5. Select **Advanced** in the left navigation bar and open the **Point Cloud** page.
6. Turn on the dedicated **Enhanced Depth Filter** switch.
7. Wait for the filter to finish initializing.

To use a different model path, update the OrbbecViewer `config.ini` file:

```ini
[EnhancedDepthFilter]
ModelPath=extensions/filters/enhanced_depth_filter/model.sm4
```

A relative path is resolved from the OrbbecViewer executable directory. An absolute path can also be used.

## Use with OrbbecSDK

The complete C++ Sample demonstrates device authorization checks, stream configuration, D2C alignment, filter creation, and frame processing:

- [Enhanced Depth Filter C++ Sample](../../examples/3.advanced.enhanced_depth_filter/)

## Troubleshooting

| Symptom | Recommended action |
| --- | --- |
| Enhanced Depth Filter is unavailable | Confirm that OrbbecViewer is running on NVIDIA Jetson/Linux ARM64 and that the camera supports License authorization |
| `LicenseInfo is missing` is displayed | Use LicenseTool to apply for and activate a License on the camera |
| `model.sm4` cannot be found | Check the model file location and the `ModelPath` setting |
| The stream configuration is not supported | Verify the feature first with Color `640x480 RGB` and Depth `640x480 Y16` |
| D2C is required | Enable software D2C or hardware D2C alignment |
| Filter initialization fails | Check the runtime environment, License, model, and stream configuration |
