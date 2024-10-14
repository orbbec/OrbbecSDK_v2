// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "IFilter.hpp"
#include <mutex>

namespace libobsensor {

class PixelValueScaler : public IFilterBase {
public:
    PixelValueScaler();
    ~PixelValueScaler() noexcept override;

    void               updateConfig(std::vector<std::string> &params) override;
    const std::string &getConfigSchema() const override;
    void               reset() override {}

private:
    std::shared_ptr<Frame> process(std::shared_ptr<const Frame> frame) override;

protected:
    std::mutex mtx_;
    float      scale_ = 1.0f;
};

class ThresholdFilter : public IFilterBase {
public:
    ThresholdFilter();
    virtual ~ThresholdFilter() noexcept;

    void               updateConfig(std::vector<std::string> &params) override;
    const std::string &getConfigSchema() const override;
    void               reset() override {}

private:
    std::shared_ptr<Frame> process(std::shared_ptr<const Frame> frame) override;

protected:
    std::mutex mtx_;
    uint32_t   min_ = 0;
    uint32_t   max_ = 16000;
};

class PixelValueOffset : public IFilterBase {
public:
    PixelValueOffset();
    virtual ~PixelValueOffset() noexcept;

    void               updateConfig(std::vector<std::string> &params) override;
    const std::string &getConfigSchema() const override;
    void               reset() override {}

private:
    std::shared_ptr<Frame> process(std::shared_ptr<const Frame> frame) override;

protected:
    std::mutex mtx_;
    int8_t     offset_ = 0;
};

}  // namespace libobsensor

