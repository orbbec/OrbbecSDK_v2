// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "PostProcessingFilterHandler.hpp"
#include "PresetHandler.hpp"
#include "exception/ObException.hpp"
#include "PresetDefinitions.hpp"

namespace libobsensor {

PostProcessingFilterHandler::PostProcessingFilterHandler(IDevice *owner, OBSensorType sensorType) : owner_(owner), sensorType_(sensorType) {
    postFilters_ = owner_->createRecommendedPostProcessingFilters(sensorType);
}

std::shared_ptr<IFilter> PostProcessingFilterHandler::getFilter(const std::string &name) {
    for(auto &filter: postFilters_) {
        if(filter->getName() == name) {
            return filter;
        }
    }
    return nullptr;
}

void PostProcessingFilterHandler::set(const std::string &k, const Json::Value &v) {
    // Save all values
    if(!v.isArray()) {
        THROW_INVALID_PARAM_EXCEPTION("Node type mismatch of post processing filter: expected array! Key: " + k);
    }
    for(Json::ArrayIndex i = 0; i < v.size(); ++i) {
        const Json::Value &config = v[i];
        if(!config.isObject()) {
            THROW_INVALID_PARAM_EXCEPTION("Node type mismatch of post processing filter: expected object but leaf!");
        }
        if(!config.isMember(kFilterNameKey)) {
            THROW_INVALID_PARAM_EXCEPTION("Missing required field in post processing filter: " + std::string(kFilterNameKey));
        }
        auto filterName = jsonmodel::JsonTraits<std::string>::from(config[kFilterNameKey]);
        auto filter     = getFilter(filterName);
        if(filter == nullptr) {
            LOG_WARN("Filter not found for name '{}', skipping this setting", filterName);
            continue;
        }
        if(config.isMember(kEnableKey)) {
            // enable
            filter->enable(jsonmodel::JsonTraits<bool>::from(config[kEnableKey]));
        }

        auto nodeNames = config.getMemberNames();
        for(const auto &nodeName: nodeNames) {
            auto nodeValue = config[nodeName];
            if(nodeName == kEnableKey || nodeName == kFilterNameKey) {
                continue;
            }
            // other configs
            auto item = filter->getConfigSchemaItem(nodeName);
            if(item.name == nullptr) {
                // config is not supported by the filter
                LOG_WARN("The current config '{}' isn't supported by the filter '{}'", nodeName, filterName);
                continue;
            }
            // set value
            switch(item.type) {
            case OB_FILTER_CONFIG_VALUE_TYPE_INT:
                filter->setConfigValueSync(nodeName, jsonmodel::JsonTraits<int>::from(nodeValue));
                break;
            case OB_FILTER_CONFIG_VALUE_TYPE_BOOLEAN:
                filter->setConfigValueSync(nodeName, jsonmodel::JsonTraits<bool>::from(nodeValue));
                break;
            case OB_FILTER_CONFIG_VALUE_TYPE_FLOAT:
            default:
                // default is double
                filter->setConfigValueSync(nodeName, jsonmodel::JsonTraits<double>::from(nodeValue));
                break;
            }
        }
    }
}

jsonmodel::ExportValue PostProcessingFilterHandler::exportValue(const std::string &k) {
    utils::unusedVar(k);
    std::vector<jsonmodel::ExportValue> filterItems;
    filterItems.reserve(postFilters_.size());

    for(auto &filter: postFilters_) {
        std::vector<jsonmodel::ExportField> fields;
        fields.reserve(static_cast<size_t>(2 + filter->getConfigSchemaVec().size()));

        // name and enable
        fields.emplace_back(jsonmodel::makeField(kFilterNameKey, filter->getName()));
        fields.emplace_back(jsonmodel::makeField(kEnableKey, filter->isEnabled()));
        // configs
        auto configNames = filter->getConfigSchemaVec();
        for(const auto &configName: configNames) {
            auto item  = filter->getConfigSchemaItem(configName.name);
            auto value = filter->getConfigValue(configName.name);
            switch(item.type) {
            case OB_FILTER_CONFIG_VALUE_TYPE_INT:
                fields.emplace_back(jsonmodel::makeField(configName.name, static_cast<int>(value)));
                break;
            case OB_FILTER_CONFIG_VALUE_TYPE_BOOLEAN:
                fields.emplace_back(jsonmodel::makeField(configName.name, static_cast<bool>(value)));
                break;
            case OB_FILTER_CONFIG_VALUE_TYPE_FLOAT:
            default:
                // default is double
                fields.emplace_back(jsonmodel::makeField(configName.name, value));
                break;
            }
        }
        filterItems.emplace_back(jsonmodel::ExportValue::object(std::move(fields)));
    }

    return jsonmodel::ExportValue::array(std::move(filterItems));
}

}  // namespace libobsensor
