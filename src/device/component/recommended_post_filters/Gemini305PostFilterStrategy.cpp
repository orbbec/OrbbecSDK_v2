// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "RecommendedPostFilterStrategies.hpp"

#include "FilterFactory.hpp"
#include "frameprocessor/FrameProcessor.hpp"

namespace libobsensor {

namespace {

// Gemini305.
class Gemini305Strategy : public IRecommendedPostFilterStrategy {
public:
    std::vector<std::shared_ptr<IFilter>> createFilters(IDevice *owner, OBSensorType type) override {
        auto filterFactory = FilterFactory::getInstance();
        if(type == OB_SENSOR_DEPTH) {
            // activate depth frame processor library
            owner->getComponentT<FrameProcessor>(OB_DEV_COMPONENT_DEPTH_FRAME_PROCESSOR, false);

            std::vector<std::shared_ptr<IFilter>> depthFilterList;

            if(filterFactory->isFilterCreatorExists("DecimationFilter")) {
                auto decimationFilter = filterFactory->createFilter("DecimationFilter");
                depthFilterList.push_back(decimationFilter);
            }

            if(filterFactory->isFilterCreatorExists("ThresholdFilter")) {
                auto ThresholdFilter = filterFactory->createFilter("ThresholdFilter");
                depthFilterList.push_back(ThresholdFilter);
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
                std::vector<std::string> params = { "1", "160", "5" };
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

            for(size_t i = 0; i < depthFilterList.size(); i++) {
                auto filter = depthFilterList[i];
                if(filter->getName() != "DisparityTransform") {
                    filter->enable(false);
                }
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
        else if(type == OB_SENSOR_COLOR_LEFT) {
            // activate color frame processor library
            owner->getComponentT<FrameProcessor>(OB_DEV_COMPONENT_LEFT_COLOR_FRAME_PROCESSOR, false);

            std::vector<std::shared_ptr<IFilter>> colorFilterList;
            if(filterFactory->isFilterCreatorExists("DecimationFilter")) {
                auto decimationFilter = filterFactory->createFilter("DecimationFilter");
                decimationFilter->enable(false);
                colorFilterList.push_back(decimationFilter);
            }
            return colorFilterList;
        }
        else if(type == OB_SENSOR_COLOR_RIGHT) {
            // activate color frame processor library
            owner->getComponentT<FrameProcessor>(OB_DEV_COMPONENT_RIGHT_COLOR_FRAME_PROCESSOR, false);

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

}  // namespace

std::shared_ptr<IRecommendedPostFilterStrategy> createGemini305PostFilterStrategy() {
    return std::make_shared<Gemini305Strategy>();
}

}  // namespace libobsensor
