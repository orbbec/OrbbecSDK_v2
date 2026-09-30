# Timestamp Tracker Tool

This tool is used to collect frame timestamps from all connected Orbbec cameras. It captures system timestamp, global timestamp, and frame timestamp for each frame, and calculates the time differences. The frame timestamp is the device-side capture time of the frame; on the few devices without device timestamp support, the SDK falls back to a sampled host system timestamp. The tool supports multiple devices simultaneously and works cross-platform on Windows, Linux, ARM64, and macOS.

**Note:** Primarily tested with the **Gemini 330** series; other series may work but are not guaranteed.

## Timestamp Reference

See the [Timestamp Usage Guide](../../docs/tutorial/timestamp.md) for timestamp sources, device support and host fallback, clock selection, global timestamp mapping, and device clock synchronization. This README describes how to collect and interpret diagnostic data with the tracker.

## Usage

### Command Line Options

```bash
./ob_timestamp_tracker [options]

Options:
  -t, --time <minutes>          Tracking duration in minutes (default: 60)
  -i, --sync-interval <seconds> Device clock sync interval in seconds (default: 0)
                                0 = sync device clock once at start
                                >0 = periodic device clock sync every N seconds
  -l, --ldp <milliseconds>      Enable per-device LDP polling (1000-10000ms)
  -c, --config <file>           Path to JSON configuration file
  -g, --generate-config <file>  Write a default JSON config to <file> and exit
  -h, --help                    Show help message

Examples:
  ./ob_timestamp_tracker                           # 1 hour tracking, auto-select streams
  ./ob_timestamp_tracker -t 30                     # 30 minutes tracking
  ./ob_timestamp_tracker -t 60 -i 60               # 1 hour with 60s sync interval
  ./ob_timestamp_tracker -l 1000                   # poll LDP once per second on supported devices
  ./ob_timestamp_tracker -g config.json            # generate default JSON config file
  ./ob_timestamp_tracker -c config.json            # use JSON config file
  ./ob_timestamp_tracker -c config.json -t 30      # JSON config, override duration
```

Press **ESC** at any time to stop the tool early.

### JSON Configuration File

To quickly generate a default config file as a starting point:

```bash
./ob_timestamp_tracker -g config.json
```

This writes a default `config.json` which you can edit before use. Stream configuration is then loaded via `-c`:

```json
{
  "duration": 60,
  "syncInterval": 0,
  "streams": [
    { "sensor": "depth", "width": 640, "height": 480, "fps": 30, "format": "Y16" },
    { "sensor": "color", "fps": 15, "format": "MJPG" },
    { "sensor": "imu", "fps": "100HZ", "accelFullScaleRange": "16g", "gyroFullScaleRange": "2000dps" }
  ],
  "deviceOverrides": {
    "<serial_number>": {
      "streams": [
        { "sensor": "depth" },
        { "sensor": "imu", "fps": "3.125HZ", "accelFullScaleRange": "8g", "gyroFullScaleRange": "1000dps" }
      ]
    }
  }
}
```

- `streams` - global stream config applied to all devices.
- `deviceOverrides` - per-device stream config keyed by serial number; fully overrides `streams` for that device.
- In each stream object, `sensor` is **required**; `width`, `height`, `fps`, and `format` are **optional** (omit or set to `0`/empty for auto-select).
- Video stream `fps` should remain an integer value.
- When `sensor` is `"imu"`, the tool opens both accel and gyro together. `fps` is their shared sample rate, and can be written either as a number or as a case-insensitive Hz string such as `"3.125HZ"`, `"6.25hz"`, or `"100HZ"`.
- IMU full-scale range strings follow SDK names such as `"16g"`, `"8g"`, `"1000dps"`, and `"2000dps"`.
- `-t` and `-i` CLI flags always override the corresponding JSON values.
- `-l <milliseconds>` enables LDP on each supported device and reads `OB_PROP_LDP_MEASURE_DISTANCE_INT` periodically. The value is discarded; unsupported devices are ignored. Before enabling it, the tool saves the original LDP and laser control values, then restores them on exit. Values below 1000 ms use 1000 ms; values above 10000 ms use 10000 ms.

When no stream config is provided, the tool auto-selects: Depth + Color (preferred), or Color_Left + Color_Right (fallback).

### Running the Tool

