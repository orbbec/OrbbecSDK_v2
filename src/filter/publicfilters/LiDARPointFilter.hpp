// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "IFilter.hpp"
#include "libobsensor/h/ObTypes.h"
// #include "IProperty.hpp"
#include "InternalTypes.hpp"
#include <mutex>

namespace libobsensor {

class LiDARPointFilter : public IFilterBase {
public:
    LiDARPointFilter();
    ~LiDARPointFilter() noexcept override;

    void               updateConfig(std::vector<std::string> &params) override;
    const std::string &getConfigSchema() const override;
    void               reset() override {}

private:
    std::shared_ptr<Frame> process(std::shared_ptr<const Frame> frame) override;
    void                   frameDataFilter(OBLiDARSpherePoint *pointData, uint32_t pointCount, uint32_t totalPointNumber);

private:
    std::recursive_mutex paramsMutex_;

    uint32_t           filterLevel_;
    uint32_t           kernelSize_;
    std::vector<float> cacheDistanceTrans_;
};

}  // namespace libobsensor
