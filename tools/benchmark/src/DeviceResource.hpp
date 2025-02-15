// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include <libobsensor/ObSensor.hpp>

#include <string>
#include <vector>
#include <mutex>

class DeviceResource {
public:
    DeviceResource(std::shared_ptr<ob::Device> device);
    ~DeviceResource() = default;

    void startStream(std::shared_ptr<ob::Config> config);
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

private:
    /**
     * @brief Resets the configuration to default values.
     */
    void resetConfig();

private:
    std::shared_ptr<ob::Device>   device_;
    std::shared_ptr<ob::Pipeline> pipeline_;
    std::shared_ptr<ob::Frame>    frames_;

    std::mutex mutex_;

    bool is_spatial_filter_enabled_;
    bool is_align_filter_enabled_;
    bool is_point_cloud_filter_enabled_;

    std::shared_ptr<ob::Align>            align_filter_;
    std::shared_ptr<ob::PointCloudFilter> point_cloud_filter_;

    // Post-processing filters
    std::shared_ptr<ob::SpatialAdvancedFilter> spatial_filter_;
};