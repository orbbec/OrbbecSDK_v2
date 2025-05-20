// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "MS600Streamer.hpp"

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
    uint16_t startAngle;         // starting angle unit 0.01c
    uint16_t endAngle;           // end angle unit 0.01c
    uint16_t angleResolution;    // angle resolution unit 0.001c
    uint8_t  scanRate;           // 1:15HZ;2:20HZ;3:25HZ;4:30HZ;
    uint8_t  dataBlockNum;       // data block index based 1
    uint16_t frameIndex;         // 1~65535
    uint32_t timestamp;          // timestamp, 0~3600e6, unit us
    uint8_t  timestampSyncMode;  // 0: free run; 1: outer sync
    uint8_t  specificMode;       // 0-noraml; 1-fog mode
    uint32_t warningInfo;
    uint16_t contaminatedAngle;  // contaminated angle 45~315 degree
    uint8_t  contaminatedLevel;  // Dirty level 0-10
    uint16_t temperature;        // temperature, unit 0.01c
    uint16_t scanSpeed;          // scan speed, RPM
    uint8_t  reserved[5];        // reserve
} MS600DataHeader;

/**
 * @brief Cartesian coordinate system point
 */
typedef struct {
    uint16_t distance;   ///< distance, unit 2mm
    uint16_t intensity;  ///< intensity, 0~2000
} LiDARScanPoint;

#pragma pack(pop)

// data block magic
#define HEAD_MAGIC "\x4D\x53\x02\xF4\xEB\x90"  // header magic for ms600(the 3rd byte=0x02)
#define HEAD_MAGIC_LEN (6)
#define TAIL_MAGIC "\xFE\xFE\xFE\xFE"
#define TAIL_MAGIC_LEN (4)

/**
 * @brief convert original scan point to OBLiDARScanPoint
 *
 * @param[in] startAngle scan start angle
 * @param[in] angleResolution angle resolution
 * @param[in] index point data index(base 0) in the current data block
 * @param[in] point original scan point
 * @param[out] obPoint OBLiDARScanPoint
 */
static inline void convertToOBLiDARScanPoint(float startAngle, float angleResolution, uint16_t index, const LiDARScanPoint *point, OBLiDARScanPoint *obPoint) {
    // to host order and to unit mm
    obPoint->angle     = startAngle + index * angleResolution;
    obPoint->distance  = ntohs(point->distance) * 2.0f;
    obPoint->intensity = ntohs(point->intensity);
}

MS600Streamer::MS600Streamer(IDevice *owner, const std::shared_ptr<IDataStreamPort> &backend)
    : owner_(owner),
      backend_(backend),
      profile_(nullptr),
      callback_(nullptr),
      running_(false),
      frameIndex_(0),
      frame_(nullptr),
      frameDataOffset_(0),
      expectedDataNumber_(0) {
    LOG_DEBUG("MS600Streamer created");
}

MS600Streamer::~MS600Streamer() noexcept {
    try {
        stopStream(profile_);
    }
    catch(const std::exception &e) {
        LOG_ERROR("Exception occurred while destroying MS600Streamer: {}", e.what());
    }
}

void MS600Streamer::startStream(std::shared_ptr<const StreamProfile> profile, MutableFrameCallback callback) {
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
        LOG_DEBUG("MS600Streamer try to start backend stream...");
        backend_->startStream([this](std::shared_ptr<Frame> frame) { MS600Streamer::parseLiDARData(frame); });

        // 3. start LiDAR stream
        LOG_DEBUG("MS600Streamer try to send start stream command...");
        trySendStartStreamVendorCmd();
        running_ = true;
        LOG_DEBUG("MS600Streamer start backend stream finished...");
    })
    CATCH_EXCEPTION_AND_EXECUTE({
        frame_.reset();
        backend_->stopStream();
        callback_ = nullptr;
        running_  = false;
        throw;
    })
}

