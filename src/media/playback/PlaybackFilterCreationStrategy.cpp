// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "DevicePids.hpp"
#include "PlaybackFilterCreationStrategy.hpp"
#include "PlaybackDepthPostFilterParamsManager.hpp"

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

std::shared_ptr<IFilterCreationStrategy> FilterCreationStrategyFactory::create(uint16_t vid, uint16_t pid, IDevice *owner) {
    // Note: G330DevPids contains the pid of DaBaiA, so it needs to ensure the order, this point needs to be optimized subsequently.
    if(isDeviceInContainer(DaBaiADevPids, vid, pid)) {
        return std::make_shared<DaBaiAFilterStrategy>(owner);
    }
    else if(isDeviceInContainer(G330DevPids, vid, pid)) {
        return std::make_shared<Gemini330FilterStrategy>(owner);
    }
    else if(isDeviceInContainer(G435LeDevPids, vid, pid)) {
        return std::make_shared<Gemini2FilterStrategy>(owner);
    }
    else if(vid == ORBBEC_DEVICE_VID) {
        if(0x0671 == pid) {
            return std::make_shared<Gemini2XLFilterStrategy>(owner);
        }
        else if((std::find(Gemini2DevPids.begin(), Gemini2DevPids.end(), pid) != Gemini2DevPids.end())) {
            return std::make_shared<Gemini2FilterStrategy>(owner);
        }
        else if(std::find(Astra2DevPids.begin(), Astra2DevPids.end(), pid) != Astra2DevPids.end()) {
            return std::make_shared<Astra2FilterStrategy>(owner);
        }
        else if(std::find(FemtoMegaDevPids.begin(), FemtoMegaDevPids.end(), pid) != FemtoMegaDevPids.end()
                || std::find(FemtoBoltDevPids.begin(), FemtoBoltDevPids.end(), pid) != FemtoBoltDevPids.end()) {
            return std::make_shared<DefaultFilterStrategy>(owner);
        }
        else if(std::find(G305DevPids.begin(), G305DevPids.end(), pid) != G305DevPids.end()) {
            return std::make_shared<Gemini305FilterStrategy>();
        }
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
DefaultFilterStrategy::DefaultFilterStrategy(IDevice *owner) : DeviceComponentBase(owner) {}

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
DaBaiAFilterStrategy::DaBaiAFilterStrategy(IDevice *owner) : DeviceComponentBase(owner) {}

std::vector<std::shared_ptr<IFilter>> DaBaiAFilterStrategy::createDepthFilters() {
    auto                                  filterFactory = FilterFactory::getInstance();
    std::vector<std::shared_ptr<IFilter>> depthFilterList;

    auto owner                         = getOwner();
    auto depthPostrFilterParamsManager = owner->getComponentT<PlaybackDepthPostFilterParamsManager>(OB_DEV_COMPONENT_DEPTH_POST_FILTER_PARAMS_MANAGER, false);

    if(filterFactory->isFilterCreatorExists("DecimationFilter")) {
        auto decimationFilter = filterFactory->createFilter("DecimationFilter");
        decimationFilter->enable(false);
        depthFilterList.push_back(decimationFilter);
    }

    if(filterFactory->isFilterCreatorExists("SpatialAdvancedFilter")) {
        auto spatFilter = filterFactory->createFilter("SpatialAdvancedFilter");
        if(depthPostrFilterParamsManager) {
            spatFilter->updateConfig(depthPostrFilterParamsManager->getSpatialAdvancedFilterUpdateParams());
            spatFilter->enable(depthPostrFilterParamsManager->isSpatialAdvancedFilterEnable());
        }
        else {
            // magnitude, alpha, disp_diff, radius
            std::vector<std::string> params = { "1", "0.5", "160", "1" };
            spatFilter->enable(false);
            spatFilter->updateConfig(params);
        }
        depthFilterList.push_back(spatFilter);
    }

    if(filterFactory->isFilterCreatorExists("TemporalFilter")) {
        auto tempFilter = filterFactory->createFilter("TemporalFilter");
        if(depthPostrFilterParamsManager) {
            tempFilter->updateConfig(depthPostrFilterParamsManager->getTemporalFilterUpdateParams());
            tempFilter->enable(depthPostrFilterParamsManager->isTemporalFilterEnable());
        }
        else {
            // diff_scale, weight
            std::vector<std::string> params = { "0.1", "0.4" };
            tempFilter->enable(false);
            tempFilter->updateConfig(params);
        }
        depthFilterList.push_back(tempFilter);
    }

    if(filterFactory->isFilterCreatorExists("HoleFillingFilter")) {
        auto hfFilter = filterFactory->createFilter("HoleFillingFilter");
        if(depthPostrFilterParamsManager) {
            hfFilter->updateConfig(depthPostrFilterParamsManager->getHoleFillingFilterUpdateParams());
            hfFilter->enable(depthPostrFilterParamsManager->isHoleFillingFilterEnable());
        }
        else {
            std::vector<std::string> params = { "2" };
            hfFilter->enable(false);
            hfFilter->updateConfig(params);
        }
        depthFilterList.push_back(hfFilter);
    }

    if(depthPostrFilterParamsManager) {
        if(filterFactory->isFilterCreatorExists("FalsePositiveFilter")) {
            auto fpFilter   = filterFactory->createFilter("FalsePositiveFilter");
            auto filterData = depthPostrFilterParamsManager->getFPFilterParams();
            fpFilter->setConfigData(filterData, sizeof(FalsePositiveFilterParams));
            fpFilter->updateConfig(depthPostrFilterParamsManager->getFPFilterUpdateParams());
            fpFilter->enable(depthPostrFilterParamsManager->isFPFilterEnable());
            depthFilterList.push_back(fpFilter);
        }
    }

    if(filterFactory->isFilterCreatorExists("DisparityTransform")) {
        auto dtFilter = filterFactory->createFilter("DisparityTransform");
        depthFilterList.push_back(dtFilter);
    }

    if(filterFactory->isFilterCreatorExists("ThresholdFilter")) {
        auto ThresholdFilter = filterFactory->createFilter("ThresholdFilter");
        ThresholdFilter->enable(false);
        depthFilterList.push_back(ThresholdFilter);
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
Gemini330FilterStrategy::Gemini330FilterStrategy(IDevice *owner) : DeviceComponentBase(owner) {}

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

    if(filterFactory->isFilterCreatorExists("SpatialFastFilter")) {
        auto spatFilter = filterFactory->createFilter("SpatialFastFilter");
        // radius
        std::vector<std::string> params = { "3" };
        spatFilter->updateConfig(params);
        depthFilterList.push_back(spatFilter);
    }

    if(filterFactory->isFilterCreatorExists("SpatialModerateFilter")) {
        auto spatFilter = filterFactory->createFilter("SpatialModerateFilter");
        // magnitude, disp_diff, radius
        std::vector<std::string> params = { "1", "160", "3" };
        spatFilter->updateConfig(params);
        depthFilterList.push_back(spatFilter);
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
Astra2FilterStrategy::Astra2FilterStrategy(IDevice *owner) : DeviceComponentBase(owner) {}

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
Gemini2FilterStrategy::Gemini2FilterStrategy(IDevice *owner) : DeviceComponentBase(owner) {}

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
Gemini2XLFilterStrategy::Gemini2XLFilterStrategy(IDevice *owner) : DeviceComponentBase(owner) {}

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

// Gemini305FilterStrategy
std::vector<std::shared_ptr<IFilter>> Gemini305FilterStrategy::createFilters(OBSensorType type) {
    switch(type) {
    case OB_SENSOR_DEPTH:
        return createDepthFilters();
    case OB_SENSOR_COLOR:
        return createColorFilters();
    case OB_SENSOR_COLOR_LEFT:
        return createColorLeftFilters();
    case OB_SENSOR_COLOR_RIGHT:
        return createColorRightFilters();
    case OB_SENSOR_IR_LEFT:
        return createIRLeftFilters();
    case OB_SENSOR_IR_RIGHT:
        return createIRRightFilters();
    default:
        break;
    }
    return {};
}

std::vector<std::shared_ptr<IFilter>> Gemini305FilterStrategy::createDepthFilters() {
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

    if(filterFactory->isFilterCreatorExists("SpatialFastFilter")) {
        auto spatFilter = filterFactory->createFilter("SpatialFastFilter");
        // radius
        std::vector<std::string> params = { "3" };
        spatFilter->updateConfig(params);
        depthFilterList.push_back(spatFilter);
    }

    if(filterFactory->isFilterCreatorExists("SpatialModerateFilter")) {
        auto spatFilter = filterFactory->createFilter("SpatialModerateFilter");
        // magnitude, disp_diff, radius
        std::vector<std::string> params = { "1", "160", "3" };
        spatFilter->updateConfig(params);
        depthFilterList.push_back(spatFilter);
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

std::vector<std::shared_ptr<IFilter>> Gemini305FilterStrategy::createColorLeftFilters() {
    auto filterFactory = FilterFactory::getInstance();

    std::vector<std::shared_ptr<IFilter>> colorFilterList;
    if(filterFactory->isFilterCreatorExists("DecimationFilter")) {
        auto decimationFilter = filterFactory->createFilter("DecimationFilter");
        decimationFilter->enable(false);
        colorFilterList.push_back(decimationFilter);
    }
    return colorFilterList;
}

std::vector<std::shared_ptr<IFilter>> Gemini305FilterStrategy::createColorRightFilters() {
    auto filterFactory = FilterFactory::getInstance();

    std::vector<std::shared_ptr<IFilter>> colorFilterList;
    if(filterFactory->isFilterCreatorExists("DecimationFilter")) {
        auto decimationFilter = filterFactory->createFilter("DecimationFilter");
        decimationFilter->enable(false);
        colorFilterList.push_back(decimationFilter);
    }
    return colorFilterList;
}

std::vector<std::shared_ptr<IFilter>> Gemini305FilterStrategy::createColorFilters() {
    auto filterFactory = FilterFactory::getInstance();

    std::vector<std::shared_ptr<IFilter>> colorFilterList;
    if(filterFactory->isFilterCreatorExists("DecimationFilter")) {
        auto decimationFilter = filterFactory->createFilter("DecimationFilter");
        decimationFilter->enable(false);
        colorFilterList.push_back(decimationFilter);
    }
    return colorFilterList;
}

std::vector<std::shared_ptr<IFilter>> Gemini305FilterStrategy::createIRLeftFilters() {
    auto filterFactory = FilterFactory::getInstance();

    std::vector<std::shared_ptr<IFilter>> leftIRFilterList;
    if(filterFactory->isFilterCreatorExists("SequenceIdFilter")) {
        auto sequenceIdFilter = filterFactory->createFilter("SequenceIdFilter");
        sequenceIdFilter->enable(false);
        leftIRFilterList.push_back(sequenceIdFilter);
    }
    return leftIRFilterList;
}

std::vector<std::shared_ptr<IFilter>> Gemini305FilterStrategy::createIRRightFilters() {
    auto filterFactory = FilterFactory::getInstance();

    std::vector<std::shared_ptr<IFilter>> rightIRFilterList;
    if(filterFactory->isFilterCreatorExists("SequenceIdFilter")) {
        auto sequenceIdFilter = filterFactory->createFilter("SequenceIdFilter");
        sequenceIdFilter->enable(false);
        rightIRFilterList.push_back(sequenceIdFilter);
    }
    return rightIRFilterList;
}

}  // namespace playback
}  // namespace libobsensor