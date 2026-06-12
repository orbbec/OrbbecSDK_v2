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
        else if(!owner_->isPlaybackDevice()) {
            // Real device: keep warning. Playback device: silently skip the non-writable property.
            LOG_WARN("Unsupported property '{}', skipping setting", propertyId_);
        }
    }

    /**
     * @brief Implementation of ILeafHandler::exportValue
     */
    jsonmodel::ExportValue exportValue(const std::string &k) override {
        utils::unusedVar(k);
        auto propServer = owner_->getPropertyServer();
        if(propServer->isPropertySupported(propertyId_, PROP_OP_READ, PROP_ACCESS_INTERNAL)) {
            T value = propServer->getPropertyValueT<T>(propertyId_);
            return jsonmodel::makeScalar(value);
        }
        else {
            LOG_WARN("Unsupported property '{}', skipping getting, return nullvalue", propertyId_);
            return jsonmodel::ExportValue::nullValue();
        }
    }

    IDevice *getOwner() {
        return owner_;
    }

protected:
    IDevice *owner_{ nullptr };  // owner
    uint32_t propertyId_{};      // property id
};

/**
 * @brief Handler for color power line frequency mapping.
 */
class ColorPowerLineFrequencyHandler : public PropertyConfigHandler<int> {
public:
    explicit ColorPowerLineFrequencyHandler(IDevice *owner);
    ~ColorPowerLineFrequencyHandler() override = default;

    void                   set(const std::string &k, const Json::Value &v) override;
    jsonmodel::ExportValue exportValue(const std::string &k) override;

private:
    std::map<int, std::string> valueMapping_;
};

/**
 * @brief Handler for color preset mapping.
 */
class ColorPresetHandler : public PropertyConfigHandler<int> {
public:
    explicit ColorPresetHandler(IDevice *owner);
    ~ColorPresetHandler() override = default;

    void                   set(const std::string &k, const Json::Value &v) override;
    jsonmodel::ExportValue exportValue(const std::string &k) override;
};

}  // namespace libobsensor
