// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "RecordDevice.hpp"
#include "DevicePids.hpp"
#include "frame/FrameFactory.hpp"
#include "DeviceBase.hpp"
#include "IAlgParamManager.hpp"
#include "property/InternalProperty.hpp"
#include "utils/MediaUtils.hpp"

namespace libobsensor {

RecordDevice::RecordDevice(std::shared_ptr<IDevice> device, const std::string &filePath, bool compressionsEnabled)
    : device_(device), filePath_(filePath), isCompressionsEnabled_(compressionsEnabled), maxFrameQueueSize_(UINT16_MAX), isPaused_(false) {

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

    // stop recorder and write device&frame info
    stopRecord();
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
        std::unique_lock<std::mutex> lock(frameQueueInitMutex_);
        frameQueueMap_[sensorType] = std::make_shared<FrameQueue<const Frame>>(maxFrameQueueSize_);
        frameQueueMap_[sensorType]->start([this, sensorType](std::shared_ptr<const Frame> frame) { writer_->writeFrame(sensorType, frame); });
    });
}

void RecordDevice::writeAllProperties() {
    writeVersionProperty();
    writeFilterProperty();
    writeFrameGeometryProperty();
    writeMetadataProperty();
    writeExposureAndGainProperty();
    writeCalibrationParamProperty();
    writeDepthWorkModeProperty();
}

void RecordDevice::stopRecord() {
    writer_->writeDeviceInfo(std::dynamic_pointer_cast<DeviceBase>(device_)->getInfo());
    writer_->writeStreamProfiles();
}

void RecordDevice::writeVersionProperty() {
    double               version = utils::getBagFileVersion();
    std::vector<uint8_t> data(sizeof(version));
    data.assign(reinterpret_cast<uint8_t *>(&version), reinterpret_cast<uint8_t *>(&version) + sizeof(version));
    writer_->writeProperty(versionPropertyId_, data.data(), static_cast<uint32_t>(data.size()));
}

void RecordDevice::writeFilterProperty() {
    if(std::find(FemtoBoltDevPids.begin(), FemtoBoltDevPids.end(), device_->getInfo()->pid_) == FemtoBoltDevPids.end()
       && std::find(FemtoMegaDevPids.begin(), FemtoMegaDevPids.end(), device_->getInfo()->pid_) == FemtoMegaDevPids.end()) {
        // filter property
        writePropertyT<bool>(OB_PROP_DISPARITY_TO_DEPTH_BOOL);
        writePropertyT<bool>(OB_PROP_SDK_DISPARITY_TO_DEPTH_BOOL);
        writePropertyT<bool>(OB_PROP_DEPTH_NOISE_REMOVAL_FILTER_BOOL);
        writePropertyT<int>(OB_PROP_DEPTH_NOISE_REMOVAL_FILTER_MAX_SPECKLE_SIZE_INT);
        writePropertyT<int>(OB_PROP_DEPTH_NOISE_REMOVAL_FILTER_MAX_DIFF_INT);
    }

    writePropertyT<bool>(OB_PROP_HW_NOISE_REMOVE_FILTER_ENABLE_BOOL);
    writePropertyT<float>(OB_PROP_HW_NOISE_REMOVE_FILTER_THRESHOLD_FLOAT);
    writePropertyT<bool>(OB_PROP_DEPTH_ALIGN_HARDWARE_BOOL);
}

void RecordDevice::writeFrameGeometryProperty() {
    writePropertyT<bool>(OB_PROP_COLOR_FLIP_BOOL);
    writePropertyT<bool>(OB_PROP_COLOR_MIRROR_BOOL);
    writePropertyT<int>(OB_PROP_COLOR_ROTATE_INT);

    writePropertyT<bool>(OB_PROP_DEPTH_FLIP_BOOL);
    writePropertyT<bool>(OB_PROP_DEPTH_MIRROR_BOOL);
    writePropertyT<int>(OB_PROP_DEPTH_ROTATE_INT);

    writePropertyT<bool>(OB_PROP_IR_FLIP_BOOL);
    writePropertyT<bool>(OB_PROP_IR_MIRROR_BOOL);
    writePropertyT<int>(OB_PROP_IR_ROTATE_INT);

    writePropertyT<bool>(OB_PROP_IR_RIGHT_FLIP_BOOL);
    writePropertyT<bool>(OB_PROP_IR_RIGHT_MIRROR_BOOL);
    writePropertyT<int>(OB_PROP_IR_RIGHT_ROTATE_INT);
}

