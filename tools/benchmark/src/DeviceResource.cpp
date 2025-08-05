// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "DeviceResource.hpp"
#include <fstream>

DeviceResource::DeviceResource(std::shared_ptr<ob::Device> device) : device_(device), frames_(nullptr) {
    pipeline_           = std::make_shared<ob::Pipeline>(device_);
    spatial_filter_     = std::make_shared<ob::SpatialAdvancedFilter>();
    align_filter_       = std::make_shared<ob::Align>(OB_STREAM_COLOR);
    point_cloud_filter_ = std::make_shared<ob::PointCloudFilter>();

    resetConfig();
}

void DeviceResource::timestampDiffSaver() {
    std::string   sn            = device_->getDeviceInfo()->getSerialNumber();
    auto          rgbFileName   = sn + "_timestamp_difference_color.csv";
    auto          depthFileName = sn + "_timestamp_difference_depth.csv";
    std::ofstream rgbFile(rgbFileName);
    std::ofstream depthFile(depthFileName);

    timestamp_diff_saver_running_.store(true);
    if(!rgbFile.is_open()) {
        std::cout << "Error! Can not open file: " << rgbFileName << std::endl;
        timestamp_diff_saver_running_.store(false);
        return;
    }
    if(!depthFile.is_open()) {
        std::cout << "Error! Can not open file: " << depthFileName << std::endl;
        timestamp_diff_saver_running_.store(false);
        return;
    }

    // write title
    std::string title = "FrameType,Resolution,FrameRate,FrameFormat,FrameNumber,SystemTimestamp(us),GlobalTimestamp(us),DeviceTimestamp(us)"
                        ",Difference(System - Global),Difference(System - Device)";
    rgbFile << title << std::endl;
    depthFile << title << std::endl;

    do {
        std::queue<std::shared_ptr<ob::FrameSet>> queue;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            timestamp_diff_cv_.wait(lock, [this] { return !timestamp_diff_saver_running_ || !frameQueue_.empty(); });
            if(!timestamp_diff_saver_running_) {
                break;
            }
            if(!frameQueue_.empty()) {
                frameQueue_.swap(queue);
            }
        }
        // save timestamp difference to file
        while(!queue.empty()) {

            auto save = [](std::ofstream &file, std::shared_ptr<ob::Frame> frame) {
                auto type            = ob::TypeHelper::convertOBFrameTypeToString(frame->getType());
                auto sysTimestamp    = frame->getSystemTimeStampUs();
                auto globalTimestamp = frame->getGlobalTimeStampUs();
                auto timestamp       = frame->getTimeStampUs();
                auto profile         = frame->getStreamProfile()->as<ob::VideoStreamProfile>();
                auto format          = frame->getFormat();

                file << type << "," << profile->getWidth() << "x" << profile->getHeight() << "," << profile->getFps() << ","
                     << ob::TypeHelper::convertOBFormatTypeToString(format) << ",";
                if(frame->hasMetadata(OB_FRAME_METADATA_TYPE_FRAME_NUMBER)) {
                    auto metadataIndex = frame->getMetadataValue(OB_FRAME_METADATA_TYPE_FRAME_NUMBER);
                    file << metadataIndex << ",";
                }
                else {
                    file << "n/a" << ",";
                }
                file << sysTimestamp << "," << globalTimestamp << "," << timestamp << "," << static_cast<int64_t>(sysTimestamp - globalTimestamp) << ","
                     << static_cast<int64_t>(sysTimestamp - timestamp) << std::endl;
            };

            auto frameset = queue.front();

            queue.pop();

            auto colorFrame = frameset->getFrame(OB_FRAME_COLOR);
            if(colorFrame) {
                save(rgbFile, colorFrame);
            }
            auto depthFrame = frameset->getFrame(OB_FRAME_DEPTH);
            if(depthFrame) {
                save(depthFile, depthFrame);
            }
        }
    } while(1);
    timestamp_diff_saver_running_.store(false);
}

void DeviceResource::startStream(std::shared_ptr<ob::Config> config, bool enableTimestampDiffSaver) {
    // check and stop timestamp save thread
    if(timestamp_diff_saver_running_) {
        timestamp_diff_saver_running_ = false;
        timestamp_diff_cv_.notify_all();
    }
    if(timestamp_diff_saver_thread_.joinable()) {
        timestamp_diff_saver_thread_.join();
    }

    // check if enable timestamp saver
    if(enableTimestampDiffSaver) {
        timestamp_diff_saver_thread_ = std::thread(&DeviceResource::timestampDiffSaver, this);
        is_timestamp_saver_enabled_  = true;
    }
    else {
        is_timestamp_saver_enabled_ = false;
    }

    pipeline_->start(config, [this](std::shared_ptr<ob::FrameSet> frameset) {
        if(frameset == nullptr) {
            return;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        frames_ = frameset;

        if(is_timestamp_saver_enabled_ && timestamp_diff_saver_running_) {
            frameQueue_.push(frameset);
            timestamp_diff_cv_.notify_all();
        }

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

    timestamp_diff_saver_running_ = false;
    timestamp_diff_cv_.notify_all();
    if(timestamp_diff_saver_thread_.joinable()) {
        timestamp_diff_saver_thread_.join();
    }

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
        is_align_filter_enabled_       = false;
    }

    pipeline_->disableFrameSync();

    enableHwNoiseRemoveFilter(false);
    enableSwNoiseRemoveFilter(false);
}

void DeviceResource::syncTimeWithHost() {
    device_->timerSyncWithHost();
}
