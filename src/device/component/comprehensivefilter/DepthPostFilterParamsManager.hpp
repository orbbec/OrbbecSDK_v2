// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "IDevice.hpp"
#include "DeviceComponentBase.hpp"
#include "IDepthPostFilterParamsManager.hpp"
#include "InternalTypes.hpp"
#include <string>
#include <vector>

namespace libobsensor {

class DepthPostFilterParamsManager : public DeviceComponentBase, public IDepthPostFilterParamsManager {
public:
    DepthPostFilterParamsManager(IDevice *owner);
    virtual ~DepthPostFilterParamsManager() override;

    std::vector<uint8_t> getRawData() {
        return posterFilterRawData_;
    }

    FalsePositiveFilterParams *getFPFilterParams() override {
        return &fpFilterConfig_;
    }

    std::vector<std::string> &getFPFilterUpdateParams() override {
        return fpFilterParams_;
    }

    bool isFPFilterEnable() override {
        return fpFilterEnable_;
    }

    std::vector<std::string> &getTemporalFilterUpdateParams() override {
        return tempFilterParams_;
    }

    bool isTemporalFilterEnable() override {
        return tempFilterEnable_;
    }

    std::vector<std::string> &getHoleFillingFilterUpdateParams() override {
        return hfFilterParams_;
    }

    bool isHoleFillingFilterEnable() override {
        return hfFilterEnable_;
    }

    std::vector<std::string> &getSpatialFastFilterUpdateParams() override {
        return spatFastFilterParams_;
    }

    bool isSpatialFastFilterEnable() override {
        return spatFastFilterEnable_;
    }

    std::vector<std::string> &getSpatialAdvancedFilterUpdateParams() override {
        return spatAdvFilterParams_;
    }

    bool isSpatialAdvancedFilterEnable() override {
        return spatAdvFilterEnable_;
    }

    std::vector<std::string> &getSpatialModerateFilterUpdateParams() override {
        return spatMdFilterParams_;
    }

    bool isSpatialModerateFilterEnable() override {
        return spatMdFilterEnable_;
    }

    std::vector<std::string> &getEdgeNoiseRemovalFilterUpdateParams() override {
        return edgeNRFilterParams_;
    }

    bool isEdgeNoiseRemovalFilterEnable() override {
        return edgeNRFilterEnable_;
    }

    std::vector<double> &getNoiseRemovalFilterUpdateParams() override {
        return noiseRemFilterParams_;
    }

    bool isNoiseRemovalFilterEnable() override {
        return noiseRemFilterEnable_;
    }

    void fetchParamFromDevice();

private:
    void parseFilterParams(const uint8_t *data, const uint16_t dataSize);
    bool calculateChecksum(const uint8_t *data, DepthPostFilterHeader *filterHeader);
    void parseFilterParamsV0102(const uint8_t *data, const uint16_t dataSize);

private:
    std::vector<uint8_t>      posterFilterRawData_;
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
