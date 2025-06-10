// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include <libobsensor/ObSensor.hpp>

#include <string>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>

class DeviceResource {
public:
    DeviceResource(std::shared_ptr<ob::Device> device);
    ~DeviceResource() = default;

    void startStream(std::shared_ptr<ob::Config> config, bool enableTimestampDiffSaver = false);
    void stopStream();

public:
    /**
     * @brief Enables or disables the spatial filter.
     */
    void enableSpatialFilter(bool enable);

    /**
     * @brief Enables or disables the hardware noise removal filter.
     */
    void enableHwNoiseRemoveFilter(bool enable);

    /**
     * @brief Enables or disables the software noise removal filter.
     * @note OrbebcSDK have a internal Noise removal filter, and it's default is on. If you use hardware noise removal filter, it is recommended to disable the internal filter.
     */
    void enableSwNoiseRemoveFilter(bool enable);

    /**
     * @brief Enables or disables the align filter.
     */
    void enableAlignFilter(bool enable);

    /**
     * @brief Enables or disables the point cloud filter.
     */
    void enablePointCloudFilter(bool enable);

    /**
     * @brief Enables or disables the rgb point cloud filter.
     */
    void enableRGBPointCloudFilter(bool enable);

    /**
     * @brief Synchronizes the host time with the device time.
     */
    void syncTimeWithHost();

private:
    /**
     * @brief Resets the configuration to default values.
     */
    void resetConfig();

    /**
     * @brief Save timestamp diffs.
     */
    void timestampDiffSaver();

private:
    std::shared_ptr<ob::Device>   device_   = nullptr;
    std::shared_ptr<ob::Pipeline> pipeline_ = nullptr;
    std::shared_ptr<ob::Frame>    frames_   = nullptr;

    std::mutex mutex_;

    std::queue<std::shared_ptr<ob::FrameSet>> frameQueue_;
    std::condition_variable                   timestamp_diff_cv_;
    std::thread                               timestamp_diff_saver_thread_;
    std::atomic<bool>                         timestamp_diff_saver_running_{ false };

    bool is_spatial_filter_enabled_     = false;
    bool is_align_filter_enabled_       = false;
    bool is_point_cloud_filter_enabled_ = false;
    bool is_timestamp_saver_enabled_    = false;

    std::shared_ptr<ob::Align>            align_filter_       = nullptr;
    std::shared_ptr<ob::PointCloudFilter> point_cloud_filter_ = nullptr;

    // Post-processing filters
    std::shared_ptr<ob::SpatialAdvancedFilter> spatial_filter_ = nullptr;
};