// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "GVCPRuntimeConfig.hpp"
#include "exception/ObException.hpp"

#include "logger/LoggerInterval.hpp"
#include "logger/LoggerHelper.hpp"
#include "utils/StringUtils.hpp"
#include "utils/Utils.hpp"
#include "environment/EnvConfig.hpp"

namespace libobsensor {

std::mutex                       GVCPRuntimeConfig::instanceMutex_;
std::weak_ptr<GVCPRuntimeConfig> GVCPRuntimeConfig::instanceWeakPtr_;

std::shared_ptr<GVCPRuntimeConfig> GVCPRuntimeConfig::getInstance() {
    std::unique_lock<std::mutex> lock(instanceMutex_);
    auto                         ctxInstance = instanceWeakPtr_.lock();
    if(!ctxInstance) {
        ctxInstance      = std::shared_ptr<GVCPRuntimeConfig>(new GVCPRuntimeConfig());
        instanceWeakPtr_ = ctxInstance;
    }
    return ctxInstance;
}

GVCPRuntimeConfig::GVCPRuntimeConfig() {
    loadGvcpConfig();
}

void GVCPRuntimeConfig::loadGvcpConfig() {
    auto             envConfig = EnvConfig::getInstance();
    std::string      value;
    OBGvcpPortScheme scheme = OB_GVCP_PORT_SCHEME_STANDARD;  // default

    if(envConfig->getStringValue("Device.GVCPPortScheme", value)) {
        if(value == "Standard") {
            scheme = OB_GVCP_PORT_SCHEME_STANDARD;
            LOG_DEBUG("GVCP STANDARD");
        }
        else if(value == "SchemeB") {
            scheme = OB_GVCP_PORT_SCHEME_B;
            LOG_DEBUG("GVCP_B");
        }
        else {
            LOG_DEBUG("Unknown GVCP port scheme, use standard scheme instead");
        }
    }
    setGvcpPortscheme(scheme);
}

uint16_t GVCPRuntimeConfig::getGvcpPort() const {
    return gvcpPort_.load();
}

OBGvcpPortScheme GVCPRuntimeConfig::getGvcpPortscheme() const {
    return gvcpPortScheme_.load();
}

void GVCPRuntimeConfig::setGvcpPortscheme(OBGvcpPortScheme scheme) {
    switch(scheme) {
    case OB_GVCP_PORT_SCHEME_STANDARD:
        gvcpPortScheme_ = scheme;
        gvcpPort_       = GVCP_PORT;
        break;
    case OB_GVCP_PORT_SCHEME_B:
        gvcpPortScheme_ = scheme;
        gvcpPort_       = GVCP_CUSTOM_PORT;
        break;
    default:
        THROW_INVALID_PARAM_EXCEPTION(utils::string::to_string() << "Invalid GVCP port scheme: " << scheme);
        break;
    }
}

}  // namespace libobsensor
