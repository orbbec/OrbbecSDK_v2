// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "LiDARImuStreamer.hpp"

#include "frame/Frame.hpp"
#include "frame/FrameFactory.hpp"
#include "stream/StreamProfile.hpp"
#include "logger/LoggerInterval.hpp"
#include "utils/Utils.hpp"
#include "property/InternalProperty.hpp"
#include "IDevice.hpp"

#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

namespace libobsensor {

#pragma pack(push, 1)
// Original LiDAR IMU stream data header
typedef struct {
    uint8_t  magic[6];           // magic data, must be 0x4D 0x53 0x02 0xF4 0xEB 0x90
    uint16_t dataLen;            // data lenth, must be 68(44+24)
    uint8_t  model;              // 0: MS600; 1:ME450
    uint8_t  scanRate;           // TODO
    uint16_t dataBlockNum;       // data block index based 1
    uint16_t frameIndex;         // 1~65535
    uint8_t  dataFormat;         // 0:IMU; 1:point cloud; 2: spherical point cloud;5: calibration point cloud
    uint64_t timestamp;          // timestamp, 0~3600e9, unit 1ns
    uint8_t  timestampSyncMode;  // 0: free run; 1: outer sync; 2: PTP sync
    uint32_t warningInfo;
    uint8_t  echoMode;          // 1:first echo; 2:last echo
    uint16_t scanSpeed;         // scan speed, RPM
    uint16_t verticalScanRate;  // vertical scan rate, unit 0.1Hz
    uint16_t temperature;       // temperature, unit 0.01c
    uint8_t  reserved[5];       // reserve
} ImuDataHeader;

typedef struct {
    float  gyroX;
    float  gyroY;
    float  gyroZ;
    float  accelX;
    float  accelY;
    float  accelZ;
}OriginImuData;

#pragma pack(pop)

// data block magic
#define HEAD_MAGIC "\x4D\x53\x02\xF4\xEB\x90"
#define HEAD_MAGIC_LEN (6)
#define TAIL_MAGIC "\xFE\xFE\xFE\xFE"
#define TAIL_MAGIC_LEN (4)

#if !(defined(_WIN32) || defined(__APPLE__))
static inline uint64_t ntohll(uint64_t val) {
#if __BYTE_ORDER == __LITTLE_ENDIAN
    return (((uint64_t)ntohl(val & 0xFFFFFFFFULL)) << 32) | ntohl(val >> 32);
#else
    return val;
#endif
}
#endif

LiDARImuStreamer::LiDARImuStreamer(IDevice *owner, const std::shared_ptr<IDataStreamPort> &backend)
    : owner_(owner), backend_(nullptr), running_(false), frameIndex_(0) {
    backend_ = std::dynamic_pointer_cast<LiDARDataStreamPort>(backend);
    if(backend_ == nullptr) {
        throw invalid_value_exception("The IMU stream port isn't LiDARDataStreamPort!");
    }

    firmwareVersionInt_ = owner_->getFirmwareVersionInt();

    LOG_DEBUG("LiDARImuStreamer created");
}

LiDARImuStreamer::~LiDARImuStreamer() noexcept {
    if(accelStreamProfile_) {
        stopStream(nullptr);
    }
    if(gyroStreamProfile_) {
        stopStream(nullptr);
    }
}

void LiDARImuStreamer::startStream(std::shared_ptr<const StreamProfile> sp, MutableFrameCallback callback) {
    LOG_INFO("Try to start stream: {}", sp);
    {
        std::lock_guard<std::mutex> lock(cbMtx_);
        if(sp->is<AccelStreamProfile>()) {
            if(accelStreamProfile_) {
                throw unsupported_operation_exception("The IMU accel stream has already been started.");
                return;
            }
            auto accel = sp->as<AccelStreamProfile>();
            if (running_) {
                // imu stream has already been started
                if(accel->getSampleRate() != gyroStreamProfile_->getSampleRate()) {
                    throw unsupported_operation_exception("The IMU stream has already been started with other sample rate.");
                    return;
                }
                // save and return immediately
                accelCallback_      = callback;
                accelStreamProfile_ = accel;
                return;
            }

            // save
            accelCallback_      = callback;
            accelStreamProfile_ = accel;
        }
        else if(sp->is<GyroStreamProfile>()) {
            if(gyroStreamProfile_) {
                throw unsupported_operation_exception("The IMU gyro stream has already been started.");
                return;
            }
            auto gyro = sp->as<GyroStreamProfile>();
            if(running_) {
                // imu stream has already been started
                if(gyro->getSampleRate() != accelStreamProfile_->getSampleRate()) {
                    throw unsupported_operation_exception("The IMU stream has already been started with other sample rate.");
                    return;
                }
                // save and return immediately
                gyroCallback_      = callback;
                gyroStreamProfile_ = gyro;
                return;
            }
            // save
            gyroCallback_      = callback;
            gyroStreamProfile_ = gyro;
        }
        else {
            throw unsupported_operation_exception("The profile is not valid IMU profile");
            return;
        }
        running_ = true;
    }

    // 1. send command to stop stream
    try {
        trySendStopStreamVendorCmd();
    }
    catch(const std::exception &e) {
        LOG_WARN("Exception occurred while send stop stream command: {}", e.what());
    }

    // start stream
    BEGIN_TRY_EXECUTE({
        // 2. start backend stream
        backend_->startStream([this](std::shared_ptr<Frame> frame) { LiDARImuStreamer::parseIMUData(frame); });
        // 3. start IMU stream
        LOG_DEBUG("LiDARImuStreamer try to send start stream command...");
        trySendStartStreamVendorCmd();
        running_ = true;
        LOG_DEBUG("LiDARImuStreamer start backend stream finished...");
    })
    CATCH_EXCEPTION_AND_EXECUTE({
        backend_->stopStream();
        accelCallback_      = nullptr;
        accelStreamProfile_ = nullptr;
        gyroCallback_       = nullptr;
        gyroStreamProfile_  = nullptr;
        running_            = false;
        throw;
    })
}

void LiDARImuStreamer::stopStream(std::shared_ptr<const StreamProfile> sp) {
    LOG_DEBUG("LiDARImuStreamer stop....");
    {
        std::lock_guard<std::mutex> lock(cbMtx_);
        if(!running_) {
            LOG_DEBUG("LiDARImuStreamer stream is off...");
            return;
        }

        if(sp->is<AccelStreamProfile>()) {
            if(accelStreamProfile_ == nullptr) {
                LOG_WARN("The IMU accel stream is off...");
                return;
            }
            // clear current profile
            accelStreamProfile_ = nullptr;
            accelCallback_      = nullptr;
            if(gyroStreamProfile_ == nullptr) {
                // all imu streams are off
                return;
            }
        }
        else if(sp->is<GyroStreamProfile>()) {
            if(gyroStreamProfile_ == nullptr) {
                LOG_WARN("The IMU gyro stream is off...");
                return;
            }
            // clear current profile
            gyroStreamProfile_ = nullptr;
            gyroCallback_      = nullptr;
            if(accelStreamProfile_ == nullptr) {
                // all imu streams are off
                return;
            }
        }
        else {
            LOG_WARN("The profile is not valid IMU profile");
            return;
        }
    }

    try {
        trySendStopStreamVendorCmd();
    }
    catch(const std::exception &e) {
        LOG_ERROR("Exception occurred while send stop stream command: {}", e.what());
    }

    LOG_DEBUG("LiDARImuStreamer stop backend....");
    backend_->stopStream();
    accelCallback_      = nullptr;
    accelStreamProfile_ = nullptr;
    gyroCallback_       = nullptr;
    gyroStreamProfile_  = nullptr;
    running_            = false;
    LOG_DEBUG("LiDARImuStreamer stop finished.");
}

void LiDARImuStreamer::trySendStartStreamVendorCmd() {
    auto            propServer = owner_->getPropertyServer();
    OBPropertyValue value;
    uint32_t        frameRate = 0;

    if(accelStreamProfile_) {
        frameRate = static_cast<uint32_t>(utils::mapIMUSampleRateToValue(accelStreamProfile_->getSampleRate()));
    }
    else if(gyroStreamProfile_) {
        frameRate = static_cast<uint32_t>(utils::mapIMUSampleRateToValue(gyroStreamProfile_->getSampleRate()));
    }

    if(frameRate == 0) {
        throw invalid_value_exception("The IMU frame rate is invalid");
    }
    // set stream port
    value.intValue = backend_->getSocketPort();
    propServer->setPropertyValue(OB_PROP_IMU_STREAM_PORT_INT, value, PROP_ACCESS_INTERNAL);
    // set frame rate
    value.intValue = frameRate;
    propServer->setPropertyValue(OB_PROP_LIDAR_IMU_FRAME_RATE_INT, value, PROP_ACCESS_INTERNAL);
}

void LiDARImuStreamer::trySendStopStreamVendorCmd() {
    auto            propServer = owner_->getPropertyServer();
    OBPropertyValue value      = { 0 };

    propServer->setPropertyValue(OB_PROP_LIDAR_IMU_FRAME_RATE_INT, value, PROP_ACCESS_INTERNAL);
}

void LiDARImuStreamer::parseIMUData(std::shared_ptr<Frame> frame) {
    auto           data          = frame->getData();
    ImuDataHeader *header        = (ImuDataHeader *)data;
    auto           dataSize      = frame->getDataSize();
    const uint16_t imuDataSize   = 4 * 6;
    const uint16_t dataBlockSize = sizeof(ImuDataHeader) + TAIL_MAGIC_LEN + imuDataSize;

    // data block format: ImuDataHeader(40) || gyroX gyroY gyroZ accelX accelY accelZ || tail magic(FE FE FE FE)

    // check data size
    if(dataSize != dataBlockSize) {
        LOG_WARN("This LiDAR block data will be dropped because data size({}) is not equal to {}!", dataSize, dataBlockSize);
        return;
    }
    
    // convert to host order
    header->dataLen          = ntohs(header->dataLen);
    header->dataBlockNum     = ntohs(header->dataBlockNum);
    header->frameIndex       = ntohs(header->frameIndex);
    header->timestamp        = ntohll(header->timestamp);
    header->warningInfo      = ntohl(header->warningInfo);
    header->scanSpeed        = ntohs(header->scanSpeed);
    header->verticalScanRate = ntohs(header->verticalScanRate);
    header->temperature      = ntohs(header->temperature);

    // check header and tail magic
    if((0 != memcmp(header->magic, HEAD_MAGIC, HEAD_MAGIC_LEN)) || (0 != memcmp(data + dataSize - TAIL_MAGIC_LEN, TAIL_MAGIC, TAIL_MAGIC_LEN))) {
        LOG_WARN("This LiDAR block data will be dropped because magic is invalid!");
        return;
    }
    
    std::shared_ptr<const AccelStreamProfile> accelStreamProfile;
    std::shared_ptr<const GyroStreamProfile>  gyroStreamProfile;
    {
        std::lock_guard<std::mutex> lock(cbMtx_);
        accelStreamProfile = accelStreamProfile_;
        gyroStreamProfile = gyroStreamProfile_;
    }

    // move data ptr
    auto                   temperature = header->temperature * 0.01f;
    auto                   originData  = (OriginImuData *)(data + sizeof(ImuDataHeader));
    std::shared_ptr<Frame> accelFrame;
    std::shared_ptr<Frame> gyroFrame;
    auto                   frameIndex = ++frameIndex_;
    auto                   timestamp  = utils::getNowTimesUs(); // TODO 20250708: timestamp in header is invalid now, use system time
    if(accelStreamProfile) {
        accelFrame     = FrameFactory::createFrameFromStreamProfile(accelStreamProfile);
        auto frameData = (AccelFrame::Data *)accelFrame->getData();

        frameData->value.x = originData->accelX;
        frameData->value.y = originData->accelY;
        frameData->value.z = originData->accelZ;
        frameData->temp    = temperature;
        convertAccelUnit(accelFrame);

        accelFrame->setNumber(frameIndex);
        accelFrame->setTimeStampUsec(timestamp);
        accelFrame->setSystemTimeStampUsec(timestamp);
    }

    if(gyroStreamProfile) {
        gyroFrame      = FrameFactory::createFrameFromStreamProfile(gyroStreamProfile);
        auto frameData = (GyroFrame::Data *)gyroFrame->getData();

        frameData->value.x = originData->gyroX;
        frameData->value.y = originData->gyroY;
        frameData->value.z = originData->gyroZ;
        frameData->temp    = temperature;
        convertGyroUnit(gyroFrame);

        gyroFrame->setNumber(frameIndex);
        gyroFrame->setTimeStampUsec(timestamp);
        gyroFrame->setSystemTimeStampUsec(timestamp);
    }
    outputFrame(accelFrame, gyroFrame);
}

void LiDARImuStreamer::outputFrame(std::shared_ptr<Frame> accelFrame, std::shared_ptr<Frame> gyroFrame) {
    MutableFrameCallback accelCallback;
    MutableFrameCallback gyroCallback;
    // save callback
    {
        std::lock_guard<std::mutex> lock(cbMtx_);
        accelCallback = accelCallback_;
        gyroCallback  = gyroCallback_;
    }

    if(accelFrame) {
        accelCallback(accelFrame);
    }
    if(gyroFrame) {
        gyroCallback(gyroFrame);
    }
}

IDevice *LiDARImuStreamer::getOwner() const {
    return owner_;
}

void LiDARImuStreamer::convertAccelUnit(std::shared_ptr<Frame> frame) {
    if (firmwareVersionInt_ >= 1000007) {
        return;
    }

    const float GRAVITY = 9.80665f;
    auto frameData = (AccelFrame::Data*)frame->getData();
    frameData->value.x = frameData->value.x * GRAVITY;
    frameData->value.y = frameData->value.y * GRAVITY;
    frameData->value.z = frameData->value.z * GRAVITY;
}

void LiDARImuStreamer::convertGyroUnit(std::shared_ptr<Frame> frame) {
    if (firmwareVersionInt_ >= 1000007) {
        return;
    }

    constexpr float MY_PI = 3.14159265358979323846f;
    constexpr float DEG_2_RAD = MY_PI / 180.0f;

    auto frameData = (GyroFrame::Data*)frame->getData();
    frameData->value.x = frameData->value.x * DEG_2_RAD;
    frameData->value.y = frameData->value.y * DEG_2_RAD;
    frameData->value.z = frameData->value.z * DEG_2_RAD;
}

}  // namespace libobsensor