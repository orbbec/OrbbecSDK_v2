// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "libobsensor/h/ObTypes.h"
#include "utils/jsonmodel/Handler.hpp"
#include "IDevice.hpp"
#include "IFilter.hpp"
#include <map>
#include <vector>
#include <string>

namespace libobsensor {

/**
 * @brief Handler for post processing filter
 */
class PostProcessingFilterHandler : public jsonmodel::ILeafHandler {
public:
    PostProcessingFilterHandler(IDevice *owner, OBSensorType sensorType);
    ~PostProcessingFilterHandler() override = default;

    /**
     * @brief Implementation of ILeafHandler::set
     */
    void set(const std::string &k, const Json::Value &v) override;

    /**
     * @brief Implementation of ILeafHandler::get
     */
    Json::Value get(const std::string &k) override;

private:
    std::shared_ptr<IFilter> getFilter(const std::string &name);

private:
    // using FilterConfigs = std::vector<std::pair<std::string, double>>;

    IDevice                              *owner_ = nullptr;
    OBSensorType                          sensorType_;
    std::vector<std::shared_ptr<IFilter>> postFilters_;
};

}  // namespace libobsensor
