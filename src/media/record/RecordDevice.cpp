// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "RecordDevice.hpp"
#include "DevicePids.hpp"
#include "frame/FrameFactory.hpp"
#include "DeviceBase.hpp"

namespace libobsensor {

RecordDevice::RecordDevice(std::shared_ptr<IDevice> device, const std::string &filePath, bool compressionsEnabled)
    : device_(device), filePath_(filePath), isCompressionsEnabled_(compressionsEnabled), maxFrameQueueSize_(100), isPaused_(false) {

    writer_ = std::make_shared<RosWriter>(filePath_, isCompressionsEnabled_);
    writeAllProperties();

    const auto &sensorTypeList = device_->getSensorTypeList();
    for(const auto &sensorType: sensorTypeList) {
        device_->getSensor(sensorType)->setFrameRecordingCallback([this](std::shared_ptr<const Frame> frame) { onFrameRecordingCallback(frame); });
        sensorOnceFlags_[sensorType] = std::unique_ptr<std::once_flag>(new std::once_flag());
    }
}

RecordDevice::~RecordDevice() {
    isPaused_                  = true;
    const auto &sensorTypeList = device_->getSensorTypeList();
    for(const auto &sensorType: sensorTypeList) {
        device_->getSensor(sensorType)->setFrameRecordingCallback(nullptr);
    }

    for(auto &item: frameQueueMap_) {
        while(!item.second->empty()) {
            std::this_thread::sleep_for(std::chrono::nanoseconds(100));
        }
    }

    writer_->writeDeviceInfo(std::dynamic_pointer_cast<DeviceBase>(device_)->getInfo());
    writer_->stop();
    LOG_DEBUG("RecordDevice Destructor");
}

void RecordDevice::onFrameRecordingCallback(std::shared_ptr<const Frame> frame) {
    if(isPaused_) {
        return;
    }

    auto sensorType = utils::mapFrameTypeToSensorType(frame->getType());
    initializeFrameQueueOnce(sensorType, frame);

    auto copy = FrameFactory::createFrameFromOtherFrame(frame, true);
    {
        std::unique_lock<std::mutex> lock(frameCallbackMutex_);
        if(frameQueueMap_.count(sensorType)) {
            frameQueueMap_[sensorType]->enqueue(copy);
        }
    }
}

void RecordDevice::pause() {
    LOG_DEBUG("pause record device");
    isPaused_ = true;
}

void RecordDevice::resume() {
    LOG_DEBUG("resume record device");
    isPaused_ = false;
}

void RecordDevice::initializeFrameQueueOnce(OBSensorType sensorType, std::shared_ptr<const Frame> frame) {
    // todo: change lazy initialization to eager initialization
    std::call_once(*sensorOnceFlags_[sensorType], [this, sensorType, frame]() {
        frameQueueMap_[sensorType] = std::make_shared<FrameQueue<const Frame>>(maxFrameQueueSize_);
        frameQueueMap_[sensorType]->start([this, sensorType](std::shared_ptr<const Frame> frame) { writer_->writeFrame(sensorType, frame); });
    });
}

void RecordDevice::writeAllProperties() {
    if(device_->getInfo()->backendType_ == OB_UVC_BACKEND_TYPE_V4L2
       && std::find(G330DevPids.begin(), G330DevPids.end(), device_->getInfo()->pid_) != G330DevPids.end()) {
        // color sensor property
        writePropertyT<bool>(OB_PROP_COLOR_AUTO_EXPOSURE_BOOL);
        writePropertyT<int>(OB_PROP_COLOR_AUTO_EXPOSURE_PRIORITY_INT);
        writePropertyT<bool>(OB_PROP_COLOR_AUTO_WHITE_BALANCE_BOOL);
        writePropertyT<int>(OB_PROP_COLOR_WHITE_BALANCE_INT);
        writePropertyT<int>(OB_PROP_COLOR_BRIGHTNESS_INT);
        writePropertyT<int>(OB_PROP_COLOR_CONTRAST_INT);
        writePropertyT<int>(OB_PROP_COLOR_SATURATION_INT);
        writePropertyT<int>(OB_PROP_COLOR_SHARPNESS_INT);
        writePropertyT<int>(OB_PROP_COLOR_BACKLIGHT_COMPENSATION_INT);
        writePropertyT<int>(OB_PROP_COLOR_HUE_INT);
        writePropertyT<int>(OB_PROP_COLOR_GAMMA_INT);
        writePropertyT<int>(OB_PROP_COLOR_POWER_LINE_FREQUENCY_INT);

        // depth sensor property
        writePropertyT<bool>(OB_PROP_DEPTH_AUTO_EXPOSURE_BOOL);
        writePropertyT<int>(OB_PROP_DEPTH_AUTO_EXPOSURE_PRIORITY_INT);

        // depth & color struct property
        auto server         = device_->getPropertyServer();
        auto depthAeRoi     = server->getStructureData(OB_STRUCT_DEPTH_AE_ROI, PROP_ACCESS_USER);
        auto colorAeRoi     = server->getStructureData(OB_STRUCT_COLOR_AE_ROI, PROP_ACCESS_USER);
        auto depthHdrConfig = server->getStructureData(OB_STRUCT_DEPTH_HDR_CONFIG, PROP_ACCESS_USER);
        auto deviceTime     = server->getStructureData(OB_STRUCT_DEVICE_TIME, PROP_ACCESS_INTERNAL);

        writer_->writeProperty(OB_STRUCT_DEPTH_AE_ROI, depthAeRoi.data(), static_cast<uint32_t>(depthAeRoi.size()));
        writer_->writeProperty(OB_STRUCT_COLOR_AE_ROI, colorAeRoi.data(), static_cast<uint32_t>(colorAeRoi.size()));
        writer_->writeProperty(OB_STRUCT_DEPTH_HDR_CONFIG, depthHdrConfig.data(), static_cast<uint32_t>(depthHdrConfig.size()));
        writer_->writeProperty(OB_STRUCT_DEVICE_TIME, deviceTime.data(), static_cast<uint32_t>(deviceTime.size()));
    }

    if(std::find(FemtoBoltDevPids.begin(), FemtoBoltDevPids.end(), device_->getInfo()->pid_) == FemtoBoltDevPids.end()
       && std::find(FemtoMegaDevPids.begin(), FemtoMegaDevPids.end(), device_->getInfo()->pid_) == FemtoMegaDevPids.end()) {
        // filter property
        writePropertyT<bool>(OB_PROP_DISPARITY_TO_DEPTH_BOOL);
        writePropertyT<bool>(OB_PROP_SDK_DISPARITY_TO_DEPTH_BOOL);
        writePropertyT<bool>(OB_PROP_DEPTH_NOISE_REMOVAL_FILTER_BOOL);
        writePropertyT<int>(OB_PROP_DEPTH_NOISE_REMOVAL_FILTER_MAX_SPECKLE_SIZE_INT);
        writePropertyT<int>(OB_PROP_DEPTH_NOISE_REMOVAL_FILTER_MAX_DIFF_INT);
    }
}

}  // namespace libobsensor
