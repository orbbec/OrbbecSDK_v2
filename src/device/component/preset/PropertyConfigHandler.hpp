// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "utils/jsonmodel/Handler.hpp"
#include "IDevice.hpp"
#include "logger/Logger.hpp"
#include "utils/Utils.hpp"

namespace libobsensor {

/**
 * @brief Handler for bool/int/float property config
 */
template <typename T> class PropertyConfigHandler : public jsonmodel::ILeafHandler {
public:
    PropertyConfigHandler(IDevice *owner, uint32_t propertyId) : owner_(owner), propertyId_(propertyId) {};
    virtual ~PropertyConfigHandler() override = default;

    /**
     * @brief Implementation of ILeafHandler::set
     */
    void set(const std::string &k, const Json::Value &v) override {
        utils::unusedVar(k);

        T    value      = jsonmodel::JsonTraits<T>::from(v);
        auto propServer = owner_->getPropertyServer();
        if(propServer->isPropertySupported(propertyId_, PROP_OP_WRITE, PROP_ACCESS_INTERNAL)) {
            propServer->setPropertyValueT<T>(propertyId_, value);
        }
        else {
            LOG_WARN("Unsupported property '{}', skipping setting", propertyId_);
        }
    }

    /**
     * @brief Implementation of ILeafHandler::get
     */
    Json::Value get(const std::string &k) override {
        utils::unusedVar(k);
        auto propServer = owner_->getPropertyServer();
        if(propServer->isPropertySupported(propertyId_, PROP_OP_READ, PROP_ACCESS_INTERNAL)) {
            T value = propServer->getPropertyValueT<T>(propertyId_);
            return jsonmodel::JsonTraits<T>::to(value);
        }
        else {
            LOG_WARN("Unsupported property '{}', skipping getting, return nullvalue", propertyId_);
            return Json::Value(Json::nullValue);
        }
    }

    IDevice *getOwner() {
        return owner_;
    }

protected:
    IDevice *owner_{ nullptr };  // owner
    uint32_t propertyId_{};      // property id
};

}  // namespace libobsensor
