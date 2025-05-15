// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "RosbagReader.hpp"

namespace libobsensor {
const uint64_t INVALID_DURATION = 6ULL * 60ULL * 60ULL * 1000000ULL;  // 6 hours
RosReader::RosReader(const std::string &filePath) : filePath_(filePath), totalDuration_(0), unit_(0.0), baseline_(0.0) {
    initView();
    queryDeviceInfo();
    querySreamProfileList();
    queryProperty();
    bindStreamProfileExtrinsic();
}
void RosReader::initView() try {
    file_.open(filePath_, rosbag::BagMode::Read);
    sensorView_            = std::unique_ptr<rosbag::View>(new rosbag::View(file_, FrameQuery()));
    sensorIterator_        = sensorView_->begin();
    startTime_             = sensorView_->getBeginTime();
    auto streamingDuration = sensorView_->getEndTime() - sensorView_->getBeginTime();
    for(rosbag::ConnectionInfo const *info: sensorView_->getConnections()) {
        enabledStreamsTopics_.push_back(info->topic);
    }
    totalDuration_ = std::chrono::nanoseconds(streamingDuration.toNSec());
    if(static_cast<uint64_t>(totalDuration_.count() / 1000) >= INVALID_DURATION) {
        throw io_exception("The streaming duration is too long, please check the rosbag file.");
    }
}
catch(const rosbag::BagException &e) {
    throw io_exception(e.what());
}

std::chrono::nanoseconds RosReader::getDuration() {
    return totalDuration_;
}
void RosReader::queryDeviceInfo() {
    rosbag::View           view(file_, DeviceInfoQuery());
    rosbag::View::iterator iter;
    for(iter = view.begin(); iter != view.end(); ++iter) {
        if(iter->isType<custom_msg::DeviceInfo>()) {
            custom_msg::DeviceInfoPtr deviceInfoMsg = iter->instantiate<custom_msg::DeviceInfo>();
            if(deviceInfoMsg->connectionType == "Ethernet") {
                auto netDeviceInfo        = std::make_shared<NetDeviceInfo>();
                netDeviceInfo->ipAddress_ = deviceInfoMsg->ipAddress;
                netDeviceInfo->localMac_  = deviceInfoMsg->localMac;
                deviceInfo_               = netDeviceInfo;
            }
            else {
                deviceInfo_ = std::make_shared<DeviceInfo>();
            }
            deviceInfo_->name_                = deviceInfoMsg->name;
            deviceInfo_->fullName_            = deviceInfoMsg->fullName + "(Playback)";
            deviceInfo_->asicName_            = deviceInfoMsg->asicName;
            deviceInfo_->vid_                 = deviceInfoMsg->vid;
            deviceInfo_->pid_                 = deviceInfoMsg->pid;
            deviceInfo_->uid_                 = deviceInfoMsg->uid;
            deviceInfo_->deviceSn_            = deviceInfoMsg->sn;
            deviceInfo_->fwVersion_           = deviceInfoMsg->fwVersion;
            deviceInfo_->hwVersion_           = deviceInfoMsg->hwVersion;
            deviceInfo_->supportedSdkVersion_ = deviceInfoMsg->supportedSdkVersion;
            deviceInfo_->connectionType_      = deviceInfoMsg->connectionType;
            deviceInfo_->type_                = deviceInfoMsg->type;
            deviceInfo_->backendType_         = static_cast<OBUvcBackendType>(deviceInfoMsg->backendType);
        }
    }
}

std::shared_ptr<StreamProfile> RosReader::getStreamProfile(OBStreamType streamType) {
    if(streamProfileList_.count(streamType) == 0) {
        return nullptr;
    }
    return streamProfileList_.at(streamType);
}

void RosReader::queryProperty() {
    rosbag::View           view(file_, PropertyQuery());
    rosbag::View::iterator iter;
    for(iter = view.begin(); iter != view.end(); ++iter) {
        rosbag::MessageInstance        msg         = *iter;
        custom_msg::property::ConstPtr propertyPtr = msg.instantiate<custom_msg::property>();
        propertyList_[propertyPtr->propertyId]     = propertyPtr->data;
    }
}

std::vector<uint8_t> RosReader::getPropertyData(uint32_t propertyId) {
    if(propertyList_.count(propertyId)) {
        return propertyList_[propertyId];
    }
    return {};
}

void RosReader::querySreamProfileList() {
    rosbag::View           view(file_, StreamProfileQuery());
    rosbag::View::iterator iter;
    for(iter = view.begin(); iter != view.end(); ++iter) {
        rosbag::MessageInstance msg = *iter;
        if(RosTopic::getStreamProfilePrefix(msg.getTopic()) == OB_STREAM_ACCEL) {
            custom_msg::ImuStreamProfileInfoPtr accelStreamProfileInfoPtr = msg.instantiate<custom_msg::ImuStreamProfileInfo>();
            std::shared_ptr<AccelStreamProfile> accelStreamProfile =
                std::make_shared<AccelStreamProfile>(nullptr, static_cast<OBAccelFullScaleRange>(accelStreamProfileInfoPtr->accelFullScaleRange),
                                                     static_cast<OBAccelSampleRate>(accelStreamProfileInfoPtr->accelSampleRate));
            double bias[3];
            memcpy(&bias, &accelStreamProfileInfoPtr->bias, sizeof(bias));
            double gravity[3];
            memcpy(&gravity, &accelStreamProfileInfoPtr->gravity, sizeof(gravity));
            double scaleMisalignment[9];
            memcpy(&scaleMisalignment, &accelStreamProfileInfoPtr->scaleMisalignment, sizeof(scaleMisalignment));
            double tempSlope[9];
            memcpy(&tempSlope, &accelStreamProfileInfoPtr->tempSlope, sizeof(tempSlope));
            OBAccelIntrinsic accelIntrinsic = { accelStreamProfileInfoPtr->noiseDensity,
                                                accelStreamProfileInfoPtr->randomWalk,
                                                accelStreamProfileInfoPtr->referenceTemp,
                                                *bias,
                                                *gravity,
                                                *scaleMisalignment,
                                                *tempSlope };
            accelStreamProfile->bindIntrinsic(accelIntrinsic);
            streamProfileList_.insert({ static_cast<OBStreamType>(accelStreamProfileInfoPtr->streamType), accelStreamProfile });
        }
        else if(RosTopic::getStreamProfilePrefix(msg.getTopic()) == OB_STREAM_GYRO) {
            custom_msg::ImuStreamProfileInfoPtr gyroStreamProfileInfoPtr = msg.instantiate<custom_msg::ImuStreamProfileInfo>();
            std::shared_ptr<GyroStreamProfile>  gyroStreamProfile =
                std::make_shared<GyroStreamProfile>(nullptr, static_cast<OBGyroFullScaleRange>(gyroStreamProfileInfoPtr->gyroFullScaleRange),
                                                    static_cast<OBGyroSampleRate>(gyroStreamProfileInfoPtr->gyroSampleRate));
            double bias[3];
            memcpy(&bias, &gyroStreamProfileInfoPtr->bias, sizeof(bias));
            double scaleMisalignment[9];
            memcpy(&scaleMisalignment, &gyroStreamProfileInfoPtr->scaleMisalignment, sizeof(scaleMisalignment));
            double tempSlope[9];
            memcpy(&tempSlope, &gyroStreamProfileInfoPtr->tempSlope, sizeof(tempSlope));
            OBGyroIntrinsic gyroIntrinsicIntrinsic = { gyroStreamProfileInfoPtr->noiseDensity,
                                                       gyroStreamProfileInfoPtr->randomWalk,
                                                       gyroStreamProfileInfoPtr->referenceTemp,
                                                       *bias,
                                                       *scaleMisalignment,
                                                       *tempSlope };
            gyroStreamProfile->bindIntrinsic(gyroIntrinsicIntrinsic);
            streamProfileList_.insert({ static_cast<OBStreamType>(gyroStreamProfileInfoPtr->streamType), gyroStreamProfile });
        }
        else if(RosTopic::getStreamProfilePrefix(msg.getTopic()) == OB_STREAM_DEPTH) {
            custom_msg::StreamProfileInfoPtr             streamProfileInfoPtr = msg.instantiate<custom_msg::StreamProfileInfo>();
            std::shared_ptr<DisparityBasedStreamProfile> depthStreamProfile   = std::make_shared<DisparityBasedStreamProfile>(
                nullptr, static_cast<OBStreamType>(streamProfileInfoPtr->streamType), static_cast<OBFormat>(streamProfileInfoPtr->format),
                static_cast<uint32_t>(streamProfileInfoPtr->width), static_cast<uint32_t>(streamProfileInfoPtr->height),
                static_cast<uint32_t>(streamProfileInfoPtr->fps));

            OBCameraIntrinsic intrinsic = { streamProfileInfoPtr->cameraIntrinsic[0],          streamProfileInfoPtr->cameraIntrinsic[1],
                                            streamProfileInfoPtr->cameraIntrinsic[2],          streamProfileInfoPtr->cameraIntrinsic[3],
                                            static_cast<int16_t>(streamProfileInfoPtr->width), static_cast<int16_t>(streamProfileInfoPtr->height) };
            depthStreamProfile->bindIntrinsic(intrinsic);

            OBCameraDistortion distortion = {
                streamProfileInfoPtr->cameraDistortion[0],
                streamProfileInfoPtr->cameraDistortion[1],
                streamProfileInfoPtr->cameraDistortion[2],
                streamProfileInfoPtr->cameraDistortion[3],
                streamProfileInfoPtr->cameraDistortion[4],
                streamProfileInfoPtr->cameraDistortion[5],
                streamProfileInfoPtr->cameraDistortion[6],
                streamProfileInfoPtr->cameraDistortion[7],
                static_cast<OBCameraDistortionModel>(streamProfileInfoPtr->distortionModel),
            };
            depthStreamProfile->bindDistortion(distortion);
            streamProfileList_.insert({ static_cast<OBStreamType>(streamProfileInfoPtr->streamType), depthStreamProfile });
        }
        else {
            custom_msg::StreamProfileInfoPtr    streamProfileInfoPtr = msg.instantiate<custom_msg::StreamProfileInfo>();
            std::shared_ptr<VideoStreamProfile> videoStreamProfile =
                std::make_shared<VideoStreamProfile>(nullptr, static_cast<OBStreamType>(streamProfileInfoPtr->streamType),
                                                     static_cast<OBFormat>(streamProfileInfoPtr->format), static_cast<uint32_t>(streamProfileInfoPtr->width),
                                                     static_cast<uint32_t>(streamProfileInfoPtr->height), static_cast<uint32_t>(streamProfileInfoPtr->fps));

            OBCameraIntrinsic intrinsic = { streamProfileInfoPtr->cameraIntrinsic[0],          streamProfileInfoPtr->cameraIntrinsic[1],
                                            streamProfileInfoPtr->cameraIntrinsic[2],          streamProfileInfoPtr->cameraIntrinsic[3],
                                            static_cast<int16_t>(streamProfileInfoPtr->width), static_cast<int16_t>(streamProfileInfoPtr->height) };
            videoStreamProfile->bindIntrinsic(intrinsic);

            OBCameraDistortion distortion = {
                streamProfileInfoPtr->cameraDistortion[0],
                streamProfileInfoPtr->cameraDistortion[1],
                streamProfileInfoPtr->cameraDistortion[2],
                streamProfileInfoPtr->cameraDistortion[3],
                streamProfileInfoPtr->cameraDistortion[4],
                streamProfileInfoPtr->cameraDistortion[5],
                streamProfileInfoPtr->cameraDistortion[6],
                streamProfileInfoPtr->cameraDistortion[7],
                static_cast<OBCameraDistortionModel>(streamProfileInfoPtr->distortionModel),
            };
            videoStreamProfile->bindDistortion(distortion);
            streamProfileList_.insert({ static_cast<OBStreamType>(streamProfileInfoPtr->streamType), videoStreamProfile });
        }
    }

    rosbag::View disparityParamView(file_, DisparityParamQuery());
    if(disparityParamView.size() > 0) {
        rosbag::MessageInstance       msg                    = *disparityParamView.begin();
        custom_msg::DisparityParamPtr disparityParamInfoPtr  = msg.instantiate<custom_msg::DisparityParam>();
        auto                          depthStreamProfileItem = streamProfileList_.find(OB_STREAM_DEPTH);
        if(depthStreamProfileItem != streamProfileList_.end()) {
            if(depthStreamProfileItem->second != nullptr) {
                auto disparity                  = depthStreamProfileItem->second->as<VideoStreamProfile>()->as<DisparityBasedStreamProfile>();
                unit_                           = disparityParamInfoPtr->unit;
                baseline_                       = disparityParamInfoPtr->baseline;
                OBDisparityParam disparityParam = { disparityParamInfoPtr->zpd,          disparityParamInfoPtr->zpps,
                                                    disparityParamInfoPtr->baseline,     disparityParamInfoPtr->fx,
                                                    disparityParamInfoPtr->bitSize,      disparityParamInfoPtr->unit,
                                                    disparityParamInfoPtr->minDisparity, disparityParamInfoPtr->packMode,
                                                    disparityParamInfoPtr->dispOffset,   disparityParamInfoPtr->invalidDisp,
                                                    disparityParamInfoPtr->dispIntPlace, disparityParamInfoPtr->isDualCamera };
                disparity->bindDisparityParam(disparityParam);
            }
        }
    }
}

void RosReader::bindStreamProfileExtrinsic() {
    OBExtrinsic  identityExtrinsicsTmp = { { 1, 0, 0, 0, 1, 0, 0, 0, 1 }, { 0, 0, 0 } };
    rosbag::View depthSpView(file_, StreamProfileQuery(OB_STREAM_DEPTH));
    if(depthSpView.size() > 0) {
        rosbag::MessageInstance          msg                  = *depthSpView.begin();
        custom_msg::StreamProfileInfoPtr streamProfileInfoPtr = msg.instantiate<custom_msg::StreamProfileInfo>();
        OBExtrinsic                      d2cExtrinsic;
        memcpy(&d2cExtrinsic.rot, &streamProfileInfoPtr->rotationMatrix, sizeof(d2cExtrinsic.rot));
        memcpy(&d2cExtrinsic.trans, &streamProfileInfoPtr->translationMatrix, sizeof(d2cExtrinsic.trans));
        auto colorStreamProfileItem   = streamProfileList_.find(OB_STREAM_COLOR);
        auto depthStreamProfileItem   = streamProfileList_.find(OB_STREAM_DEPTH);
        auto irStreamProfileItem      = streamProfileList_.find(OB_STREAM_IR);
        auto irLeftStreamProfileItem  = streamProfileList_.find(OB_STREAM_IR_LEFT);
        auto irRightStreamProfileItem = streamProfileList_.find(OB_STREAM_IR_RIGHT);

        if(colorStreamProfileItem != streamProfileList_.end() && depthStreamProfileItem != streamProfileList_.end()) {
            depthStreamProfileItem->second->bindExtrinsicTo(colorStreamProfileItem->second, d2cExtrinsic);
        }

        if(irStreamProfileItem != streamProfileList_.end() && depthStreamProfileItem != streamProfileList_.end()) {
            irStreamProfileItem->second->bindSameExtrinsicTo(depthStreamProfileItem->second);
        }
        if(irLeftStreamProfileItem != streamProfileList_.end() && depthStreamProfileItem != streamProfileList_.end()) {
            irLeftStreamProfileItem->second->bindSameExtrinsicTo(depthStreamProfileItem->second);
        }

        if(irLeftStreamProfileItem != streamProfileList_.end() && irRightStreamProfileItem != streamProfileList_.end() && baseline_ != 0 && unit_ != 0) {
            auto leftToRight     = identityExtrinsicsTmp;
            leftToRight.trans[0] = -1.0f * baseline_ * unit_;
            irLeftStreamProfileItem->second->bindExtrinsicTo(irRightStreamProfileItem->second, leftToRight);
        }
    }

    rosbag::View accelSpView(file_, StreamProfileQuery(OB_STREAM_ACCEL));
    if(accelSpView.size() > 0) {
        rosbag::MessageInstance             msg                     = *accelSpView.begin();
        custom_msg::ImuStreamProfileInfoPtr imuStreamProfileInfoPtr = msg.instantiate<custom_msg::ImuStreamProfileInfo>();
        OBExtrinsic                         extrinsic;
        memcpy(&extrinsic.rot, &imuStreamProfileInfoPtr->rotationMatrix, sizeof(extrinsic.rot));
        memcpy(&extrinsic.trans, &imuStreamProfileInfoPtr->translationMatrix, sizeof(extrinsic.trans));
        auto depthStreamProfileItem = streamProfileList_.find(OB_STREAM_DEPTH);
        auto accelStreamProfileItem = streamProfileList_.find(OB_STREAM_ACCEL);
        auto gyrotStreamProfileItem = streamProfileList_.find(OB_STREAM_GYRO);

        if(accelStreamProfileItem != streamProfileList_.end() && depthStreamProfileItem != streamProfileList_.end()) {
            accelStreamProfileItem->second->bindExtrinsicTo(depthStreamProfileItem->second, extrinsic);
            if(gyrotStreamProfileItem != streamProfileList_.end()) {
                gyrotStreamProfileItem->second->bindSameExtrinsicTo(accelStreamProfileItem->second);
            }
        }
    }
}

std::shared_ptr<DeviceInfo> RosReader::getDeviceInfo() {
    return deviceInfo_;
}

std::vector<OBSensorType> RosReader::getSensorTypeList() const {
    std::vector<OBSensorType> sensorTypesList;
    for(auto &item: streamProfileList_) {
        sensorTypesList.push_back(utils::mapStreamTypeToSensorType(item.first));
    }
    return sensorTypesList;
}

void RosReader::seekToTime(const std::chrono::nanoseconds &seekTime) {
    std::lock_guard<std::mutex> lock(readMutex_);
    if(seekTime > totalDuration_) {
        throw invalid_value_exception("Seek time is greater than total duration");
    }
    auto seekTimeAsSecs    = std::chrono::duration_cast<std::chrono::duration<double>>(seekTime);
    auto seekTimeAsRostime = orbbecRosbag::Time(seekTimeAsSecs.count() + startTime_.toSec());
    sensorView_.reset(new rosbag::View(file_, FalseQuery()));
    for(auto topic: enabledStreamsTopics_) {
        sensorView_->addQuery(file_, rosbag::TopicQuery(topic), seekTimeAsRostime);
    }
    sensorIterator_ = sensorView_->begin();
}

void RosReader::stop() {
    std::lock_guard<std::mutex> lock(readMutex_);

    sensorView_.reset(new rosbag::View(file_, FalseQuery()));
    for(auto topic: enabledStreamsTopics_) {
        sensorView_->addQuery(file_, rosbag::TopicQuery(topic));
    }
    sensorIterator_ = sensorView_->begin();
}

bool RosReader::getIsEndOfFile() {
    std::lock_guard<std::mutex> lock(readMutex_);
    return sensorIterator_ == sensorView_->end();
}

std::shared_ptr<Frame> RosReader::readNextData() {
    std::lock_guard<std::mutex> lock(readMutex_);
    if(!sensorView_) {
        sensorView_ = std::unique_ptr<rosbag::View>(new rosbag::View(file_, FalseQuery()));
    }
    if(sensorIterator_ == sensorView_->end()) {
        LOG_DEBUG("End of file reached");
    }

    rosbag::MessageInstance nextMsg = *sensorIterator_;
    ++sensorIterator_;

    if(nextMsg.isType<sensor_msgs::Image>()) {
        return createVideoFrame(nextMsg);
    }
    else if(nextMsg.isType<sensor_msgs::Imu>()) {
        return createImuFrame(nextMsg);
    }

    return nullptr;
}

std::shared_ptr<Frame> RosReader::createImuFrame(const rosbag::MessageInstance &msg) {
    auto                       imuMsgTopic = msg.getTopic();
    sensor_msgs::Imu::ConstPtr imuPtr      = msg.instantiate<sensor_msgs::Imu>();
    std::shared_ptr<Frame>     frame;
    if(imuPtr->linear_acceleration.x != 0 && imuPtr->linear_acceleration.y != 0 && imuPtr->linear_acceleration.z != 0) {
        frame = FrameFactory::createFrameFromUserBuffer(OB_FRAME_ACCEL, OB_FORMAT_ACCEL, const_cast<uint8_t *>(imuPtr->data.data()),
                                                        static_cast<size_t>(imuPtr->datasize));
    }
    else {
        frame = FrameFactory::createFrameFromUserBuffer(OB_FRAME_GYRO, OB_FORMAT_GYRO, const_cast<uint8_t *>(imuPtr->data.data()),
                                                        static_cast<size_t>(imuPtr->datasize));
    }
    frame->setNumber(imuPtr->number);
    frame->setTimeStampUsec(imuPtr->timestamp_usec);
    frame->setSystemTimeStampUsec(imuPtr->timestamp_systemusec);
    frame->setGlobalTimeStampUsec(imuPtr->timestamp_globalusec);
    if(streamProfileList_.count(utils::mapFrameTypeToStreamType(RosTopic::getFrameTypeIdentifier(imuMsgTopic)))) {
        frame->setStreamProfile(streamProfileList_[utils::mapFrameTypeToStreamType(RosTopic::getFrameTypeIdentifier(imuMsgTopic))]);
    }
    return frame;
}

std::shared_ptr<Frame> RosReader::createVideoFrame(const rosbag::MessageInstance &msg) {
    auto                         videoMsgTopic = msg.getTopic();
    sensor_msgs::Image::ConstPtr imagePtr      = msg.instantiate<sensor_msgs::Image>();
    auto                         frame         = libobsensor::FrameFactory::createVideoFrameFromUserBuffer(
        RosTopic::getFrameTypeIdentifier(videoMsgTopic), convertStringToFormat(imagePtr->encoding), imagePtr->width, imagePtr->height,
        (uint8_t *)imagePtr->data.data(), imagePtr->data.size());

    frame->updateMetadata(imagePtr->metadata.data(), imagePtr->metadatasize);
    frame->setNumber(imagePtr->number);
    frame->setTimeStampUsec(imagePtr->timestamp_usec);
    frame->setSystemTimeStampUsec(imagePtr->timestamp_systemusec);
    frame->setGlobalTimeStampUsec(imagePtr->timestamp_globalusec);
    if(streamProfileList_.count(utils::mapFrameTypeToStreamType(RosTopic::getFrameTypeIdentifier(videoMsgTopic)))) {
        frame->setStreamProfile(streamProfileList_[utils::mapFrameTypeToStreamType(RosTopic::getFrameTypeIdentifier(videoMsgTopic))]);
    }
    return frame;
}

std::chrono::nanoseconds RosReader::getCurTime() {
    std::lock_guard<std::mutex> lock(readMutex_);
    if(sensorIterator_ == sensorView_->end()) {
        return getDuration();
    }
    return static_cast<std::chrono::nanoseconds>(sensorIterator_->getTime().toNSec() - startTime_.toNSec());
}

bool RosReader::isPropertySupported(uint32_t propertyId) const {
    return propertyList_.count(propertyId);
}

}  // namespace libobsensor