**Windows:**
```powershell
ob_timestamp_tracker.exe
ob_timestamp_tracker.exe -g config.json       # generate default config
ob_timestamp_tracker.exe -c config.json
```

**Linux/macOS/ARM64:**
```bash
sudo ./ob_timestamp_tracker
sudo ./ob_timestamp_tracker -g config.json    # generate default config
sudo ./ob_timestamp_tracker -c config.json
```

The tool will automatically:
1. Enumerate all connected Orbbec devices
2. Perform time synchronization (once or periodically based on settings)
3. Enable streams per config (or auto-select if no config provided)
4. Collect frame timestamps and save to CSV files
5. Stop after the specified duration (or immediately when ESC is pressed)

### Output Files

The tool generates CSV files in the **current directory** with the following naming convention:
```
Timestamps_<SerialNumber>_<SensorType>_<Width>x<Height>_<FPS>_<Format>.csv
```

For IMU streams, the filename uses the full-scale range and sample rate instead:
```
Timestamps_<SerialNumber>_<SensorType>_<FullScaleRange>_<SampleRate>_<Format>.csv
```

For example:
```
Timestamps_CP4H74D0001K_Color_1280x720_30_MJPG.csv
Timestamps_CP4H74D0001K_Depth_848x480_30_Y16.csv
Timestamps_CP4H74D0001K_Accel_16g_100_HZ_ACCEL.csv
```

When a file reaches **1,024,000 rows**, it automatically rolls over to a new volume (appending `-2`, `-3`, etc.):
```
Timestamps_CP4H74D0001K_Color_1280x720_30_MJPG.csv
Timestamps_CP4H74D0001K_Color_1280x720_30_MJPG-2.csv
```

### CSV File Format

Each CSV file contains only timestamp-related columns (resolution, FPS, and format are encoded in the filename):

| Column | Description |
|--------|-------------|
| FrameIndex | SDK frame index |
| FrameNumber | Metadata frame number (if available, otherwise "n/a") |
| RecvTS(us) | Host clock timestamp taken when the tool receives the frame from the SDK, at pipeline callback entry; uses the same host clock type as SysTS (microseconds) |
| SysTS(us) | SDK system timestamp (microseconds) |
| GlobalTS(us) | Frame timestamp mapped to the host clock domain, representing the same instant during exposure (microseconds) |
| DevTS(us) | Frame timestamp returned by `getTimeStampUs()`: the device capture time (a sampled host system timestamp on the few devices without device timestamp support) |
| SensorTS(us) | Video streams only: sensor timestamp metadata (microseconds; "n/a" when unsupported) |
| Diff_SG(us) | SysTS - GlobalTS: frame end-to-end latency from the exposure instant represented by the frame timestamp to SDK host receipt (`n/a` if GlobalTS is zero) |
| Diff_SD(us) | SysTS - DevTS (`n/a` only when DevTS is zero) |

**Sample Video Output:**
```
FrameIndex,FrameNumber,RecvTS(us),SysTS(us),GlobalTS(us),DevTS(us),SensorTS(us),Diff_SG(us),Diff_SD(us)
1,1,1774342520618000,1774342520622037,1774342520605178,1774342520604183,1774342520604183,16859,17854
2,2,1774342520651000,1774342520654397,1774342520638555,1774342520637556,1774342520637556,15842,16841
```

### Depth and Color Latency

