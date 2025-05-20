// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "LiDARStreamer.hpp"

#include "frame/Frame.hpp"
#include "frame/FrameFactory.hpp"
#include "stream/StreamProfile.hpp"
#include "logger/LoggerInterval.hpp"
#include "utils/Utils.hpp"
#include "property/InternalProperty.hpp"
#include "IDevice.hpp"
#include "DevicePids.hpp"

#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

#undef min
#undef max
#include <iostream>
#include <cmath>
#include <algorithm>

namespace libobsensor {

#pragma pack(push, 1)
// Original LiDAR stream data header
typedef struct {
    uint8_t  magic[6];           // magic data, must be 0x4D 0x53 0x02 0xF4 0xEB 0x90
    uint16_t dataLen;            // data lenth
    uint8_t  model;              // 0: MS600; 1:TL2401
    uint8_t  scanRate;           // 1:5HZ;2:10HZ;3:15HZ;4:20HZ;
    uint16_t dataBlockNum;       // data block index based 1
    uint16_t frameIndex;         // 1~65535
    uint8_t  dataFormat;         // 0:IMU; 1:point cloud; 2: spherical point cloud;5: calibration point cloud
    uint64_t timestamp;          // timestamp, 0~3600e9, unit 1ns
    uint8_t  timestampSyncMode;  // 0: free run; 1: outer sync; 2: PTP sync
    uint32_t warningInfo;
    uint8_t  echoMode;          // 1:first echo; 2:last echo
    uint16_t scanSpeed;         // scan speed, RPM
    uint16_t verticalScanRate;  // vertical scan rate, unit 0.1Hz
    uint16_t apdtemperature;    // APD temperature, unit 0.01c
    uint8_t  reserved[5];       // reserve
} LiDARDataHeader;

/**
 * @brief Cartesian coordinate system point
 */
typedef struct {
    uint16_t x;             ///< X coordinate, unit 2mm
    uint16_t y;             ///< Y coordinate, unit 2mm
    uint16_t z;             ///< Z coordinate, unit 2mm
    uint8_t  reflectivity;  ///< reflectivity
    uint8_t  tag;           ///< tag
} LiDARPoint;

/**
 * @brief 3D point structure with LiDAR information
 */
typedef struct {
    uint16_t distance;      ///< distance, unit: 2mm
    uint16_t theta;         ///< azimuth angle, unit: 0.01 degrees
    uint16_t phi;           ///< zenith angle, unit: 0.01 degrees
    uint8_t  reflectivity;  ///< reflectance
    uint8_t  tag;           ///< tag
} LiDARSpherePoint;

#pragma pack(pop)

// data block magic
#define HEAD_MAGIC "\x4D\x53\x02\xF4\xEB\x90"
#define HEAD_MAGIC_LEN (6)
#define TAIL_MAGIC "\xFE\xFE\xFE\xFE"
#define TAIL_MAGIC_LEN (4)

#if !defined(_WIN32)
static inline uint64_t ntohll(uint64_t val) {
#if __BYTE_ORDER == __LITTLE_ENDIAN
    return (((uint64_t)ntohl(val & 0xFFFFFFFFULL)) << 32) | ntohl(val >> 32);
#else
    return val;
#endif
}
#endif

static inline void copyToOBLiDARSpherePoint(const LiDARSpherePoint *point, OBLiDARSpherePoint *obPoint) {
    // to host order and to unit mm / degrees
    obPoint->distance     = ntohs(point->distance) * 2.0f;
    obPoint->theta        = ntohs(point->theta) * 0.01f;
    obPoint->phi          = ntohs(point->phi) * 0.01f;
    obPoint->reflectivity = point->reflectivity;
    obPoint->tag          = point->tag;
}

static inline void convertToCartesianCoordinate(const LiDARSpherePoint *sphere, OBLiDARPoint *point) {
    // to host order
    float distance = ntohs(sphere->distance);
    float theta    = ntohs(sphere->theta);
    float phi      = ntohs(sphere->phi);

    constexpr float MY_PI = 3.14159265358979323846f;

    distance *= 2;                    // to unit mm
    theta *= 0.01f * MY_PI / 180.0f;  // unit 0.01 degrees to unit rad
    phi *= 0.01f * MY_PI / 180.0f;    // unit 0.01 degrees to unit rad

    point->x            = distance * cos(theta) * cos(phi);
    point->y            = distance * sin(theta) * cos(phi);
    point->z            = distance * sin(phi);
    point->reflectivity = sphere->reflectivity;
    point->tag          = sphere->tag;
}

LiDARStreamer::LiDARStreamer(IDevice *owner, const std::shared_ptr<IDataStreamPort> &backend)
    : owner_(owner),
      backend_(backend),
      profile_(nullptr),
      callback_(nullptr),
      running_(false),
      frameIndex_(0),
      frame_(nullptr),
      frameDataOffset_(0),
      expectedDataNumber_(0) {
    LOG_DEBUG("LiDARStreamer created");
}

LiDARStreamer::~LiDARStreamer() noexcept {
    try {
        stopStream(profile_);
    }
    catch(const std::exception &e) {
        LOG_ERROR("Exception occurred while destroying LiDARStreamer: {}", e.what());
    }
}

void LiDARStreamer::startStream(std::shared_ptr<const StreamProfile> profile, MutableFrameCallback callback) {
    LOG_INFO("Try to start stream: {}", profile);

    // check if stream is already running
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if(running_) {
            throw unsupported_operation_exception(utils::string::to_string() << "The LiDAR stream has already been started.");
            return;
        }
        // check stream profile and convert to scan profile
        checkAndConvertProfile(profile);
        profile_            = profile;
        callback_           = callback;
        running_            = true;
        expectedDataNumber_ = 1;  // the first data block
    }

