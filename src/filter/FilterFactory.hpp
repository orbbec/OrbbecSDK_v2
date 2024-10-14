// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "IFilter.hpp"
#include "logger/Logger.hpp"
#include "privatefilters/PrivFilterLoader.hpp"
#include <memory>
#include <mutex>

namespace libobsensor {

class FilterFactory {
private:
    FilterFactory();

    static std::mutex                   instanceMutex_;
    static std::weak_ptr<FilterFactory> instanceWeakPtr_;

public:
    static std::shared_ptr<FilterFactory> getInstance();

    ~FilterFactory() noexcept;

    std::shared_ptr<IFilter>        createFilter(const std::string &filterName);
    std::shared_ptr<IFilter>        createPrivateFilter(const std::string &filterName, const std::string &activationKey);
    std::shared_ptr<IFilterCreator> getFilterCreator(const std::string &filterName);
    bool                            isFilterCreatorExists(const std::string &filterName);

private:
    std::map<std::string, std::shared_ptr<IFilterCreator>> filterCreators_;

    std::shared_ptr<Logger> logger_;  // Manages the lifecycle of the logger object.
};

}  // namespace libobsensor
