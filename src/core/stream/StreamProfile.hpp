// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "IStreamProfile.hpp"
#include "libobsensor/h/ObTypes.h"
#include "exception/ObException.hpp"
#include <memory>
#include <vector>

namespace libobsensor {

class Logger;
class StreamIntrinsicsManager;
class StreamExtrinsicsManager;
struct LazySensor;

class StreamProfileBackendLifeSpan {
public:
    StreamProfileBackendLifeSpan();
    ~StreamProfileBackendLifeSpan();

private:
    std::shared_ptr<Logger>                      logger_;
    std::shared_ptr<StreamIntrinsicsManager>     intrinsicsManager_;
    std::shared_ptr<StreamExtrinsicsManager>     extrinsicsManager_;
};

class StreamProfile : public std::enable_shared_from_this<StreamProfile>, private StreamProfileBackendLifeSpan {
public:
    StreamProfile(std::shared_ptr<LazySensor> owner, OBStreamType type, OBFormat format);

    virtual ~StreamProfile() noexcept = default;

    std::shared_ptr<LazySensor> getOwner() const;

    void         bindOwner(std::shared_ptr<LazySensor> owner);
    void         setType(OBStreamType type);
    OBStreamType getType() const;
    void         setFormat(OBFormat format);
    OBFormat     getFormat() const;
    void         setIndex(uint8_t index);
    uint8_t      getIndex() const;

    OBExtrinsic getExtrinsicTo(std::shared_ptr<const StreamProfile> targetStreamProfile) const;
    void        bindExtrinsicTo(std::shared_ptr<const StreamProfile> targetStreamProfile, const OBExtrinsic &extrinsic);
    void        bindSameExtrinsicTo(std::shared_ptr<const StreamProfile> targetStreamProfile);

    virtual std::shared_ptr<StreamProfile> clone() const;
    virtual std::shared_ptr<StreamProfile> clone(OBFormat newFormat) const;

    template <typename T> bool               is() const {
        return std::dynamic_pointer_cast<const T>(shared_from_this()) != nullptr;
    }

    template <typename T> bool               is() {
        return std::dynamic_pointer_cast<T>(shared_from_this()) != nullptr;
    }

    template <typename T> std::shared_ptr<T> as() {
        if(!is<T>()) {
            throw unsupported_operation_exception("unsupported operation, object's type is not require type");
        }

        return std::dynamic_pointer_cast<T>(shared_from_this());
    }

    template <typename T> std::shared_ptr<const T> as() const {
        if(!is<const T>()) {
            throw unsupported_operation_exception("unsupported operation, object's type is not require type");
        }

        return std::dynamic_pointer_cast<const T>(shared_from_this());
    }

    virtual std::ostream &operator<<(std::ostream &os) const;

protected:
    std::weak_ptr<LazySensor> owner_;
    OBStreamType              type_;
    OBFormat                  format_;
    uint8_t                   index_;  // for multi-stream sensor (multi pin uvc device)
};
class VideoStreamProfile : public StreamProfile {
public:
    VideoStreamProfile(std::shared_ptr<LazySensor> owner, OBStreamType type, OBFormat format, uint32_t width, uint32_t height, uint32_t fps);
    VideoStreamProfile(std::shared_ptr<const VideoStreamProfile> other) = delete;

    ~VideoStreamProfile() noexcept override = default;

    void               setWidth(uint32_t width);
    uint32_t           getWidth() const;
    void               setHeight(uint32_t height);
    uint32_t           getHeight() const;
    uint32_t           getFps() const;
    OBCameraIntrinsic  getIntrinsic() const;
    void               bindIntrinsic(const OBCameraIntrinsic &intrinsic);
    OBCameraDistortion getDistortion() const;
    void               bindDistortion(const OBCameraDistortion &distortion);
    uint32_t           getMaxFrameDataSize() const;

