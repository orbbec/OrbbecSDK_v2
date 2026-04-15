# C++ Sample: 4.lidar_ip_config

## Overview

This sample provides a tool for configuring the network settings (IP address and subnet mask) on Pulsar SL450 and ME450 LiDAR devices.

**Note:** Gateway configuration is not supported by the LiDAR firmware protocol.

### Supported Devices

- Pulsar SL450 (PID: 0x1301)
- Pulsar ME450 (PID: 0x1302)
- MS600 (PID: 0x1300)

### Knowledge

Unlike GigE Vision cameras (Gemini 335Le, 435Le) which use the GVCP ForceIP command, LiDAR devices use a proprietary vendor command protocol for network configuration. This tool uses the SDK's structured data API to send the appropriate vendor commands.

**Available Configuration Options:**
- IP Address (`OB_RAW_DATA_LIDAR_IP_ADDRESS` = 8000)
- Subnet Mask (`OB_RAW_DATA_LIDAR_SUBNET_MASK` = 8003)

**Not Available:**
- Gateway (not supported by LiDAR firmware)

## Code Overview

1. **Discover and select LiDAR device**

    ```cpp
    ob::Context context;
    auto deviceList = context.queryDeviceList();
    auto device = selectLiDARDevice(deviceList);
    ```

2. **Read current network configuration**

    ```cpp
    auto data = device->getStructuredData(OB_RAW_DATA_LIDAR_IP_ADDRESS);
    // data contains 4 bytes: [octet1, octet2, octet3, octet4]
    ```

3. **Set new IP address**

    ```cpp
    std::vector<uint8_t> ipData = {192, 168, 1, 100};
    device->setStructuredData(OB_RAW_DATA_LIDAR_IP_ADDRESS, ipData);
    ```

4. **Set new subnet mask**

    ```cpp
    std::vector<uint8_t> maskData = {255, 255, 255, 0};
    device->setStructuredData(OB_RAW_DATA_LIDAR_SUBNET_MASK, maskData);
    ```

5. **Apply configuration**

    ```cpp
    device->setIntProperty(OB_PROP_LIDAR_APPLY_CONFIGS_INT, 1);
    ```

6. **Reboot device (optional)**

    ```cpp
    device->setBoolProperty(OB_PROP_REBOOT_DEVICE_BOOL, true);
    ```

## Run Sample

```shell
========================================
  LiDAR IP Configuration Tool
  For Pulsar SL450/ME450 devices
========================================

LiDAR Device List:
------------------------------------------------------------------------
0. Name: LiDAR SL450, SN: ABC123456, Connection: Ethernet, IP: 192.168.1.10
------------------------------------------------------------------------
Select a device (0-0): 0

========================================
Selected Device: LiDAR SL450
Serial Number:   ABC123456
Firmware:        2.2.4.5
========================================

Current Network Configuration:
  IP Address:  192.168.1.10
  Subnet Mask: 255.255.255.0

Enter new IP address [current: 192.168.1.10] (press Enter to keep current): 192.168.1.100
Enter new subnet mask [current: 255.255.255.0] (press Enter to keep current):

========================================
Pending Changes:
  IP Address:  192.168.1.10 -> 192.168.1.100
========================================

Apply changes? (y/n): y
Setting IP address... OK
Applying configuration... OK

Configuration applied successfully!

Would you like to reboot the device now? (y/n): y
Rebooting device... OK

Device is rebooting. Please wait a few seconds and reconnect.

Press any key to exit.
```

## Important Notes

1. **Reboot Required**: After changing network settings, a device reboot may be required for changes to take effect.

2. **Network Access**: Make sure your computer's network interface is configured to communicate with the device's new IP address before rebooting.

3. **Recovery**: If you lose connection to the device after changing settings, you may need to:
   - Configure your computer's IP to be on the same subnet as the device
   - Use DHCP if the device supports it
   - Contact Orbbec support for recovery procedures

4. **Gateway Limitation**: The LiDAR firmware does not support setting a gateway address. If routing is required, configure it on your network infrastructure.
