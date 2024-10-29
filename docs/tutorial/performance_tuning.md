# Performance Tuning

## 1. Enhancing Performance by Switching UVC Backend (Linux Only)

For cameras connected via USB, the Orbbec SDK supports both LibUVC and V4L2 as backends, with the V4L2 backend generally offering better performance and lower resource consumption. However, the V4L2 backend may have compatibility issues with different Linux versions, which is why the Orbbec SDK defaults to using the LibUVC backend. If you encounter frame dropping or latency issues, you can try switching to the V4L2 backend.

**Note**: The V4L2 backend requires a kernel version of at least 4.16 (Ubuntu version 20.04 or higher) to function properly. Otherwise, there may be issues with obtaining the correct data frame timestamps and metadata.

If you face performance issues, you can attempt to switch to the V4L2 backend by following these steps:

1. Locate the `OrbbecSDKConfig.xml` file within the SDK package (usually found in the `./lib` or `./bin` directory, which can be located through a file search), and copy it to your application's working directory.

2. In the `OrbbecSDKConfig.xml` file, find the `<LinuxUVCBackend>` tag and change `LibUVC` to `V4L2`:

    ```xml
    <LinuxUVCBackend>V4L2</LinuxUVCBackend>
    ```

3. For G330 series devices, you need to upgrade the firmware to version v1.0.60 or higher and in the `OrbbecSDKConfig.xml` file, change the `FrameMetadataParsingPath` tag from `ExtensionHeader` to `PayloadHeader`. Otherwise, metadata will not be retrieved correctly.

   ```xml
   <FrameMetadataParsingPath>PayloadHeader</FrameMetadataParsingPath>
   ```

4. After configuration, you will need to re-plug the device and restart your application.

## 2. Improving usbfs Buffer Sizes (Linux Only)

The `usbfs` kernel module is responsible for handling USB communication, and it has several parameters that can be tuned to improve performance. One of the most important parameters is the `usbfs_buffer_size` parameter, which determines the size of the kernel buffer used to store USB data.

**Note**: It is strongly recommended to increase the `usbfs_buffer_size` parameter when using multiple usb devices or when streaming large amounts of data, as the default value may not be sufficient to handle the data rate.

To increase the `usbfs_buffer_size`, follow these steps:

1. execute the following command on terminal:

    ``` shell
    echo 128 | sudo tee /sys/module/usbcore/parameters/usbfs_memory_mb
    ```

2. Reboot the system.

After the system reboots, you should see the `usbfs_buffer_size` parameter in the output of the `dmesg` command. You can verify that the `usbfs_buffer_size` has been set correctly by running the following command:

``` shell
cat /sys/module/usbcore/parameters/usbfs_memory_mb
```

The output should be `128`.
