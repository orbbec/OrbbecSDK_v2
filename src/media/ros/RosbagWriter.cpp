// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "RosbagWriter.hpp"

#include <cstdio>

namespace libobsensor {
const uint64_t INVALID_DIFF = 6ULL * 60ULL * 60ULL * 1000000ULL;  // 6 hours

RosWriter::RosWriter(const std::string &file, bool compressWhileRecord) : filePath_(file), startTime_(0), minFrameTime_(0), maxFrameTime_(0) {
    file_ = std::make_shared<rosbag::Bag>();
    file_->open(filePath_, rosbag::BagMode::Write);
    if(compressWhileRecord) {
        file_->setCompression(rosbag::CompressionType::LZ4);
    }
}

RosWriter::~RosWriter() {
    file_.reset();
    file_ = nullptr;

    if(maxFrameTime_ - minFrameTime_ >= INVALID_DIFF) {
        LOG_DEBUG("Error timestamp data frames during recording! maxFrametime: {}, minFrameTime: {}, diff: {}", maxFrameTime_, minFrameTime_, maxFrameTime_ - minFrameTime_);
        LOG_WARN("Error when saving rosbag file! There are abnormal timestamp data frames during recording!");
        std::string errFilePath = filePath_ + "_error";
        std::rename(filePath_.c_str(), errFilePath.c_str());
    }
}

void RosWriter::writeFrame(const OBSensorType &sensorType, std::shared_ptr<const Frame> curFrame) {
    std::lock_guard<std::mutex> lock(writeMutex_);
    auto curTime = curFrame->getTimeStampUsec();
    if (curTime == 0) {
        LOG_WARN("Invalid timestamp frame! curFrame device timestamp: {}", curFrame->getTimeStampUsec());
        return;
    }

    if (minFrameTime_ == 0 && maxFrameTime_ == 0) {
        minFrameTime_ = maxFrameTime_ = curTime;
    }
    else {
        minFrameTime_ = std::min(minFrameTime_, curTime);
        maxFrameTime_ = std::max(maxFrameTime_, curTime);
    }

    if(sensorType == OB_SENSOR_GYRO || sensorType == OB_SENSOR_ACCEL) {
        writeImuFrame(sensorType, curFrame);
    }
    else {
        writeVideoFrame(sensorType, curFrame);
    }
}
void RosWriter::writeImuFrame(const OBSensorType &sensorType, std::shared_ptr<const Frame> curFrame) {
    if(startTime_ == 0) {
        startTime_ = curFrame->getTimeStampUsec();
    }
    auto                                      imuTopic = RosTopic::imuDataTopic((uint8_t)sensorType, (uint8_t)curFrame->getType());
    std::chrono::duration<double, std::micro> timestampUs(curFrame->getTimeStampUsec());
    sensor_msgs::ImuPtr                       imuMsg(new sensor_msgs::Imu());
    imuMsg->header.stamp = orbbecRosbag::Time(std::chrono::duration<double>(timestampUs).count());
    streamProfileMap_.insert({ sensorType, curFrame->getStreamProfile() });
    if(sensorType == OB_SENSOR_ACCEL) {
        auto accelFrame               = curFrame->as<AccelFrame>();
        imuMsg->linear_acceleration.x = static_cast<double>(accelFrame->value().x);
        imuMsg->linear_acceleration.y = static_cast<double>(accelFrame->value().y);
        imuMsg->linear_acceleration.z = static_cast<double>(accelFrame->value().z);
        imuMsg->data.insert(imuMsg->data.begin(), accelFrame->getData(), accelFrame->getData() + accelFrame->getDataSize());
        imuMsg->datasize             = (uint32_t)accelFrame->getDataSize();
        imuMsg->number               = accelFrame->getNumber();
        imuMsg->temperature          = accelFrame->temperature();
        imuMsg->timestamp_usec       = accelFrame->getTimeStampUsec();
        imuMsg->timestamp_systemusec = accelFrame->getSystemTimeStampUsec();
        imuMsg->timestamp_globalusec = accelFrame->getGlobalTimeStampUsec();
        file_->write(imuTopic, imuMsg->header.stamp, imuMsg);
    }

    else {
        auto gyroFrame             = curFrame->as<GyroFrame>();
        imuMsg->angular_velocity.x = static_cast<double>(gyroFrame->value().x);
        imuMsg->angular_velocity.y = static_cast<double>(gyroFrame->value().y);
        imuMsg->angular_velocity.z = static_cast<double>(gyroFrame->value().z);
        imuMsg->data.insert(imuMsg->data.begin(), gyroFrame->getData(), gyroFrame->getData() + gyroFrame->getDataSize());
        imuMsg->datasize             = static_cast<uint32_t>(gyroFrame->getDataSize());
        imuMsg->number               = gyroFrame->getNumber();
        imuMsg->temperature          = gyroFrame->temperature();
        imuMsg->timestamp_usec       = gyroFrame->getTimeStampUsec();
        imuMsg->timestamp_systemusec = gyroFrame->getSystemTimeStampUsec();
        imuMsg->timestamp_globalusec = gyroFrame->getGlobalTimeStampUsec();
        file_->write(imuTopic, imuMsg->header.stamp, imuMsg);
    }
}

void RosWriter::writeVideoFrame(const OBSensorType &sensorType, std::shared_ptr<const Frame> curFrame) {
    if(startTime_ == 0) {
        startTime_ = curFrame->getTimeStampUsec();
    }
    std::chrono::duration<double, std::micro> timestampUs(curFrame->getTimeStampUsec());
    auto                                      imageTopic = RosTopic::frameDataTopic((uint8_t)sensorType, (uint8_t)curFrame->getType());
    streamProfileMap_.insert({ sensorType, curFrame->getStreamProfile() });
    try {
        sensor_msgs::ImagePtr imageMsg(new sensor_msgs::Image());
        imageMsg->header.stamp = orbbecRosbag::Time(std::chrono::duration<double>(timestampUs).count());

        imageMsg->width                = curFrame->as<VideoFrame>()->getWidth();
        imageMsg->height               = curFrame->as<VideoFrame>()->getHeight();
        imageMsg->number               = curFrame->getNumber();
        imageMsg->timestamp_usec       = curFrame->getTimeStampUsec();
        imageMsg->timestamp_systemusec = curFrame->getSystemTimeStampUsec();
        imageMsg->timestamp_globalusec = curFrame->getGlobalTimeStampUsec();
        imageMsg->step                 = curFrame->as<VideoFrame>()->getStride();
        imageMsg->metadatasize         = static_cast<uint32_t>(curFrame->getMetadataSize());
        imageMsg->data.clear();
        imageMsg->encoding = convertFormatToString(curFrame->getFormat());
        imageMsg->metadata.insert(imageMsg->metadata.begin(), curFrame->getMetadata(), curFrame->getMetadata() + curFrame->getMetadataSize());
        imageMsg->data.insert(imageMsg->data.begin(), curFrame->getData(), curFrame->getData() + curFrame->getDataSize());
        file_->write(imageTopic, imageMsg->header.stamp, imageMsg);
    }
    catch(const std::exception &e) {
        LOG_WARN("Write frame data exception! Message: {}", e.what());
    }
}

void RosWriter::writeProperty(uint32_t propertyID, const uint8_t *data, const uint32_t datasize) {
    std::lock_guard<std::mutex> lock(writeMutex_);
    auto                        now            = std::chrono::system_clock::now();
    auto                        timestamp_usec = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
    uint32_t                    sec            = timestamp_usec / 1000000;
    uint32_t                    nsec           = (timestamp_usec % 1000000) * 1000;
    auto                        propertyTopic  = RosTopic::propertyTopic();
    custom_msg::propertyPtr     propertyMsg(new custom_msg::property());
    propertyMsg->header.stamp = orbbecRosbag::Time(sec, nsec);
    propertyMsg->propertyId   = propertyID;
    propertyMsg->data.assign(data, data + datasize);
    propertyMsg->datasize = datasize;
    file_->write(propertyTopic, propertyMsg->header.stamp, propertyMsg);
}

void RosWriter::writeDeviceInfo(const std::shared_ptr<const DeviceInfo> &deviceInfo) {
    std::lock_guard<std::mutex> lock(writeMutex_);
    if(startTime_ == 0) {
        return;
    }
    custom_msg::DeviceInfoPtr                 deviceInfoMsg(new custom_msg::DeviceInfo());
    std::chrono::duration<double, std::micro> timestampUs(startTime_);
    deviceInfoMsg->header.stamp        = orbbecRosbag::Time(std::chrono::duration<double>(timestampUs).count());
    auto deviceInfoTopic               = RosTopic::deviceTopic();
    deviceInfoMsg->name                = deviceInfo->name_;
    deviceInfoMsg->fullName            = deviceInfo->fullName_;
    deviceInfoMsg->asicName            = deviceInfo->asicName_;
    deviceInfoMsg->vid                 = deviceInfo->vid_;
    deviceInfoMsg->pid                 = deviceInfo->pid_;
    deviceInfoMsg->uid                 = deviceInfo->uid_;
    deviceInfoMsg->sn                  = deviceInfo->deviceSn_;
    deviceInfoMsg->fwVersion           = deviceInfo->fwVersion_;
    deviceInfoMsg->hwVersion           = deviceInfo->hwVersion_;
    deviceInfoMsg->supportedSdkVersion = deviceInfo->supportedSdkVersion_;
    deviceInfoMsg->connectionType      = deviceInfo->connectionType_;
    deviceInfoMsg->type                = deviceInfo->type_;
    deviceInfoMsg->backendType         = deviceInfo->backendType_;

    if(deviceInfo->connectionType_ == "Ethernet") {
        auto netDeviceInfo       = std::static_pointer_cast<const NetDeviceInfo>(deviceInfo);
        deviceInfoMsg->ipAddress = netDeviceInfo->ipAddress_;
        deviceInfoMsg->localMac  = netDeviceInfo->localMac_;
    }

    file_->write(deviceInfoTopic, deviceInfoMsg->header.stamp, deviceInfoMsg);
}

void RosWriter::writeStreamProfiles() {
    std::lock_guard<std::mutex> lock(writeMutex_);
    for(auto &it: streamProfileMap_) {
        if(it.first == OB_SENSOR_GYRO) {
            writeGyroStreamProfile(it.second);
        }
        else if(it.first == OB_SENSOR_ACCEL) {
            writeAccelStreamProfile(it.second);
        }
        else {
            writeVideoStreamProfile(it.first, it.second);
        }
    }
}

void RosWriter::writeDisparityParam(std::shared_ptr<const DisparityBasedStreamProfile> disparityParam) {
    custom_msg::DisparityParamPtr             disparityParamMsg(new custom_msg::DisparityParam());
    std::chrono::duration<double, std::micro> timestampUs(startTime_);
    disparityParamMsg->header.stamp = orbbecRosbag::Time(std::chrono::duration<double>(timestampUs).count());
    auto disparityParamTopic        = RosTopic::disparityParmTopic();
    auto tmpDisparityParam          = disparityParam->getDisparityParam();
    disparityParamMsg->zpd          = tmpDisparityParam.zpd;
    disparityParamMsg->zpps         = tmpDisparityParam.zpps;
    disparityParamMsg->baseline     = tmpDisparityParam.baseline;
    disparityParamMsg->fx           = tmpDisparityParam.fx;
    disparityParamMsg->bitSize      = tmpDisparityParam.bitSize;
    disparityParamMsg->unit         = tmpDisparityParam.unit;
    disparityParamMsg->minDisparity = tmpDisparityParam.minDisparity;
    disparityParamMsg->packMode     = tmpDisparityParam.packMode;
    disparityParamMsg->dispOffset   = tmpDisparityParam.dispOffset;
    disparityParamMsg->invalidDisp  = tmpDisparityParam.invalidDisp;
    disparityParamMsg->dispIntPlace = tmpDisparityParam.dispIntPlace;
    disparityParamMsg->isDualCamera = tmpDisparityParam.isDualCamera;
    file_->write(disparityParamTopic, disparityParamMsg->header.stamp, disparityParamMsg);
}

void RosWriter::writeVideoStreamProfile(const OBSensorType sensorType, const std::shared_ptr<const StreamProfile> &streamProfile) {
    custom_msg::StreamProfileInfoPtr          streamInfoMsg(new custom_msg::StreamProfileInfo());
    std::chrono::duration<double, std::micro> timestampUs(startTime_);
    streamInfoMsg->header.stamp = orbbecRosbag::Time(std::chrono::duration<double>(timestampUs).count());
    auto strStreamProfile       = RosTopic::streamProfileTopic((uint8_t)streamProfile->getType());
    auto videoStreamProfile     = streamProfile->as<VideoStreamProfile>();
    if(videoStreamProfile != nullptr) {
        if(videoStreamProfile->is<const DisparityBasedStreamProfile>()) {
            writeDisparityParam(videoStreamProfile->as<const DisparityBasedStreamProfile>());
        }

        std::array<float, 8> distortion = { videoStreamProfile->getDistortion().k1, videoStreamProfile->getDistortion().k2,
                                            videoStreamProfile->getDistortion().k3, videoStreamProfile->getDistortion().k4,
                                            videoStreamProfile->getDistortion().k5, videoStreamProfile->getDistortion().k6,
                                            videoStreamProfile->getDistortion().p1, videoStreamProfile->getDistortion().p2 };
        streamInfoMsg->cameraDistortion = distortion;
        streamInfoMsg->distortionModel  = static_cast<uint8_t>(videoStreamProfile->getDistortion().model);

        streamInfoMsg->cameraIntrinsic[0] = videoStreamProfile->getIntrinsic().fx;
        streamInfoMsg->cameraIntrinsic[1] = videoStreamProfile->getIntrinsic().fy;
        streamInfoMsg->cameraIntrinsic[2] = videoStreamProfile->getIntrinsic().cx;
        streamInfoMsg->cameraIntrinsic[3] = videoStreamProfile->getIntrinsic().cy;
        streamInfoMsg->width              = videoStreamProfile->getWidth();
        streamInfoMsg->height             = videoStreamProfile->getHeight();
        streamInfoMsg->fps                = videoStreamProfile->getFps();
        streamInfoMsg->streamType         = static_cast<uint8_t>(videoStreamProfile->getType());
        streamInfoMsg->format             = static_cast<uint8_t>(videoStreamProfile->getFormat());

        // deal with extrinsic
        if(sensorType == OB_SENSOR_DEPTH && streamProfileMap_.count(OB_SENSOR_COLOR) && streamProfileMap_.count(OB_SENSOR_DEPTH)) {
            auto extrinsic = streamProfile->getExtrinsicTo(streamProfileMap_.at(OB_SENSOR_COLOR));
            for(int i = 0; i < 9; i++) {
                streamInfoMsg->rotationMatrix[i] = extrinsic.rot[i];
            }
            for(int i = 0; i < 3; i++) {
                streamInfoMsg->translationMatrix[i] = extrinsic.trans[i];
            }
        }

        file_->write(strStreamProfile, streamInfoMsg->header.stamp, streamInfoMsg);
    }
}

void RosWriter::writeAccelStreamProfile(const std::shared_ptr<const StreamProfile> &streamProfile) {
    custom_msg::ImuStreamProfileInfoPtr       accelStreamInfoMsg(new custom_msg::ImuStreamProfileInfo());
    std::chrono::duration<double, std::micro> timestampUs(startTime_);
    accelStreamInfoMsg->header.stamp = orbbecRosbag::Time(std::chrono::duration<double>(timestampUs).count());
    auto strStreamProfile            = RosTopic::streamProfileTopic((uint8_t)streamProfile->getType());
    auto accelStreamProfile          = streamProfile->as<AccelStreamProfile>();

    accelStreamInfoMsg->format              = static_cast<OBFormat>(accelStreamProfile->getFormat());
    accelStreamInfoMsg->streamType          = static_cast<OBStreamType>(accelStreamProfile->getType());
    accelStreamInfoMsg->accelFullScaleRange = static_cast<OBAccelFullScaleRange>(accelStreamProfile->getFullScaleRange());
    accelStreamInfoMsg->accelSampleRate     = static_cast<OBAccelSampleRate>(accelStreamProfile->getSampleRate());
    accelStreamInfoMsg->noiseDensity        = accelStreamProfile->getIntrinsic().noiseDensity;
    accelStreamInfoMsg->referenceTemp       = accelStreamProfile->getIntrinsic().randomWalk;
    accelStreamInfoMsg->referenceTemp       = accelStreamProfile->getIntrinsic().referenceTemp;
    for(int i = 0; i < 3; i++) {
        accelStreamInfoMsg->bias[i] = accelStreamProfile->getIntrinsic().bias[i];
    }
    for(int i = 0; i < 3; i++) {
        accelStreamInfoMsg->gravity[i] = accelStreamProfile->getIntrinsic().gravity[i];
    }
    for(int i = 0; i < 9; i++) {
        accelStreamInfoMsg->scaleMisalignment[i] = accelStreamProfile->getIntrinsic().scaleMisalignment[i];
    }
    for(int i = 0; i < 9; i++) {
        accelStreamInfoMsg->tempSlope[i] = accelStreamProfile->getIntrinsic().tempSlope[i];
    }
    OBExtrinsic extrinsic;
    if(streamProfileMap_.count(OB_SENSOR_DEPTH)) {
        extrinsic = streamProfile->getExtrinsicTo(streamProfileMap_.at(OB_SENSOR_DEPTH));
        for(int i = 0; i < 9; i++) {
            accelStreamInfoMsg->rotationMatrix[i] = extrinsic.rot[i];
        }

        for(int i = 0; i < 3; i++) {
            accelStreamInfoMsg->translationMatrix[i] = extrinsic.trans[i];
        }
    }
    file_->write(strStreamProfile, accelStreamInfoMsg->header.stamp, accelStreamInfoMsg);
}

void RosWriter::writeGyroStreamProfile(const std::shared_ptr<const StreamProfile> &streamProfile) {
    custom_msg::ImuStreamProfileInfoPtr       gyroStreamInfoMsg(new custom_msg::ImuStreamProfileInfo());
    std::chrono::duration<double, std::micro> timestampUs(startTime_);
    gyroStreamInfoMsg->header.stamp = orbbecRosbag::Time(std::chrono::duration<double>(timestampUs).count());
    auto strStreamProfile           = RosTopic::streamProfileTopic((uint8_t)streamProfile->getType());
    auto gyroStreamProfile          = streamProfile->as<GyroStreamProfile>();

    gyroStreamInfoMsg->format             = static_cast<OBFormat>(gyroStreamProfile->getFormat());
    gyroStreamInfoMsg->streamType         = static_cast<OBStreamType>(gyroStreamProfile->getType());
    gyroStreamInfoMsg->gyroFullScaleRange = static_cast<OBGyroFullScaleRange>(gyroStreamProfile->getFullScaleRange());
    gyroStreamInfoMsg->gyroSampleRate     = static_cast<OBGyroSampleRate>(gyroStreamProfile->getSampleRate());
    gyroStreamInfoMsg->noiseDensity       = gyroStreamProfile->getIntrinsic().noiseDensity;
    gyroStreamInfoMsg->referenceTemp      = gyroStreamProfile->getIntrinsic().randomWalk;
    gyroStreamInfoMsg->referenceTemp      = gyroStreamProfile->getIntrinsic().referenceTemp;
    for(int i = 0; i < 3; i++) {
        gyroStreamInfoMsg->bias[i] = gyroStreamProfile->getIntrinsic().bias[i];
    }
    for(int i = 0; i < 9; i++) {
        gyroStreamInfoMsg->scaleMisalignment[i] = gyroStreamProfile->getIntrinsic().scaleMisalignment[i];
    }
    for(int i = 0; i < 9; i++) {
        gyroStreamInfoMsg->tempSlope[i] = gyroStreamProfile->getIntrinsic().tempSlope[i];
    }
    file_->write(strStreamProfile, gyroStreamInfoMsg->header.stamp, gyroStreamInfoMsg);
}

}  // namespace libobsensor