// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "RecommendedPostFilterStrategies.hpp"

#include "FilterFactory.hpp"

namespace libobsensor {

namespace {

// OpenNI devices.
class OpenNIStrategy : public IRecommendedPostFilterStrategy {
public:
    std::vector<std::shared_ptr<IFilter>> createFilters(IDevice *owner, OBSensorType type) override {
        (void)owner;
        if(type != OB_SENSOR_DEPTH) {
            return {};
        }
        auto                                  filterFactory = FilterFactory::getInstance();
        std::vector<std::shared_ptr<IFilter>> depthFilterList;

        if(filterFactory->isFilterCreatorExists("EdgeNoiseRemovalFilter")) {
            auto enrFilter = filterFactory->createFilter("EdgeNoiseRemovalFilter");
            enrFilter->enable(false);
            std::vector<std::string> params = { "6", "0", "120", "120", "1", "1280", "800" };
            enrFilter->updateConfig(params);
            depthFilterList.push_back(enrFilter);
        }

        if(filterFactory->isFilterCreatorExists("MgcNoiseRemovalFilter")) {
            auto mgcNRFilter = filterFactory->createFilter("MgcNoiseRemovalFilter");
            mgcNRFilter->enable(false);
            std::vector<std::string> params = { "80", "80", "640", "6", "0", "120", "120", "1280", "800" };
            mgcNRFilter->updateConfig(params);
            depthFilterList.push_back(mgcNRFilter);
        }

        if(filterFactory->isFilterCreatorExists("LutNoiseRemovalFilter")) {
            auto lutNRFilter = filterFactory->createFilter("LutNoiseRemovalFilter");
            lutNRFilter->enable(false);
            std::vector<std::string> params = { "1920", "1920", "1920", "1920", "1920", "200", "200", "1920", "800", "200",
                                                "200",  "800",  "800",  "800",  "800",  "800", "256", "1280", "800" };
            lutNRFilter->updateConfig(params);
            depthFilterList.push_back(lutNRFilter);
        }

        if(filterFactory->isFilterCreatorExists("SpatialFastFilter")) {
            auto spatFilter = filterFactory->createFilter("SpatialFastFilter");
            spatFilter->enable(false);
            std::vector<std::string> params = { "3" };
            spatFilter->updateConfig(params);
            depthFilterList.push_back(spatFilter);
        }

        if(filterFactory->isFilterCreatorExists("SpatialModerateFilter")) {
            auto spatFilter = filterFactory->createFilter("SpatialModerateFilter");
            spatFilter->enable(false);
            std::vector<std::string> params = { "2", "96", "5" };
            spatFilter->updateConfig(params);
            depthFilterList.push_back(spatFilter);
        }

        if(filterFactory->isFilterCreatorExists("SpatialAdvancedFilter")) {
            auto spatFilter = filterFactory->createFilter("SpatialAdvancedFilter");
            spatFilter->enable(false);
            std::vector<std::string> params = { "1", "0.5", "64", "1" };
            spatFilter->updateConfig(params);
            depthFilterList.push_back(spatFilter);
        }

        if(filterFactory->isFilterCreatorExists("TemporalFilter")) {
            auto tempFilter = filterFactory->createFilter("TemporalFilter");
            tempFilter->enable(false);
            std::vector<std::string> params = { "0.1", "0.4" };
            tempFilter->updateConfig(params);
            depthFilterList.push_back(tempFilter);
        }

        if(filterFactory->isFilterCreatorExists("HoleFillingFilter")) {
            auto hfFilter = filterFactory->createFilter("HoleFillingFilter");
            hfFilter->enable(false);
            depthFilterList.push_back(hfFilter);
        }

        if(filterFactory->isFilterCreatorExists("ThresholdFilter")) {
            auto thresholdFilter = filterFactory->createFilter("ThresholdFilter");
            depthFilterList.push_back(thresholdFilter);
        }
        return depthFilterList;
    }
};

}  // namespace

std::shared_ptr<IRecommendedPostFilterStrategy> createOpenNIPostFilterStrategy() {
    return std::make_shared<OpenNIStrategy>();
}

}  // namespace libobsensor