Use `Diff_SG` (`SysTS - GlobalTS`) to measure frame end-to-end latency when a valid global timestamp is available. Its starting point is the exposure instant represented by the frame timestamp, and its endpoint is SDK host receipt. Gemini 330 long-baseline devices default to end of exposure, short-baseline devices use the middle of exposure, and devices with Intra-Camera Sync Reference set to Start of Exposure use the start of exposure. See [Frame Timestamp and Intra-Camera Sync Reference](../../docs/tutorial/timestamp.md#frame-timestamp-and-intra-camera-sync-reference) before comparing results across devices or settings. This measurement includes processing and transport between these points and is affected by clock-mapping accuracy.

When the device clock is synchronized to the selected host clock, `Diff_SD` can also be used to observe the difference between device capture time and host receive time. Account for synchronization error and subsequent device clock drift when interpreting it.

On the few devices without device timestamp support, `DevTS` is the host fallback, so `Diff_SD` compares two host-derived values and must not be interpreted as device-to-host capture latency.

**Default exposure:** The tracker disables auto-exposure and sets a fixed exposure time of **3 ms** for depth and color streams where the corresponding properties are supported. This stabilizes exposure during latency measurements; it does not change the Intra-Camera Sync Reference setting or guarantee measurement accuracy.

### Time Synchronization

In one-time mode, every connected device must support device clock synchronization; if synchronization fails on any device, the tracker reports an error and exits. Periodic mode skips devices without clock synchronization support.

The tool supports two time synchronization modes:

| Mode | Behavior | Typical use | API |
|------|----------|-------------|-----|
| One-time sync (default, `-i 0`) | Synchronizes each device with the host once before starting its streams | Short tracking sessions (< 10 minutes) | `device->timerSyncWithHost()` |
| Periodic clock sync (e.g., `-i 60`) | Synchronizes device clocks with the host every N seconds | Long tracking sessions to limit device clock drift | `ctx.enableDeviceClockSync(intervalMs)` |

### Multi-Device Support

The tool automatically detects and records from all connected Orbbec devices simultaneously. Each device gets its own set of CSV files. Per-device stream configuration is supported via `deviceOverrides` in the JSON config (keyed by serial number). The console output shows the progress for each device:

```
[1/2] Starting collector for Gemini335 (SN123456789)
  Device: Gemini335 (SN123456789)
  Found 2 sensor(s)
    - Depth sensor enabled
    - Color sensor enabled
  Collector started successfully

[2/2] Starting collector for Gemini330 (SN987654321)
  Device: Gemini330 (SN987654321)
  Found 2 sensor(s)
    - Depth sensor enabled
    - Color sensor enabled
  Collector started successfully

================================================
Tracking started on 2 device(s)
Press ESC to stop early or wait for timeout
================================================
```

## Building from Source

### Step 1: Clone the repository
```bash
git clone https://github.com/orbbec/OrbbecSDK_v2.git
cd OrbbecSDK_v2
```

### Step 2: Build the SDK
Follow the build instructions in the [Building Orbbec SDK](https://github.com/orbbec/OrbbecSDK_v2/blob/main/docs/tutorial/building_orbbec_sdk.md) guide.

Make sure to enable tools during build:
```bash
cmake -DOB_BUILD_TOOLS=ON ..
```

### Step 3: Run the tool
After building, the executable will be located in:
- Windows: `build/win_x64/bin/ob_timestamp_tracker.exe`
- Linux: `build/linux_x86_64/bin/ob_timestamp_tracker`
- ARM64: `build/linux_arm64/bin/ob_timestamp_tracker`

## Troubleshooting

### No devices found
- Ensure the camera is properly connected
- On Linux, install udev rules: `sudo ./scripts/env_setup/install_udev_rules.sh`
- Run with sudo on Linux: `sudo ./ob_timestamp_tracker`

### Permission denied (Linux)
The tool requires elevated permissions to access USB devices:
```bash
sudo ./ob_timestamp_tracker
```

### Timestamp drift in long tracking sessions
Use periodic time synchronization:
```bash
./ob_timestamp_tracker -t 120 -i 60  # Sync every 60 seconds
```

### CSV files not generated
Check that the current directory has write permissions.

## Advanced Configuration

### Modifying Default Tracking Duration
Edit `src/cmdline_parser.hpp`:
```cpp
struct CmdLineConfig {
    int durationMinutes = 60;  // Change default here
    // ...
};
```

### Sensor Types
Any sensor type recognized by the SDK can be specified in the JSON config by name (case-insensitive), e.g. `"depth"`, `"color"`, `"ir"`, `"ir_left"`, `"ir_right"`. The tool resolves names via the SDK's own string conversion at runtime.

In addition, `"imu"` is a tool-level alias that expands to both accel and gyro and starts them together.

When no config is provided, the tool auto-selects: **Depth + Color** (preferred), or **Color_Left + Color_Right** (fallback if depth/color are unavailable).

## Notes

- Primarily tested with the **Gemini 330** series; other series may work but are not guaranteed.
- Supported sensors: Depth, Color, IR, Color_Left, Color_Right, Accel, Gyro, and the `imu` alias.
- When no stream config is provided, auto-selects Depth + Color, or Color_Left + Color_Right as fallback.
- The tool uses the `libobsensor::tools` namespace for all internal components.
- Frame processing runs in a dedicated thread per device for real-time performance.
- CSV files are flushed every 100 frames to balance I/O performance with data persistence.
