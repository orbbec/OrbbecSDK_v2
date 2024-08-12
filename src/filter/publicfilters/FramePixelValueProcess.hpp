#pragma once
#include "FilterBase.hpp"
#include <mutex>

namespace libobsensor {

class PixelValueScaler : public FilterBase {
public:
    PixelValueScaler(const std::string &name);
    ~PixelValueScaler() noexcept override;

    void               updateConfig(std::vector<std::string> &params) override;
    const std::string &getConfigSchema() const override;

private:
    std::shared_ptr<Frame> processFunc(std::shared_ptr<const Frame> frame) override;

protected:
    std::mutex mtx_;
    float scale_ = 1.0f;
};

class ThresholdFilter : public FilterBase {
public:
    ThresholdFilter(const std::string &name);
    virtual ~ThresholdFilter() noexcept;

    void               updateConfig(std::vector<std::string> &params) override;
    const std::string &getConfigSchema() const override;

private:
    std::shared_ptr<Frame> processFunc(std::shared_ptr<const Frame> frame) override;

protected:
    std::mutex mtx_;
    uint32_t min_ = 0;
    uint32_t max_ = 16000;
};

class PixelValueOffset : public FilterBase {
public:
    PixelValueOffset(const std::string &name);
    virtual ~PixelValueOffset() noexcept;

    void               updateConfig(std::vector<std::string> &params) override;
    const std::string &getConfigSchema() const override;

private:
    std::shared_ptr<Frame> processFunc(std::shared_ptr<const Frame> frame) override;

protected:
    std::mutex mtx_;
    int8_t offset_ = 0;
};

}  // namespace libobsensor
