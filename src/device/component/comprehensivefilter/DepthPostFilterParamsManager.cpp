// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "DepthPostFilterParamsManager.hpp"
#include "exception/ObException.hpp"
#include "logger/Logger.hpp"
#include "component/property/InternalProperty.hpp"

namespace libobsensor {
DepthPostFilterParamsManager::DepthPostFilterParamsManager(IDevice *owner) : DeviceComponentBase(owner) {
    fetchParamFromDevice();
}

DepthPostFilterParamsManager::~DepthPostFilterParamsManager() noexcept {}

void DepthPostFilterParamsManager::fetchParamFromDevice() {
    auto owner      = getOwner();
    auto propServer = owner->getPropertyServer();
    std::vector<uint8_t> data;
    BEGIN_TRY_EXECUTE({
        propServer->getRawData(
            OB_RAW_DATA_DEPTH_POST_FILTER_PARAMS,
            [&](OBDataTranState state, OBDataChunk *dataChunk) {
                if(state == DATA_TRAN_STAT_TRANSFERRING) {
                    data.insert(data.end(), dataChunk->data, dataChunk->data + dataChunk->size);
                }
            },
            PROP_ACCESS_INTERNAL);
    })
    CATCH_EXCEPTION_AND_EXECUTE({
        LOG_WARN("Get depth poster filter raw data failed!");
        data.clear();
    })

    if(!data.empty()) {
        uint16_t dataSize = static_cast<uint16_t>(data.size());
        parseFilterParams(data.data(), dataSize);
        posterFilterRawData_.clear();
        posterFilterRawData_ = std::move(data);
    }
    else {
        throw libobsensor::invalid_value_exception("Invalid data size for depth poster filter.");
    }
}

void DepthPostFilterParamsManager::parseFilterParams(const uint8_t *data, const uint16_t dataSize) {
    uint16_t headerSize = static_cast<uint16_t>(sizeof(DepthPostFilterHeader));
    if(!data && dataSize < headerSize) {
        throw libobsensor::invalid_value_exception("Filter params parse error: data size is too small.");
    }

    // check magic
    memset(&filterHeader_, 0, headerSize);
    memcpy(&filterHeader_, data, headerSize);
    const uint8_t headerMagic[4] = { 'D', 'P', 'A', 'H' };
    if(memcmp(filterHeader_.magic, headerMagic, 4) != 0) {
        throw libobsensor::invalid_value_exception("Filter params parse error: magic check failed.");
    }

    // check header size
    if(filterHeader_.header_size != sizeof(DepthPostFilterHeader)) {
        throw libobsensor::invalid_value_exception("Filter params parse error: header size check failed.");
    }

    // check total size
    uint32_t totalSize = filterHeader_.header_size + filterHeader_.data_size;
    if(dataSize != totalSize) {
        throw libobsensor::invalid_value_exception("Filter params parse error: data total size check failed.");
    }

    // checksum
    if(!calculateChecksum(data, &filterHeader_)) {
        throw libobsensor::invalid_value_exception("Filter params parse error: checksum verification failed.");
    }

    if(filterHeader_.version == POSTFILTER_PARAMS_VERSION_0102) {
        parseFilterParamsV0102(data, dataSize);
        headerVersion_ = "v1.0.2";
    }
    else {
        throw libobsensor::invalid_value_exception(utils::string::to_string() << "Filter params parse error, got version: " << filterHeader_.version);
    }
}

bool DepthPostFilterParamsManager::calculateChecksum(const uint8_t *data, DepthPostFilterHeader *filterHeader) {
    const uint8_t *paramDataStart = data + filterHeader->header_size;
    uint32_t sum = 0;
    for(uint32_t i = 0; i < filterHeader->data_size; i++) {
        sum += paramDataStart[i];
    }
    return sum == filterHeader->checksum;
}

void DepthPostFilterParamsManager::parseFilterParamsV0102(const uint8_t *data, const uint16_t dataSize) {
    uint16_t totalParamsSize = static_cast<uint16_t>(sizeof(DepthPostFilterParams));
    if(dataSize != totalParamsSize) {
        throw libobsensor::invalid_value_exception("V0102 filter params parse error: data size mismatch.");
    }

    memset(&depthPostFilterParams_, 0, totalParamsSize);
    memcpy(&depthPostFilterParams_, data, totalParamsSize);
    fpFilterConfig_ = depthPostFilterParams_.fp_filter_params;

    TemporalFilterParams tempFilterParams = depthPostFilterParams_.temp_filter_params;
    tempFilterParams_.clear();
    tempFilterParams_.push_back(std::to_string(tempFilterParams.diff_scale));
    tempFilterParams_.push_back(std::to_string(tempFilterParams.weight));
    tempFilterEnable_ = tempFilterParams.enabled;

    HoleFillingFilterParams holeFilterParams = depthPostFilterParams_.hole_fill_filter_params;
    hfFilterParams_.clear();
    hfFilterParams_.push_back(std::to_string(holeFilterParams.hole_filling_mode));
    hfFilterEnable_ = holeFilterParams.enabled;

    SpatialFastFilterParams spatFastFilterParams = depthPostFilterParams_.spat_fast_filter_params;
    spatFastFilterParams_.push_back(std::to_string(spatFastFilterParams.win_size));
    spatFastFilterEnable_ = spatFastFilterParams.enabled;

    SpatialAdvancedFilterParams spatAdvFilterParams = depthPostFilterParams_.spat_adv_filter_params;
    spatAdvFilterParams_.clear();
    spatAdvFilterParams_.push_back(std::to_string(spatAdvFilterParams.magnitude));
    spatAdvFilterParams_.push_back(std::to_string(spatAdvFilterParams.alpha));
    spatAdvFilterParams_.push_back(std::to_string(spatAdvFilterParams.disp_diff));
    spatAdvFilterParams_.push_back(std::to_string(spatAdvFilterParams.radius));
    spatAdvFilterEnable_ = spatAdvFilterParams.enabled;

    SpatialModerateFilterParams spatMdFilterParams = depthPostFilterParams_.spat_mod_filter_params;
    spatMdFilterParams_.clear();
    spatMdFilterParams_.push_back(std::to_string(spatMdFilterParams.magnitude));
    spatMdFilterParams_.push_back(std::to_string(spatMdFilterParams.disp_diff));
    spatMdFilterParams_.push_back(std::to_string(spatMdFilterParams.win_size));
    spatMdFilterEnable_ = spatMdFilterParams.enabled;

    EdgeNoiseRemoveFilterParams edgeNMFilterParams = depthPostFilterParams_.edge_noise_rem_filter_params;
    edgeNRFilterParams_.clear();
    edgeNRFilterParams_.push_back(std::to_string(edgeNMFilterParams.margin_x_th));
    edgeNRFilterParams_.push_back(std::to_string(edgeNMFilterParams.margin_y_th));
    edgeNRFilterParams_.push_back(std::to_string(edgeNMFilterParams.limit_x_th));
    edgeNRFilterParams_.push_back(std::to_string(edgeNMFilterParams.limit_y_th));
    edgeNRFilterParams_.push_back(std::to_string(edgeNMFilterParams.enable_vertical_direction));
    edgeNRFilterParams_.push_back(std::to_string(edgeNMFilterParams.width));
    edgeNRFilterParams_.push_back(std::to_string(edgeNMFilterParams.height));
    edgeNRFilterEnable_ = edgeNMFilterParams.enabled;

    NoiseRemoveFilterParams noiseNMFilterParams = depthPostFilterParams_.noise_rem_params;
    noiseRemFilterParams_.clear();
    noiseRemFilterParams_.push_back(static_cast<double>(noiseNMFilterParams.max_size));
    noiseRemFilterParams_.push_back(static_cast<double>(noiseNMFilterParams.min_diff));
    noiseRemFilterParams_.push_back(static_cast<double>(noiseNMFilterParams.width));
    noiseRemFilterParams_.push_back(static_cast<double>(noiseNMFilterParams.height));
    noiseRemFilterEnable_ = noiseNMFilterParams.enabled;

    FalsePositiveFilterParams fpFilterParams = depthPostFilterParams_.fp_filter_params;
    fpFilterParams_.clear();
    fpFilterParams_.push_back(std::to_string(fpFilterParams.edge_bleed_params.is_edgeBleedNoise_enable));
    fpFilterParams_.push_back(std::to_string(fpFilterParams.edge_bleed_params.min_x_ratio));
    fpFilterParams_.push_back(std::to_string(fpFilterParams.edge_bleed_params.max_x_ratio));
    fpFilterParams_.push_back(std::to_string(fpFilterParams.edge_bleed_params.min_y_ratio));
    fpFilterParams_.push_back(std::to_string(fpFilterParams.edge_bleed_params.max_y_ratio));

    fpFilterParams_.push_back(std::to_string(fpFilterParams.texture_sparsity_params.is_textureSparsityNoise_enable));
    fpFilterParams_.push_back(std::to_string(fpFilterParams.texture_sparsity_params.min_x_ratio));
    fpFilterParams_.push_back(std::to_string(fpFilterParams.texture_sparsity_params.max_x_ratio));
    fpFilterParams_.push_back(std::to_string(fpFilterParams.texture_sparsity_params.min_y_ratio));
    fpFilterParams_.push_back(std::to_string(fpFilterParams.texture_sparsity_params.max_y_ratio));
    fpFilterParams_.push_back(std::to_string(fpFilterParams.texture_sparsity_params.max_depth));
    fpFilterParams_.push_back(std::to_string(fpFilterParams.texture_sparsity_params.max_size));

    fpFilterParams_.push_back(std::to_string(fpFilterParams.pattern_ambiguity_params.is_patternAmbiguityNoise_enable));
    fpFilterParams_.push_back(std::to_string(fpFilterParams.pattern_ambiguity_params.min_x_ratio));
    fpFilterParams_.push_back(std::to_string(fpFilterParams.pattern_ambiguity_params.max_x_ratio));
    fpFilterParams_.push_back(std::to_string(fpFilterParams.pattern_ambiguity_params.min_y_ratio));
    fpFilterParams_.push_back(std::to_string(fpFilterParams.pattern_ambiguity_params.max_y_ratio));
    fpFilterParams_.push_back(std::to_string(fpFilterParams.pattern_ambiguity_params.max_depth));
    fpFilterParams_.push_back(std::to_string(fpFilterParams.pattern_ambiguity_params.max_size));

    fpFilterEnable_ = fpFilterParams.enabled;
}

}  // namespace libobsensor