    // 1. send command to stop stream
    try {
        trySendStopStreamVendorCmd();
    }
    catch(const std::exception &e) {
        LOG_WARN("Exception occurred while send stop stream command: {}", e.what());
    }

    BEGIN_TRY_EXECUTE({
        // 2. start backend stream
        LOG_DEBUG("LiDARStreamer try to start backend stream...");
        backend_->startStream([this](std::shared_ptr<Frame> frame) { LiDARStreamer::parseLiDARData(frame); });

        // 3. start LiDAR stream
        LOG_DEBUG("LiDARStreamer try to send start stream command...");
        trySendStartStreamVendorCmd();
        running_ = true;
        LOG_DEBUG("LiDARStreamer start backend stream finished...");
    })
    CATCH_EXCEPTION_AND_EXECUTE({
        frame_.reset();
        backend_->stopStream();
        callback_ = nullptr;
        running_  = false;
        throw;
    })
}

void LiDARStreamer::stopStream(std::shared_ptr<const StreamProfile> profile) {
    LOG_DEBUG("LiDARStreamer stop...");
    utils::unusedVar(profile);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if(!running_) {
            LOG_DEBUG("LiDARStreamer stream is off...");
            return;
        }
        callback_ = nullptr;
    }
    try {
        trySendStopStreamVendorCmd();
    }
    catch(const std::exception &e) {
        LOG_ERROR("Exception occurred while send stop stream command: {}", e.what());
    }

    LOG_DEBUG("LiDARStreamer stop backend...");
    backend_->stopStream();
    profile.reset();
    profileInfo_.clear();
    frameIndex_ = 0;
    frame_.reset();
    frameDataOffset_    = 0;
    expectedDataNumber_ = 0;
    running_            = false;
    LOG_DEBUG("LiDARStreamer stop finished.");
}

void LiDARStreamer::trySendStopStreamVendorCmd() {
    auto            owner      = getOwner();
    auto            propServer = owner->getPropertyServer();
    OBPropertyValue value      = { 0 };

    propServer->setPropertyValue(OB_PROP_LIDAR_STREAMING_ON_OFF_INT, value, PROP_ACCESS_INTERNAL);
}

