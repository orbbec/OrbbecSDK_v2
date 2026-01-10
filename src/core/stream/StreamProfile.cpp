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

void StreamProfile::bindExtrinsicTo(const OBStreamType &type, const OBExtrinsic &extrinsic) {
    auto extrinsicsMgr = StreamExtrinsicsManager::getInstance();
    extrinsicsMgr->registerExtrinsics(shared_from_this(), type, extrinsic);
}

void StreamProfile::bindSameExtrinsicTo(std::shared_ptr<const StreamProfile> targetStreamProfile) {
    auto extrinsicsMgr = StreamExtrinsicsManager::getInstance();
    extrinsicsMgr->registerSameExtrinsics(shared_from_this(), targetStreamProfile);
}

std::shared_ptr<StreamProfile> StreamProfile::clone() const {
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

VideoStreamProfile::VideoStreamProfile(std::shared_ptr<LazySensor> owner, OBStreamType type, OBFormat format, OBHardwareDecimationConfig decimationConfig,
                                       uint32_t fps)
    : StreamProfile(owner, type, format), width_(0), height_(0), decimationConfig_(decimationConfig), fps_(fps) {}
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

void VideoStreamProfile::setDecimationConfig(OBHardwareDecimationConfig decimationConfig) {
    decimationConfig_ = decimationConfig;
}

OBHardwareDecimationConfig VideoStreamProfile::getDecimationConfig() const {
    return decimationConfig_;
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
    auto sp            = std::make_shared<VideoStreamProfile>(owner_.lock(), type_, format_, width_, height_, fps_);
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
    auto sp            = std::make_shared<DisparityBasedStreamProfile>(owner_.lock(), type_, format_, width_, height_, fps_);
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
    auto sp            = std::make_shared<AccelStreamProfile>(owner_.lock(), fullScaleRange_, sampleRate_);
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
    auto sp            = std::make_shared<GyroStreamProfile>(owner_.lock(), fullScaleRange_, sampleRate_);
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

LiDARStreamProfile::LiDARStreamProfile(std::shared_ptr<LazySensor> owner, OBLiDARScanRate scanRate, OBFormat format)
    : StreamProfile{ owner, OB_STREAM_LIDAR, format }, scanRate_(scanRate) {}

LiDARProfileInfo LiDARStreamProfile::getInfo() const {
    LiDARProfileInfo info;

    // TODO hard-coded for LiDAR device
    // frame type
    info.frameType = utils::mapStreamTypeToFrameType(getType());
    // format
    info.format = getFormat();
    if(info.format == OB_FORMAT_LIDAR_SCAN) {
        // For single-lines LiDAR device
        switch(getScanRate()) {
        case OB_LIDAR_SCAN_15HZ: {
            info.scanSpeed       = 900;
            info.maxDataBlockNum = 18;
            info.pointsNum       = 200;
            info.dataBlockSize   = 844;
        } break;
        case OB_LIDAR_SCAN_20HZ: {
            info.scanSpeed       = 1200;
            info.maxDataBlockNum = 18;
            info.pointsNum       = 150;
            info.dataBlockSize   = 644;
        } break;
        case OB_LIDAR_SCAN_25HZ: {
            info.scanSpeed       = 1500;
            info.maxDataBlockNum = 18;
            info.pointsNum       = 120;
            info.dataBlockSize   = 524;
        } break;
        case OB_LIDAR_SCAN_30HZ: {
            info.scanSpeed       = 1800;
            info.maxDataBlockNum = 18;
            info.pointsNum       = 100;
            info.dataBlockSize   = 444;
        } break;
        case OB_LIDAR_SCAN_40HZ: {
            info.scanSpeed       = 2400;
            info.maxDataBlockNum = 18;
            info.pointsNum       = 75;
            info.dataBlockSize   = 344;
        } break;
        default:
            throw invalid_value_exception("Invalid LiDAR scan rate");
            break;
        }
        info.frameSize = info.pointsNum * info.maxDataBlockNum * sizeof(OBLiDARScanPoint);
    }
    else {
        // For multi-lines LiDAR device
        struct OBLiDARScanRateHash {
            size_t operator()(OBLiDARScanRate v) const noexcept {
                typedef typename std::underlying_type<OBLiDARScanRate>::type UT;
                return std::hash<UT>()(static_cast<UT>(v));
            }
        };

        // speed and max data block number
        using LiDARInfoMap = std::unordered_map<OBLiDARScanRate, std::pair<uint32_t, uint32_t>, OBLiDARScanRateHash>;
        // std::pair: first scan speed; second: data block num for a circle
        static LiDARInfoMap mapScanRate = {
            { OB_LIDAR_SCAN_5HZ, { 300, 240 } },
            { OB_LIDAR_SCAN_10HZ, { 600, 120 } },
            { OB_LIDAR_SCAN_15HZ, { 900, 80 } },
            { OB_LIDAR_SCAN_20HZ, { 1200, 60 } },
        };
        // std::pair: first scan speed; second: data block num for a circle
        static LiDARInfoMap mapScanRateCalibration = {
            { OB_LIDAR_SCAN_5HZ, { 300, 1200 } },
            { OB_LIDAR_SCAN_10HZ, { 600, 600 } },
            { OB_LIDAR_SCAN_15HZ, { 900, 400 } },
            { OB_LIDAR_SCAN_20HZ, { 1200, 300 } },
        };

        uint32_t      dataSizePerBlock = 0;
        LiDARInfoMap *infoMap          = nullptr;
        if(info.format == OB_FORMAT_LIDAR_CALIBRATION) {
            // calibration data
            info.dataBlockSize = 944;
            info.pointsNum     = 25;
            // Point data in the block was copied directly to the frame without any conversion
            dataSizePerBlock = info.dataBlockSize - 40 - 4;  // header and tai magic
            infoMap          = &mapScanRateCalibration;
        }
        else if(info.format == OB_FORMAT_LIDAR_POINT) {
            // LiDAR point
            info.pointsNum     = 125;
            info.dataBlockSize = 1044;
            // Point data in the block was converted to OBLiDARPoint and then save to the frame
            dataSizePerBlock = info.pointsNum * sizeof(OBLiDARPoint);
            infoMap          = &mapScanRate;
        }
        else if(info.format == OB_FORMAT_LIDAR_SPHERE_POINT) {
            // LiDAR sphere point
            info.pointsNum     = 125;
            info.dataBlockSize = 1044;
            // Point data in the block was converted to OBLiDARSpherePoint and then save to the frame
            dataSizePerBlock = info.pointsNum * sizeof(OBLiDARSpherePoint);
            infoMap          = &mapScanRate;
        }
        else {
            info.clear();
            throw invalid_value_exception("Invalid LiDAR format");
        }

        auto iter = infoMap->find(scanRate_);
        if(iter == infoMap->end()) {
            throw invalid_value_exception("Invalid LiDAR scan rate");
        }
        info.scanSpeed       = (*iter).second.first;
        info.maxDataBlockNum = (*iter).second.second;
        info.frameSize       = dataSizePerBlock * info.maxDataBlockNum;
    }

    return info;
}

OBLiDARScanRate LiDARStreamProfile::getScanRate() const {
    return scanRate_;
}

std::shared_ptr<StreamProfile> LiDARStreamProfile::clone() const {
    auto sp = std::make_shared<LiDARStreamProfile>(owner_.lock(), scanRate_, format_);
    return sp;
}

std::ostream &LiDARStreamProfile::operator<<(std::ostream &os) const {
    os << "{"
       << "type: " << type_ << ", format: " << format_ << ", scanRate: " << scanRate_ << "}";
    return os;
}

std::ostream &operator<<(std::ostream &os, const std::shared_ptr<const StreamProfile> &streamProfile) {
    return streamProfile->operator<<(os);
}

std::vector<std::shared_ptr<const VideoStreamProfile>> matchVideoStreamProfile(const StreamProfileList &profileList, uint32_t width, uint32_t height,
                                                                               uint32_t fps, OBFormat format, OBHardwareDecimationConfig decimationConfig) {
    std::vector<std::shared_ptr<const VideoStreamProfile>> matchProfileList;
    auto                                                   decimationScale = decimationConfig.decimationFactor;

    for(auto profile: profileList) {
        if(profile->is<VideoStreamProfile>()) {
            auto videoProfile = profile->as<VideoStreamProfile>();

            // Get the profile that matches the user's items of interest

            if(decimationScale == 0) {
                if((width == OB_WIDTH_ANY || videoProfile->getWidth() == width) && (height == OB_HEIGHT_ANY || videoProfile->getHeight() == height)
                   && (format == OB_FORMAT_ANY || videoProfile->getFormat() == format) && (fps == OB_FPS_ANY || videoProfile->getFps() == fps)) {
                    matchProfileList.push_back(videoProfile);
                }
            }
            else {
                auto profileDecimationConfig = videoProfile->getDecimationConfig();
                if(decimationConfig.originWidth == profileDecimationConfig.originWidth && decimationConfig.originHeight == profileDecimationConfig.originHeight
                   && decimationScale == profileDecimationConfig.decimationFactor && videoProfile->getFormat() == format && videoProfile->getFps() == fps) {
                    matchProfileList.push_back(videoProfile);
                }
            }
        }
    }
    return matchProfileList;
}

std::vector<std::shared_ptr<const VideoStreamProfile>> matchVideoStreamProfile(const StreamProfileList   &profileList,
                                                                               OBHardwareDecimationConfig decimationConfig, uint32_t fps, OBFormat format) {
    std::vector<std::shared_ptr<const VideoStreamProfile>> matchProfileList;

    for(auto profile: profileList) {
        if(profile->is<VideoStreamProfile>()) {
            auto videoProfile            = profile->as<VideoStreamProfile>();
            auto profileDecimationConfig = videoProfile->getDecimationConfig();
            if((format == OB_FORMAT_ANY || videoProfile->getFormat() == format) && (fps == OB_FPS_ANY || videoProfile->getFps() == fps)
               && decimationConfig.originWidth == profileDecimationConfig.originWidth && decimationConfig.originHeight == profileDecimationConfig.originHeight
               && decimationConfig.decimationFactor == profileDecimationConfig.decimationFactor) {
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

std::vector<std::shared_ptr<const LiDARStreamProfile>> matchLiDARStreamProfile(const StreamProfileList &profileList, OBLiDARScanRate scanRate,
                                                                               OBFormat format) {
    std::vector<std::shared_ptr<const LiDARStreamProfile>> matchProfileList;

    for(auto profile: profileList) {
        if(profile->is<LiDARStreamProfile>()) {
            auto lidarProfile = profile->as<LiDARStreamProfile>();

            // Get the profile that matches the user's items of interest
            // format
            if((lidarProfile->getFormat() != format) && (format != OB_FORMAT_ANY)) {
                continue;
            }
            // rate
            if((lidarProfile->getScanRate() != scanRate) && (scanRate != OB_LIDAR_SCAN_ANY)) {
                continue;
            }
            matchProfileList.push_back(lidarProfile);
        }
    }
    return matchProfileList;
}

}  // namespace libobsensor
