// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "FrameGeometricTransform.hpp"
#include "exception/ObException.hpp"
#include "logger/LoggerInterval.hpp"
#include "frame/FrameFactory.hpp"
#include "libobsensor/h/ObTypes.h"
#include "utils/CameraParamProcess.hpp"
#include <libyuv.h>
#include <turbojpeg.h>

namespace libobsensor {

template <typename T> void imageMirror(const T *src, T *dst, uint32_t width, uint32_t height) {
    const T *srcPixel;
    T       *dstPixel = dst;
    for(uint32_t h = 0; h < height; h++) {
        srcPixel = src + (h + 1) * width - 1;
        for(uint32_t w = 0; w < width; w++) {
            // mirror row
            *dstPixel = *srcPixel;
            srcPixel--;
            dstPixel++;
        }
    }
}

void mirrorRGBImage(const uint8_t *src, uint8_t *dst, uint32_t width, uint32_t height) {
    const uint8_t *srcPixel;
    uint8_t       *dstPixel = dst;
    for(uint32_t h = 0; h < height; h++) {
        srcPixel = src + (h + 1) * width * 3 - 3;
        for(uint32_t w = 0; w < width; w++) {
            // mirror row for each RGB component
            for(int i = 0; i < 3; i++) {
                dstPixel[i] = srcPixel[i];
            }
            srcPixel -= 3;
            dstPixel += 3;
        }
    }
}

void mirrorRGBAImage(const uint8_t *src, uint8_t *dst, uint32_t width, uint32_t height) {
    const uint8_t *srcPixel;
    uint8_t       *dstPixel = dst;
    for(uint32_t h = 0; h < height; h++) {
        srcPixel = src + (h + 1) * width * 4 - 4;
        for(uint32_t w = 0; w < width; w++) {
            // mirror row for each RGB component
            for(int i = 0; i < 4; i++) {
                dstPixel[i] = srcPixel[i];
            }
            srcPixel -= 4;
            dstPixel += 4;
        }
    }
}

void mirrorYUYVImage(uint8_t *src, uint8_t *dst, int width, int height) {
    uint8_t *dstPixel = dst;
    for(int h = 0; h < height; h++) {
        uint8_t *srcPixel = src + width * 2 * (h + 1) - 4;
        for(int w = 0; w < width / 2; w++) {
            *dstPixel       = *(srcPixel + 2);
            *(dstPixel + 1) = *(srcPixel + 1);
            *(dstPixel + 2) = *srcPixel;
            *(dstPixel + 3) = *(srcPixel + 3);

            srcPixel -= 4;
            dstPixel += 4;
        }
    }
}

template <typename T> void imageFlip(const T *src, T *dst, uint32_t width, uint32_t height) {

    const T *flipSrc = src + (width * height);
    for(uint32_t h = 0; h < height; h++) {
        flipSrc -= width;
        memcpy(dst, flipSrc, width * sizeof(T));
        dst += width;
    }
}

void flipRGBImage(int pixelSize, const uint8_t *src, uint8_t *dst, uint32_t width, uint32_t height) {
    // const uint32_t pixelSize = 3;  // RGB888 format occupies 3 bytes per pixel
    const uint32_t rowSize = width * pixelSize;

    for(uint32_t h = 0; h < height; h++) {
        const uint8_t *flipSrc = src + (height - h - 1) * rowSize;
        memcpy(dst, flipSrc, rowSize);
        dst += rowSize;
    }
}

// #define imageRotate180 imageFlip

// Rotate 90 degrees clockwise
template <typename T> void imageRotate90(const T *src, T *dst, uint32_t width, uint32_t height) {
    const T *srcPixel;
    T       *dstPixel = dst;
    for(uint32_t h = 0; h < width; h++) {
        for(uint32_t w = 0; w < height; w++) {
            srcPixel  = src + width * (height - w - 1) + h;
            *dstPixel = *srcPixel;
            dstPixel++;
        }
    }
}

template <typename T> void imageRotate180(const T *src, T *dst, uint32_t width, uint32_t height) {
    const T *srcPixel;
    T       *dstPixel = dst;
    for(uint32_t h = 0; h < height; h++) {
        for(uint32_t w = 0; w < width; w++) {
            srcPixel  = src + width * (height - h - 1) + (width - w - 1);
            *dstPixel = *srcPixel;
            dstPixel++;
        }
    }
}

template <typename T> void imageRotate270(const T *src, T *dst, uint32_t width, uint32_t height) {
    const T *srcPixel;
    T       *dstPixel = dst;
    for(uint32_t h = 0; h < width; h++) {
        for(uint32_t w = 0; w < height; w++) {
            srcPixel  = src + width * w + (width - h - 1);
            *dstPixel = *srcPixel;
            dstPixel++;
        }
    }
}

template <typename T> void imageRotate(const T *src, T *dst, uint32_t width, uint32_t height, uint32_t rotateDegree) {
    switch(rotateDegree) {
    case 90:
        imageRotate90<T>(src, dst, width, height);
        break;
    case 180:
        imageRotate180<T>(src, dst, width, height);
        break;
    case 270:
        imageRotate270<T>(src, dst, width, height);
        break;
    default:
        LOG_WARN_INTVL_THREAD("Unsupported rotate degree!");
        break;
    }
}

template <typename T> void rgbImageRotate90(const uint8_t *src, uint8_t *dst, uint32_t width, uint32_t height, uint32_t pixelSize) {
    for(uint32_t h = 0; h < width; h++) {
        for(uint32_t w = 0; w < height; w++) {
            const uint8_t *srcPixel = src + (height - w - 1) * width * pixelSize + h * pixelSize;
            uint8_t       *dstPixel = dst + h * height * pixelSize + w * pixelSize;
            memcpy(dstPixel, srcPixel, pixelSize);
        }
    }
}

template <typename T> void rgbImageRotate180(const uint8_t *src, uint8_t *dst, uint32_t width, uint32_t height, uint32_t pixelSize) {
    for(uint32_t h = 0; h < height; h++) {
        for(uint32_t w = 0; w < width; w++) {
            const uint8_t *srcPixel = src + (height - h - 1) * width * pixelSize + (width - w - 1) * pixelSize;
            uint8_t       *dstPixel = dst + h * width * pixelSize + w * pixelSize;
            memcpy(dstPixel, srcPixel, pixelSize);
        }
    }
}

template <typename T> void rgbImageRotate270(const uint8_t *src, uint8_t *dst, uint32_t width, uint32_t height, uint32_t pixelSize) {
    for(uint32_t h = 0; h < width; h++) {
        for(uint32_t w = 0; w < height; w++) {
            const T *srcPixel = src + ((w * width + h) * pixelSize);
            T       *dstPixel = dst + (((width - h - 1) * height + w) * pixelSize);
            memcpy(dstPixel, srcPixel, pixelSize * sizeof(T));
        }
    }
}

template <typename T> void rotateRGBImage(const T *src, T *dst, uint32_t width, uint32_t height, uint32_t rotateDegree, uint32_t pixelSize) {
    switch(rotateDegree) {
    case 90:
        rgbImageRotate90<T>(src, dst, width, height, pixelSize);
        break;
    case 180:
        rgbImageRotate180<T>(src, dst, width, height, pixelSize);
        break;
    case 270:
        rgbImageRotate270<T>(src, dst, width, height, pixelSize);
        break;
    default:
        LOG_WARN_INTVL_THREAD("Unsupported rotate degree!");
        break;
    }
}

void yuyvImageRotate(uint8_t *src, uint8_t *dst, uint32_t width, uint32_t height, uint32_t rotateDegree) {
    libyuv::RotationMode rotationMode;
    switch(rotateDegree) {
    case 90:
        rotationMode = libyuv::kRotate90;
        break;
    case 180:
        rotationMode = libyuv::kRotate180;
        break;
    case 270:
        rotationMode = libyuv::kRotate270;
        break;
    default:
        LOG_WARN_INTVL_THREAD("Unsupported rotate degree!");
        return;
    }

    // 1. yuyv to I420, since yuyv is a packed format, only plane format can be rotated; there will be data loss when rotating to plane.
    uint32_t dst_width    = width;
    uint32_t dst_height   = height;
    uint8_t *dst_y        = dst;
    uint32_t dst_stride_y = dst_width;
    uint8_t *dst_u        = dst + dst_width * dst_height;
    uint32_t dst_stride_u = dst_width / 2;
    uint8_t *dst_v        = dst + dst_width * dst_height + (dst_width * dst_height) / 4;
    uint32_t dst_stride_v = dst_width / 2;
    libyuv::YUY2ToI420(src, width * 2, dst_y, dst_stride_y, dst_u, dst_stride_u, dst_v, dst_stride_v, dst_width, dst_height);

    // 2. rotate
    uint8_t *src_y        = dst_y;
    uint32_t src_stride_y = dst_stride_y;
    uint8_t *src_u        = dst_u;
    uint32_t src_stride_u = dst_stride_u;
    uint8_t *src_v        = dst_v;
    uint32_t src_stride_v = dst_stride_v;
    if(rotationMode != libyuv::kRotate180) {
        dst_width  = height;
        dst_height = width;
    }
    std::swap(src, dst);
    dst_y        = dst;
    dst_stride_y = dst_width;
    dst_u        = dst + dst_width * dst_height;
    dst_stride_u = dst_width / 2;
    dst_v        = dst + dst_width * dst_height + (dst_width * dst_height) / 4;
    dst_stride_v = dst_width / 2;
    libyuv::I420Rotate(src_y, src_stride_y, src_u, src_stride_u, src_v, src_stride_v, dst_y, dst_stride_y, dst_u, dst_stride_u, dst_v, dst_stride_v, width,
                       height, rotationMode);

    // 3.I420 to yuyv
    src_y        = dst_y;
    src_stride_y = dst_stride_y;
    src_u        = dst_u;
    src_stride_u = dst_stride_u;
    src_v        = dst_v;
    src_stride_v = dst_stride_v;
    std::swap(src, dst);
    uint8_t *dst_yuy2        = dst;
    uint32_t dst_stride_yuy2 = dst_width * 2;
    libyuv::I420ToYUY2(src_y, src_stride_y, src_u, src_stride_u, src_v, src_stride_v, dst_yuy2, dst_stride_yuy2, dst_width, dst_height);
}

FrameMirror::FrameMirror() {}
FrameMirror::~FrameMirror() noexcept {}

void FrameMirror::updateConfig(std::vector<std::string> &params) {
    if(params.size() != 0) {
        throw unsupported_operation_exception("Frame mirror update config error: unsupported operation.");
    }
}

const std::string &FrameMirror::getConfigSchema() const {
    static const std::string schema = "";  // empty schema
    return schema;
}

void FrameMirror::reset() {
    srcStreamProfile_.reset();
    rstStreamProfile_.reset();
}

std::shared_ptr<Frame> FrameMirror::process(std::shared_ptr<const Frame> frame) {
    if(!frame) {
        return nullptr;
    }

    auto outFrame = FrameFactory::createFrameFromOtherFrame(frame);
    if(frame->is<FrameSet>()) {
        return outFrame;
    }

    if(frame->getFormat() != OB_FORMAT_Y16 && frame->getFormat() != OB_FORMAT_Y8 && frame->getFormat() != OB_FORMAT_YUYV && frame->getFormat() != OB_FORMAT_RGB
       && frame->getFormat() != OB_FORMAT_BGR && frame->getFormat() != OB_FORMAT_RGBA && frame->getFormat() != OB_FORMAT_BGRA) {
        LOG_WARN_INTVL("FrameMirror unsupported to process this format: {}", frame->getFormat());
        return outFrame;
    }

    auto videoFrame      = frame->as<VideoFrame>();
    bool isMirrorSupport = true;
    switch(frame->getFormat()) {
    case OB_FORMAT_Y8:
        imageMirror<uint8_t>((uint8_t *)videoFrame->getData(), (uint8_t *)outFrame->getData(), videoFrame->getWidth(), videoFrame->getHeight());
        break;
    case OB_FORMAT_Y16:
        imageMirror<uint16_t>((uint16_t *)videoFrame->getData(), (uint16_t *)outFrame->getData(), videoFrame->getWidth(), videoFrame->getHeight());
        break;
    case OB_FORMAT_YUYV:
        if(frame->getType() == OB_FRAME_COLOR) {
            mirrorYUYVImage((uint8_t *)videoFrame->getData(), (uint8_t *)outFrame->getData(), videoFrame->getWidth(), videoFrame->getHeight());
        }
        else {
            imageMirror<uint32_t>((uint32_t *)videoFrame->getData(), (uint32_t *)outFrame->getData(), videoFrame->getWidth() / 2, videoFrame->getHeight());
        }
        break;
    case OB_FORMAT_RGB:
    case OB_FORMAT_BGR:
        mirrorRGBImage((uint8_t *)videoFrame->getData(), (uint8_t *)outFrame->getData(), videoFrame->getWidth(), videoFrame->getHeight());
        break;
    case OB_FORMAT_RGBA:
    case OB_FORMAT_BGRA:
        mirrorRGBAImage((uint8_t *)videoFrame->getData(), (uint8_t *)outFrame->getData(), videoFrame->getWidth(), videoFrame->getHeight());
        break;
    default:
        isMirrorSupport = false;
        break;
    }

    try {
        if(isMirrorSupport) {
            auto streampProfile = frame->getStreamProfile();
            if(!srcStreamProfile_ || srcStreamProfile_ != streampProfile) {
                srcStreamProfile_          = streampProfile;
                auto srcVideoStreamProfile = srcStreamProfile_->as<VideoStreamProfile>();
                auto srcIntrinsic          = srcVideoStreamProfile->getIntrinsic();
                auto rstIntrinsic          = mirrorOBCameraIntrinsic(srcIntrinsic);
                auto srcDistortion         = srcVideoStreamProfile->getDistortion();
                auto rstDistortion         = mirrorOBCameraDistortion(srcDistortion);
                rstStreamProfile_          = srcVideoStreamProfile->clone()->as<VideoStreamProfile>();
                rstStreamProfile_->bindIntrinsic(rstIntrinsic);
                rstStreamProfile_->bindDistortion(rstDistortion);

                OBExtrinsic rstExtrinsic = { {
                                                 -1,
                                                 0,
                                                 0,
                                                 0,
                                                 1,
                                                 0,
                                                 0,
                                                 0,
                                                 1,
                                             },
                                             { 0, 0, 0 } };
                rstStreamProfile_->bindExtrinsicTo(srcStreamProfile_, rstExtrinsic);
            }
            outFrame->setStreamProfile(rstStreamProfile_);
        }
    }
    catch(libobsensor_exception &error) {
        LOG_WARN_INTVL("Frame mirror camera intrinsic conversion failed{0}, exception type: {1}", error.get_message(), error.get_exception_type());
    }

    return outFrame;
}

OBCameraIntrinsic FrameMirror::mirrorOBCameraIntrinsic(const OBCameraIntrinsic &src) {
    auto intrinsic = src;
    CameraParamProcessor::cameraIntrinsicParamsMirror(&intrinsic);
    return intrinsic;
}

OBCameraDistortion FrameMirror::mirrorOBCameraDistortion(const OBCameraDistortion &src) {
    auto distortion = src;
    CameraParamProcessor::distortionParamMirror(&distortion);
    return distortion;
}

FrameFlip::FrameFlip() {}
FrameFlip::~FrameFlip() noexcept {}

void FrameFlip::updateConfig(std::vector<std::string> &params) {
    if(params.size() != 0) {
        throw unsupported_operation_exception("Frame flip update config error: unsupported operation.");
    }
}

const std::string &FrameFlip::getConfigSchema() const {
    static const std::string schema = "";  // empty schema
    return schema;
}

void FrameFlip::reset() {
    srcStreamProfile_.reset();
    rstStreamProfile_.reset();
}

std::shared_ptr<Frame> FrameFlip::process(std::shared_ptr<const Frame> frame) {
    if(!frame) {
        return nullptr;
    }

    auto outFrame = FrameFactory::createFrameFromOtherFrame(frame);
    if(frame->is<FrameSet>()) {
        return outFrame;
    }

    if(frame->getFormat() != OB_FORMAT_Y16 && frame->getFormat() != OB_FORMAT_Y8 && frame->getFormat() != OB_FORMAT_YUYV && frame->getFormat() != OB_FORMAT_BGR
       && frame->getFormat() != OB_FORMAT_RGB && frame->getFormat() != OB_FORMAT_RGBA && frame->getFormat() != OB_FORMAT_BGRA) {
        LOG_WARN_INTVL("FrameFlip unsupported to process this format:{}", frame->getFormat());
        return outFrame;
    }

    bool isSupportFlip = true;
    auto videoFrame    = frame->as<VideoFrame>();
    switch(frame->getFormat()) {
    case OB_FORMAT_Y8:
        imageFlip<uint8_t>((uint8_t *)videoFrame->getData(), (uint8_t *)outFrame->getData(), videoFrame->getWidth(), videoFrame->getHeight());
        break;
    case OB_FORMAT_YUYV:
    case OB_FORMAT_Y16:
        imageFlip<uint16_t>((uint16_t *)videoFrame->getData(), (uint16_t *)outFrame->getData(), videoFrame->getWidth(), videoFrame->getHeight());
        break;
    case OB_FORMAT_BGR:
    case OB_FORMAT_RGB:
        flipRGBImage(3, (uint8_t *)videoFrame->getData(), (uint8_t *)outFrame->getData(), videoFrame->getWidth(), videoFrame->getHeight());
        break;
    case OB_FORMAT_RGBA:
    case OB_FORMAT_BGRA:
        flipRGBImage(4, (uint8_t *)videoFrame->getData(), (uint8_t *)outFrame->getData(), videoFrame->getWidth(), videoFrame->getHeight());
        break;
    default:
        isSupportFlip = false;
        break;
    }

    try {
        if(isSupportFlip) {
            auto streampProfile = frame->getStreamProfile();
            if(!srcStreamProfile_ || srcStreamProfile_ != streampProfile) {
                srcStreamProfile_          = streampProfile;
                auto srcVideoStreamProfile = srcStreamProfile_->as<VideoStreamProfile>();
                auto srcIntrinsic          = srcVideoStreamProfile->getIntrinsic();
                auto rstIntrinsic          = flipOBCameraIntrinsic(srcIntrinsic);
                auto srcDistortion         = srcVideoStreamProfile->getDistortion();
                auto rstDistortion         = flipOBCameraDistortion(srcDistortion);
                rstStreamProfile_          = srcVideoStreamProfile->clone()->as<VideoStreamProfile>();
                rstStreamProfile_->bindIntrinsic(rstIntrinsic);
                rstStreamProfile_->bindDistortion(rstDistortion);

                OBExtrinsic rstExtrinsic = { { 1, 0, 0, 0, -1, 0, 0, 0, 1 }, { 0, 0, 0 } };
                rstStreamProfile_->bindExtrinsicTo(srcStreamProfile_, rstExtrinsic);
            }
            outFrame->setStreamProfile(rstStreamProfile_);
        }
    }
    catch(libobsensor_exception &error) {
        LOG_WARN_INTVL("Frame flip camera intrinsic conversion failed{0}, exception type: {1}", error.get_message(), error.get_exception_type());
    }

    return outFrame;
}

OBCameraIntrinsic FrameFlip::flipOBCameraIntrinsic(const OBCameraIntrinsic &src) {
    auto intrinsic = src;
    CameraParamProcessor::cameraIntrinsicParamsFlip(&intrinsic);
    return intrinsic;
}

OBCameraDistortion FrameFlip::flipOBCameraDistortion(const OBCameraDistortion &src) {
    auto distortion = src;
    CameraParamProcessor::distortionParamFlip(&distortion);
    return distortion;
}

FrameRotate::FrameRotate() {}
FrameRotate::~FrameRotate() noexcept {}

void FrameRotate::updateConfig(std::vector<std::string> &params) {
    if(params.size() != 1) {
        throw invalid_value_exception("Frame rotate config error: params size not match");
    }
    try {
        int rotateDegree = static_cast<int>(std::stoi(params[0]));
        if(rotateDegree == 0 || rotateDegree == 90 || rotateDegree == 180 || rotateDegree == 270) {
            std::lock_guard<std::mutex> rotateLock(mtx_);
            rotateDegree_        = rotateDegree;
            rotateDegreeUpdated_ = true;
        }
    }
    catch(const std::exception &e) {
        throw invalid_value_exception("Frame rotate config error: " + std::string(e.what()));
    }
}

const std::string &FrameRotate::getConfigSchema() const {
    // csv format: name，type， min，max，step，default，description
    static const std::string schema = "rotate, int, 0, 270, 90, 0, frame image rotation angle";
    return schema;
}

void FrameRotate::reset() {
    rotateDegreeUpdated_ = true;
    srcStreamProfile_.reset();
    rstStreamProfile_.reset();
}

std::shared_ptr<Frame> FrameRotate::process(std::shared_ptr<const Frame> frame) {
    if(!frame) {
        return nullptr;
    }

    auto outFrame = FrameFactory::createFrameFromOtherFrame(frame);
    if(frame->is<FrameSet>()) {
        return outFrame;
    }

    if(rotateDegree_ == 0) {
        return outFrame;
    }

    if(frame->getFormat() != OB_FORMAT_Y16 && frame->getFormat() != OB_FORMAT_Y8 && frame->getFormat() != OB_FORMAT_YUYV && frame->getFormat() != OB_FORMAT_RGB
       && frame->getFormat() != OB_FORMAT_BGR && frame->getFormat() != OB_FORMAT_RGBA && frame->getFormat() != OB_FORMAT_BGRA) {
        LOG_WARN_INTVL("FrameRotate unsupported to process this format: {}", frame->getFormat());
        return outFrame;
    }

    std::lock_guard<std::mutex> rotateLock(mtx_);
    bool                        isSupportRotate = true;
    auto                        videoFrame      = frame->as<VideoFrame>();
    switch(frame->getFormat()) {
    case OB_FORMAT_Y8:
        imageRotate<uint8_t>((uint8_t *)videoFrame->getData(), (uint8_t *)outFrame->getData(), videoFrame->getWidth(), videoFrame->getHeight(), rotateDegree_);
        break;
    case OB_FORMAT_Y16:
        imageRotate<uint16_t>((uint16_t *)videoFrame->getData(), (uint16_t *)outFrame->getData(), videoFrame->getWidth(), videoFrame->getHeight(),
                              rotateDegree_);
        break;
    case OB_FORMAT_YUYV:
        // Note: This operation will also modify the data content of the original data frame.
        yuyvImageRotate((uint8_t *)videoFrame->getData(), (uint8_t *)outFrame->getData(), videoFrame->getWidth(), videoFrame->getHeight(), rotateDegree_);
        break;
    case OB_FORMAT_RGB:
    case OB_FORMAT_BGR:
        rotateRGBImage<uint8_t>((uint8_t *)videoFrame->getData(), (uint8_t *)outFrame->getData(), videoFrame->getWidth(), videoFrame->getHeight(),
                                rotateDegree_, 3);
        break;
    case OB_FORMAT_RGBA:
    case OB_FORMAT_BGRA:
        rotateRGBImage<uint8_t>((uint8_t *)videoFrame->getData(), (uint8_t *)outFrame->getData(), videoFrame->getWidth(), videoFrame->getHeight(),
                                rotateDegree_, 4);
        break;
    default:
        isSupportRotate = false;
        break;
    }

    try {
        if(isSupportRotate) {
            auto streampProfile = frame->getStreamProfile();
            if(!rstStreamProfile_ || !srcStreamProfile_ || srcStreamProfile_ != streampProfile || rotateDegreeUpdated_) {
                rotateDegreeUpdated_       = false;
                srcStreamProfile_          = streampProfile;
                auto srcVideoStreamProfile = srcStreamProfile_->as<VideoStreamProfile>();
                auto srcIntrinsic          = srcVideoStreamProfile->getIntrinsic();
                auto rstIntrinsic          = rotateOBCameraIntrinsic(srcIntrinsic, rotateDegree_);
                auto srcDistortion         = srcVideoStreamProfile->getDistortion();
                auto rstDistortion         = rotateOBCameraDistortion(srcDistortion, rotateDegree_);
                auto rstExtrinsic          = rotateOBExtrinsic(rotateDegree_);
                rstStreamProfile_          = srcVideoStreamProfile->clone()->as<VideoStreamProfile>();
                rstStreamProfile_->bindIntrinsic(rstIntrinsic);
                rstStreamProfile_->bindDistortion(rstDistortion);
                rstStreamProfile_->bindExtrinsicTo(srcStreamProfile_, rstExtrinsic);
            }
            if(rotateDegree_ == 90 || rotateDegree_ == 270) {
                rstStreamProfile_->setWidth(videoFrame->getHeight());
                rstStreamProfile_->setHeight(videoFrame->getWidth());
            }
            outFrame->setStreamProfile(rstStreamProfile_);
        }
    }
    catch(libobsensor_exception &error) {
        LOG_WARN_INTVL("Frame rotate camera intrinsic conversion failed{0}, exception type: {1}", error.get_message(), error.get_exception_type());
    }

    return outFrame;
}

OBCameraIntrinsic FrameRotate::rotateOBCameraIntrinsic(const OBCameraIntrinsic &src, uint32_t rotateDegree) {
    auto intrinsic = src;
    if(rotateDegree == 90) {
        CameraParamProcessor::cameraIntrinsicParamsRotate90(&intrinsic);
    }
    else if(rotateDegree == 180) {
        CameraParamProcessor::cameraIntrinsicParamsRotate180(&intrinsic);
    }
    else if(rotateDegree == 270) {
        CameraParamProcessor::cameraIntrinsicParamsRotate270(&intrinsic);
    }

    return intrinsic;
}

OBCameraDistortion FrameRotate::rotateOBCameraDistortion(const OBCameraDistortion &src, uint32_t rotateDegree) {
    auto distortion = src;

    if(rotateDegree == 90) {
        CameraParamProcessor::distortionParamRotate90(&distortion);
    }
    else if(rotateDegree == 180) {
        CameraParamProcessor::distortionParamRotate180(&distortion);
    }
    else if(rotateDegree == 270) {
        CameraParamProcessor::distortionParamRotate270(&distortion);
    }

    return distortion;
}

OBExtrinsic FrameRotate::rotateOBExtrinsic(uint32_t rotateDegree) {
    // rotate clockwise
    if(rotateDegree == 90) {
        return { { 0, 1, 0, -1, 0, 0, 0, 0, 1 }, { 0, 0, 0 } };
    }
    else if(rotateDegree == 180) {
        return { { -1, 0, 0, 0, -1, 0, 0, 0, 1 }, { 0, 0, 0 } };
    }
    else if(rotateDegree == 270) {
        return { { 0, -1, 0, 1, 0, 0, 0, 0, 1 }, { 0, 0, 0 } };
    }
    return { { 1, 0, 0, 0, 1, 0, 0, 0, 1 }, { 0, 0, 0 } };
}

}  // namespace libobsensor

