# Multi-Devices Firmware Update Tool

## Overview

This tool is designed for batch firmware updates across multiple Orbbec devices simultaneously. It automatically detects all connected devices, performs firmware updates in sequence, and provides a comprehensive summary of the update results.

> **Note**: This tool is not suitable for Femto Mega, Femto Mega I, and Femto Bolt devices. For these devices, please refer to this repository: [https://github.com/orbbec/OrbbecFirmware](https://github.com/orbbec/OrbbecFirmware)

### Key Features

- **Automatic Multi-Device Detection**: Automatically discovers and lists all connected Orbbec devices
- **Sequential Batch Updates**: Updates multiple devices one by one automatically
- **Two-Stage Update Support**: Handles devices that require a reboot and second update cycle
- **Comprehensive Status Reporting**: Provides detailed success/failure/mismatch reporting for each device
- **Automatic Reconnection**: Waits for devices to reconnect after reboot during two-stage updates

### Knowledge

**Context** is the environment context, the first object created during initialization, which can be used to perform some settings, including but not limited to device status change callbacks, log level settings, etc. Context can access multiple Devices.

**Device** is the device object, which can be used to obtain the device information, such as the model, serial number, and various sensors. One actual hardware device corresponds to one Device object.

**Firmware Update States**:
- `STAT_START`: Starting the upgrade
- `STAT_VERIFY_IMAGE`: Verifying image file
- `STAT_VERIFY_SUCCESS`: Image file verification success
- `STAT_FILE_TRANSFER`: File transfer in progress
- `STAT_IN_PROGRESS`: Upgrade in progress
- `STAT_DONE`: Update completed
- `STAT_DONE_REBOOT_AND_REUPDATE`: Update completed but requires reboot and second update
- `ERR_MISMATCH`: Mismatch between device and image file

## Usage

### Command Line Syntax

```bash
ob_multi_devices_firmware_update <firmware_file_path>
```

### Parameters

| Parameter | Description |
|-----------|-------------|
| `firmware_file_path` | Path to the firmware image file (`.bin` or `.img` format) |

### Examples

**Update all connected devices:**

```bash
# Windows
ob_multi_devices_firmware_update.exe Gemini330_v1.6.00.bin

# Linux
./ob_multi_devices_firmware_update Gemini330_v1.6.00.bin
```

**Using absolute path:**

```bash
ob_multi_devices_firmware_update.exe "C:\Firmware\Gemini330_v1.6.00.bin"
```

## How It Works

### 1. Device Detection

The tool automatically queries and lists all connected Orbbec devices:

```
Devices found:
--------------------------------------------------------------------------------
[0] Device: Gemini 330 | SN: CP3S34D0000T | Firmware version: 1.4.20
[1] Device: Gemini 330 | SN: CP3S34D0001T | Firmware version: 1.4.20
--------------------------------------------------------------------------------
```

### 2. Sequential Update Process

Devices are updated one by one in the order they were detected:

```
Upgrading device: 1/2 - Gemini 330

Upgrading device firmware, please wait for a moment...

Progress: 45%
Status  : File transfer in progress
Message : Transferring firmware data to device
```

### 3. Two-Stage Update (If Required)

Some devices require a two-stage update process:

1. **First Update**: Transfers firmware to the device
2. **Automatic Reboot**: Device reboots to apply the firmware
3. **Second Update**: Completes the firmware update process

The tool handles this automatically:

```
Firmware update completed but not finalized. The device will reboot now and then perform the second update...

Waiting for device to reconnect... (1000/12000 ms)
Waiting for device to reconnect... (2000/12000 ms)

The device is online now, starting the second update...
```

### 4. Update Progress Display

During the update, the tool displays real-time progress:

```
Progress: 100%
Status  : Update completed
Message : Firmware update successful
```

## Understanding Results

### Success Criteria

A device is marked as **SUCCESS** when:
- Firmware verification passes
- File transfer completes successfully
- Update process finishes without errors
- Device reboots successfully (if required)
- Second update completes (if required)

### Mismatch Criteria

A device is marked as **MISMATCH** when:
- The firmware file is not compatible with the device model
- Device PID does not match the firmware image

**Resolution**: Use the correct firmware file for the specific device model.

### Failure Criteria

A device is marked as **FAILURE** when:
- Device disconnects during the update process
- Communication error occurs
- Device fails to reconnect after reboot
- Unknown error state encountered

## Final Summary Report

After all devices are processed, a comprehensive summary is displayed:

```
Upgrade Summary:
==================================================
Success (2):
  - Name: Gemini 330 | SN: CP3S34D0000T
  - Name: Gemini 330 | SN: CP3S34D0001T

Mismatch (0):

Failure (0):

Upgrade process completed.
```

### Summary Categories

| Category | Description | Action Required |
|----------|-------------|-----------------|
| **Success** | Device updated successfully and rebooted | None |
| **Mismatch** | Firmware incompatible with device | Use correct firmware file and retry |
| **Failure** | Update failed due to error | Check device connection and retry |

## Important Notes

### Pre-Update Checklist

1. **Ensure stable USB connection** - Do not disconnect devices during the update process
2. **Verify firmware compatibility** - Use the correct firmware file for your device model
3. **Close other SDK applications** - Avoid conflicts with other programs accessing the devices
4. **Linux users**: The tool automatically uses LibUVC backend for better reliability

### During Update

- Do not unplug devices while the update is in progress
- Progress percentage and status messages are displayed in real-time
- Devices requiring two-stage updates will reboot automatically
- The tool waits up to 12 seconds for device reconnection after reboot

### Post-Update

- Successfully updated devices are automatically rebooted
- Press any key to exit after the summary is displayed

## Troubleshooting

### No Devices Found

**Symptom**: `No device found. Please connect a device first!`

**Solutions**:
- Check USB connections
- Ensure devices are powered on
- Try different USB ports
- On Linux, check udev rules are properly configured

### Firmware Mismatch

**Symptom**: Device listed under `Mismatch` in summary

**Solutions**:
- Verify you are using the correct firmware file for your device model
- Check device name and PID match the firmware requirements

### Update Failure

**Symptom**: Device listed under `Failure` in summary

**Solutions**:
- Check USB cable and connection stability
- Ensure no other applications are accessing the device
- Retry the update process
- If device failed to reconnect, unplug and replug the device, then retry

### Device Not Reconnecting

**Symptom**: `Device failed to reconnect within timeout`

**Solutions**:
- Manually unplug and replug the device
- Check if the device appears in device manager
- Retry the update process

## Run Sample

### Successful Multi-Device Update

```
Firmware file confirmed: Gemini330_v1.6.00.bin

Devices found:
--------------------------------------------------------------------------------
[0] Device: Gemini 330 | SN: CP3S34D0000T | Firmware version: 1.4.20
[1] Device: Gemini 330 | SN: CP3S34D0001T | Firmware version: 1.4.20
---------------------------------------------------------------------------------

Upgrading device: 1/2 - Gemini 330

Upgrading device firmware, please wait for a moment...

Progress: 100%
Status  : Update completed
Message : Firmware update successful

Upgrading device: 2/2 - Gemini 330

Upgrading device firmware, please wait for a moment...

Progress: 100%
Status  : Update completed
Message : Firmware update successful

Upgrade Summary:
==================================================
Success (2):
  - Name: Gemini 330 | SN: CP3S34D0000T
  - Name: Gemini 330 | SN: CP3S34D0001T

Mismatch (0):

Failure (0):

Upgrade process completed.
Press any key to exit...
```

### Update with Mismatch

```
Firmware file confirmed: WrongFirmware.bin

Devices found:
--------------------------------------------------------------------------------
[0] Device: Gemini 330 | SN: CP3S34D0000T | Firmware version: 1.4.20
[1] Device: Gemini 335 | SN: CP3S35D0001T | Firmware version: 1.4.20
---------------------------------------------------------------------------------

Upgrading device: 1/2 - Gemini 330
...

Upgrade Summary:
==================================================
Success (1):
  - Name: Gemini 330 | SN: CP3S34D0000T

Mismatch (1):
  - Name: Gemini 335 | SN: CP3S35D0001T
Please check use the correct firmware version and retry the upgrade.

Failure (0):

Upgrade process completed.
Press any key to exit...
```

## See Also

- [Firmware Update Sample](../../examples/2.device.firmware_update/README.md) - Single device firmware update with more control options
- [Orbbec SDK Documentation](https://orbbec.github.io/docs/OrbbecSDKv2_API_User_Guide/)
- [Firmware Repository](https://github.com/orbbec/OrbbecFirmware) - This tool is not suitable for Femto Mega, Femto Mega I, and Femto Bolt devices. For these devices, please refer to this repository.
