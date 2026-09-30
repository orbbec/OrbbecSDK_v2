# Timestamp Usage Guide

Orbbec SDK frames expose frame, system, and global timestamp values. Global timestamps are available only on devices that support them. Choosing the correct timestamp requires understanding which clock produced it and which event it represents.

All timestamp APIs in this guide return microseconds.

## Timestamp Types

| Timestamp | C++ API | C API | Clock domain | Meaning |
|-----------|---------|-------|--------------|---------|
| Frame timestamp | `Frame::getTimeStampUs()` | `ob_frame_get_timestamp_us()` | Device when supported; otherwise host | Time when the frame was captured as reported by the device, or a sampled host system timestamp used as fallback |
| System timestamp | `Frame::getSystemTimeStampUs()` | `ob_frame_get_system_timestamp_us()` | Host | Time when the frame was received by the host |
| Global timestamp | `Frame::getGlobalTimeStampUs()` | `ob_frame_get_global_timestamp_us()` | Host | Frame timestamp mapped to the host clock domain, representing the same instant during exposure |

Important rules:

- Compare or subtract timestamps only when they use the same clock domain.
- The frame timestamp, also called the device timestamp, is the capture time of the frame on the device. A few devices do not support device timestamps; on these devices the SDK falls back to a sampled host system timestamp for `getTimeStampUs()`, following the selected host clock type. **Unless stated otherwise, the rest of this guide assumes the device supports device timestamps, so the frame timestamp is the device capture time**.
- On devices and streams that supply the frame timestamp through frame metadata, the value is nonzero during normal operation and becomes `0` only when the metadata is missing or abnormal (for example, when metadata is not registered on Windows).
- System timestamps from different devices are comparable in the host clock domain, but they describe frame arrival rather than the original capture time.
- Global timestamps are normally the best choice for comparing capture times across streams or synchronized devices.
- Some devices enable global timestamps by default. Check support and explicitly enable them when needed. A value of `0` means the global timestamp is unavailable for that frame.

### Frame Timestamp and Intra-Camera Sync Reference

For devices that support device timestamps, the frame timestamp is generated in the device clock domain. Whether it represents the start, middle, or end of exposure depends on the device and its Intra-Camera Sync Reference setting:

| Device or setting | Exposure Time Represented by the Frame Timestamp |
|----------------|---------------------------------|
| Gemini 330 long-baseline devices, default mode | End of exposure |
| Gemini 330 short-baseline devices, default mode | Middle of exposure |
| Devices supporting configurable Intra-Camera Sync Reference, set to Start of Exposure | Start of Exposure |

The global timestamp maps this frame timestamp to the host clock domain without changing the instant it represents. For example, an end-of-exposure frame timestamp produces an end-of-exposure global timestamp. Host fallback timestamps represent host sampling rather than the start, middle, or end of device exposure.

## Select the Host Clock

System and global timestamps use the host clock selected for the SDK runtime:

| Clock type | Description | Recommended use |
|------------|-------------|-----------------|
| `OB_CLOCK_TYPE_REALTIME` | Wall clock, epoch-based; this is the default | Correlation with logs and external wall-clock events |
| `OB_CLOCK_TYPE_MONOTONIC` | Monotonic clock, not epoch-based | Durations, latency, and measurements that must not be affected by wall-clock adjustments |

Configure the clock before starting streams:

```cpp
ob::Context context;
context.setTimestampClockType(OB_CLOCK_TYPE_MONOTONIC);
```

All `ob::Context` instances share the underlying SDK runtime and clock selection. Do not switch the clock type while streams are running. If it must be changed, stop streaming, change the clock, synchronize the device clocks again, and then restart streaming.

The C equivalent is `ob_context_set_timestamp_clock_type()`.

## Enable Global Timestamps

Check support before enabling global timestamps:

```cpp
auto device = pipeline.getDevice();

bool hasGlobalTimestamp = device->isGlobalTimestampSupported();
if(hasGlobalTimestamp) {
    device->enableGlobalTimestamp(true);
}
```

The C equivalents are `ob_device_is_global_timestamp_supported()` and `ob_device_enable_global_timestamp()`.

Enabling global timestamps starts the host-side mapping between the device clock and the selected host clock. Global timestamps do not require prior device clock synchronization. To accommodate devices that do not support global timestamps, a one-time synchronization before streaming is generally recommended for devices that support `timerSyncWithHost()`.

On devices that do not support global timestamps, `getGlobalTimeStampUs()` returns `0`. Call `enableGlobalTimestamp(true)` only after the support check succeeds; calling it on an unsupported device reports an error.

## Synchronize Device Clocks

### One-Time Synchronization Before Streaming

Devices that support device timestamps also support synchronizing their device clock with the host. For these devices, a one-time synchronization before opening streams is generally recommended, including when global timestamps are unavailable:

```cpp
device->timerSyncWithHost();
```

For multiple devices, synchronize each supported device before starting any stream. The call is synchronous and reports an error if synchronization is unsupported or fails. Applications must decide whether to stop or continue using valid global timestamps or unsynchronized frame timestamps. A synchronization failure does not change the frame timestamp source or activate host fallback.

The C equivalent is `ob_device_timer_sync_with_host()`.

### Periodic Synchronization

Device clocks can drift during long-running capture. A context can manage repeated synchronization for all devices already created by the SDK runtime:

