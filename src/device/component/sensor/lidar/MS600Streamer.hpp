// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "IFilter.hpp"
#include "ISourcePort.hpp"
#include "IDeviceComponent.hpp"
#include "ILiDARStreamer.hpp"
#include "InternalTypes.hpp"
#include <atomic>
#include <map>
#include <mutex>

namespace libobsensor {

/**
 * @brief MS600Streamer class for MS600 LiDAR
 */
class MS600Streamer : public ILiDARStreamer {
public:
    MS600Streamer(IDevice *owner, const std::shared_ptr<IDataStreamPort> &backend);
    virtual ~MS600Streamer() noexcept override;

    virtual void     startStream(std::shared_ptr<const StreamProfile> profile, MutableFrameCallback callback) override;
    virtual void     stopStream(std::shared_ptr<const StreamProfile> profile) override;
    virtual IDevice *getOwner() const override;

private:
    void trySendStopStreamVendorCmd();
    void trySendStartStreamVendorCmd();
    void checkAndConvertProfile(std::shared_ptr<const StreamProfile> profile);
    void parseLiDARData(std::shared_ptr<Frame> frame);

private:
    IDevice *                            owner_;
    std::shared_ptr<IDataStreamPort>     backend_;
    std::mutex                           mutex_;
    std::shared_ptr<const StreamProfile> profile_;
    LiDARProfileInfo                     profileInfo_;
    MutableFrameCallback                 callback_;
    std::atomic_bool                     running_;
    uint64_t                             frameIndex_;
    std::shared_ptr<Frame>               frame_;               // cache frame data
    uint32_t                             frameDataOffset_;     // frame data offset
    uint16_t                             expectedDataNumber_;  // expected data block number in the next data block
};

}  // namespace libobsensor
