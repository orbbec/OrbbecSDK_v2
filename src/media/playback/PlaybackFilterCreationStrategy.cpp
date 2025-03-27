// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "DevicePids.hpp"
#include "PlaybackFilterCreationStrategy.hpp"

#include <algorithm>

namespace libobsensor {
namespace playback {

std::mutex                                     FilterCreationStrategyFactory::instanceMutex_;
std::weak_ptr<FilterCreationStrategyFactory>   FilterCreationStrategyFactory::instanceWeakPtr_;
std::shared_ptr<FilterCreationStrategyFactory> FilterCreationStrategyFactory::getInstance() {
    std::unique_lock<std::mutex> lk(instanceMutex_);
    auto                         instance = instanceWeakPtr_.lock();
    if(!instance) {
        instance         = std::shared_ptr<FilterCreationStrategyFactory>(new FilterCreationStrategyFactory());
        instanceWeakPtr_ = instance;
    }
    return instance;
}

std::shared_ptr<IFilterCreationStrategy> FilterCreationStrategyFactory::create(uint16_t pid) {
    // Note: G330DevPids contains the pid of DaBaiA, so it needs to ensure the order, this point needs to be optimized subsequently.
    if(std::find(DaBaiADevPids.begin(), DaBaiADevPids.end(), pid) != DaBaiADevPids.end()) {
        return std::make_shared<DaBaiAFilterStrategy>();
    }
    else if(std::find(G330DevPids.begin(), G330DevPids.end(), pid) != G330DevPids.end()) {
        return std::make_shared<Gemini330FilterStrategy>();
    }
    else if(0x0671 == pid) {
        return std::make_shared<Gemini2XLFilterStrategy>();
    }
    else if(std::find(Gemini2DevPids.begin(), Gemini2DevPids.end(), pid) != Gemini2DevPids.end()) {
        return std::make_shared<Gemini2FilterStrategy>();
    }
    else if(std::find(Astra2DevPids.begin(), Astra2DevPids.end(), pid) != Astra2DevPids.end()) {
        return std::make_shared<Astra2FilterStrategy>();
    }
    else if(std::find(FemtoMegaDevPids.begin(), FemtoMegaDevPids.end(), pid) != FemtoMegaDevPids.end()
            || std::find(FemtoBoltDevPids.begin(), FemtoBoltDevPids.end(), pid) != FemtoBoltDevPids.end()) {
        return std::make_shared<DefaultFilterStrategy>();
    }
    return nullptr;
}

std::vector<std::shared_ptr<IFilter>> FilterCreationStrategyBase::createFilters(OBSensorType type) {
    switch(type) {
    case OB_SENSOR_DEPTH:
        return createDepthFilters();
    case OB_SENSOR_COLOR:
        return createColorFilters();
    default:
        break;
    }

    return {};
}

// DefaultFilterStrategy
std::vector<std::shared_ptr<IFilter>> DefaultFilterStrategy::createDepthFilters() {
    auto filterFactory = FilterFactory::getInstance();

    std::vector<std::shared_ptr<IFilter>> depthFilterList;
    if(filterFactory->isFilterCreatorExists("ThresholdFilter")) {
        auto ThresholdFilter = filterFactory->createFilter("ThresholdFilter");
        depthFilterList.push_back(ThresholdFilter);
    }
    return depthFilterList;
}
std::vector<std::shared_ptr<IFilter>> DefaultFilterStrategy::createColorFilters() {
    return {};
}

// DaBaiAFilterStrategy
std::vector<std::shared_ptr<IFilter>> DaBaiAFilterStrategy::createDepthFilters() {
    auto                                  filterFactory = FilterFactory::getInstance();
    std::vector<std::shared_ptr<IFilter>> depthFilterList;

    if(filterFactory->isFilterCreatorExists("DecimationFilter")) {
        auto decimationFilter = filterFactory->createFilter("DecimationFilter");
        depthFilterList.push_back(decimationFilter);
    }

    if(filterFactory->isFilterCreatorExists("SpatialAdvancedFilter")) {
        auto spatFilter = filterFactory->createFilter("SpatialAdvancedFilter");
        // magnitude, alpha, disp_diff, radius
        std::vector<std::string> params = { "1", "0.5", "160", "1" };
        spatFilter->updateConfig(params);
        depthFilterList.push_back(spatFilter);
    }

    if(filterFactory->isFilterCreatorExists("TemporalFilter")) {
        auto tempFilter = filterFactory->createFilter("TemporalFilter");
        // diff_scale, weight
        std::vector<std::string> params = { "0.1", "0.4" };
        tempFilter->updateConfig(params);
        depthFilterList.push_back(tempFilter);
    }

    if(filterFactory->isFilterCreatorExists("HoleFillingFilter")) {
        auto                     hfFilter = filterFactory->createFilter("HoleFillingFilter");
        std::vector<std::string> params   = { "2" };
        hfFilter->updateConfig(params);
        depthFilterList.push_back(hfFilter);
    }

    if(filterFactory->isFilterCreatorExists("DisparityTransform")) {
        auto dtFilter = filterFactory->createFilter("DisparityTransform");
        depthFilterList.push_back(dtFilter);
    }

    if(filterFactory->isFilterCreatorExists("ThresholdFilter")) {
        auto ThresholdFilter = filterFactory->createFilter("ThresholdFilter");
        depthFilterList.push_back(ThresholdFilter);
    }

    for(size_t i = 0; i < depthFilterList.size(); i++) {
        auto filter = depthFilterList[i];
        if(filter->getName() != "DisparityTransform") {
            filter->enable(false);
        }
    }
    return depthFilterList;
}

std::vector<std::shared_ptr<IFilter>> DaBaiAFilterStrategy::createColorFilters() {
    auto filterFactory = FilterFactory::getInstance();

    std::vector<std::shared_ptr<IFilter>> colorFilterList;
    if(filterFactory->isFilterCreatorExists("DecimationFilter")) {
        auto decimationFilter = filterFactory->createFilter("DecimationFilter");
        decimationFilter->enable(false);
        colorFilterList.push_back(decimationFilter);
    }
    return colorFilterList;
}

// Gemini330FilterStrategy
std::vector<std::shared_ptr<IFilter>> Gemini330FilterStrategy::createFilters(OBSensorType type) {
    switch(type) {
    case OB_SENSOR_DEPTH:
        return createDepthFilters();
    case OB_SENSOR_COLOR:
        return createColorFilters();
    case OB_SENSOR_IR_LEFT:
        return createIRLeftFilters();
    case OB_SENSOR_IR_RIGHT:
        return createIRRightFilters();
    default:
        break;
    }
    return {};
}

std::vector<std::shared_ptr<IFilter>> Gemini330FilterStrategy::createDepthFilters() {
    auto filterFactory = FilterFactory::getInstance();

    std::vector<std::shared_ptr<IFilter>> depthFilterList;
    if(filterFactory->isFilterCreatorExists("DecimationFilter")) {
        auto decimationFilter = filterFactory->createFilter("DecimationFilter");
        depthFilterList.push_back(decimationFilter);
    }

    if(filterFactory->isFilterCreatorExists("HDRMerge")) {
        auto hdrMergeFilter = filterFactory->createFilter("HDRMerge");
        depthFilterList.push_back(hdrMergeFilter);
    }

    if(filterFactory->isFilterCreatorExists("SequenceIdFilter")) {
        auto sequenceIdFilter = filterFactory->createFilter("SequenceIdFilter");
        depthFilterList.push_back(sequenceIdFilter);
    }

    if(filterFactory->isFilterCreatorExists("SpatialAdvancedFilter")) {
        auto spatFilter = filterFactory->createFilter("SpatialAdvancedFilter");
        // magnitude, alpha, disp_diff, radius
        std::vector<std::string> params = { "1", "0.5", "160", "1" };
        spatFilter->updateConfig(params);
        depthFilterList.push_back(spatFilter);
    }

    if(filterFactory->isFilterCreatorExists("TemporalFilter")) {
        auto tempFilter = filterFactory->createFilter("TemporalFilter");
        // diff_scale, weight
        std::vector<std::string> params = { "0.1", "0.4" };
        tempFilter->updateConfig(params);
        depthFilterList.push_back(tempFilter);
    }

    if(filterFactory->isFilterCreatorExists("HoleFillingFilter")) {
        auto                     hfFilter = filterFactory->createFilter("HoleFillingFilter");
        std::vector<std::string> params   = { "2" };
        hfFilter->updateConfig(params);
        depthFilterList.push_back(hfFilter);
    }

    if(filterFactory->isFilterCreatorExists("DisparityTransform")) {
        auto dtFilter = filterFactory->createFilter("DisparityTransform");
        depthFilterList.push_back(dtFilter);
    }

    if(filterFactory->isFilterCreatorExists("ThresholdFilter")) {
        auto ThresholdFilter = filterFactory->createFilter("ThresholdFilter");
        depthFilterList.push_back(ThresholdFilter);
    }

    for(size_t i = 0; i < depthFilterList.size(); i++) {
        auto filter = depthFilterList[i];
        if(filter->getName() != "DisparityTransform") {
            filter->enable(false);
        }
    }
    return depthFilterList;
}

std::vector<std::shared_ptr<IFilter>> Gemini330FilterStrategy::createColorFilters() {
    auto filterFactory = FilterFactory::getInstance();

    std::vector<std::shared_ptr<IFilter>> colorFilterList;
    if(filterFactory->isFilterCreatorExists("DecimationFilter")) {
        auto decimationFilter = filterFactory->createFilter("DecimationFilter");
        decimationFilter->enable(false);
        colorFilterList.push_back(decimationFilter);
    }
    return colorFilterList;
}

std::vector<std::shared_ptr<IFilter>> Gemini330FilterStrategy::createIRLeftFilters() {
    auto filterFactory = FilterFactory::getInstance();

    std::vector<std::shared_ptr<IFilter>> leftIRFilterList;
    if(filterFactory->isFilterCreatorExists("SequenceIdFilter")) {
        auto sequenceIdFilter = filterFactory->createFilter("SequenceIdFilter");
        sequenceIdFilter->enable(false);
        leftIRFilterList.push_back(sequenceIdFilter);
    }
    return leftIRFilterList;
}

std::vector<std::shared_ptr<IFilter>> Gemini330FilterStrategy::createIRRightFilters() {
    auto filterFactory = FilterFactory::getInstance();

    std::vector<std::shared_ptr<IFilter>> rightIRFilterList;
    if(filterFactory->isFilterCreatorExists("SequenceIdFilter")) {
        auto sequenceIdFilter = filterFactory->createFilter("SequenceIdFilter");
        sequenceIdFilter->enable(false);
        rightIRFilterList.push_back(sequenceIdFilter);
    }
    return rightIRFilterList;
}

// Astra2FilterStrategy
std::vector<std::shared_ptr<IFilter>> Astra2FilterStrategy::createDepthFilters() {
    auto filterFactory = FilterFactory::getInstance();

    std::vector<std::shared_ptr<IFilter>> depthFilterList;
    if(filterFactory->isFilterCreatorExists("DecimationFilter")) {
        auto decimationFilter = filterFactory->createFilter("DecimationFilter");
        depthFilterList.push_back(decimationFilter);
    }

    if(filterFactory->isFilterCreatorExists("SpatialAdvancedFilter")) {
        auto spatFilter = filterFactory->createFilter("SpatialAdvancedFilter");
        // magnitude, alpha, disp_diff, radius
        std::vector<std::string> params = { "1", "0.5", "160", "1" };
        spatFilter->updateConfig(params);
        depthFilterList.push_back(spatFilter);
    }

    if(filterFactory->isFilterCreatorExists("TemporalFilter")) {
        auto tempFilter = filterFactory->createFilter("TemporalFilter");
        // diff_scale, weight
        std::vector<std::string> params = { "0.1", "0.4" };
        tempFilter->updateConfig(params);
        depthFilterList.push_back(tempFilter);
    }

    if(filterFactory->isFilterCreatorExists("HoleFillingFilter")) {
        auto hfFilter = filterFactory->createFilter("HoleFillingFilter");
        depthFilterList.push_back(hfFilter);
    }

    if(filterFactory->isFilterCreatorExists("DisparityTransform")) {
        auto dtFilter = filterFactory->createFilter("DisparityTransform");
        depthFilterList.push_back(dtFilter);
    }

    if(filterFactory->isFilterCreatorExists("ThresholdFilter")) {
        auto thresholdFilter = filterFactory->createFilter("ThresholdFilter");
        depthFilterList.push_back(thresholdFilter);
    }

    for(size_t i = 0; i < depthFilterList.size(); i++) {
        auto filter = depthFilterList[i];
        if(filter->getName() != "DisparityTransform") {
            filter->enable(false);
        }
    }
    return depthFilterList;
}

std::vector<std::shared_ptr<IFilter>> Astra2FilterStrategy::createColorFilters() {
    auto filterFactory = FilterFactory::getInstance();

    std::vector<std::shared_ptr<IFilter>> colorFilterList;
    if(filterFactory->isFilterCreatorExists("DecimationFilter")) {
        auto decimationFilter = filterFactory->createFilter("DecimationFilter");
        decimationFilter->enable(false);
        colorFilterList.push_back(decimationFilter);
    }
    return colorFilterList;
}

// Gemini2FilterStrategy
std::vector<std::shared_ptr<IFilter>> Gemini2FilterStrategy::createDepthFilters() {
    auto filterFactory = FilterFactory::getInstance();

    std::vector<std::shared_ptr<IFilter>> depthFilterList;
    if(filterFactory->isFilterCreatorExists("EdgeNoiseRemovalFilter")) {
        auto enrFilter = filterFactory->createFilter("EdgeNoiseRemovalFilter");
        enrFilter->enable(false);
        // todo: set default values
        depthFilterList.push_back(enrFilter);
    }

    if(filterFactory->isFilterCreatorExists("SpatialAdvancedFilter")) {
        auto spatFilter = filterFactory->createFilter("SpatialAdvancedFilter");
        spatFilter->enable(false);
        // magnitude, alpha, disp_diff, radius
        std::vector<std::string> params = { "1", "0.5", "160", "1" };
        spatFilter->updateConfig(params);
        depthFilterList.push_back(spatFilter);
    }

    if(filterFactory->isFilterCreatorExists("TemporalFilter")) {
        auto tempFilter = filterFactory->createFilter("TemporalFilter");
        tempFilter->enable(false);
        // diff_scale, weight
        std::vector<std::string> params = { "0.1", "0.4" };
        tempFilter->updateConfig(params);
        depthFilterList.push_back(tempFilter);
    }

    if(filterFactory->isFilterCreatorExists("HoleFillingFilter")) {
        auto hfFilter = filterFactory->createFilter("HoleFillingFilter");
        hfFilter->enable(false);
        depthFilterList.push_back(hfFilter);
    }

    if(filterFactory->isFilterCreatorExists("DisparityTransform")) {
        auto dtFilter = filterFactory->createFilter("DisparityTransform");
        dtFilter->enable(true);
        depthFilterList.push_back(dtFilter);
    }

    if(filterFactory->isFilterCreatorExists("ThresholdFilter")) {
        auto ThresholdFilter = filterFactory->createFilter("ThresholdFilter");
        depthFilterList.push_back(ThresholdFilter);
    }
    return depthFilterList;
}

std::vector<std::shared_ptr<IFilter>> Gemini2FilterStrategy::createColorFilters() {
    return {};
}

// Gemini2XLFilterStrategy
std::vector<std::shared_ptr<IFilter>> Gemini2XLFilterStrategy::createDepthFilters() {
    auto filterFactory = FilterFactory::getInstance();

    std::vector<std::shared_ptr<IFilter>> depthFilterList;
    if(filterFactory->isFilterCreatorExists("EdgeNoiseRemovalFilter")) {
        auto enrFilter = filterFactory->createFilter("EdgeNoiseRemovalFilter");
        enrFilter->enable(false);
        // todo: set default values
        depthFilterList.push_back(enrFilter);
    }

    if(filterFactory->isFilterCreatorExists("SpatialAdvancedFilter")) {
        auto spatFilter = filterFactory->createFilter("SpatialAdvancedFilter");
        spatFilter->enable(false);
        // magnitude, alpha, disp_diff, radius
        std::vector<std::string> params = { "1", "0.5", "64", "1" };
        spatFilter->updateConfig(params);
        depthFilterList.push_back(spatFilter);
    }

    if(filterFactory->isFilterCreatorExists("TemporalFilter")) {
        auto tempFilter = filterFactory->createFilter("TemporalFilter");
        tempFilter->enable(false);
        // diff_scale, weight
        std::vector<std::string> params = { "0.1", "0.4" };
        tempFilter->updateConfig(params);
        depthFilterList.push_back(tempFilter);
    }

    if(filterFactory->isFilterCreatorExists("HoleFillingFilter")) {
        auto hfFilter = filterFactory->createFilter("HoleFillingFilter");
        hfFilter->enable(false);
        depthFilterList.push_back(hfFilter);
    }

    if(filterFactory->isFilterCreatorExists("DisparityTransform")) {
        auto dtFilter = filterFactory->createFilter("DisparityTransform");
        dtFilter->enable(true);
        depthFilterList.push_back(dtFilter);
    }

    if(filterFactory->isFilterCreatorExists("ThresholdFilter")) {
        auto ThresholdFilter = filterFactory->createFilter("ThresholdFilter");
        depthFilterList.push_back(ThresholdFilter);
    }

    return depthFilterList;
}

std::vector<std::shared_ptr<IFilter>> Gemini2XLFilterStrategy::createColorFilters() {
    return {};
}

}  // namespace playback
}  // namespace libobsensor