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
 * @brief LiDARStreamer class for TL2401 LiDAR
 */
class LiDARStreamer : public ILiDARStreamer {
public:
    LiDARStreamer(IDevice *owner, const std::shared_ptr<IDataStreamPort> &backend);
    LiDARStreamer(IDevice *owner, const std::shared_ptr<IDataStreamPort> &backend, std::vector<std::pair<std::string, std::shared_ptr<IFilter>>> filters);

    virtual ~LiDARStreamer() noexcept;

    virtual void     startStream(std::shared_ptr<const StreamProfile> profile, MutableFrameCallback callback) override;
    virtual void     stopStream(std::shared_ptr<const StreamProfile> profile) override;
    virtual IDevice *getOwner() const override;

private:
    void trySendStopStreamVendorCmd();
    void trySendStartStreamVendorCmd();
    void checkAndConvertProfile(std::shared_ptr<const StreamProfile> profile);
    void parseLiDARData(std::shared_ptr<Frame> frame);

    virtual void             outputFrame(std::shared_ptr<Frame> frame);
    std::shared_ptr<IFilter> getFormatConverter();
    std::shared_ptr<IFilter> getPointFilter();

    uint8_t calculateReflectivity(const float &distance, const uint16_t &pulseWidth, const uint16_t &targetFlag);
    void    copyToOBLiDARSpherePoint(const LiDARSpherePoint *point, OBLiDARSpherePoint *obPoint);

    typedef struct {
        float lowThresh;
        float mediumThresh;
    } ReflectivityFactors;

private:
    IDevice                             *owner_;
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

    std::vector<std::pair<std::string, std::shared_ptr<IFilter>>> filters_;

    ReflectivityFactors lowPowerFactors_;
    ReflectivityFactors highPowerFactors_;
};

}  // namespace libobsensor