void RecordDevice::writeMetadataProperty() {
    if(device_->getInfo()->backendType_ == OB_UVC_BACKEND_TYPE_V4L2
       && std::find(G330DevPids.begin(), G330DevPids.end(), device_->getInfo()->pid_) != G330DevPids.end()) {
        // color sensor property
        // writePropertyT<bool>(OB_PROP_COLOR_AUTO_EXPOSURE_BOOL);
        // writePropertyT<int>(OB_PROP_COLOR_AUTO_EXPOSURE_PRIORITY_INT);
        // writePropertyT<bool>(OB_PROP_COLOR_AUTO_WHITE_BALANCE_BOOL);
        // writePropertyT<int>(OB_PROP_COLOR_WHITE_BALANCE_INT);
        // writePropertyT<int>(OB_PROP_COLOR_BRIGHTNESS_INT);
        // writePropertyT<int>(OB_PROP_COLOR_CONTRAST_INT);
        // writePropertyT<int>(OB_PROP_COLOR_SATURATION_INT);
        // writePropertyT<int>(OB_PROP_COLOR_SHARPNESS_INT);
        writePropertyT<int>(OB_PROP_COLOR_BACKLIGHT_COMPENSATION_INT);
        // writePropertyT<int>(OB_PROP_COLOR_HUE_INT);
        // writePropertyT<int>(OB_PROP_COLOR_GAMMA_INT);
        // writePropertyT<int>(OB_PROP_COLOR_POWER_LINE_FREQUENCY_INT);

        // depth sensor property
        // writePropertyT<bool>(OB_PROP_DEPTH_AUTO_EXPOSURE_BOOL);
        writePropertyT<int>(OB_PROP_DEPTH_AUTO_EXPOSURE_PRIORITY_INT);

        // depth & color struct property
        std::vector<std::pair<OBPropertyID, PropertyAccessType>> propertyList = { { OB_STRUCT_DEPTH_AE_ROI, PROP_ACCESS_USER },
                                                                                  { OB_STRUCT_COLOR_AE_ROI, PROP_ACCESS_USER },
                                                                                  { OB_STRUCT_DEPTH_HDR_CONFIG, PROP_ACCESS_USER },
                                                                                  { OB_STRUCT_DEVICE_TIME, PROP_ACCESS_INTERNAL } };

        auto server = device_->getPropertyServer();
        for(const auto &property: propertyList) {
            if(server->isPropertySupported(property.first, PROP_OP_READ, property.second)) {
                auto data = server->getStructureData(property.first, property.second);
                writer_->writeProperty(property.first, data.data(), static_cast<uint32_t>(data.size()));
            }
        }
    }
    writePropertyT<int>(OB_PROP_DISP_SEARCH_RANGE_MODE_INT);
}

void RecordDevice::writeExposureAndGainProperty() {
    // depth property
    writePropertyT<bool>(OB_PROP_DEPTH_AUTO_EXPOSURE_BOOL);
    writePropertyT<int>(OB_PROP_DEPTH_EXPOSURE_INT);
    writePropertyT<int>(OB_PROP_DEPTH_GAIN_INT);

    // color property
    writePropertyT<bool>(OB_PROP_COLOR_AUTO_EXPOSURE_BOOL);
    writePropertyT<bool>(OB_PROP_COLOR_AUTO_WHITE_BALANCE_BOOL);
    writePropertyT<int>(OB_PROP_COLOR_POWER_LINE_FREQUENCY_INT);
    writePropertyT<int>(OB_PROP_COLOR_AE_MAX_EXPOSURE_INT);
    writePropertyT<int>(OB_PROP_COLOR_AUTO_EXPOSURE_PRIORITY_INT);
    writePropertyT<int>(OB_PROP_COLOR_EXPOSURE_INT);
    writePropertyT<int>(OB_PROP_COLOR_GAIN_INT);
    writePropertyT<int>(OB_PROP_COLOR_WHITE_BALANCE_INT);
    writePropertyT<int>(OB_PROP_COLOR_BRIGHTNESS_INT);
    writePropertyT<int>(OB_PROP_COLOR_SHARPNESS_INT);
    writePropertyT<int>(OB_PROP_COLOR_SATURATION_INT);
    writePropertyT<int>(OB_PROP_COLOR_CONTRAST_INT);
    writePropertyT<int>(OB_PROP_COLOR_GAMMA_INT);
    writePropertyT<int>(OB_PROP_COLOR_HUE_INT);

    // ir property
    writePropertyT<bool>(OB_PROP_IR_AUTO_EXPOSURE_BOOL);
    writePropertyT<int>(OB_PROP_IR_AE_MAX_EXPOSURE_INT);
    writePropertyT<int>(OB_PROP_IR_BRIGHTNESS_INT);
    writePropertyT<int>(OB_PROP_IR_EXPOSURE_INT);
    writePropertyT<int>(OB_PROP_IR_GAIN_INT);
}

void RecordDevice::writeCalibrationParamProperty() {
    // d2c profile list
    auto algParamManager = device_->getComponentT<IAlgParamManager>(OB_DEV_COMPONENT_ALG_PARAM_MANAGER, false);
    if(algParamManager) {
        auto d2cProfileList = algParamManager->getD2CProfileList();
        writer_->writeProperty(OB_RAW_DATA_D2C_ALIGN_SUPPORT_PROFILE_LIST, reinterpret_cast<uint8_t *>(d2cProfileList.data()),
                               static_cast<uint32_t>(d2cProfileList.size() * sizeof(OBD2CProfile)));

        // calibration param list
        auto calibrationList = algParamManager->getCalibrationCameraParamList();
        writer_->writeProperty(OB_RAW_DATA_ALIGN_CALIB_PARAM, reinterpret_cast<uint8_t *>(calibrationList.data()),
                               static_cast<uint32_t>(calibrationList.size() * sizeof(OBCameraParam)));

        auto imuCalibrationParam = algParamManager->getIMUCalibrationParam();
        writer_->writeProperty(OB_RAW_DATA_IMU_CALIB_PARAM, reinterpret_cast<uint8_t *>(&imuCalibrationParam), sizeof(OBIMUCalibrateParams));
    }
}

void RecordDevice::writeDepthWorkModeProperty() {
    // depth work mode
    auto propertyServer = device_->getComponentT<IPropertyServer>(OB_DEV_COMPONENT_PROPERTY_SERVER, false);
    if(propertyServer && propertyServer->isPropertySupported(OB_STRUCT_CURRENT_DEPTH_ALG_MODE, PROP_OP_READ, PROP_ACCESS_INTERNAL)) {
        auto depthMode = propertyServer->getStructureDataProtoV1_1_T<OBDepthWorkMode_Internal, 0>(OB_STRUCT_CURRENT_DEPTH_ALG_MODE);
        writer_->writeProperty(OB_STRUCT_CURRENT_DEPTH_ALG_MODE, reinterpret_cast<uint8_t *>(&depthMode), sizeof(OBDepthWorkMode_Internal));
    }
}

}  // namespace libobsensor
