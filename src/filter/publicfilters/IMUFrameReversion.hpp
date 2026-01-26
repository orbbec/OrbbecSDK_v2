// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "IFilter.hpp"
#include <mutex>

namespace libobsensor {

class IMUFrameReversion : public IFilterBase {
public:
    IMUFrameReversion();
    virtual ~IMUFrameReversion() noexcept override;

    void               updateConfig(std::vector<std::string> &params) override;
    void               setConfigData(void *data, uint32_t size) override;
    const std::string &getConfigSchema() const override;
    void               reset() override {}

private:
    std::shared_ptr<Frame> process(std::shared_ptr<const Frame> frame) override;
};

}  // namespace libobsensor

