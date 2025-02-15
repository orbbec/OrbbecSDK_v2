// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "DeviceResource.hpp"

DeviceResource::DeviceResource(std::shared_ptr<ob::Device> device) : device_(device), frames_(nullptr) {
    pipeline_           = std::make_shared<ob::Pipeline>(device_);
    spatial_filter_     = std::make_shared<ob::SpatialAdvancedFilter>();
    align_filter_       = std::make_shared<ob::Align>(OB_STREAM_COLOR);
    point_cloud_filter_ = std::make_shared<ob::PointCloudFilter>();

    resetConfig();
}

void DeviceResource::startStream(std::shared_ptr<ob::Config> config) {
    pipeline_->start(config, [this](std::shared_ptr<ob::FrameSet> frameset) {
        if(frameset == nullptr) {
            return;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        frames_ = frameset;

        // Note: It is not recommended to handle d2c or point cloud consuming operations in the callbacks set by the pipeline.
        if(is_align_filter_enabled_ && align_filter_) {
            frames_ = align_filter_->process(frames_);
        }

        if(is_point_cloud_filter_enabled_ && point_cloud_filter_) {
            frames_ = align_filter_->process(frames_);
            point_cloud_filter_->process(frames_);
        }
    });
}

void DeviceResource::stopStream() {
    pipeline_->stop();

    {
        std::lock_guard<std::mutex> lock(mutex_);
        frames_ = nullptr;
    }
    resetConfig();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

void DeviceResource::enableSpatialFilter(bool enable) {
    std::lock_guard<std::mutex> lock(mutex_);
    is_spatial_filter_enabled_ = enable;
}

void DeviceResource::enableHwNoiseRemoveFilter(bool enable) {
    if(device_->isPropertySupported(OB_PROP_HW_NOISE_REMOVE_FILTER_ENABLE_BOOL, OB_PERMISSION_READ_WRITE)) {
        device_->setBoolProperty(OB_PROP_HW_NOISE_REMOVE_FILTER_ENABLE_BOOL, enable);
    }
}

void DeviceResource::enableSwNoiseRemoveFilter(bool enable) {
    device_->setBoolProperty(OB_PROP_DEPTH_NOISE_REMOVAL_FILTER_BOOL, enable);
}

void DeviceResource::enableAlignFilter(bool enable) {
    std::lock_guard<std::mutex> lock(mutex_);
    pipeline_->enableFrameSync();
    is_align_filter_enabled_ = enable;
}

void DeviceResource::enablePointCloudFilter(bool enable) {
    std::lock_guard<std::mutex> lock(mutex_);
    pipeline_->enableFrameSync();
    is_point_cloud_filter_enabled_ = enable;
    point_cloud_filter_->setCreatePointFormat(OB_FORMAT_POINT);
}

void DeviceResource::enableRGBPointCloudFilter(bool enable) {
    std::lock_guard<std::mutex> lock(mutex_);
    pipeline_->enableFrameSync();
    is_point_cloud_filter_enabled_ = enable;
    point_cloud_filter_->setCreatePointFormat(OB_FORMAT_RGB_POINT);
}

void DeviceResource::resetConfig() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        is_point_cloud_filter_enabled_ = false;
        is_align_filter_enabled_ = false;
    }

    pipeline_->disableFrameSync();

    enableHwNoiseRemoveFilter(false);
    enableSwNoiseRemoveFilter(false);
}