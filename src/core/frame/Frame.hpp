// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "IFrame.hpp"
#include "IStreamProfile.hpp"
#include "exception/ObException.hpp"

#include <atomic>
#include <memory>
#include <mutex>
#include <vector>
#include <typeinfo>

namespace libobsensor {

class logger;
class FrameMemoryPool;
class FrameMemoryAllocator;
class FrameBackendLifeSpan {
public:
    FrameBackendLifeSpan();
    ~FrameBackendLifeSpan();

private:
    std::shared_ptr<Logger>               logger_;
    std::shared_ptr<FrameMemoryPool>      memoryPool_;
    std::shared_ptr<FrameMemoryAllocator> memoryAllocator_;
};

class FrameSet;
class PointsFrame;
class VideoFrame;
class ColorFrame;
class DisparityFrame;
class DepthFrame;
class IRFrame;
class AccelFrame;
class GyroFrame;

using FrameBufferReclaimFunc = std::function<void(void)>;

class Frame : public std::enable_shared_from_this<Frame>, private FrameBackendLifeSpan {
public:
    Frame(uint8_t *data, size_t dataBufSize, OBFrameType type, FrameBufferReclaimFunc bufferReclaimFunc = nullptr);
    Frame(uint8_t *data, size_t dataBufSize, FrameBufferReclaimFunc bufferReclaimFunc = nullptr);
    virtual ~Frame() noexcept;

    OBFrameType getType() const;

    uint64_t       getNumber() const;
    void           setNumber(const uint64_t number);
    size_t         getDataSize() const;
    void           setDataSize(size_t dataSize);
    const uint8_t *getData() const;
    uint8_t       *getDataMutable() const;
    void           updateData(const uint8_t *data, size_t dataSize);
    uint64_t       getTimeStampUsec() const;
    void           setTimeStampUsec(uint64_t ts);
    uint64_t       getSystemTimeStampUsec() const;
    void           setSystemTimeStampUsec(uint64_t ts);
    uint64_t       getGlobalTimeStampUsec() const;
    void           setGlobalTimeStampUsec(uint64_t ts);

    size_t         getMetadataSize() const;
    void           updateMetadata(const uint8_t *metadata, size_t metadataSize);
    void           appendMetadata(const uint8_t *metadata, size_t metadataSize);
    const uint8_t *getMetadata() const;

    uint8_t *getMetadataMutable() const;  // use with caution, metadata may be changed while other threads are using it
    void     setMetadataSize(size_t metadataSize);

    void    registerMetadataParsers(std::shared_ptr<IFrameMetadataParserContainer> parsers);
    bool    hasMetadata(OBFrameMetadataType type) const;
    int64_t getMetadataValue(OBFrameMetadataType type) const;

    std::shared_ptr<const StreamProfile> getStreamProfile() const;
    void                                 setStreamProfile(std::shared_ptr<const StreamProfile> streamProfile);
    OBFormat                             getFormat() const;  // get from stream profile

    virtual void copyInfoFromOther(std::shared_ptr<const Frame> otherFrame);

    template <typename T> bool is() const {
        return std::dynamic_pointer_cast<const T>(shared_from_this()) != nullptr;
    }

    // template <typename T> bool               is(){
    //     return std::dynamic_pointer_cast<T>(shared_from_this()) != nullptr;
    // }

    template <typename T> std::shared_ptr<T> as() {
        if(!is<T>()) {
            throw unsupported_operation_exception("unsupported operation, object's type is not require type");
        }

        return std::dynamic_pointer_cast<T>(shared_from_this());
    }

    template <typename T> std::shared_ptr<const T> as() const {
        if(!is<const T>())
            throw unsupported_operation_exception("unsupported operation, object's type is not require type");

        return std::dynamic_pointer_cast<const T>(shared_from_this());
    }

protected:
    size_t getDataBufSize() const;

protected:
    size_t                                         dataSize_;
    uint64_t                                       number_;
    uint64_t                                       timeStampUsec_;
    uint64_t                                       systemTimeStampUsec_;
    uint64_t                                       globalTimeStampUsec_;
    size_t                                         metadataSize_;
    uint8_t                                        metadata_[12 + 255];  // standard uvc payload size is 12bytes, add some extra space for metadata
    std::shared_ptr<IFrameMetadataParserContainer> metadataPhasers_;
    std::shared_ptr<const StreamProfile>           streamProfile_;

