# Timestamp Tracker Sample Data

Screenshots of `ob_timestamp_tracker` CSV output collected by the product team on Gemini 330 series devices. Each group contains one screenshot of the depth CSV and one of the RGB CSV. Use them as a reference for what the columns look like in practice; see the [Timestamp Tracker README](../README.md) for the column definitions.

## Common Test Conditions

- Firmware: V1.8.10
- SDK: Orbbec SDK 2.10.6 for all data sets
- Host clock: realtime (`OB_CLOCK_TYPE_REALTIME`) for all data sets
- Device clock synchronization: once before streaming, with no periodic resynchronization
- Exposure: depth 3 ms, RGB 3 ms
- Streams: depth, left/right IR and RGB, at the resolution and frame rate listed below

| Host | Configuration |
|------|---------------|
| Dell 3050 desktop | Intel Core i7-14700 x 28, 32 GiB RAM, NVIDIA GeForce RTX 3050, Ubuntu 22.04 (Linux x86) |
| AGX Orin (64 GB) | Linux ARM, JetPack 6.1 |

## Data Sets

| Directory | Device | Intra-Camera Sync Reference | Multi-device sync mode | Host | Depth | RGB |
|-----------|--------|---------------------------|-----------|------|-------|-----|
| [g335_middle_x86](g335_middle_x86) | G335 | Middle of exposure | Standalone | Dell 3050, Ubuntu 22.04 | 848x480 30 fps Y16 | 1280x720 30 fps MJPG |
| [g335l_end_x86](g335l_end_x86) | G335L | End of exposure | Standalone | Dell 3050, Ubuntu 22.04 | 848x480 30 fps Y16 | 1280x720 30 fps MJPG |
| [g335le_end_x86](g335le_end_x86) | G335Le | End of exposure | Standalone | Dell 3050, Ubuntu 22.04 | 640x400 15 fps Y16 | 640x400 15 fps MJPG |
| [g335lg_start_orin_jp61](g335lg_start_orin_jp61) | G335Lg | Start of exposure | Hardware triggering | AGX Orin, JetPack 6.1 | 848x480 30 fps Y16 | 1280x720 30 fps YUYV |

The left and right IR streams were 848x480 30 fps Y8 for all groups except G335Le (640x400 15 fps Y8). The Intra-Camera Sync Reference column is the exposure instant represented by the frame timestamp; see [Frame Timestamp and Intra-Camera Sync Reference](../../../docs/tutorial/timestamp.md#frame-timestamp-and-intra-camera-sync-reference).

For `g335lg_start_orin_jp61`, capture used hardware triggering with frame rate boosting disabled. The 30 fps values in the table describe the configured stream profiles; the screenshots show an actual output rate of approximately 15 fps (about 66.7 ms between consecutive frames).

Each directory contains `depth.png` and `rgb.png`.

## Long-Running Behavior

During long-running capture, host realtime clock adjustments by NTP and gradual device clock drift can change the offset between the device and host clocks. As a result, `Diff_SD` may grow or change even when frame delivery latency remains unchanged, especially when the device clock is synchronized only once before streaming. Global timestamp mapping continuously tracks the relationship between the device and host clocks, making `Diff_SG` more suitable for observing frame delivery latency. Periodic synchronization (`-i`) can limit accumulated device-to-host clock offset, but consumers must account for possible timestamp discontinuities during resynchronization.

## Screenshot Precision

The G335Lg screenshots were produced using Microsoft Excel on Windows. Excel did not preserve the final digit of the 16-digit timestamp values and replaced it with `0`. Subtracting the displayed timestamps can therefore differ slightly from the `Diff_SG` and `Diff_SD` columns, which were calculated by the tracker before import into Excel. This is an Excel import precision limitation, not a timestamp precision loss during SDK collection. Use the original CSV values for exact arithmetic; when opening the CSV in Excel, import timestamp columns as text to preserve all digits.
