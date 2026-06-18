// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "RecommendedPostFilterStrategies.hpp"

#include "FilterFactory.hpp"
#include "frameprocessor/FrameProcessor.hpp"

namespace libobsensor {

namespace {

// Gemini2 (G2Device).
class Gemini2Strategy : public IRecommendedPostFilterStrategy {
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
};

// Gemini2 XL.
class Gemini2XLStrategy : public IRecommendedPostFilterStrategy {
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
};

// Gemini 435Le.
class G435LeStrategy : public IRecommendedPostFilterStrategy {
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
                std::vector<std::string> params = { "1", "0.5", "64", "1" };
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

std::shared_ptr<IRecommendedPostFilterStrategy> createGemini2PostFilterStrategy() {
    return std::make_shared<Gemini2Strategy>();
}

std::shared_ptr<IRecommendedPostFilterStrategy> createGemini2XLPostFilterStrategy() {
    return std::make_shared<Gemini2XLStrategy>();
}

std::shared_ptr<IRecommendedPostFilterStrategy> createG435LePostFilterStrategy() {
    return std::make_shared<G435LeStrategy>();
}

}  // namespace libobsensor
