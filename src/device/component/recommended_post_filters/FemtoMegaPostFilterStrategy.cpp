// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "RecommendedPostFilterStrategies.hpp"

#include "FilterFactory.hpp"
#include "frameprocessor/FrameProcessor.hpp"

namespace libobsensor {

namespace {

// Femto Mega (USB / Net / Mega I Net) and Femto Bolt share the same list.
class FemtoMegaBoltStrategy : public IRecommendedPostFilterStrategy {
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

            if(filterFactory->isFilterCreatorExists("SpatialAdvancedFilter")) {
                auto spatFilter = filterFactory->createFilter("SpatialAdvancedFilter");
                // magnitude, alpha, disp_diff, radius
                std::vector<std::string> params = { "1", "0.5", "5", "1" };
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

            if(filterFactory->isFilterCreatorExists("ThresholdFilter")) {
                auto ThresholdFilter = filterFactory->createFilter("ThresholdFilter");
                depthFilterList.push_back(ThresholdFilter);
            }

            for(size_t i = 0; i < depthFilterList.size(); i++) {
                auto filter = depthFilterList[i];
                filter->enable(false);
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
        return {};
    }
};

}  // namespace

std::shared_ptr<IRecommendedPostFilterStrategy> createFemtoMegaBoltPostFilterStrategy() {
    return std::make_shared<FemtoMegaBoltStrategy>();
}

}  // namespace libobsensor
