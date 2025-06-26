// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "IFilter.hpp"
#include "libobsensor/h/ObTypes.h"
// #include "IProperty.hpp"
#include "InternalTypes.hpp"

namespace libobsensor {

class LiDARFormatConverter : public IFilterBase {
public:
    LiDARFormatConverter();

    virtual ~LiDARFormatConverter() override = default;

    void               updateConfig(std::vector<std::string> &params) override;
    const std::string &getConfigSchema() const override;
    void               reset() override {}

private:
    std::shared_ptr<Frame> process(std::shared_ptr<const Frame> frame) override;

private:
};

}  // namespace libobsensor
