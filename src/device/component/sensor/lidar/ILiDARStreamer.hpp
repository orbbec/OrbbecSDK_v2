// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "IFrame.hpp"
#include "IStreamProfile.hpp"
#include "IDeviceComponent.hpp"

namespace libobsensor {

/**
 * @brief LiDAR scan profile
 */
typedef struct LiDARScanProfile {
    OBFrameType frameType;        // frame type
    OBFormat    format;           // frame data format
    int32_t     scanSpeed;        // related to OBLiDARScanRate
    uint32_t    maxDataBlockNum;  // data block num per frame, related to scan speed
    uint16_t    pointsNum;        // points num per block
    uint16_t    dataBlockSize;    // data block size per block
    uint32_t    frameSize;        // frame data size

    LiDARScanProfile()
        : frameType(OB_FRAME_UNKNOWN), format(OB_FORMAT_UNKNOWN), scanSpeed(0), maxDataBlockNum(0), pointsNum(0), dataBlockSize(0), frameSize(0) {}

    void clear() {
        frameType       = OB_FRAME_UNKNOWN;
        format          = OB_FORMAT_UNKNOWN;
        scanSpeed       = 0;
        maxDataBlockNum = 0;
        pointsNum       = 0;
        dataBlockSize   = 0;
        frameSize       = 0;
    }
} LiDARScanProfile;

/**
 * @brief ILiDARStreamer interface
 */
class ILiDARStreamer : public IDeviceComponent {
public:
    virtual ~ILiDARStreamer() = default;

    virtual void start(std::shared_ptr<const StreamProfile> sp, MutableFrameCallback callback) = 0;
    virtual void stop()                                                                        = 0;
};

}  // namespace libobsensor
