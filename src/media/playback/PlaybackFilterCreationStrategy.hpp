// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "libobsensor/h/ObTypes.h"
#include "IFilter.hpp"
#include "FilterFactory.hpp"

#include <memory>
#include <vector>
#include <unordered_map>
#include <functional>
#include <mutex>

namespace libobsensor {
namespace playback {

// FilterCreationStrategy
class IFilterCreationStrategy {
public:
    virtual ~IFilterCreationStrategy() noexcept = default;

    virtual std::vector<std::shared_ptr<IFilter>> createFilters(OBSensorType type) = 0;

protected:
    virtual std::vector<std::shared_ptr<IFilter>> createDepthFilters() = 0;
    virtual std::vector<std::shared_ptr<IFilter>> createColorFilters() = 0;
};

class IIRFilterCreationStrategy {
public:
    virtual ~IIRFilterCreationStrategy() noexcept = default;

protected:
    virtual std::vector<std::shared_ptr<IFilter>> createIRLeftFilters()  = 0;
    virtual std::vector<std::shared_ptr<IFilter>> createIRRightFilters() = 0;
};

class FilterCreationStrategyBase : public IFilterCreationStrategy {
public:
    virtual ~FilterCreationStrategyBase() noexcept = default;

    virtual std::vector<std::shared_ptr<IFilter>> createFilters(OBSensorType type) override;
};

// FilterCreationStrategyFactory
class FilterCreationStrategyFactory {
private:
    FilterCreationStrategyFactory() = default;

    static std::mutex                                   instanceMutex_;
    static std::weak_ptr<FilterCreationStrategyFactory> instanceWeakPtr_;

public:
    static std::shared_ptr<FilterCreationStrategyFactory> getInstance();
    static std::shared_ptr<IFilterCreationStrategy>       create(uint16_t pid);
};

// Subclass of FilterCreationStrategyBase
class DefaultFilterStrategy : public FilterCreationStrategyBase {
public:
    virtual ~DefaultFilterStrategy() noexcept override = default;

private:
    virtual std::vector<std::shared_ptr<IFilter>> createDepthFilters() override;
    virtual std::vector<std::shared_ptr<IFilter>> createColorFilters() override;
};

class DaBaiAFilterStrategy : public FilterCreationStrategyBase {
public:
    virtual ~DaBaiAFilterStrategy() noexcept override = default;

private:
    virtual std::vector<std::shared_ptr<IFilter>> createDepthFilters() override;
    virtual std::vector<std::shared_ptr<IFilter>> createColorFilters() override;
};

class Gemini330FilterStrategy : public FilterCreationStrategyBase, IIRFilterCreationStrategy {
public:
    virtual ~Gemini330FilterStrategy() noexcept override = default;
    virtual std::vector<std::shared_ptr<IFilter>> createFilters(OBSensorType type) override;

private:
    virtual std::vector<std::shared_ptr<IFilter>> createDepthFilters() override;
    virtual std::vector<std::shared_ptr<IFilter>> createColorFilters() override;
    virtual std::vector<std::shared_ptr<IFilter>> createIRLeftFilters() override;
    virtual std::vector<std::shared_ptr<IFilter>> createIRRightFilters() override;
};

class Astra2FilterStrategy : public FilterCreationStrategyBase {
public:
    virtual ~Astra2FilterStrategy() noexcept override = default;

private:
    virtual std::vector<std::shared_ptr<IFilter>> createDepthFilters() override;
    virtual std::vector<std::shared_ptr<IFilter>> createColorFilters() override;
};

class Gemini2FilterStrategy : public FilterCreationStrategyBase {
public:
    virtual ~Gemini2FilterStrategy() noexcept override = default;

private:
    virtual std::vector<std::shared_ptr<IFilter>> createDepthFilters() override;
    virtual std::vector<std::shared_ptr<IFilter>> createColorFilters() override;
};

class Gemini2XLFilterStrategy : public FilterCreationStrategyBase {
public:
    virtual ~Gemini2XLFilterStrategy() noexcept override = default;

private:
    virtual std::vector<std::shared_ptr<IFilter>> createDepthFilters() override;
    virtual std::vector<std::shared_ptr<IFilter>> createColorFilters() override;
};

}  // namespace playback
}  // namespace libobsensor