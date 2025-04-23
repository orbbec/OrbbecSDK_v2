// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "IFilter.hpp"
#include "stream/StreamProfile.hpp"
#include "logger/LoggerInterval.hpp"

#include <mutex>
#include <thread>
#include <atomic>

namespace libobsensor {
void mirrorRGBImage(const uint8_t *src, uint8_t *dst, uint32_t width, uint32_t height);
void mirrorRGBAImage(const uint8_t *src, uint8_t *dst, uint32_t width, uint32_t height);
void mirrorYUYVImage(uint8_t *src, uint8_t *dst, int width, int height);
void flipRGBImage(int pixelSize, const uint8_t *src, uint8_t *dst, uint32_t width, uint32_t height);
void yuyvImageRotate(uint8_t *src, uint8_t *dst, uint32_t width, uint32_t height, uint32_t rotateDegree);

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

template <typename T> void imageFlip(const T *src, T *dst, uint32_t width, uint32_t height) {

    const T *flipSrc = src + (width * height);
    for(uint32_t h = 0; h < height; h++) {
        flipSrc -= width;
        memcpy(dst, flipSrc, width * sizeof(T));
        dst += width;
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

class FrameMirror : public IFilterBase {
public:
    FrameMirror();
    virtual ~FrameMirror() noexcept override;

    void               updateConfig(std::vector<std::string> &params) override;
    const std::string &getConfigSchema() const override;
    void               reset() override;

private:
    std::shared_ptr<Frame> process(std::shared_ptr<const Frame> frame) override;

    static OBCameraIntrinsic  mirrorOBCameraIntrinsic(const OBCameraIntrinsic &src);
    static OBCameraDistortion mirrorOBCameraDistortion(const OBCameraDistortion &src);

protected:
    std::shared_ptr<const StreamProfile> srcStreamProfile_;
    std::shared_ptr<VideoStreamProfile>  rstStreamProfile_;
};

class FrameFlip : public IFilterBase {
public:
    FrameFlip();
    virtual ~FrameFlip() noexcept override;

    void               updateConfig(std::vector<std::string> &params) override;
    const std::string &getConfigSchema() const override;
    void               reset() override;

private:
    std::shared_ptr<Frame> process(std::shared_ptr<const Frame> frame) override;

    static OBCameraIntrinsic  flipOBCameraIntrinsic(const OBCameraIntrinsic &src);
    static OBCameraDistortion flipOBCameraDistortion(const OBCameraDistortion &src);

protected:
    std::shared_ptr<const StreamProfile> srcStreamProfile_;
    std::shared_ptr<VideoStreamProfile>  rstStreamProfile_;
};

class FrameRotate : public IFilterBase {
public:
    FrameRotate();
    virtual ~FrameRotate() noexcept override;

    void               updateConfig(std::vector<std::string> &params) override;
    const std::string &getConfigSchema() const override;
    void               reset() override;

private:
    std::shared_ptr<Frame> process(std::shared_ptr<const Frame> frame) override;

    static OBCameraIntrinsic  rotateOBCameraIntrinsic(const OBCameraIntrinsic &src, uint32_t rotateDegree);
    static OBCameraDistortion rotateOBCameraDistortion(const OBCameraDistortion &src, uint32_t rotateDegree);
    static OBExtrinsic        rotateOBExtrinsic(uint32_t rotateDegree);

protected:
    std::mutex                           mtx_;
    uint32_t                             rotateDegree_ = 0;
    std::atomic<bool>                    rotateDegreeUpdated_;
    std::shared_ptr<const StreamProfile> srcStreamProfile_;
    std::shared_ptr<VideoStreamProfile>  rstStreamProfile_;
};
}  // namespace libobsensor