    const OBFrameType type_;  // Determined during construction, it is an inherent property of the object and cannot be changed.

private:
    uint8_t const         *frameData_;
    const size_t           dataBufSize_;
    FrameBufferReclaimFunc bufferReclaimFunc_;
};

class VideoFrame : public Frame {
public:
    VideoFrame(uint8_t *data, size_t dataBufSize, OBFrameType type, FrameBufferReclaimFunc bufferReclaimFunc = nullptr);
    VideoFrame(uint8_t *data, size_t dataBufSize, FrameBufferReclaimFunc bufferReclaimFunc = nullptr);

    uint32_t getWidth() const;
    uint32_t getHeight() const;
    uint32_t getFps() const;

    void     setStride(uint32_t stride);
    uint32_t getStride() const;

    void        setPixelType(OBPixelType pixelType);
    OBPixelType getPixelType() const;
    uint8_t     getPixelAvailableBitSize() const;
    void        setPixelAvailableBitSize(uint8_t bitSize);

    virtual void copyInfoFromOther(std::shared_ptr<const Frame> sourceFrame) override;

protected:
    OBPixelType pixelType_;              // 0: depth, 1: disparity for structure light camara， 2： raw phase for tof camara
    uint8_t     availablePixelBitSize_;  // available bit size of each pixel
    uint32_t    stride_ = 0;
};

class ColorFrame : public VideoFrame {
public:
    ColorFrame(uint8_t *data, size_t dataBufSize, FrameBufferReclaimFunc bufferReclaimFunc = nullptr);
};

class DepthFrame : public VideoFrame {
public:
    DepthFrame(uint8_t *data, size_t dataBufSize, FrameBufferReclaimFunc bufferReclaimFunc = nullptr);

    void  setValueScale(float valueScale);
    float getValueScale() const;

    virtual void copyInfoFromOther(std::shared_ptr<const Frame> sourceFrame) override;

private:
    float valueScale_;
};

class IRFrame : public VideoFrame {
public:
    IRFrame(uint8_t *data, size_t dataBufSize, FrameBufferReclaimFunc bufferReclaimFunc = nullptr, OBFrameType frameType = OB_FRAME_IR);
};

class IRLeftFrame : public IRFrame {
public:
    IRLeftFrame(uint8_t *data, size_t dataBufSize, FrameBufferReclaimFunc bufferReclaimFunc = nullptr);
};

class IRRightFrame : public IRFrame {
public:
    IRRightFrame(uint8_t *data, size_t dataBufSize, FrameBufferReclaimFunc bufferReclaimFunc = nullptr);
};

class PointsFrame : public Frame {
public:
    PointsFrame(uint8_t *data, size_t dataBufSize, FrameBufferReclaimFunc bufferReclaimFunc = nullptr);

    void  setCoordinateValueScale(float valueScale);
    float getCoordinateValueScale() const;

private:
    float coordValueScale_;  // coordinate value scale, multiply by this value to get actual coordinate value in mm
};

class AccelFrame : public Frame {
public:
#pragma pack(push, 1)
    typedef struct {
        OBAccelValue value;  // Acceleration values in three directions (xyz), unit: g (9.80665 m/s^2)
        float        temp;   // Temperature in Celsius
    } Data;
#pragma pack(pop)

public:
    AccelFrame(uint8_t *data, size_t dataBufSize, FrameBufferReclaimFunc bufferReclaimFunc = nullptr);

    OBAccelValue value();
    float        temperature();
};

class GyroFrame : public Frame {
public:
#pragma pack(push, 1)
    typedef struct {
        OBGyroValue value;  // Acceleration values ​​in three directions (xyz), unit: dps (degrees per second)
        float       temp;   // Temperature in Celsius
    } Data;
#pragma pack(pop)

public:
    GyroFrame(uint8_t *data, size_t dataBufSize, FrameBufferReclaimFunc bufferReclaimFunc = nullptr);

    OBGyroValue value();
    float       temperature();
};

class FrameSet : public Frame {
    typedef std::function<bool(void *)> ForeachBack;

public:
    FrameSet(uint8_t *data, size_t dataBufSize, FrameBufferReclaimFunc bufferReclaimFunc = nullptr);
    ~FrameSet() noexcept;

    uint32_t getCount() const;

    std::shared_ptr<const Frame> getFrame(OBFrameType frameType) const;
    std::shared_ptr<const Frame> getFrame(int index) const;

    std::shared_ptr<Frame> getFrameMutable(OBFrameType frameType) const;
    std::shared_ptr<Frame> getFrameMutable(int index) const;

    // It is recommended to use the rvalue reference interface. If you really need it, you can uncomment the following
    // void pushFrame(std::shared_ptr<Frame> frame);
    void pushFrame(std::shared_ptr<const Frame> &&frame);
    void clearAllFrame();

public:
    void foreachFrame(ForeachBack foreachBack) const;
};

}  // namespace libobsensor

