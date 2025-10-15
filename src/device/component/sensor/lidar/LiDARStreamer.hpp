// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "IFilter.hpp"
#include "ISourcePort.hpp"
#include "IDeviceComponent.hpp"

#include <atomic>
#include <map>
#include <mutex>

namespace libobsensor {

/**
 * @brief LiDARStreamer class for TL2401 LiDAR
 */
class LiDARStreamer : public IDeviceComponent {
public:
    LiDARStreamer(IDevice *owner, const std::shared_ptr<IDataStreamPort> &backend);
    virtual ~LiDARStreamer() noexcept;

    void start(std::shared_ptr<const StreamProfile> sp, MutableFrameCallback callback);
    void stop();

    IDevice *getOwner() const override;

private:
    void trySendStopStreamVendorCmd();
    void trySendStartStreamVendorCmd();
    void checkAndConvertProfile(std::shared_ptr<const StreamProfile> profile);
    void parseLiDARData(std::shared_ptr<Frame> frame);

private:
    /**
     * @brief LiDAR scan profile
     */
    typedef struct ScanProfile {
        OBFrameType frameType;        // frame type
        OBFormat    format;           // frame data format
        int32_t     scanSpeed;        // related to OBLiDARScanSpeed
        uint32_t    maxDataBlockNum;  // related to scan speed
        uint16_t    pointsNum;        // points num per block
        uint16_t    dataBlockSize;    // data block size per block
        uint32_t    frameSize;        // frame data size

        ScanProfile() : frameType(OB_FRAME_UNKNOWN), format(OB_FORMAT_UNKNOWN), scanSpeed(0), maxDataBlockNum(0), pointsNum(0), dataBlockSize(0), frameSize(0) {}

        void clear() {
            frameType       = OB_FRAME_UNKNOWN;
            format          = OB_FORMAT_UNKNOWN;
            scanSpeed       = 0;
            maxDataBlockNum = 0;
            pointsNum       = 0;
            dataBlockSize   = 0;
            maxDataBlockNum = 0;
            frameSize       = 0;
        }
    } ScanProfile;

    IDevice *                            owner_;
    std::shared_ptr<IDataStreamPort>     backend_;
    std::mutex                           mutex_;
    std::shared_ptr<const StreamProfile> profile_;
    ScanProfile                          scanProfile_;
    MutableFrameCallback                 callback_;
    std::atomic_bool                     running_;
    uint64_t                             frameIndex_;
    std::shared_ptr<Frame>               frame_;      // cache frame data
    uint32_t                             frameDataOffset_;  // frame data offset
    uint16_t                             expectedDataNumber_; // expected data block number in the next data block
};

}  // namespace libobsensor