```cpp
// Synchronize every 60 seconds. The argument is in milliseconds.
context.enableDeviceClockSync(60 * 1000);
```

Passing `0` requests a one-time context-managed synchronization. The C equivalent is `ob_enable_device_clock_sync()`.

Synchronizing a supported device while it is streaming can cause one discontinuity in the frame timestamp and in timestamps derived from it. Consumers should detect or tolerate this event instead of assuming the timestamp sequence is continuous across resynchronization. Synchronization does not turn a host fallback timestamp into a device hardware timestamp.

## Read Frame Timestamps

For a frame received from an active stream:

```cpp
const uint64_t frameTsUs = frame->getTimeStampUs();
const uint64_t systemTsUs = frame->getSystemTimeStampUs();
const uint64_t globalTsUs = frame->getGlobalTimeStampUs();
```

On the few devices without device timestamp support, `frameTsUs` holds the host fallback value instead of a device capture time. A global timestamp of `0` is unavailable and must be excluded from capture-time comparisons.

## Choose a Timestamp for the Scenario

| Scenario | Recommended timestamp | Notes |
|------|-----------------------|-------|
| Measure frame intervals within one device | Frame timestamp | Follows the device capture timeline and is not affected by host receive jitter |
| Compare capture times across streams or devices | Global timestamp | Check support, enable global timestamps, and use nonzero values; prior device clock synchronization is not required |
| Observe when frames reach the host | System timestamp | Includes transport, scheduling, and SDK receive-path effects |
| Measure frame end-to-end latency | `system - global` | From the exposure instant represented by the frame timestamp to SDK host receipt; requires a valid global timestamp |
| Correlate frames with application events | System or global timestamp | The application event timestamp must use the same host clock type |

The difference `system - global` measures frame end-to-end latency from the exposure instant represented by the frame timestamp to the SDK's host receive timestamp. It includes the processing and transport between those instants, rather than only transport time, and is affected by clock-mapping accuracy. When comparing results, ensure the timestamps represent the same stage of exposure: start-, middle-, and end-of-exposure timestamps define different latency starting points. A global timestamp of `0` is unavailable and must not be used in this calculation.

Application timestamps must use the same clock source as the SDK. In realtime mode, the SDK uses `std::chrono::system_clock`. In monotonic mode, it uses `CLOCK_MONOTONIC_RAW` on Linux/Android, so do not directly compare SDK timestamps with `std::chrono::steady_clock` values there; on other platforms it uses `std::chrono::steady_clock`.

Do not directly subtract a frame timestamp from a system timestamp merely because both values are expressed in microseconds. Their clock domains are different unless an explicit synchronization and mapping strategy makes the comparison meaningful.

When `getTimeStampUs()` contains the host fallback, it may be close to `getSystemTimeStampUs()`, but their difference is not device-to-host capture latency. The public value itself does not identify its provenance, so applications that require hardware capture timing must confirm support for the target device rather than relying on a nonzero timestamp or a numerical heuristic.

## C API Reference

| Operation | C API |
|-----------|-------|
| Select the host clock | `ob_context_set_timestamp_clock_type()` |
| Read the selected host clock | `ob_context_get_timestamp_clock_type()` |
| Check global timestamp support | `ob_device_is_global_timestamp_supported()` |
| Enable global timestamps | `ob_device_enable_global_timestamp()` |
| Synchronize one device with the host | `ob_device_timer_sync_with_host()` |
| Synchronize all created devices once or periodically | `ob_enable_device_clock_sync()` |
| Read frame timestamp | `ob_frame_get_timestamp_us()` |
| Read system timestamp | `ob_frame_get_system_timestamp_us()` |
| Read global timestamp | `ob_frame_get_global_timestamp_us()` |

## Diagnostic Tool

The [Timestamp Tracker](../../tools/timestamp_tracker/README.md) collects frame, system, global, sensor, and application receive timestamps from one or more devices and writes them to CSV files. Use it to observe drift, compare timestamp differences, and validate a synchronization strategy over long-running sessions.

Sample tracker output collected on Gemini 330 series devices is available in the [sample data](../../tools/timestamp_tracker/samples/README.md).

## Common Pitfalls

- Using a global timestamp of `0` in calculations. It means the global timestamp is unavailable for that frame.
- Assuming every device supports global timestamps. Always check support first.
- Assuming `getTimeStampUs()` always comes from the device. Devices without device timestamp support use a sampled host system timestamp instead.
- Treating a zero metadata timestamp as an actual device clock value of zero. When the frame timestamp comes from frame metadata, a value of `0` usually indicates missing or abnormal metadata (for example, metadata not registered on Windows).
- Comparing timestamps from different clock domains.
- Switching the host clock type while streaming.
- Assuming timestamps remain continuous when a device clock is synchronized or reset during streaming.
- Using system timestamps as capture timestamps. System timestamps describe host receipt time.
- Comparing frame timestamps from multiple devices without initial synchronization or without accounting for later drift.
- Treating `system - device` as device-to-host latency without first confirming that the device supports device timestamps and that its clock has been synchronized with the host, or without accounting for drift since the last synchronization.
- On Windows, failing to register frame metadata. Follow the [Windows metadata setup guide](../../scripts/env_setup/obsensor_metadata_win10.md); missing metadata can produce abnormal frame timestamps and affect frame synchronization.