void MS600Streamer::stopStream(std::shared_ptr<const StreamProfile> profile) {
    LOG_DEBUG("MS600Streamer stop...");
    utils::unusedVar(profile);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if(!running_) {
            LOG_DEBUG("MS600Streamer stream is off...");
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

    LOG_DEBUG("MS600Streamer stop backend...");
    backend_->stopStream();
    profile_.reset();
    profileInfo_.clear();
    frameIndex_ = 0;
    frame_.reset();
    frameDataOffset_    = 0;
    expectedDataNumber_ = 0;
    running_            = false;
    LOG_DEBUG("MS600Streamer stop finished.");
}

void MS600Streamer::trySendStopStreamVendorCmd() {
    auto            owner      = getOwner();
    auto            propServer = owner->getPropertyServer();
    OBPropertyValue value      = { 0 };

    propServer->setPropertyValue(OB_PROP_LIDAR_STREAMING_ON_OFF_INT, value, PROP_ACCESS_INTERNAL);
}

void MS600Streamer::trySendStartStreamVendorCmd() {
    auto            propServer = owner_->getPropertyServer();
    OBPropertyValue value;

    // format
    if(profileInfo_.format != OB_FORMAT_LIDAR_SCAN) {
        throw invalid_value_exception("Invalid LiDAR format");
    }

    // speed
    value.intValue = propServer->getPropertyValueT<int32_t>(OB_PROP_LIDAR_SCAN_SPEED_INT);
    if(value.intValue != profileInfo_.scanSpeed) {
        value.intValue = profileInfo_.scanSpeed;
        propServer->setPropertyValueT<int32_t>(OB_PROP_LIDAR_SCAN_SPEED_INT, value.intValue);
    }

    // set streaming on
    value.intValue = 1;
    propServer->setPropertyValue(OB_PROP_LIDAR_STREAMING_ON_OFF_INT, value, PROP_ACCESS_INTERNAL);
}

void MS600Streamer::checkAndConvertProfile(std::shared_ptr<const StreamProfile> profile) {
    auto lidarProfile = profile->as<LiDARStreamProfile>();

    profileInfo_ = lidarProfile->getInfo();
}

void MS600Streamer::parseLiDARData(std::shared_ptr<Frame> frame) {
    const uint16_t   dataBlockSize   = profileInfo_.dataBlockSize;
    const uint32_t   maxDataBlockNum = profileInfo_.maxDataBlockNum;
    const uint16_t   pointsNum       = profileInfo_.pointsNum;
    const uint32_t   frameSize       = profileInfo_.frameSize;
    auto             data            = frame->getData();
    auto             dataSize        = frame->getDataSize();
    MS600DataHeader *header          = (MS600DataHeader *)data;

    // data block format: MS600DataHeader(40) || point 1 ... point n || tail magic(FE FE FE FE)
    // The input parameter "frame" represents a data block.
    // Each frame consists of n blocks (determined by the scan rate).
    // We must acquire all data blocks in order to assemble a complete data frame.
    // Currently, the out-of-order issue is not considered.
    // If the data is discontinuous or incomplete, the data blocks will be discarded.

    // check data size
    if(dataSize != dataBlockSize) {
        LOG_WARN("This LiDAR block data will be dropped because data size({}) is not equal to {}!", dataSize, dataBlockSize);
        return;
    }
    // convert to host order
    header->dataLen           = ntohs(header->dataLen);
    header->startAngle        = ntohs(header->startAngle);
    header->endAngle          = ntohs(header->endAngle);
    header->angleResolution   = ntohs(header->angleResolution);
    header->frameIndex        = ntohs(header->frameIndex);
    header->timestamp         = ntohl(header->timestamp);
    header->warningInfo       = ntohl(header->warningInfo);
    header->contaminatedAngle = ntohs(header->contaminatedAngle);
    header->temperature       = ntohs(header->temperature);
    header->scanSpeed         = ntohs(header->scanSpeed);

    // check header and tail magic
    if((0 != memcmp(header->magic, HEAD_MAGIC, HEAD_MAGIC_LEN)) || (0 != memcmp(data + dataSize - TAIL_MAGIC_LEN, TAIL_MAGIC, TAIL_MAGIC_LEN))) {
        LOG_WARN("This LiDAR block data will be dropped because magic is invalid!");
        return;
    }

    // check data size
    uint32_t pointDataSize = static_cast<uint32_t>(dataSize - sizeof(MS600DataHeader) - TAIL_MAGIC_LEN);  // point data size in this block data
    uint16_t curPointsNum  = static_cast<uint16_t>(pointDataSize / sizeof(LiDARScanPoint));

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
        frame_ = FrameFactory::createFrame(OB_FRAME_LIDAR_POINTS, OB_FORMAT_LIDAR_SCAN, frameSize);
        frame_->setStreamProfile(profile_);
        frameDataOffset_ = 0;
    }

    data += sizeof(MS600DataHeader);
    auto frameData = frame_->getDataMutable() + frameDataOffset_;
    // update data offset
    frameDataOffset_ += curPointsNum * sizeof(OBLiDARScanPoint);
    if(frameDataOffset_ <= frameSize) {
        // convert to ob scan point
        float startAngle      = header->startAngle * 0.01f;
        float angleResolution = header->angleResolution * 0.001f;
        for(uint16_t i = 0; i < curPointsNum; ++i) {
            auto point   = reinterpret_cast<const LiDARScanPoint *>(data) + i;
            auto obPoint = reinterpret_cast<OBLiDARScanPoint *>(frameData) + i;
            convertToOBLiDARScanPoint(startAngle, angleResolution, i, point, obPoint);
        }
    }
    else {
        LOG_WARN("This LiDAR block data will be dropped because frame data is invalid. Data number: {}", header->dataBlockNum);
        return;
    }

    // timestamp
    // Tips: timestamp in header cycles every 0 to 1 hour, use system time
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

IDevice *MS600Streamer::getOwner() const {
    return owner_;
}

}  // namespace libobsensor