# Timestamp Tracker Tool

This tool is used to collect frame timestamps from all connected Orbbec cameras. It captures system timestamp, global timestamp, and device timestamp for each frame, and calculates the time differences. The tool supports multiple devices simultaneously and works cross-platform on Windows, Linux, ARM64, and macOS.

**Note:** Primarily tested with the **Gemini 330** series; other series may work but are not guaranteed.

## Usage

### Command Line Options

```bash
./timestamp_tracker [options]

Options:
  -t, --time <minutes>          Tracking duration in minutes (default: 60)
  -i, --sync-interval <seconds> Device clock sync interval in seconds (default: 0)
                                0 = sync device clock once at start
                                >0 = periodic device clock sync every N seconds
  -c, --config <file>           Path to JSON configuration file
  -g, --generate-config <file>  Write a default JSON config to <file> and exit
  -h, --help                    Show help message

Examples:
  ./timestamp_tracker                           # 1 hour tracking, auto-select streams
  ./timestamp_tracker -t 30                     # 30 minutes tracking
  ./timestamp_tracker -t 60 -i 60               # 1 hour with 60s sync interval
  ./timestamp_tracker -g config.json            # generate default JSON config file
  ./timestamp_tracker -c config.json            # use JSON config file
  ./timestamp_tracker -c config.json -t 30      # JSON config, override duration
```

Press **ESC** at any time to stop the tool early.

### JSON Configuration File

To quickly generate a default config file as a starting point:

```bash
./timestamp_tracker -g config.json
```

This writes a default `config.json` which you can edit before use. Stream configuration is then loaded via `-c`:

```json
{
  "duration": 60,
  "syncInterval": 0,
  "streams": [
    { "sensor": "depth", "width": 640, "height": 480, "fps": 30, "format": "Y16" },
    { "sensor": "color", "fps": 15, "format": "MJPG" }
  ],
  "deviceOverrides": {
    "<serial_number>": {
      "streams": [
        { "sensor": "depth" }
      ]
    }
  }
}
```

- `streams` — global stream config applied to all devices.
- `deviceOverrides` — per-device stream config keyed by serial number; fully overrides `streams` for that device.
- In each stream object, `sensor` is **required**; `width`, `height`, `fps`, and `format` are **optional** (omit or set to `0`/empty for auto-select).
- `-t` and `-i` CLI flags always override the corresponding JSON values.

When no stream config is provided, the tool auto-selects: Depth + Color (preferred), or Color_Left + Color_Right (fallback).

### Running the Tool

**Windows:**
```powershell
timestamp_tracker.exe
timestamp_tracker.exe -g config.json       # generate default config
timestamp_tracker.exe -c config.json
```

**Linux/macOS/ARM64:**
```bash
sudo ./timestamp_tracker
sudo ./timestamp_tracker -g config.json    # generate default config
sudo ./timestamp_tracker -c config.json
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

For example:
```
Timestamps_CP4H74D0001K_Color_1280x720_30_MJPG.csv
Timestamps_CP4H74D0001K_Depth_848x480_30_Y16.csv
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
| RecvTS(us) | App receive timestamp — system clock captured at pipeline callback entry (microseconds) |
| SysTS(us) | SDK system timestamp (microseconds) |
| GlobalTS(us) | Global timestamp (microseconds) |
| DevTS(us) | Device timestamp (microseconds) |
| Diff_SG(us) | SysTS − GlobalTS (empty if GlobalTS unavailable) |
| Diff_SD(us) | SysTS − DevTS (empty if DevTS unavailable) |

**Sample Output:**
```
FrameIndex,FrameNumber,RecvTS(us),SysTS(us),GlobalTS(us),DevTS(us),Diff_SG(us),Diff_SD(us)
1,1,1774342520618000,1774342520622037,1774342520605178,1774342520604183,16859,17854
2,2,1774342520651000,1774342520654397,1774342520638555,1774342520637556,15842,16841
```

### Depth and Color Latency

The CSV files can be used to measure the latency of depth and color streams. The latency is represented by the difference between the system timestamp (`Diff_SD`) and the device timestamp.

**Note:** For accurate latency measurement, the tool automatically disables auto-exposure and sets a fixed exposure time of 3ms to avoid exposure adjustments affecting frame timing.

### Time Synchronization

The tool supports two time synchronization modes:

1. **One-time sync** (default, `-i 0`):
   - Synchronizes device clock with host once before starting tracking
   - Suitable for short tracking sessions (< 10 minutes)

2. **Periodic clock sync** (e.g., `-i 60`):
   - Continuously synchronizes device clock with host every N seconds
   - Recommended for long tracking sessions to prevent timestamp drift
   - Uses `ctx.enableDeviceClockSync(intervalMs)` API

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
- Windows: `build/win_x64/bin/timestamp_tracker.exe`
- Linux: `build/linux_x86_64/bin/timestamp_tracker`
- ARM64: `build/linux_arm64/bin/timestamp_tracker`

## Troubleshooting

### No devices found
- Ensure the camera is properly connected
- On Linux, install udev rules: `sudo ./scripts/env_setup/install_udev_rules.sh`
- Run with sudo on Linux: `sudo ./timestamp_tracker`

### Permission denied (Linux)
The tool requires elevated permissions to access USB devices:
```bash
sudo ./timestamp_tracker
```

### Timestamp drift in long tracking sessions
Use periodic time synchronization:
```bash
./timestamp_tracker -t 120 -i 60  # Sync every 60 seconds
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

When no config is provided, the tool auto-selects: **Depth + Color** (preferred), or **Color_Left + Color_Right** (fallback if depth/color are unavailable).

## Notes

- Primarily tested with the **Gemini 330** series; other series may work but are not guaranteed.
- Supported sensors: Depth, Color, IR, Color_Left, Color_Right (any type the SDK recognizes).
- When no stream config is provided, auto-selects Depth + Color, or Color_Left + Color_Right as fallback.
- The tool uses the `libobsensor::tools` namespace for all internal components.
- Frame processing runs in a dedicated thread per device for real-time performance.
- CSV files are flushed every 100 frames to balance I/O performance with data persistence.
