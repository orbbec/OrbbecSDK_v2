// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "libobsensor/h/ObTypes.h"
#include "IFilter.hpp"
#include "FilterFactory.hpp"
#include "IDevice.hpp"
#include "DeviceComponentBase.hpp"

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
    virtual ~FilterCreationStrategyBase() noexcept override = default;

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
    static std::shared_ptr<IFilterCreationStrategy>       create(uint16_t vid, uint16_t pid, IDevice *owner);
};

// Subclass of FilterCreationStrategyBase
class DefaultFilterStrategy : public FilterCreationStrategyBase, DeviceComponentBase {
public:
    DefaultFilterStrategy(IDevice *owner);
    virtual ~DefaultFilterStrategy() noexcept override = default;

private:
    virtual std::vector<std::shared_ptr<IFilter>> createDepthFilters() override;
    virtual std::vector<std::shared_ptr<IFilter>> createColorFilters() override;
};

class DaBaiAFilterStrategy : public FilterCreationStrategyBase, DeviceComponentBase {
public:
    DaBaiAFilterStrategy(IDevice *owner);
    virtual ~DaBaiAFilterStrategy() noexcept override = default;

private:
    virtual std::vector<std::shared_ptr<IFilter>> createDepthFilters() override;
    virtual std::vector<std::shared_ptr<IFilter>> createColorFilters() override;
};

class Gemini330FilterStrategy : public FilterCreationStrategyBase, DeviceComponentBase, IIRFilterCreationStrategy {
public:
    Gemini330FilterStrategy(IDevice *owner);
    virtual ~Gemini330FilterStrategy() noexcept override = default;
    virtual std::vector<std::shared_ptr<IFilter>> createFilters(OBSensorType type) override;

private:
    virtual std::vector<std::shared_ptr<IFilter>> createDepthFilters() override;
    virtual std::vector<std::shared_ptr<IFilter>> createColorFilters() override;
    virtual std::vector<std::shared_ptr<IFilter>> createIRLeftFilters() override;
    virtual std::vector<std::shared_ptr<IFilter>> createIRRightFilters() override;
};

class Gemini305FilterStrategy : public FilterCreationStrategyBase, IIRFilterCreationStrategy {
public:
    virtual ~Gemini305FilterStrategy() noexcept override = default;
    virtual std::vector<std::shared_ptr<IFilter>> createFilters(OBSensorType type) override;

private:
    virtual std::vector<std::shared_ptr<IFilter>> createDepthFilters() override;
    virtual std::vector<std::shared_ptr<IFilter>> createColorFilters() override;
    virtual std::vector<std::shared_ptr<IFilter>> createIRLeftFilters() override;
    virtual std::vector<std::shared_ptr<IFilter>> createIRRightFilters() override;
    std::vector<std::shared_ptr<IFilter>>         createColorLeftFilters();
    std::vector<std::shared_ptr<IFilter>>         createColorRightFilters();
};

class Astra2FilterStrategy : public FilterCreationStrategyBase, DeviceComponentBase {
public:
    Astra2FilterStrategy(IDevice *owner);
    virtual ~Astra2FilterStrategy() noexcept override = default;

private:
    virtual std::vector<std::shared_ptr<IFilter>> createDepthFilters() override;
    virtual std::vector<std::shared_ptr<IFilter>> createColorFilters() override;
};

class Gemini2FilterStrategy : public FilterCreationStrategyBase, DeviceComponentBase {
public:
    Gemini2FilterStrategy(IDevice *owner);
    virtual ~Gemini2FilterStrategy() noexcept override = default;

private:
    virtual std::vector<std::shared_ptr<IFilter>> createDepthFilters() override;
    virtual std::vector<std::shared_ptr<IFilter>> createColorFilters() override;
};

class Gemini2XLFilterStrategy : public FilterCreationStrategyBase, DeviceComponentBase {
public:
    Gemini2XLFilterStrategy(IDevice *owner);
    virtual ~Gemini2XLFilterStrategy() noexcept override = default;

private:
    virtual std::vector<std::shared_ptr<IFilter>> createDepthFilters() override;
    virtual std::vector<std::shared_ptr<IFilter>> createColorFilters() override;
};

}  // namespace playback
}  // namespace libobsensor