void LiDARStreamer::trySendStartStreamVendorCmd() {
    auto            propServer = owner_->getPropertyServer();
    OBPropertyValue value;
    bool            isCalibrationMode = false;  // default is normal mode

    // get work mode
    try {
        auto workMode     = propServer->getPropertyValueT<int32_t>(OB_PROP_LIDAR_WORK_MODE_INT, PROP_ACCESS_INTERNAL);
        isCalibrationMode = workMode == 0x03;
    }
    catch(std::exception &e) {
        LOG_WARN("Get LiDAR work mode failed, error: %s", e.what());
        isCalibrationMode = false;  // default is normal mode
    }
    // format
    switch(profileInfo_.format) {
    case OB_FORMAT_LIDAR_POINT:
    case OB_FORMAT_LIDAR_SPHERE_POINT:
        // support by multi-lines LiDAR
        if(isCalibrationMode) {
            throw invalid_value_exception("Invalid LiDAR format");
        }
        break;
    case OB_FORMAT_LIDAR_CALIBRATION:
        // support by multi-lines LiDAR
        if(!isCalibrationMode) {
            throw invalid_value_exception("Invalid LiDAR format");
        }
        break;
    case OB_FORMAT_LIDAR_SCAN:  // support by single-line LiDAR
    default:
        throw invalid_value_exception("Invalid LiDAR format");
        break;
    }

    // speed
    value.intValue = profileInfo_.scanSpeed;
    propServer->setPropertyValue(OB_PROP_LIDAR_SCAN_SPEED_INT, value, PROP_ACCESS_INTERNAL);

    // set streaming on
    value.intValue = 1;
    propServer->setPropertyValue(OB_PROP_LIDAR_STREAMING_ON_OFF_INT, value, PROP_ACCESS_INTERNAL);
}

void LiDARStreamer::checkAndConvertProfile(std::shared_ptr<const StreamProfile> profile) {
    auto lidarProfile = profile->as<LiDARStreamProfile>();

    profileInfo_ = lidarProfile->getInfo();
}

