// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "RecommendedPostFilterStrategies.hpp"

#include "InternalTypes.hpp"
#include "FilterFactory.hpp"
#include "frameprocessor/FrameProcessor.hpp"
#include "comprehensivefilter/IDepthPostFilterParamsManager.hpp"

namespace libobsensor {

namespace {

// Gemini 330 family (G330Device / G330NetDevice).
class Gemini330Strategy : public IRecommendedPostFilterStrategy {
public:
    std::vector<std::shared_ptr<IFilter>> createFilters(IDevice *owner, OBSensorType type) override {
        auto filterFactory = FilterFactory::getInstance();
        if(type == OB_SENSOR_DEPTH) {
            // activate depth frame processor library
            owner->getComponentT<FrameProcessor>(OB_DEV_COMPONENT_DEPTH_FRAME_PROCESSOR, false);

            std::vector<std::shared_ptr<IFilter>> depthFilterList;

            auto depthPostFilterParamsManager = owner->getComponentT<IDepthPostFilterParamsManager>(OB_DEV_COMPONENT_DEPTH_POST_FILTER_PARAMS_MANAGER, false);

            if(filterFactory->isFilterCreatorExists("DecimationFilter")) {
                auto decimationFilter = filterFactory->createFilter("DecimationFilter");
                decimationFilter->enable(false);
                depthFilterList.push_back(decimationFilter);
            }

            if(filterFactory->isFilterCreatorExists("ThresholdFilter")) {
                auto thresholdFilter = filterFactory->createFilter("ThresholdFilter");
                thresholdFilter->enable(false);
                depthFilterList.push_back(thresholdFilter);
            }

            if(filterFactory->isFilterCreatorExists("HDRMerge")) {
                auto hdrMergeFilter = filterFactory->createFilter("HDRMerge");
                hdrMergeFilter->enable(false);
                depthFilterList.push_back(hdrMergeFilter);
            }

            if(filterFactory->isFilterCreatorExists("SequenceIdFilter")) {
                auto sequenceIdFilter = filterFactory->createFilter("SequenceIdFilter");
                sequenceIdFilter->enable(false);
                depthFilterList.push_back(sequenceIdFilter);
            }

            if(filterFactory->isFilterCreatorExists("SpatialFastFilter")) {
                auto spatFilter = filterFactory->createFilter("SpatialFastFilter");
                if(depthPostFilterParamsManager) {
                    spatFilter->updateConfig(depthPostFilterParamsManager->getSpatialFastFilterUpdateParams());
                    spatFilter->enable(depthPostFilterParamsManager->isSpatialFastFilterEnable());
                }
                else {
                    // radius
                    std::vector<std::string> params = { "3" };
                    spatFilter->updateConfig(params);
                    spatFilter->enable(false);
                }
                depthFilterList.push_back(spatFilter);
            }

            if(filterFactory->isFilterCreatorExists("SpatialModerateFilter")) {
                auto spatFilter = filterFactory->createFilter("SpatialModerateFilter");
                if(depthPostFilterParamsManager) {
                    spatFilter->updateConfig(depthPostFilterParamsManager->getSpatialModerateFilterUpdateParams());
                    spatFilter->enable(depthPostFilterParamsManager->isSpatialModerateFilterEnable());
                }
                else {
                    // magnitude, disp_diff, radius
                    std::vector<std::string> params = { "1", "160", "5" };
                    spatFilter->updateConfig(params);
                    spatFilter->enable(false);
                }
                depthFilterList.push_back(spatFilter);
            }

            if(filterFactory->isFilterCreatorExists("SpatialAdvancedFilter")) {
                auto spatFilter = filterFactory->createFilter("SpatialAdvancedFilter");
                if(depthPostFilterParamsManager) {
                    spatFilter->updateConfig(depthPostFilterParamsManager->getSpatialAdvancedFilterUpdateParams());
                    spatFilter->enable(depthPostFilterParamsManager->isSpatialAdvancedFilterEnable());
                }
                else {
                    // magnitude, alpha, disp_diff, radius
                    std::vector<std::string> params = { "1", "0.5", "160", "1" };
                    spatFilter->updateConfig(params);
                    spatFilter->enable(false);
                }
                depthFilterList.push_back(spatFilter);
            }

            if(filterFactory->isFilterCreatorExists("TemporalFilter")) {
                auto tempFilter = filterFactory->createFilter("TemporalFilter");
                if(depthPostFilterParamsManager) {
                    tempFilter->updateConfig(depthPostFilterParamsManager->getTemporalFilterUpdateParams());
                    tempFilter->enable(depthPostFilterParamsManager->isTemporalFilterEnable());
                }
                else {
                    // diff_scale, weight
                    std::vector<std::string> params = { "0.1", "0.4" };
                    tempFilter->updateConfig(params);
                    tempFilter->enable(false);
                }
                depthFilterList.push_back(tempFilter);
            }

            if(filterFactory->isFilterCreatorExists("HoleFillingFilter")) {
                auto hfFilter = filterFactory->createFilter("HoleFillingFilter");
                if(depthPostFilterParamsManager) {
                    hfFilter->updateConfig(depthPostFilterParamsManager->getHoleFillingFilterUpdateParams());
                    hfFilter->enable(depthPostFilterParamsManager->isHoleFillingFilterEnable());
                }
                else {
                    std::vector<std::string> params = { "2" };
                    hfFilter->updateConfig(params);
                    hfFilter->enable(false);
                }
                depthFilterList.push_back(hfFilter);
            }

            if(depthPostFilterParamsManager) {
                if(filterFactory->isFilterCreatorExists("FalsePositiveFilter")) {
                    auto comprehensiveFilter = filterFactory->createFilter("FalsePositiveFilter");
                    auto filterData          = depthPostFilterParamsManager->getFPFilterParams();
                    comprehensiveFilter->setConfigData(filterData, sizeof(FalsePositiveFilterParams));
                    comprehensiveFilter->updateConfig(depthPostFilterParamsManager->getFPFilterUpdateParams());
                    comprehensiveFilter->enable(depthPostFilterParamsManager->isFPFilterEnable());
                    depthFilterList.push_back(comprehensiveFilter);
                }
            }

            if(filterFactory->isFilterCreatorExists("DisparityTransform")) {
                auto dtFilter = filterFactory->createFilter("DisparityTransform");
                dtFilter->enable(true);
                depthFilterList.push_back(dtFilter);
            }
            return depthFilterList;
        }
        else if(type == OB_SENSOR_COLOR) {
            // activate color frame processor library
            owner->getComponentT<FrameProcessor>(OB_DEV_COMPONENT_COLOR_FRAME_PROCESSOR, false);

            std::vector<std::shared_ptr<IFilter>> colorFilterList;
            if(filterFactory->isFilterCreatorExists("DecimationFilter")) {
                auto decimationFilter = filterFactory->createFilter("DecimationFilter");
                decimationFilter->enable(false);
                colorFilterList.push_back(decimationFilter);
            }
            return colorFilterList;
        }
        else if(type == OB_SENSOR_IR_LEFT) {
            owner->getComponentT<FrameProcessor>(OB_DEV_COMPONENT_LEFT_IR_FRAME_PROCESSOR, false);
            std::vector<std::shared_ptr<IFilter>> leftIRFilterList;
            if(filterFactory->isFilterCreatorExists("SequenceIdFilter")) {
                auto sequenceIdFilter = filterFactory->createFilter("SequenceIdFilter");
                sequenceIdFilter->enable(false);
                leftIRFilterList.push_back(sequenceIdFilter);
            }
            return leftIRFilterList;
        }
        else if(type == OB_SENSOR_IR_RIGHT) {
            owner->getComponentT<FrameProcessor>(OB_DEV_COMPONENT_RIGHT_IR_FRAME_PROCESSOR, false);
            std::vector<std::shared_ptr<IFilter>> rightIRFilterList;
            if(filterFactory->isFilterCreatorExists("SequenceIdFilter")) {
                auto sequenceIdFilter = filterFactory->createFilter("SequenceIdFilter");
                sequenceIdFilter->enable(false);
                rightIRFilterList.push_back(sequenceIdFilter);
            }
            return rightIRFilterList;
        }
        return {};
    }
};

// DaBaiA (a Gemini 330 family variant with its own list).
class DabaiAStrategy : public IRecommendedPostFilterStrategy {
public:
    std::vector<std::shared_ptr<IFilter>> createFilters(IDevice *owner, OBSensorType type) override {
        auto filterFactory = FilterFactory::getInstance();
        if(type == OB_SENSOR_DEPTH) {
            std::vector<std::shared_ptr<IFilter>> depthFilterList;

            auto depthPostFilterParamsManager = owner->getComponentT<IDepthPostFilterParamsManager>(OB_DEV_COMPONENT_DEPTH_POST_FILTER_PARAMS_MANAGER, false);

            if(filterFactory->isFilterCreatorExists("DecimationFilter")) {
                auto decimationFilter = filterFactory->createFilter("DecimationFilter");
                decimationFilter->enable(false);
                depthFilterList.push_back(decimationFilter);
            }

            if(filterFactory->isFilterCreatorExists("SpatialAdvancedFilter")) {
                auto spatFilter = filterFactory->createFilter("SpatialAdvancedFilter");
                // magnitude, alpha, disp_diff, radius
                if(depthPostFilterParamsManager) {
                    spatFilter->updateConfig(depthPostFilterParamsManager->getSpatialAdvancedFilterUpdateParams());
                    spatFilter->enable(depthPostFilterParamsManager->isSpatialAdvancedFilterEnable());
                }
                else {
                    std::vector<std::string> params = { "1", "0.5", "160", "1" };
                    spatFilter->updateConfig(params);
                    spatFilter->enable(false);
                }
                depthFilterList.push_back(spatFilter);
            }

            if(filterFactory->isFilterCreatorExists("TemporalFilter")) {
                auto tempFilter = filterFactory->createFilter("TemporalFilter");
                if(depthPostFilterParamsManager) {
                    tempFilter->updateConfig(depthPostFilterParamsManager->getTemporalFilterUpdateParams());
                    tempFilter->enable(depthPostFilterParamsManager->isTemporalFilterEnable());
                }
                else {
                    // diff_scale, weight
                    std::vector<std::string> params = { "0.1", "0.4" };
                    tempFilter->updateConfig(params);
                    tempFilter->enable(false);
                }
                depthFilterList.push_back(tempFilter);
            }

            if(filterFactory->isFilterCreatorExists("HoleFillingFilter")) {
                auto hfFilter = filterFactory->createFilter("HoleFillingFilter");
                if(depthPostFilterParamsManager) {
                    hfFilter->updateConfig(depthPostFilterParamsManager->getHoleFillingFilterUpdateParams());
                    hfFilter->enable(depthPostFilterParamsManager->isHoleFillingFilterEnable());
                }
                else {
                    std::vector<std::string> params = { "2" };
                    hfFilter->updateConfig(params);
                    hfFilter->enable(false);
                }
                depthFilterList.push_back(hfFilter);
            }

            if(depthPostFilterParamsManager) {
                if(filterFactory->isFilterCreatorExists("FalsePositiveFilter")) {
                    auto falsePositiveFilter = filterFactory->createFilter("FalsePositiveFilter");
                    auto filterData          = depthPostFilterParamsManager->getFPFilterParams();
                    falsePositiveFilter->setConfigData(filterData, sizeof(FalsePositiveFilterParams));
                    falsePositiveFilter->updateConfig(depthPostFilterParamsManager->getFPFilterUpdateParams());
                    falsePositiveFilter->enable(depthPostFilterParamsManager->isFPFilterEnable());
                    depthFilterList.push_back(falsePositiveFilter);
                }
            }

            if(filterFactory->isFilterCreatorExists("DisparityTransform")) {
                auto dtFilter = filterFactory->createFilter("DisparityTransform");
                depthFilterList.push_back(dtFilter);
            }

            if(filterFactory->isFilterCreatorExists("ThresholdFilter")) {
                auto thresholdFilter = filterFactory->createFilter("ThresholdFilter");
                thresholdFilter->enable(false);
                depthFilterList.push_back(thresholdFilter);
            }
            return depthFilterList;
        }
        else if(type == OB_SENSOR_COLOR) {
            std::vector<std::shared_ptr<IFilter>> colorFilterList;
            if(filterFactory->isFilterCreatorExists("DecimationFilter")) {
                auto decimationFilter = filterFactory->createFilter("DecimationFilter");
                decimationFilter->enable(false);
                colorFilterList.push_back(decimationFilter);
            }
            return colorFilterList;
        }
        return {};
    }
};

}  // namespace

std::shared_ptr<IRecommendedPostFilterStrategy> createGemini330PostFilterStrategy() {
    return std::make_shared<Gemini330Strategy>();
}

std::shared_ptr<IRecommendedPostFilterStrategy> createDabaiAPostFilterStrategy() {
    return std::make_shared<DabaiAStrategy>();
}

}  // namespace libobsensor
