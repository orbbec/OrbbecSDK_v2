# Open Source Orbbec SDK

> [!IMPORTANT]
> Welcome to Orbbec's latest open-source Orbbec SDK! Before you begin using this version of the SDK, it's crucial to check the [device support list](#12-supported-devices) to verify that your device is supported to ensure compatibility.

## 1. Introduction

This is the open source library of the Orbbec SDK v2.x. The Orbbec SDK is a cross-platform library for interfacing with the Orbbec RGB-D cameras. It provides C/C++ APIs and many other languages or frameworks [wrappers](wrappers/README.md).

<div align=center>
<img src="docs/resource/Overview.jpg" width="400" align="center" />
</div>

To enhance performance and accommodate the diverse needs of our customers, the Orbbec SDK has been open source since version 2.0.0. For projects currently utilizing version 1.x of the Orbbec SDK that are considering an upgrade to Orbbec SDK v2.x, we advise using the pre-compiled binaries available in the [Release](https://github.com/orbbec/OrbbecSDK_v2/releases) page. This ensures a seamless transition and maintains consistency with your existing setup.

We have made every effort to maintain backward compatibility with our APIs. However, in pursuit of enhanced performance and an improved user experience, we have made necessary updates. Some interfaces have been removed or altered due to infrequent use or design imperfections. These changes are intended to be minimally disruptive and should not interfere with the standard functionality of the SDK.

For a detailed list of features not available in the current version, please refer to our documentation: [features_not_available_at_v2.x.x](docs/api/features_not_available_at_v2.x.x.md).

Additionally, a comprehensive overview of the API changes from Orbbec SDK v1.x to v2.x.x can be found here: [api_changes_at_v2.x.x](docs/api/api_changes_at_v2.x.x.md).

If you prefer to continue with Orbbec SDK v1.x, you can still access the pre-compiled binaries on our [Orbbec SDK](https://github.com/orbbec/OrbbecSDK) repository. Nonetheless, we encourage you to consider transitioning to Orbbec SDK v2.x. This move will allow you to take advantage of  improved and new features and ensure ongoing support. Our dedication to v2.x is part of a broader strategy aimed at delivering robust long-term support, fostering an open-source community, and driving innovation.

### 1.1 Supported Platforms

- Windows 10 or later: x86 and x64 architectures
- Linux x64: tested on Ubuntu 20.04, 22.04 and 24.04
- Linux ARM64: tested on AGX Orin and AGX Xavier
- Android: coming soon
- macOS: coming soon
- Linux ARM32: coming soon

### 1.2 Supported Devices

| **Products List** | **Minimal Firmware Version** |
|-------------------|------------------------------|
| Gemini 330        | 1.2.20                       |
| Gemini 330L       | 1.2.20                       |
| Gemini 335        | 1.2.20                       |
| Gemini 335L       | 1.2.20                       |
| Gemini 336        | 1.2.20                       |
| Gemini 336L       | 1.2.20                       |
| Gemini 335Lg      | 1.3.46                       |
| Femto Bolt        | 1.0.6/1.0.9                  |
| Femto Mega        | 1.1.7/1.2.7                  |
| Astra 2           | 2.8.20                       |
| Gemini 2 L        | 1.4.32                       |
| Gemini 2          | 1.4.60 /1.4.76               |

For optimal performance, we strongly recommend updating to the latest firmware version. This ensures that you benefit from the most recent enhancements and bug fixes.

More devices support will be added in the near future. If you can not find your device in the table above currently, try the version v1.x.y at [Orbbec SDK](https://github.com/orbbec/OrbbecSDK)

## 2. Documentation

*The Orbbec SDK documentation is available on [GitHub Pages](https://orbbec.github.io/OrbbecSDK_v2/).*

### 2.1 Quick Start

If you do not intend to modify the SDK itself, it is recommended to use a pre-compiled distribution. For a comprehensive guide on installing the SDK, setting up your development environment, and developing applications, please consult the [Tutorial](docs/tutorial/installation_and_development_guide.md) section for detailed information.

To explore practical examples and gain insights on utilizing the SDK, please navigate to [Examples](examples/README.md) section for more information.

Orbbec Viewer is an all-in-one graphical user interface (GUI) built on the Orbbec SDK, offering a suite of features including data stream preview, camera configuration, post-processing, and more. To learn how to effectively use the Orbbec Viewer, please refer to the  [Orbbec Viewer User Guide](docs/tutorial/orbbecviewer.md).

### 2.2 API Reference

API Reference: [Orbbec_SDK_API_Reference.html](https://orbbec.github.io/docs/OrbbecSDKv2/index.html)

<!--
Remove the following as it is not updatded to v2.x.x yet.
OrbbecSDK C++ API user guide：[orbbecsdk_c++_api_user_guide-v1.0.pdf](docs/api/orbbecsdk_c++_api_user_guide-v1.0.pdf)
-->

### 2.3 Performance tuning

For different use cases, the SDK can be tuned to achieve optimal performance. Please refer to the [performance tuning guide](docs/tutorial/performance_tuning.md) for more information.

### 2.4 Building from Source

If you would like to modify the SDK itself, you can build the SDK from source. Please refer to the [build guide](docs/tutorial/building_orbbec_sdk.md) for more information.

### 2.5 FAQ

Most frequently asked questions can be found in the [FAQ](docs/FAQ.md) file.

More asked and answered questions can be found in the [issues](https://github.com/orbbec/OrbbecSDK_v2/issues) channel. Use key words for searching may help you find the answer you are looking for.

## 3. Gallery

### 3.1 The QuickStart Example

```c++
// Create a pipeline.
ob::Pipeline pipe;

// Start the pipeline with default config.
pipe.start();

// Create a window for showing the frames, and set the size of the window.
ob_smpl::CVWindow win("QuickStart", 1280, 720, ob_smpl::ARRANGE_ONE_ROW);

while(win.run()) {
    // Wait for frameSet from the pipeline, the default timeout is 1000ms.
    auto frameSet = pipe.waitForFrameset();

    // Push the frames to the window for showing.
    win.pushFramesToView(frameSet);
}

// Stop the Pipeline, no frame data will be generated
pipe.stop();
```

![QuickStart Example](docs/resource/QuickStart.jpg)

### 3.2 The Orbbec Viewer

![Orbbec Viewer](docs/resource/OrbbecViewer.jpg)

## 4. Contributing

At present, we are focusing our efforts on the internal development of the SDK and are not accepting external contributions for functionality expansion. However, we value your input and are open to reviewing any pull requests or suggestions you might have.

Should you have any questions or concerns, please feel free to reach out to us through the [issues](https://github.com/orbbec/OrbbecSDK_v2/issues) channel.

## 5. License

This project is licensed under the MIT License with some other licenses for third-party libraries and extensions - see the [LICENSE](LICENSE.txt) file for details.

## 6. Links

- [Orbbec SDK V2.x Open Source Library](https://github.com/orbbec/OrbbecSDK_v2)
- [Orbbec SDK V1.x Pre-Compiled Library](https://github.com/orbbec/OrbbecSDK)
- [Orbbec Company Main Page](https://www.orbbec.com/)
- [Orbbec 3D Club](https://3dclub.orbbec3d.com)
