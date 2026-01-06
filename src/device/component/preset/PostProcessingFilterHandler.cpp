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

Json::Value PostProcessingFilterHandler::get(const std::string &k) {
    utils::unusedVar(k);
    Json::Value node(Json::arrayValue);

    for(auto &filter: postFilters_) {
        Json::Value filterNode(Json::objectValue);

        // name and enable
        filterNode[kFilterNameKey] = filter->getName();
        filterNode[kEnableKey]     = filter->isEnabled();
        // configs
        auto configNames = filter->getConfigSchemaVec();
        for(const auto &configName: configNames) {
            auto item  = filter->getConfigSchemaItem(configName.name);
            auto value = filter->getConfigValue(configName.name);
            switch(item.type) {
            case OB_FILTER_CONFIG_VALUE_TYPE_INT:
                filterNode[configName.name] = jsonmodel::JsonTraits<int>::to(static_cast<int>(value));
                break;
            case OB_FILTER_CONFIG_VALUE_TYPE_BOOLEAN:
                filterNode[configName.name] = jsonmodel::JsonTraits<bool>::to(static_cast<bool>(value));
                break;
            case OB_FILTER_CONFIG_VALUE_TYPE_FLOAT:
            default:
                // default is double
                filterNode[configName.name] = jsonmodel::JsonTraits<double>::to(value);
                break;
            }
        }
        node.append(filterNode);
    }

    return node;
}

}  // namespace libobsensor
