# Frequently Asked Questions

## 1. No Data Stream from Multiple Cameras

**Insufficient Power Supply**:

- Ensure that all cameras are not connected to the same hub.
- Use a powered hub to provide sufficient power to each camera.

**High Resolution**:

- Try lowering the resolution to resolve data stream issues.

**Increase usbfs_memory_mb Value**:

- Increase the `usbfs_memory_mb` value to 128MB by running the following command:

    ```bash
    echo 128 | sudo tee /sys/module/usbcore/parameters/usbfs_memory_mb
    ```

- For making this change permanent, check [this link](https://github.com/OpenKinect/libfreenect2/issues/807).
