// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "IDevice.hpp"
#include "DeviceComponentBase.hpp"
#include "InternalTypes.hpp"
#include "PlaybackDevicePort.hpp"
#include <string>
#include <vector>

namespace libobsensor {

class PlaybackDepthPostFilterParamsManager : public DeviceComponentBase {
public:
    PlaybackDepthPostFilterParamsManager(IDevice *owner, std::shared_ptr<PlaybackDevicePort> port);
    virtual ~PlaybackDepthPostFilterParamsManager() override;

    FalsePositiveFilterParams *getFPFilterParams() {
        return &fpFilterConfig_;
    }

    std::vector<std::string> &getFPFilterUpdateParams() {
        return fpFilterParams_;
    }

    bool isFPFilterEnable() {
        return fpFilterEnable_;
    }

    std::vector<std::string> &getTemporalFilterUpdateParams() {
        return tempFilterParams_;
    }

    bool isTemporalFilterEnable() {
        return tempFilterEnable_;
    }

    std::vector<std::string> &getHoleFillingFilterUpdateParams() {
        return hfFilterParams_;
    }

    bool isHoleFillingFilterEnable() {
        return hfFilterEnable_;
    }

    std::vector<std::string> &getSpatialFastFilterUpdateParams() {
        return spatFastFilterParams_;
    }

    bool isSpatialFastFilterEnable() {
        return spatFastFilterEnable_;
    }

    std::vector<std::string> &getSpatialAdvancedFilterUpdateParams() {
        return spatAdvFilterParams_;
    }

    bool isSpatialAdvancedFilterEnable() {
        return spatAdvFilterEnable_;
    }

    std::vector<std::string> &getSpatialModerateFilterUpdateParams() {
        return spatMdFilterParams_;
    }

    bool isSpatialModerateFilterEnable() {
        return spatMdFilterEnable_;
    }

    std::vector<std::string> &getEdgeNoiseRemovalFilterUpdateParams() {
        return edgeNRFilterParams_;
    }

    bool isEdgeNoiseRemovalFilterEnable() {
        return edgeNRFilterEnable_;
    }

    std::vector<double> &getNoiseRemovalFilterUpdateParams() {
        return noiseRemFilterParams_;
    }

    bool isNoiseRemovalFilterEnable() {
        return noiseRemFilterEnable_;
    }

private:
    void fetchParamFromDevice();
    void parseFilterParams(const uint8_t *data, const uint16_t dataSize);
    bool calculateChecksum(const uint8_t *data, DepthPostFilterHeader *filterHeader);
    void parseFilterParamsV0102(const uint8_t *data, const uint16_t dataSize);

private:
    std::shared_ptr<PlaybackDevicePort> port_;

    std::string               headerVersion_;
    DepthPostFilterHeader     filterHeader_{};
    DepthPostFilterParams     depthPostFilterParams_{};
    FalsePositiveFilterParams fpFilterConfig_{};

    bool                     tempFilterEnable_;
    std::vector<std::string> tempFilterParams_;

    bool                     hfFilterEnable_;
    std::vector<std::string> hfFilterParams_;

    bool                     spatFastFilterEnable_;
    std::vector<std::string> spatFastFilterParams_;

    bool                     spatAdvFilterEnable_;
    std::vector<std::string> spatAdvFilterParams_;

    bool                     spatMdFilterEnable_;
    std::vector<std::string> spatMdFilterParams_;

    bool                     edgeNRFilterEnable_;
    std::vector<std::string> edgeNRFilterParams_;

    bool                     fpFilterEnable_;
    std::vector<std::string> fpFilterParams_;

    bool                noiseRemFilterEnable_;
    std::vector<double> noiseRemFilterParams_;
};

}  // namespace libobsensor