void LiDARStreamer::parseLiDARData(std::shared_ptr<Frame> frame) {
    const uint16_t   dataBlockSize   = profileInfo_.dataBlockSize;
    const uint32_t   maxDataBlockNum = profileInfo_.maxDataBlockNum;
    const uint16_t   pointsNum       = profileInfo_.pointsNum;
    const uint32_t   frameSize       = profileInfo_.frameSize;
    auto             data            = frame->getData();
    auto             dataSize        = frame->getDataSize();
    LiDARDataHeader *header          = (LiDARDataHeader *)data;

    // data block format: LiDARDataHeader(40) || point 1 ... point n || tail magic(FE FE FE FE)
    // The input parameter "frame" represents a data block.
    // Each frame consists of n blocks (determined by the scan speed).
    // We must acquire all data blocks in order to assemble a complete data frame.
    // Currently, the out-of-order issue is not considered.
    // If the data is discontinuous or incomplete, the data blocks will be discarded.

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
    header->apdtemperature   = ntohs(header->apdtemperature);

    // check header and tail magic
    if((0 != memcmp(header->magic, HEAD_MAGIC, HEAD_MAGIC_LEN)) || (0 != memcmp(data + dataSize - TAIL_MAGIC_LEN, TAIL_MAGIC, TAIL_MAGIC_LEN))) {
        LOG_WARN("This LiDAR block data will be dropped because magic is invalid!");
        return;
    }

    // check data size
    uint32_t pointDataSize = static_cast<uint32_t>(dataSize - sizeof(LiDARDataHeader) - TAIL_MAGIC_LEN);  // point data size in this block data
    uint16_t curPointsNum  = 0;

    // format and data size
    OBFormat format = OB_FORMAT_UNKNOWN;
    switch(header->dataFormat) {
    case 2:
        // Sphere point -> OB_FORMAT_LIDAR_SPHERE_POINT
        if((profileInfo_.format != OB_FORMAT_LIDAR_POINT) && (profileInfo_.format != OB_FORMAT_LIDAR_SPHERE_POINT)) {
            break;
        }
        format       = OB_FORMAT_LIDAR_SPHERE_POINT;
        curPointsNum = static_cast<uint16_t>(pointDataSize / sizeof(LiDARSpherePoint));
        break;
    case 5:
        // Point in calibration mode
        if(profileInfo_.format != OB_FORMAT_LIDAR_CALIBRATION) {
            break;
        }
        format       = OB_FORMAT_LIDAR_CALIBRATION;
        curPointsNum = pointsNum;  // we don't know how many points in calibration data, set to default points num
        break;
    case 1:  // Cartesian point -> OB_FORMAT_LIDAR_POINT. The LiDAR never output data in this format
    default:
        break;
    }
    if(format == OB_FORMAT_UNKNOWN) {
        LOG_WARN("This LiDAR block data will be dropped because data format is unknown! format: {}, profile format: {}", header->dataFormat,
                 profileInfo_.format);
        return;
    }
    if(curPointsNum != pointsNum) {
        LOG_WARN("This LiDAR block data will be dropped because data point num({}) is not equal to {}", curPointsNum, pointsNum);
        return;
    }

    if(expectedDataNumber_ != header->dataBlockNum) {
        // not the first data block?
        if(header->dataBlockNum != 1) {
            expectedDataNumber_ = 1;  // reset to 1
            LOG_WARN("This LiDAR block data will be dropped because data block number({}) is not equal to {}", header->dataBlockNum, expectedDataNumber_);
            return;
        }
        // Received the new first data block
        if(frameDataOffset_ > 0) {
            LOG_WARN("This LiDAR last frame data will be dropped because we received the new first data block. Data size: {}", frameDataOffset_);
        }
        expectedDataNumber_ = 1;  // reset to 1
        frame_              = nullptr;
        frameDataOffset_    = 0;
    }

    // alloc frame memory
    if(1 == header->dataBlockNum) {
        // the first data block, all the frame memory
        frame_ = FrameFactory::createFrame(OB_FRAME_LIDAR_POINTS, format, frameSize);
        frame_->setStreamProfile(profile_);
        frameDataOffset_ = 0;
    }

    data += sizeof(LiDARDataHeader);
    auto frameData = frame_->getDataMutable() + frameDataOffset_;
    // convert coordinate system
    if(format == OB_FORMAT_LIDAR_SPHERE_POINT) {
        if(profileInfo_.format == OB_FORMAT_LIDAR_POINT) {
            // update data offset
            frameDataOffset_ += curPointsNum * sizeof(OBLiDARPoint);
            if(frameDataOffset_ <= frameSize) {
                // to ob cartesian coordinate
                for(uint16_t i = 0; i < curPointsNum; ++i) {
                    auto spherePoint = reinterpret_cast<const LiDARSpherePoint *>(data) + i;
                    auto point       = reinterpret_cast<OBLiDARPoint *>(frameData) + i;
                    convertToCartesianCoordinate(spherePoint, point);
                }
            }
            else {
                LOG_WARN("This LiDAR block data will be dropped because frame data is invalid. Data number: {}", header->dataBlockNum);
                return;
            }
        }
        else {
            // update data offset
            frameDataOffset_ += curPointsNum * sizeof(OBLiDARSpherePoint);
            if(frameDataOffset_ <= frameSize) {
                // copy to ob sphere point
                for(uint16_t i = 0; i < curPointsNum; ++i) {
                    auto point   = reinterpret_cast<const LiDARSpherePoint *>(data) + i;
                    auto obPoint = reinterpret_cast<OBLiDARSpherePoint *>(frameData) + i;
                    copyToOBLiDARSpherePoint(point, obPoint);
                }
            }
            else {
                LOG_WARN("This LiDAR block data will be dropped because frame data is invalid. Data number: {}", header->dataBlockNum);
                return;
            }
        }
    }
    else {
        // OB_FORMAT_LIDAR_CALIBRATION
        // just copy all data
        memcpy(frameData, data, pointDataSize);
        // update data offset
        frameDataOffset_ += pointDataSize;
    }

    // timestamp
    // TODO 20250417: timestamp in header is invalid now, use system time
    auto timestamp = utils::getNowTimesUs();
    frame_->setTimeStampUsec(timestamp);
    frame_->setSystemTimeStampUsec(timestamp);

    if(header->dataBlockNum >= maxDataBlockNum) {
        // reach the max data block num - all data for a circle
        // or get the last data block for a circle
        // Tips: for now, we do not consider out-of-order transmission or packet loss

        // update frame info
        auto frameIndex = frameIndex_++;
        frame_->setDataSize(frameDataOffset_);
        frame_->setNumber(frameIndex);
        // output frame
        std::lock_guard<std::mutex> lock(mutex_);
        if(callback_ && running_) {
            callback_(frame_);
        }
        // release the frame
        frame_              = nullptr;
        frameDataOffset_    = 0;
        expectedDataNumber_ = 1;  // reset to 1
    }
    else {
        // wait for more data
        ++expectedDataNumber_;
    }
}

IDevice *LiDARStreamer::getOwner() const {
    return owner_;
}

}  // namespace libobsensor