    std::shared_ptr<StreamProfile> clone() const override;
    bool                           operator==(const VideoStreamProfile &other) const;
    std::ostream                  &operator<<(std::ostream &os) const override;

protected:
    uint32_t width_;
    uint32_t height_;
    uint32_t fps_;
};

class DisparityBasedStreamProfile : public VideoStreamProfile {
public:
    DisparityBasedStreamProfile(std::shared_ptr<LazySensor> owner, OBStreamType type, OBFormat format, uint32_t width, uint32_t height, uint32_t fps);
    ~DisparityBasedStreamProfile() noexcept override = default;

    OBDisparityParam getDisparityParam() const;
    void             bindDisparityParam(const OBDisparityParam &param);

    std::shared_ptr<StreamProfile> clone() const override;
};

class AccelStreamProfile : public StreamProfile {
public:
    AccelStreamProfile(std::shared_ptr<LazySensor> owner, OBAccelFullScaleRange fullScaleRange, OBAccelSampleRate sampleRate);
    ~AccelStreamProfile() noexcept override = default;

    OBAccelFullScaleRange          getFullScaleRange() const;
    OBAccelSampleRate              getSampleRate() const;
    void                           bindIntrinsic(const OBAccelIntrinsic &intrinsic);
    OBAccelIntrinsic               getIntrinsic() const;
    std::shared_ptr<StreamProfile> clone() const override;
    std::ostream                  &operator<<(std::ostream &os) const override;

protected:
    OBAccelFullScaleRange fullScaleRange_;
    OBAccelSampleRate     sampleRate_;
};

class GyroStreamProfile : public StreamProfile {
public:
    GyroStreamProfile(std::shared_ptr<LazySensor> owner, OBGyroFullScaleRange fullScaleRange, OBGyroSampleRate sampleRate);
    ~GyroStreamProfile() noexcept override = default;

    OBGyroFullScaleRange           getFullScaleRange() const;
    OBGyroSampleRate               getSampleRate() const;
    void                           bindIntrinsic(const OBGyroIntrinsic &intrinsic);
    OBGyroIntrinsic                getIntrinsic() const;
    std::shared_ptr<StreamProfile> clone() const override;
    std::ostream                  &operator<<(std::ostream &os) const override;

protected:
    OBGyroFullScaleRange fullScaleRange_;
    OBGyroSampleRate     sampleRate_;
};

std::ostream &operator<<(std::ostream &os, const std::shared_ptr<const StreamProfile> &streamProfile);

std::vector<std::shared_ptr<const VideoStreamProfile>> matchVideoStreamProfile(const StreamProfileList &profileList, uint32_t width, uint32_t height,
                                                                               uint32_t fps, OBFormat format);

std::vector<std::shared_ptr<const AccelStreamProfile>> matchAccelStreamProfile(const StreamProfileList &profileList, OBAccelFullScaleRange fullScaleRange,
                                                                               OBAccelSampleRate sampleRate);

std::vector<std::shared_ptr<const GyroStreamProfile>> matchGyroStreamProfile(const StreamProfileList &profileList, OBGyroFullScaleRange fullScaleRange,
                                                                             OBGyroSampleRate sampleRate);

}  // namespace libobsensor

OB_LOG_FORMATTER(std::shared_ptr<libobsensor::StreamProfile>)
OB_LOG_FORMATTER(std::shared_ptr<const libobsensor::StreamProfile>)

OB_LOG_FORMATTER(std::shared_ptr<libobsensor::VideoStreamProfile>)
OB_LOG_FORMATTER(std::shared_ptr<const libobsensor::VideoStreamProfile>)

OB_LOG_FORMATTER(std::shared_ptr<libobsensor::DisparityBasedStreamProfile>)
OB_LOG_FORMATTER(std::shared_ptr<const libobsensor::DisparityBasedStreamProfile>)

OB_LOG_FORMATTER(std::shared_ptr<libobsensor::AccelStreamProfile>)
OB_LOG_FORMATTER(std::shared_ptr<const libobsensor::AccelStreamProfile>)

OB_LOG_FORMATTER(std::shared_ptr<libobsensor::GyroStreamProfile>)
OB_LOG_FORMATTER(std::shared_ptr<const libobsensor::GyroStreamProfile>)
