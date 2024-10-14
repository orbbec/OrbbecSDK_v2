// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "StreamProfile.hpp"
#include "StreamExtrinsicsManager.hpp"
#include "StreamIntrinsicsManager.hpp"
#include "utils/PublicTypeHelper.hpp"

#include "frame/Frame.hpp"

namespace libobsensor {

StreamProfileBackendLifeSpan::StreamProfileBackendLifeSpan()
    : logger_(Logger::getInstance()), intrinsicsManager_(StreamIntrinsicsManager::getInstance()), extrinsicsManager_(StreamExtrinsicsManager::getInstance()) {}

StreamProfileBackendLifeSpan::~StreamProfileBackendLifeSpan() {
    intrinsicsManager_.reset();
    extrinsicsManager_.reset();
    logger_.reset();
}

StreamProfile::StreamProfile(std::shared_ptr<LazySensor> owner, OBStreamType type, OBFormat format) : owner_(owner), type_(type), format_(format), index_(0) {}

std::shared_ptr<LazySensor> StreamProfile::getOwner() const {
    return owner_.lock();
}

void StreamProfile::bindOwner(std::shared_ptr<LazySensor> owner) {
    owner_ = owner;
}

void StreamProfile::setType(OBStreamType type) {
    type_ = type;
}

OBStreamType StreamProfile::getType() const {
    return type_;
}

void StreamProfile::setFormat(OBFormat format) {
    format_ = format;
}

OBFormat StreamProfile::getFormat() const {
    return format_;
}

void StreamProfile::setIndex(uint8_t index) {
    index_ = index;
}

uint8_t StreamProfile::getIndex() const {
    return index_;
}

void StreamProfile::bindExtrinsicTo(std::shared_ptr<const StreamProfile> targetStreamProfile, const OBExtrinsic &extrinsic) {
    auto extrinsicsMgr = StreamExtrinsicsManager::getInstance();
    extrinsicsMgr->registerExtrinsics(shared_from_this(), targetStreamProfile, extrinsic);
}

void StreamProfile::bindSameExtrinsicTo(std::shared_ptr<const StreamProfile> targetStreamProfile) {
    auto extrinsicsMgr = StreamExtrinsicsManager::getInstance();
    extrinsicsMgr->registerSameExtrinsics(shared_from_this(), targetStreamProfile);
}

std::shared_ptr<StreamProfile> StreamProfile::clone() const{
    auto sp = std::make_shared<StreamProfile>(owner_.lock(), type_, format_);
    sp->bindSameExtrinsicTo(shared_from_this());
    return sp;
}

std::shared_ptr<StreamProfile> StreamProfile::clone(OBFormat newFormat) const {
    auto newSp = clone();
    newSp->setFormat(newFormat);
    return newSp;
}

OBExtrinsic StreamProfile::getExtrinsicTo(std::shared_ptr<const StreamProfile> targetStreamProfile) const {
    auto extrinsicsMgr = StreamExtrinsicsManager::getInstance();
    return extrinsicsMgr->getExtrinsics(shared_from_this(), targetStreamProfile);
}

std::ostream &StreamProfile::operator<<(std::ostream &os) const {
    os << "{" << "type: " << type_ << ", format: " << format_ << "}";
    return os;
}

VideoStreamProfile::VideoStreamProfile(std::shared_ptr<LazySensor> owner, OBStreamType type, OBFormat format, uint32_t width, uint32_t height, uint32_t fps)
    : StreamProfile(owner, type, format), width_(width), height_(height), fps_(fps) {}

void VideoStreamProfile::setWidth(uint32_t width) {
    width_ = width;
}

uint32_t VideoStreamProfile::getWidth() const {
    return width_;
}

void VideoStreamProfile::setHeight(uint32_t height) {
    height_ = height;
}

uint32_t VideoStreamProfile::getHeight() const {
    return height_;
}

uint32_t VideoStreamProfile::getFps() const {
    return fps_;
}

OBCameraIntrinsic VideoStreamProfile::getIntrinsic() const {
    auto intrinsicsMgr = StreamIntrinsicsManager::getInstance();
    return intrinsicsMgr->getVideoStreamIntrinsics(shared_from_this());
}

void VideoStreamProfile::bindIntrinsic(const OBCameraIntrinsic &intrinsic) {
    auto intrinsicsMgr = StreamIntrinsicsManager::getInstance();
    intrinsicsMgr->registerVideoStreamIntrinsics(shared_from_this(), intrinsic);
}

OBCameraDistortion VideoStreamProfile::getDistortion() const {
    auto intrinsicsMgr = StreamIntrinsicsManager::getInstance();
    return intrinsicsMgr->getVideoStreamDistortion(shared_from_this());
}

void VideoStreamProfile::bindDistortion(const OBCameraDistortion &distortion) {
    auto intrinsicsMgr = StreamIntrinsicsManager::getInstance();
    intrinsicsMgr->registerVideoStreamDistortion(shared_from_this(), distortion);
}

uint32_t VideoStreamProfile::getMaxFrameDataSize() const {
    return utils::calcVideoFrameMaxDataSize(format_, width_, height_);
}

std::shared_ptr<StreamProfile> VideoStreamProfile::clone() const {
    auto sp = std::make_shared<VideoStreamProfile>(owner_.lock(), type_, format_, width_, height_, fps_);
    auto intrinsicsMgr = StreamIntrinsicsManager::getInstance();
    if(intrinsicsMgr->containsVideoStreamIntrinsics(shared_from_this())) {
        auto intrinsic = intrinsicsMgr->getVideoStreamIntrinsics(shared_from_this());
        intrinsicsMgr->registerVideoStreamIntrinsics(sp, intrinsic);
    }
    if(intrinsicsMgr->containsVideoStreamDistortion(shared_from_this())) {
        auto distortion = intrinsicsMgr->getVideoStreamDistortion(shared_from_this());
        intrinsicsMgr->registerVideoStreamDistortion(sp, distortion);
    }
    sp->bindSameExtrinsicTo(shared_from_this());
    return sp;
}

DisparityBasedStreamProfile::DisparityBasedStreamProfile(std::shared_ptr<LazySensor> owner, OBStreamType type, OBFormat format, uint32_t width, uint32_t height,
                                                         uint32_t fps)
    : VideoStreamProfile(owner, type, format, width, height, fps) {}

OBDisparityParam DisparityBasedStreamProfile::getDisparityParam() const {
    auto intrinsicsMgr = StreamIntrinsicsManager::getInstance();
    return intrinsicsMgr->getDisparityBasedStreamDisparityParam(shared_from_this());
}

void DisparityBasedStreamProfile::bindDisparityParam(const OBDisparityParam &param) {
    auto intrinsicsMgr = StreamIntrinsicsManager::getInstance();
    intrinsicsMgr->registerDisparityBasedStreamDisparityParam(shared_from_this(), param);
}

std::shared_ptr<StreamProfile> DisparityBasedStreamProfile::clone() const {
    auto sp = std::make_shared<DisparityBasedStreamProfile>(owner_.lock(), type_, format_, width_, height_, fps_);
    auto intrinsicsMgr = StreamIntrinsicsManager::getInstance();
    if(intrinsicsMgr->containsVideoStreamIntrinsics(shared_from_this())) {
        auto intrinsic = intrinsicsMgr->getVideoStreamIntrinsics(shared_from_this());
        intrinsicsMgr->registerVideoStreamIntrinsics(sp, intrinsic);
    }
    if(intrinsicsMgr->containsVideoStreamDistortion(shared_from_this())) {
        auto distortion = intrinsicsMgr->getVideoStreamDistortion(shared_from_this());
        intrinsicsMgr->registerVideoStreamDistortion(sp, distortion);
    }
    if(intrinsicsMgr->containsDisparityBasedStreamDisparityParam(shared_from_this())) {
        auto disparityParam = intrinsicsMgr->getDisparityBasedStreamDisparityParam(shared_from_this());
        intrinsicsMgr->registerDisparityBasedStreamDisparityParam(sp, disparityParam);
    }
    sp->bindSameExtrinsicTo(shared_from_this());
    return sp;
}

AccelStreamProfile::AccelStreamProfile(std::shared_ptr<LazySensor> owner, OBAccelFullScaleRange fullScaleRange, OBAccelSampleRate sampleRate)
    : StreamProfile{ owner, OB_STREAM_ACCEL, OB_FORMAT_ACCEL }, fullScaleRange_(fullScaleRange), sampleRate_(sampleRate) {}
bool VideoStreamProfile::operator==(const VideoStreamProfile &other) const {
    return (type_ == other.type_) && (format_ == other.format_) && (width_ == other.width_) && (height_ == other.height_) && (fps_ == other.fps_);
}

std::ostream &VideoStreamProfile::operator<<(std::ostream &os) const {
    os << "{" << "type: " << type_ << ", format: " << format_ << ", width: " << width_ << ", height: " << height_ << ", fps: " << fps_ << "}";
    return os;
}

OBAccelFullScaleRange AccelStreamProfile::getFullScaleRange() const {
    return fullScaleRange_;
}

OBAccelSampleRate AccelStreamProfile::getSampleRate() const {
    return sampleRate_;
}

void AccelStreamProfile::bindIntrinsic(const OBAccelIntrinsic &intrinsic) {
    StreamIntrinsicsManager::getInstance()->registerAccelStreamIntrinsics(shared_from_this(), intrinsic);
}

OBAccelIntrinsic AccelStreamProfile::getIntrinsic() const {
    return StreamIntrinsicsManager::getInstance()->getAccelStreamIntrinsics(shared_from_this());
}

std::shared_ptr<StreamProfile> AccelStreamProfile::clone() const {
    auto sp = std::make_shared<AccelStreamProfile>(owner_.lock(), fullScaleRange_, sampleRate_);
    auto intrinsicsMgr = StreamIntrinsicsManager::getInstance();
    if(intrinsicsMgr->containsAccelStreamIntrinsics(shared_from_this())) {
        auto intrinsic = intrinsicsMgr->getAccelStreamIntrinsics(shared_from_this());
        intrinsicsMgr->registerAccelStreamIntrinsics(sp, intrinsic);
    }
    sp->bindSameExtrinsicTo(shared_from_this());
    return sp;
}

std::ostream &AccelStreamProfile::operator<<(std::ostream &os) const {
    os << "{" << "type: " << type_ << ", format: " << format_ << ", fullScaleRange: " << fullScaleRange_ << ", sampleRate: " << sampleRate_ << "}";
    return os;
}

GyroStreamProfile::GyroStreamProfile(std::shared_ptr<LazySensor> owner, OBGyroFullScaleRange fullScaleRange, OBGyroSampleRate sampleRate)
    : StreamProfile{ owner, OB_STREAM_GYRO, OB_FORMAT_GYRO }, fullScaleRange_(fullScaleRange), sampleRate_(sampleRate) {}

OBGyroFullScaleRange GyroStreamProfile::getFullScaleRange() const {
    return fullScaleRange_;
}

OBGyroSampleRate GyroStreamProfile::getSampleRate() const {
    return sampleRate_;
}

void GyroStreamProfile::bindIntrinsic(const OBGyroIntrinsic &intrinsic) {
    auto intrinsicsMgr = StreamIntrinsicsManager::getInstance();
    intrinsicsMgr->registerGyroStreamIntrinsics(shared_from_this(), intrinsic);
}

OBGyroIntrinsic GyroStreamProfile::getIntrinsic() const {
    auto intrinsicsMgr = StreamIntrinsicsManager::getInstance();
    return intrinsicsMgr->getGyroStreamIntrinsics(shared_from_this());
}

std::shared_ptr<StreamProfile> GyroStreamProfile::clone() const {
    auto sp = std::make_shared<GyroStreamProfile>(owner_.lock(), fullScaleRange_, sampleRate_);
    auto intrinsicsMgr = StreamIntrinsicsManager::getInstance();
    if(intrinsicsMgr->containsGyroStreamIntrinsics(shared_from_this())) {
        auto intrinsic = intrinsicsMgr->getGyroStreamIntrinsics(shared_from_this());
        intrinsicsMgr->registerGyroStreamIntrinsics(sp, intrinsic);
    }
    sp->bindSameExtrinsicTo(shared_from_this());
    return sp;
}

std::ostream &GyroStreamProfile::operator<<(std::ostream &os) const {
    os << "{" << "type: " << type_ << ", format: " << format_ << ", fullScaleRange: " << fullScaleRange_ << ", sampleRate: " << sampleRate_ << "}";
    return os;
}

std::ostream &operator<<(std::ostream &os, const std::shared_ptr<const StreamProfile> &streamProfile) {
    return streamProfile->operator<<(os);
}

std::vector<std::shared_ptr<const VideoStreamProfile>> matchVideoStreamProfile(const StreamProfileList &profileList, uint32_t width, uint32_t height,
                                                                               uint32_t fps, OBFormat format) {
    std::vector<std::shared_ptr<const VideoStreamProfile>> matchProfileList;
    for(auto profile: profileList) {
        if(profile->is<VideoStreamProfile>()) {
            auto videoProfile = profile->as<VideoStreamProfile>();

            // Get the profile that matches the user's items of interest
            if((width == OB_WIDTH_ANY || videoProfile->getWidth() == width) && (height == OB_HEIGHT_ANY || videoProfile->getHeight() == height)
               && (format == OB_FORMAT_ANY || videoProfile->getFormat() == format) && (fps == OB_FPS_ANY || videoProfile->getFps() == fps)) {
                matchProfileList.push_back(videoProfile);
            }
        }
    }
    return matchProfileList;
}

std::vector<std::shared_ptr<const AccelStreamProfile>> matchAccelStreamProfile(const StreamProfileList &profileList, OBAccelFullScaleRange fullScaleRange,
                                                                               OBAccelSampleRate sampleRate) {
    std::vector<std::shared_ptr<const AccelStreamProfile>> matchProfileList;
    for(auto profile: profileList) {
        if(profile->is<AccelStreamProfile>()) {
            auto AccelProfile = profile->as<AccelStreamProfile>();

            // Get the profile that matches the user's items of interest
            if((fullScaleRange == OB_ACCEL_FULL_SCALE_RANGE_ANY || AccelProfile->getFullScaleRange() == fullScaleRange)
               && (sampleRate == OB_ACCEL_SAMPLE_RATE_ANY || AccelProfile->getSampleRate() == sampleRate)) {
                matchProfileList.push_back(AccelProfile);
            }
        }
    }
    return matchProfileList;
}

std::vector<std::shared_ptr<const GyroStreamProfile>> matchGyroStreamProfile(const StreamProfileList &profileList, OBGyroFullScaleRange fullScaleRange,
                                                                             OBGyroSampleRate sampleRate) {
    std::vector<std::shared_ptr<const GyroStreamProfile>> matchProfileList;
    for(auto profile: profileList) {
        if(profile->is<GyroStreamProfile>()) {
            auto GyroProfile = profile->as<GyroStreamProfile>();

            // Get the profile that matches the user's items of interest
            if((fullScaleRange == OB_GYRO_FULL_SCALE_RANGE_ANY || GyroProfile->getFullScaleRange() == fullScaleRange)
               && (sampleRate == OB_GYRO_SAMPLE_RATE_ANY || GyroProfile->getSampleRate() == sampleRate)) {
                matchProfileList.push_back(GyroProfile);
            }
        }
    }
    return matchProfileList;
}

}  // namespace libobsensor
