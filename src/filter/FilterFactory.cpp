// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "FilterFactory.hpp"
#include "publicfilters/publicFilterLoader.hpp"
#include "privatefilters/PrivFilterLoader.hpp"
#include "utils/PublicTypeHelper.hpp"
#include "exception/ObException.hpp"

#include <mutex>

namespace libobsensor {

std::mutex                     FilterFactory::instanceMutex_;
std::weak_ptr<FilterFactory>   FilterFactory::instanceWeakPtr_;
std::shared_ptr<FilterFactory> FilterFactory::getInstance() {
    std::unique_lock<std::mutex> lk(instanceMutex_);
    auto                         instance = instanceWeakPtr_.lock();
    if(!instance) {
        instance         = std::shared_ptr<FilterFactory>(new FilterFactory());
        instanceWeakPtr_ = instance;
    }
    return instance;
}

FilterFactory::FilterFactory() : logger_(Logger::getInstance()) {
    auto publicFilterCreators  = PublicFilterCreatorLoader::getCreators();
    auto privateFilterCreators = PrivFilterCreatorLoader::getCreators();
    filterCreators_.insert(publicFilterCreators.begin(), publicFilterCreators.end());
    filterCreators_.insert(privateFilterCreators.begin(), privateFilterCreators.end());

    LOG_INFO("Registered {} filter creators", filterCreators_.size());
    for(auto &creator : filterCreators_) {
        LOG_INFO(" - {}", creator.first);
    }
}

FilterFactory::~FilterFactory() noexcept {}

std::shared_ptr<IFilter> FilterFactory::createFilter(const std::string &filterName) {
    auto iter = filterCreators_.find(filterName);
    if(iter == filterCreators_.end()) {
        throw invalid_value_exception("Invalid filter name, cannot find filter creator for filter name: " + filterName);
    }
    return iter->second->create();
}

std::shared_ptr<IFilter> FilterFactory::createPrivateFilter(const std::string &filterName, const std::string &activationKey) {
    auto iter = filterCreators_.find(filterName);
    if(iter == filterCreators_.end()) {
        throw invalid_value_exception("Invalid filter name, cannot find filter creator for filter name: " + filterName);
    }
    auto privFilterCreator = std::dynamic_pointer_cast<IPrivFilterCreator>(iter->second);
    if(!privFilterCreator) {
        LOG_WARN("Filter name {} is not a private filter, dose not need activation key to create filter", filterName);
        return iter->second->create();
    }
    return privFilterCreator->create(activationKey);
}

bool FilterFactory::isFilterCreatorExists(const std::string &filterName) {
    return filterCreators_.find(filterName) != filterCreators_.end();
}

std::shared_ptr<IFilterCreator> FilterFactory::getFilterCreator(const std::string &filterName) {
    auto iter = filterCreators_.find(filterName);
    if(iter == filterCreators_.end()) {
        throw invalid_value_exception("Invalid filter name, cannot find filter creator for filter name: " + filterName);
    }
    return iter->second;
}

}  // namespace libobsensor
