// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "RecommendedPostFilterStrategies.hpp"

#include "FilterFactory.hpp"

namespace libobsensor {

namespace {

// Astra2.
class Astra2Strategy : public IRecommendedPostFilterStrategy {
public:
    std::vector<std::shared_ptr<IFilter>> createFilters(IDevice *owner, OBSensorType type) override {
        (void)owner;
        auto filterFactory = FilterFactory::getInstance();
        if(type == OB_SENSOR_DEPTH) {
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

std::shared_ptr<IRecommendedPostFilterStrategy> createAstra2PostFilterStrategy() {
    return std::make_shared<Astra2Strategy>();
}

}  // namespace libobsensor
